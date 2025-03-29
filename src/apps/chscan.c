#include "chscan.h"

#include "../external/FreeRTOS/include/FreeRTOS.h"
#include "../external/FreeRTOS/include/portable.h"
#include "../external/FreeRTOS/include/timers.h"
#include "../external/FreeRTOS/portable/GCC/ARM_CM0/portmacro.h"
#include "../helper/channels.h"
#include "../helper/lootlist.h"
#include "../radio.h"
#include "../ui/components.h"
#include "../ui/graphics.h"

void CHSCAN_init(void) {}

void CHSCAN_deinit(void) {}

void CHSCAN_update(void) {
  if (!gIsListening) {
    CHANNELS_Next(true);
  }
  vTaskDelay(pdMS_TO_TICKS(60));
  Measurement m = {
      .f = radio.rxF,
      .rssi = RADIO_GetRSSI(),
      .snr = RADIO_GetSNR(),
      .noise = BK4819_GetNoise(),
      .glitch = BK4819_GetGlitch(),
  };
  m.open = RADIO_IsSquelchOpen();
  if (!gMonitorMode) {
    LOOT_Update(&m);
  }
  RADIO_ToggleRX(m.open);
  gRedrawScreen = true;
}

bool CHSCAN_key(KEY_Code_t Key, Key_State_t state) {
  if (state == KEY_RELEASED) {
    switch (Key) {
    case KEY_UP:
    case KEY_DOWN:
      return true;
    default:
      break;
    }
  }
  return false;
}

void CHSCAN_render(void) {
  if (gIsListening) {
    PrintMediumEx(LCD_XCENTER, 18, POS_C, C_FILL, "MR %u", radio.channel + 1);
    PrintSmallEx(LCD_XCENTER, 24, POS_C, C_FILL, "%u.%05u", radio.rxF / MHZ,
                 radio.rxF % MHZ);
    UI_RSSIBar(26);
  } else {
    PrintMediumEx(LCD_XCENTER, 18, POS_C, C_FILL, "Scanning...");
    PrintSmallEx(LCD_XCENTER, 24, POS_C, C_FILL, "%u.%05u", radio.rxF / MHZ,
                 radio.rxF % MHZ);
  }
}
