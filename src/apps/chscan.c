#include "chscan.h"

#include "../driver/system.h"
#include "../driver/uart.h"
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
  SYS_DelayMs(SQL_DELAY);
  RADIO_CheckAndListen();
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
  if (gScanlistSize) {
    if (gIsListening) {
      PrintMediumBoldEx(LCD_XCENTER, 18, POS_C, C_FILL, "%s", activeCh.name);
    } else {
      PrintMediumEx(LCD_XCENTER, 18, POS_C, C_FILL,
                    isWaiting ? "Waiting..." : "Scanning...");
    }
  }

  UI_RenderScanScreen();
}
