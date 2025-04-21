#include "scan.h"
#include "../driver/st7565.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/spectrum.h"
#include "bands.h"

uint32_t delay = 1500;
uint16_t sqLevel = 0;

static Measurement *m;
static bool thinking = false;
static bool wasThinkingEarlier = false;

static uint32_t timeout = 0;
static bool lastListenState = false;
static bool isMultiband = false;

static uint16_t measure(uint32_t f) {
  RADIO_TuneToPure(f, true);
  vTaskDelay(delay / 100);
  return RADIO_GetRSSI();
}

static void onNewBand() {
  radio.rxF = gCurrentBand.rxF;
  RADIO_Setup();
  SP_Init(&gCurrentBand);
}

void SCAN_setBand(Band b) {
  gCurrentBand = b;
  onNewBand();
}

void SCAN_setStartF(uint32_t f) {
  gCurrentBand.rxF = f;
  onNewBand();
}

void SCAN_setEndF(uint32_t f) {
  gCurrentBand.txF = f;
  onNewBand();
}

static void next() {
  radio.rxF += StepFrequencyTable[radio.step];

  if (radio.rxF > gCurrentBand.txF) {
    if (isMultiband) {
      BANDS_SelectBandRelativeByScanlist(true);
      onNewBand();
    }
    radio.rxF = gCurrentBand.rxF;
    gRedrawScreen = true;
  }
}

static void nextWithTimeout() {
  if (lastListenState != gIsListening) {
    lastListenState = gIsListening;
    SetTimeout(&timeout, gIsListening
                             ? SCAN_TIMEOUTS[gSettings.sqOpenedTimeout]
                             : SCAN_TIMEOUTS[gSettings.sqClosedTimeout]);
  }

  if (CheckTimeout(&timeout)) {
    SetTimeout(&timeout, 0);
    next();
    return;
  }
}

void SCAN_Next(bool up) { nextWithTimeout(); }

void SCAN_Init(bool multiband) {
  isMultiband = multiband;
  m = &gLoot;
  m->snr = 0;

  onNewBand();
}

void SCAN_Check(bool isAnalyserMode) {
  if (m->open) {
    m->open = RADIO_IsSquelchOpen();
  } else {
    m->f = radio.rxF;
    m->rssi = measure(radio.rxF);

    if (!sqLevel && m->rssi) {
      sqLevel = m->rssi - 1;
    }

    m->open = m->rssi >= sqLevel;
    if (isAnalyserMode) {
      m->open = false;
    }

    SP_AddPoint(m);
  }

  if (gSettings.skipGarbageFrequencies && (radio.rxF % 1300000 == 0)) {
    m->open = false;
  }

  // really good level?
  if (m->open && !gIsListening && !isAnalyserMode) {
    thinking = true;
    wasThinkingEarlier = true;
    gRedrawScreen = true;
    vTaskDelay(pdMS_TO_TICKS(SQL_DELAY));
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
