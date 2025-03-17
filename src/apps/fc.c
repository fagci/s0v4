#include "fc.h"
#include "../dcs.h"
#include "../driver/system.h"
#include "../driver/uart.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../settings.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "vfo1.h"
#include <stdint.h>

static const char *FILTER_NAMES[] = {
    [FILTER_OFF] = "ALL",
    [FILTER_VHF] = "VHF",
    [FILTER_UHF] = "UHF",
};

static bool bandAutoSwitch = false;
static uint32_t nextSwitch = 0;

static FreqScanTime T = F_SC_T_0_2s;
static uint32_t fcTimeMs;
static uint32_t scanF = 0;

static Filter filter = FILTER_OFF;
static uint32_t bound;

static const uint32_t DIFF = 300;

static bool scanning = false;

static void startScan() {
  T = gSettings.fcTime;
  fcTimeMs = 200 << T;
  BK4819_EnableFrequencyScanEx(T);
  scanning = true;
  Log("Scan START");
}

static void stopScan() {
  BK4819_StopScan();
  scanning = false;
  Log("Scan STOP");
}

static void gotF(uint32_t f) {
  stopScan();
  BK4819_RX_TurnOn();
  RADIO_TuneTo(f);
  RADIO_ToggleRX(true);
  SYS_DelayMs(60);
  gRedrawScreen = true;
  Log("[+] f: %u", f);
}

static void switchBand(bool automatic) {
  Log("Switch band auto=%u", automatic);
  stopScan();
  if (automatic) {
    switch (filter) {
    case FILTER_UHF:
      filter = FILTER_VHF;
      break;
    case FILTER_VHF:
    default:
      filter = FILTER_UHF;
      break;
    }
  } else {
    filter = IncDecU(filter, 0, 3, true);
  }
  // scanF = 0;
  BK4819_SelectFilterEx(filter);
  bound = SETTINGS_GetFilterBound();
  startScan();
}

void FC_init() {
  RADIO_LoadCurrentVFO();

  gMonitorMode = false;
  RADIO_ToggleRX(false);

  bound = SETTINGS_GetFilterBound();
  startScan();
}

void FC_deinit() {
  stopScan();
  BK4819_RX_TurnOn();
}

void FC_update() {
  if (gIsListening) {
    Log("Listening & check");
    vTaskDelay(pdMS_TO_TICKS(60));
    RADIO_CheckAndListen();
    gRedrawScreen = true;
    return;
  }

  if (bandAutoSwitch && Now() >= nextSwitch) {
    switchBand(true);
    stopScan();
    nextSwitch = Now() + (uint32_t)(fcTimeMs * 2) + 200;
    return;
  }

  uint32_t f = 0;
  if (scanning && !BK4819_GetFrequencyScanResult(&f)) {
    return;
  }

  if (!scanning) {
    startScan();
    return;
  }

  if (!f || (f >= 8800000 && f < 10800000)) {
    return;
  }
  gRedrawScreen = true;

  /* if ((filter == FILTER_VHF && f >= bound) ||
      (filter == FILTER_UHF && f < bound)) {
    return;
  } */

  if (filter == FILTER_VHF && f >= bound) {
    Log("F not in VHF %u", f);
    f /= 2;
  }

  if (filter == FILTER_UHF && f < bound) {
    Log("F not in UHF %u", f);
    f *= 2;
  }

  Loot *loot = LOOT_Get(f);
  if (loot && (loot->blacklist || loot->whitelist)) {
    Log("F in BL/WL %u", f);
    return;
  }

  Log("check %u <=> %u", f, scanF);
  if (DeltaF(f, scanF) < DIFF) {
    gotF(f);
    return;
  }
  scanF = f;
  stopScan();
  vTaskDelay(pdMS_TO_TICKS(1));
}

bool FC_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    default:
      break;
    }
  }

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_1:
    case KEY_7:
      gSettings.fcTime = IncDecI(gSettings.fcTime, 0, 3 + 1, key == KEY_1);
      SETTINGS_DelayedSave();
      FC_init();
      break;
    case KEY_3:
    case KEY_9:
      radio->squelch.value = IncDecU(radio->squelch.value, 0, 11, key == KEY_3);
      RADIO_SaveCurrentVFODelayed();
      RADIO_Setup();
      break;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      return true;
    case KEY_SIDE2:
      LOOT_WhitelistLast();
      return true;
    case KEY_F:
      switchBand(false);
      return true;
    case KEY_0:
      bandAutoSwitch = !bandAutoSwitch;
      return true;
    case KEY_PTT:
      if (gLastActiveLoot) {
        stopScan();
        RADIO_TuneToSave(gLastActiveLoot->f);
        gVfo1ProMode = true;
        APPS_run(APP_VFO1);
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

void FC_render() {
  PrintMediumEx(0, 16, POS_L, C_FILL, "%s %ums SQ %u %s", FILTER_NAMES[filter],
                fcTimeMs, radio->squelch.value, bandAutoSwitch ? "[A]" : "");
  UI_BigFrequency(40, scanF);

  if (gLastActiveLoot) {
    UI_DrawLoot(gLastActiveLoot, LCD_WIDTH, 48, POS_R);

    if (gLastActiveLoot->ct != 0xFF) {
      PrintSmallEx(LCD_WIDTH, 40 + 8 + 6, POS_R, C_FILL, "CT:%u.%uHz",
                   CTCSS_Options[gLastActiveLoot->ct] / 10,
                   CTCSS_Options[gLastActiveLoot->ct] % 10);
    } else if (gLastActiveLoot->cd != 0xFF) {
      PrintSmallEx(LCD_WIDTH, 40 + 8 + 6, POS_R, C_FILL, "DCS:D%03oN",
                   DCS_Options[gLastActiveLoot->cd]);
    }
  }
}
