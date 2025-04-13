#include "chscan.h"

#include "../driver/uart.h"
#include "../external/FreeRTOS/include/FreeRTOS.h"
#include "../external/FreeRTOS/include/portable.h"
#include "../external/FreeRTOS/include/timers.h"
#include "../external/FreeRTOS/portable/GCC/ARM_CM0/portmacro.h"
#include "../helper/channels.h"
#include "../helper/lootlist.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "apps.h"

CH activeCh;

static bool lastListenState;
static uint32_t timeout = 0;
static bool isWaiting;

static void nextWithTimeout() {
  if (lastListenState != gIsListening) {
    lastListenState = gIsListening;
    if (gIsListening) {
      CHANNELS_Load(radio.channel, &activeCh);
      isWaiting = true;
    }
    SetTimeout(&timeout, gIsListening
                             ? SCAN_TIMEOUTS[gSettings.sqOpenedTimeout]
                             : SCAN_TIMEOUTS[gSettings.sqClosedTimeout]);
  }

  if (CheckTimeout(&timeout)) {
    CHANNELS_Next(true);
    isWaiting = false;
    SetTimeout(&timeout, 0);
    return;
  }
}

void CHSCAN_init(void) {
  CHANNELS_LoadScanlist(TYPE_FILTER_CH, gSettings.currentScanlist);
  CHANNELS_LoadCurrentScanlistCH();
}

void CHSCAN_deinit(void) {}

void CHSCAN_update(void) {
  nextWithTimeout();
  vTaskDelay(pdMS_TO_TICKS(60));
  Measurement m = {
      .f = radio.rxF,
      .rssi = RADIO_GetRSSI(),
      .snr = RADIO_GetSNR(),
      .noise = BK4819_GetNoise(),
      .glitch = BK4819_GetGlitch(),
  };
  m.open = RADIO_IsSquelchOpen();
  LOOT_Update(&m);
  RADIO_ToggleRX(m.open);

  gRedrawScreen = true;
}

bool CHSCAN_key(KEY_Code_t key, Key_State_t state) {
  bool longHeld = state == KEY_LONG_PRESSED;
  bool simpleKeypress = state == KEY_RELEASED;
  if ((longHeld || simpleKeypress) && (key > KEY_0 && key < KEY_9)) {
    gSettings.currentScanlist = CHANNELS_ScanlistByKey(
        gSettings.currentScanlist, key, longHeld && !simpleKeypress);
    CHANNELS_LoadScanlist(TYPE_FILTER_CH, gSettings.currentScanlist);
    CHANNELS_LoadCurrentScanlistCH();
    SETTINGS_DelayedSave();
    isWaiting = false;
    return true;
  }
  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      nextWithTimeout();
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      nextWithTimeout();
      return true;
    case KEY_SIDE2:
      LOOT_WhitelistLast();
      nextWithTimeout();
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    default:
      break;
    }
  }
  return false;
}

void CHSCAN_render(void) {
  if (gIsListening) {
    PrintMediumBoldEx(LCD_XCENTER, 18, POS_C, C_FILL, "%s", activeCh.name);
    PrintMediumEx(LCD_XCENTER, 26, POS_C, C_FILL, "%u.%05u", radio.rxF / MHZ,
                  radio.rxF % MHZ);
    UI_RSSIBar(28);
  } else {
    if (gScanlistSize) {
      PrintMediumEx(LCD_XCENTER, 18, POS_C, C_FILL,
                    isWaiting ? "Waiting..." : "Scanning...");
      PrintMediumEx(LCD_XCENTER, 26, POS_C, C_FILL, "%u.%05u", radio.rxF / MHZ,
                    radio.rxF % MHZ);
    } else {
      PrintMediumEx(LCD_XCENTER, 18, POS_C, C_FILL, "Scanlist empty");
    }
  }
  uint16_t sl = gSettings.currentScanlist;
  PrintMediumEx(LCD_XCENTER, 44, POS_C, C_FILL, "%s %s %s %s %s %s %s %s",
                (sl >> 0) & 1 ? "01" : "__", //
                (sl >> 1) & 1 ? "02" : "__", //
                (sl >> 2) & 1 ? "03" : "__", //
                (sl >> 3) & 1 ? "04" : "__", //
                (sl >> 4) & 1 ? "05" : "__", //
                (sl >> 5) & 1 ? "06" : "__", //
                (sl >> 6) & 1 ? "07" : "__", //
                (sl >> 7) & 1 ? "08" : "__"  //
  );
  PrintMediumEx(LCD_XCENTER, 52, POS_C, C_FILL, "%s %s %s %s %s %s %s %s",
                (sl >> 8) & 1 ? "09" : "__",  //
                (sl >> 9) & 1 ? "10" : "__",  //
                (sl >> 10) & 1 ? "11" : "__", //
                (sl >> 11) & 1 ? "12" : "__", //
                (sl >> 12) & 1 ? "13" : "__", //
                (sl >> 13) & 1 ? "14" : "__", //
                (sl >> 14) & 1 ? "15" : "__", //
                (sl >> 15) & 1 ? "16" : "__"  //
  );
}
