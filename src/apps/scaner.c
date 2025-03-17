#include "scaner.h"
#include "../driver/st7565.h"
#include "../driver/uart.h"
#include "../helper/bands.h"
#include "../helper/lootlist.h"
#include "../helper/measurements.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/components.h"
#include "../ui/spectrum.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chlist.h"
#include "finput.h"
#include <stdint.h>

static Band *b;
static Measurement *m;

static bool selStart = true;

static uint32_t delay = 1000;
static uint8_t afc = 0;

static uint16_t sqLevel = 0;
static bool thinking = false;
static bool wasThinkingEarlier = false;

static uint16_t msmLow;
static uint16_t msmHigh;

static uint32_t cursorRangeTimeout = 0;

static bool isAnalyserMode = false;

typedef enum {
  SET_AGC,
  SET_BW,
  SET_AFC,
  SET_SQL_T,
  SET_SQL_V,
  SET_MOD,
  SET_COUNT,
} Setting;

static Setting setting;

static uint16_t measure(uint32_t f) {
  RADIO_TuneToPure(f, true);
  vTaskDelay(delay / 100);
  return RADIO_GetRSSI();
}

static void onNewBand() {
  gCurrentBand = *b;
  radio->rxF = b->rxF;
  RADIO_Setup();
  SP_Init(b);
  isAnalyserMode = BANDS_RangeIndex() == RANGES_STACK_SIZE - 1;
}

static void setStartF(uint32_t f) {
  b->rxF = f;
  onNewBand();
}

static void setEndF(uint32_t f) {
  b->txF = f;
  onNewBand();
}

static void changeSetting(bool up) {
  switch (setting) {
  case SET_AFC:
    afc = IncDecU(afc, 0, 8 + 1, up);
    BK4819_SetAFC(afc);
    break;
  case SET_BW:
    radio->bw =
        IncDecU(radio->bw, BK4819_FILTER_BW_6k, BK4819_FILTER_BW_26k + 1, up);
    break;
  case SET_AGC:
    radio->gainIndex = IncDecU(radio->gainIndex, 0, ARRAY_SIZE(gainTable), up);
    break;
  case SET_SQL_T:
    radio->squelch.type =
        IncDecU(radio->squelch.type, 0, ARRAY_SIZE(sqTypeNames), up);
    break;
  case SET_SQL_V:
    radio->squelch.value = IncDecU(radio->squelch.value, 0, 11, up);
    break;
  case SET_MOD:
    RADIO_ToggleModulationEx(up);
    break;
  default:
    break;
  }
  RADIO_Setup();
}

void SCANER_init(void) {
  SPECTRUM_Y = 8;
  SPECTRUM_H = 44;

  gMonitorMode = false;
  if (!gCurrentBand.rxF) {
    RADIO_LoadCurrentVFO();
    BANDS_SelectByFrequency(radio->rxF, true);
  }

  m = &gLoot[gSettings.activeVFO];
  m->snr = 0;

  gCurrentBand.meta.type = TYPE_BAND_DETACHED;

  BANDS_RangeClear(); // TODO: push only if gCurrentBand was changed from
                      // outside

  BANDS_RangePush(gCurrentBand);
  b = BANDS_RangePeek();

  gCurrentBand = *b;
  BANDS_SetRadioParamsFromCurrentBand();

  onNewBand();
}

static void next() {
  radio->rxF += StepFrequencyTable[radio->step];

  if (radio->rxF > b->txF) {
    radio->rxF = b->rxF;
    gRedrawScreen = true;
  }
}

static uint32_t lastSettedF = 0;
static bool lastScanForward = true;
static uint32_t timeout = 0;
static bool lastListenState = false;

static void nextWithTimeout() {
  if (lastListenState != gIsListening) {
    lastListenState = gIsListening;
    SetTimeout(&timeout, gIsListening
                             ? SCAN_TIMEOUTS[gSettings.sqOpenedTimeout]
                             : SCAN_TIMEOUTS[gSettings.sqClosedTimeout]);
  }

  if (CheckTimeout(&timeout)) {
    lastSettedF = radio->rxF;
    SetTimeout(&timeout, 0);
    next();
    return;
  }
}

void SCANER_update(void) {
  if (m->open) {
    m->open = RADIO_IsSquelchOpen();
  } else {
    m->f = radio->rxF;
    m->rssi = measure(radio->rxF);

    if (!sqLevel && m->rssi) {
      sqLevel = m->rssi - 1;
    }

    m->open = m->rssi >= sqLevel;

    SP_AddPoint(m);

    if (m->rssi > msmHigh) {
      msmHigh = m->rssi;
    }
    if (m->rssi < msmLow) {
      msmLow = m->rssi;
    }
  }

  if (gSettings.skipGarbageFrequencies && (radio->rxF % 1300000 == 0)) {
    m->open = false;
  }

  // really good level?
  if (m->open && !gIsListening) {
    thinking = true;
    wasThinkingEarlier = true;
    gRedrawScreen = true;
    vTaskDelay(pdMS_TO_TICKS(60));
    m->open = RADIO_IsSquelchOpen();
    thinking = false;
    gRedrawScreen = true;
    if (!m->open) {
      sqLevel++;
    }
  }

  LOOT_Update(m);

  RADIO_ToggleRX(m->open);

  if (m->open) {
    gRedrawScreen = true;
  }

  static uint8_t stepsPassed;

  if (!m->open) {
    if (stepsPassed++ > 64) {
      stepsPassed = 0;
      gRedrawScreen = true;
      if (!wasThinkingEarlier) {
        sqLevel--;
      }
      wasThinkingEarlier = false;
    }
  }

  nextWithTimeout();
}

bool SCANER_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_LONG_PRESSED) {
    Band _b;
    switch (key) {
    case KEY_6:
      if (gLastActiveLoot) {
        _b = *b;
        _b.rxF = gLastActiveLoot->f - StepFrequencyTable[radio->step] * 64;
        _b.txF = _b.rxF + StepFrequencyTable[radio->step] * 128;
        BANDS_RangePush(_b);
        b = BANDS_RangePeek();
        CUR_Reset();
        onNewBand();
      }
      return true;
    case KEY_5:
      selStart = !selStart;
      return true;
    case KEY_4:
      setting = IncDecU(setting, 0, SET_COUNT, false);
      return true;
    case KEY_0:
      gChListFilter = TYPE_FILTER_BAND;
      APPS_run(APP_CH_LIST);
      return true;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    switch (key) {
    case KEY_1:
    case KEY_7:
      delay = AdjustU(delay, 200, 10000, key == KEY_1 ? 100 : -100);
      return true;
    case KEY_3:
    case KEY_9:
      radio->step = b->step =
          IncDecU(b->step, STEP_0_02kHz, STEP_500_0kHz + 1, key == KEY_3);
      onNewBand();
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_UP:
    case KEY_DOWN:
      CUR_Move(key == KEY_UP);
      cursorRangeTimeout = Now() + 2000;
      return true;

    case KEY_2:
    case KEY_8:
      changeSetting(key == KEY_2);
      return true;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_4:
      setting = IncDecU(setting, 0, SET_COUNT, true);
      return true;
    case KEY_5:
      gFInputCallback = selStart ? setStartF : setEndF;
      APPS_run(APP_FINPUT);
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      return true;
    case KEY_SIDE2:
      LOOT_WhitelistLast();
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;

    case KEY_0:
      BANDS_RangePush(
          CUR_GetRange(BANDS_RangePeek(), StepFrequencyTable[radio->step]));
      b = BANDS_RangePeek();
      CUR_Reset();
      onNewBand();
      return true;
    case KEY_F:
      BANDS_RangePop();
      b = BANDS_RangePeek();
      CUR_Reset();
      onNewBand();
      return true;

    case KEY_PTT:
      if (gLastActiveLoot) {
        RADIO_TuneToSave(gLastActiveLoot->f);
        APPS_run(APP_VFO1);
        return true;
      }
      break;
    default:
      break;
    }
  }

  return false;
}

static void renderAnalyzerUI() {
  PrintSmallEx(0, 18, POS_L, C_FILL, "%u", msmHigh);
  PrintSmallEx(0, 24, POS_L, C_FILL, "%u", msmLow);
}

void SCANER_render(void) {
  const uint32_t step = StepFrequencyTable[radio->step];

  if (thinking) {
    PrintSmallEx(LCD_XCENTER, 4, POS_C, C_FILL, "...");
  }

  const int8_t vGain = -gainTable[radio->gainIndex].gainDb + 33;

  STATUSLINE_SetText(                                                     //
      "%c%+d%c%s%cAFC%u%c%s%c%u%c%s",                                     //
      setting == SET_AGC ? '>' : ' ', vGain,                              //
      setting == SET_BW ? '>' : ' ', RADIO_GetBWName(radio),              //
      setting == SET_AFC ? '>' : ' ', afc,                                //
      setting == SET_SQL_T ? '>' : ' ', sqTypeNames[radio->squelch.type], //
      setting == SET_SQL_V ? '>' : ' ', radio->squelch.value,             //
      setting == SET_MOD ? '>' : ' ',
      modulationTypeOptions[RADIO_GetModulation()] //
  );

  SP_Render(b);
  SP_RenderArrow(b, radio->rxF);

  // top
  if (gLastActiveLoot) {
    UI_DrawLoot(gLastActiveLoot, LCD_XCENTER, 14, POS_C);
  }

  PrintSmallEx(0, 12, POS_L, C_FILL, "%uus", delay);

  PrintSmallEx(LCD_WIDTH, 12, POS_R, C_FILL, "%u.%02uk", step / 100,
               step % 100);
  if (BANDS_RangeIndex() > 0) {
    PrintSmallEx(LCD_WIDTH, 18, POS_R, C_FILL, "Zoom %u",
                 BANDS_RangeIndex() + 1);
  }

  if (isAnalyserMode) {
    renderAnalyzerUI();
  }

  // bottom
  Band r = CUR_GetRange(b, step);
  bool showCurRange = Now() < cursorRangeTimeout;
  FSmall(1, LCD_HEIGHT - 2, POS_L, showCurRange ? r.rxF : b->rxF);
  FSmall(LCD_XCENTER, LCD_HEIGHT - 2, POS_C,
         showCurRange ? CUR_GetCenterF(b, step) : radio->rxF);
  FSmall(LCD_WIDTH - 1, LCD_HEIGHT - 2, POS_R, showCurRange ? r.txF : b->txF);

  FillRect(selStart ? 0 : LCD_WIDTH - 42, LCD_HEIGHT - 7, 42, 7, C_INVERT);

  CUR_Render();

  if (gIsListening) {
    UI_RSSIBar(16);
  }
}

void SCANER_deinit(void) {}
