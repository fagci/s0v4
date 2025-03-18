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

static const uint8_t REQUIRED_FREQUENCY_HITS = 2;
static const uint8_t FILTER_SWITCH_INTERVAL = REQUIRED_FREQUENCY_HITS;

static const char *FILTER_NAMES[] = {
    [FILTER_OFF] = "ALL",
    [FILTER_VHF] = "VHF",
    [FILTER_UHF] = "UHF",
};
static Filter filter = FILTER_VHF;

static bool bandAutoSwitch = false;

static uint32_t bound;

static bool isScanning = true;
static uint32_t currentFrequency = 0;
static uint32_t lastDetectedFrequency = 0;
static uint8_t frequencyHits = 0;
static uint8_t filterSwitchCounter = 0;

static void enableScan() {
  Log("FC enable");
  BK4819_EnableFrequencyScanEx(gSettings.fcTime);
  isScanning = true;
}

static void disableScan() {
  Log("FC disable");
  BK4819_DisableFrequencyScan();
  BK4819_RX_TurnOn();
  isScanning = false;
}

void FC_init() {
  // BK4819_Init();
  Log("FC init");
  RADIO_LoadCurrentVFO();
  bound = SETTINGS_GetFilterBound();
  BK4819_SelectFilterEx(filter);
  enableScan();
  frequencyHits = 0;
  filterSwitchCounter = 0;
}

void FC_deinit() { disableScan(); }

void switchFilter() {
  if (filter == FILTER_VHF) {
    filter = FILTER_UHF;
  } else {
    filter = FILTER_VHF;
  }

  BK4819_SelectFilterEx(filter);
  filterSwitchCounter = 0;
  gRedrawScreen = true;
}

void FC_update(void) {
  if (isScanning) {
    if (BK4819_GetFrequencyScanResult(&currentFrequency)) {
      Log("FC got %u", currentFrequency);
      bool freqIsOk = true;

      gRedrawScreen = true;
      if (bandAutoSwitch) {
        filterSwitchCounter++;
      }

      if (currentFrequency >= 88 * MHZ && currentFrequency <= 108 * MHZ) {
        freqIsOk = false;
      }

      if (filter == FILTER_VHF && currentFrequency >= bound) {
        freqIsOk = false;
      }

      if (filter == FILTER_UHF && currentFrequency < bound) {
        freqIsOk = false;
      }

      if (freqIsOk && DeltaF(currentFrequency, lastDetectedFrequency) < 300) {
        frequencyHits++;
      } else {
        frequencyHits = 1;
      }
      Log("FC hits %u", frequencyHits);

      lastDetectedFrequency = currentFrequency;

      if (frequencyHits >= REQUIRED_FREQUENCY_HITS) {
        Log("FC hit! Tuning");
        disableScan();
        RADIO_TuneTo(currentFrequency);
        frequencyHits = 0;
      } else {
        disableScan();
        enableScan();
      }
    }

    if (filterSwitchCounter >= FILTER_SWITCH_INTERVAL) {
      Log("FC switch filter");
      switchFilter();
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  } else {
    if (!gIsListening) {
      vTaskDelay(pdMS_TO_TICKS(60));
    }
    Log("FC checklisten");
    RADIO_CheckAndListen();
    if (gIsListening) {
      gRedrawScreen = true;
    } else {
      enableScan();
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
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
      switchFilter();
      return true;
    case KEY_0:
      bandAutoSwitch = !bandAutoSwitch;
      return true;
    case KEY_PTT:
      if (gLastActiveLoot) {
        FC_deinit();
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
                200 << gSettings.fcTime, radio->squelch.value, bandAutoSwitch ? "[A]" : "");
  UI_BigFrequency(40, currentFrequency);

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
