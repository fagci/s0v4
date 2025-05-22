#include "chscan.h"

#include "../driver/uart.h"
#include "../helper/bands.h"
#include "../helper/channels.h"
#include "../helper/lootlist.h"
#include "../helper/regs-menu.h"
#include "../helper/scan.h"
#include "../radio.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "apps.h"

static bool lastListenState;
static uint32_t timeout = 0;
static bool isWaiting;

void BANDSCAN_init(void) {
  CHANNELS_LoadScanlist(TYPE_FILTER_BAND, gSettings.currentScanlist);
  SCAN_Init(true);
}

void BANDSCAN_deinit(void) {}

void BANDSCAN_update(void) {
  if (gScanlistSize) {
    SCAN_Check(false);
  }
}

bool BANDSCAN_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED && REGSMENU_Key(key, state)) {
    return true;
  }
  bool longHeld = state == KEY_LONG_PRESSED;
  bool simpleKeypress = state == KEY_RELEASED;
  if ((longHeld || simpleKeypress) && (key > KEY_0 && key < KEY_9)) {
    gSettings.currentScanlist = CHANNELS_ScanlistByKey(
        gSettings.currentScanlist, key, longHeld && !simpleKeypress);
    CHANNELS_LoadScanlist(TYPE_FILTER_BAND, gSettings.currentScanlist);
    BANDS_SelectScan(0);
    SETTINGS_DelayedSave();
    isWaiting = false;
    return true;
  }
  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      SCAN_Next(key == KEY_UP);
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      SCAN_Next(true);
      return true;
    case KEY_SIDE2:
      LOOT_WhitelistLast();
      SCAN_Next(true);
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_PTT:
      if (gLastActiveLoot && !gSettings.keylock) {
        RADIO_TuneToSave(gLastActiveLoot->f);
        APPS_run(APP_VFO1);
        return true;
      }
    default:
      break;
    }
  }
  return false;
}

void BANDSCAN_render(void) {
  STATUSLINE_RenderRadioSettings();
  if (gScanlistSize) {
    PrintMediumBoldEx(LCD_XCENTER, 18, POS_C, C_FILL, "%s", gCurrentBand.name);
  }

  UI_RenderScanScreen();
  REGSMENU_Draw();
}
