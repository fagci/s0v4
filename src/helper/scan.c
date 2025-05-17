#include "scan.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/spectrum.h"
#include "bands.h"

uint32_t delay = 1200;
uint16_t sqLevel = 0;

static bool thinking = false;
static bool wasThinkingEarlier = false;

static uint32_t stay_at_timeout;
static uint32_t scan_listen_timeout;

static bool lastListenState = false;
static bool isMultiband = false;

static uint32_t scanCycles = 0;
static uint32_t lastCpsTime = 0;

static uint16_t measure(uint32_t f, bool precise) {
  RADIO_TuneToPure(f, precise);
  SYSTICK_DelayUs(delay);
  return RADIO_GetRSSI();
}

static void onNewBand() {
  radio.rxF = gCurrentBand.rxF;
  radio.step = gCurrentBand.step;
  RADIO_Setup();
  SP_Init(&gCurrentBand);
}

uint32_t SCAN_GetCps() {
  uint32_t cps = scanCycles * 1000 / (Now() - lastCpsTime);
  lastCpsTime = Now();
  scanCycles = 0;
  return cps;
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
  RADIO_ToggleRX(false);
  radio.rxF += StepFrequencyTable[radio.step];

  if (radio.rxF > gCurrentBand.txF) {
    if (isMultiband) {
      BANDS_SelectBandRelativeByScanlist(true);
      onNewBand();
    }
    radio.rxF = gCurrentBand.rxF;
    gRedrawScreen = true;
  }

  LOOT_Replace(&gLoot, radio.rxF);
  SetTimeout(&scan_listen_timeout, 0);
  SetTimeout(&stay_at_timeout, 0);
  scanCycles++;
}

static void nextWithTimeout() {
  if (lastListenState != gIsListening) {
    lastListenState = gIsListening;

    if (gIsListening) {
      SetTimeout(&scan_listen_timeout,
                 SCAN_TIMEOUTS[gSettings.sqOpenedTimeout]);
      SetTimeout(&stay_at_timeout, UINT32_MAX);
    } else {
      SetTimeout(&stay_at_timeout, SCAN_TIMEOUTS[gSettings.sqClosedTimeout]);
    }
  }

  if (CheckTimeout(&scan_listen_timeout) && gIsListening) {
    next();
    return;
  }

  if (CheckTimeout(&stay_at_timeout)) {
    next();
    return;
  }
}

void SCAN_Next(bool up) { next(); }

void SCAN_Init(bool multiband) {
  isMultiband = multiband;
  gLoot.snr = 0;

  lastCpsTime = Now();
  scanCycles = 0;

  onNewBand();
}

static uint32_t lastRender;

void SCAN_Check(bool isAnalyserMode) {
  if (isAnalyserMode) {
    gLoot.f = radio.rxF;
    gLoot.rssi = measure(radio.rxF, !isAnalyserMode);
    SP_AddPoint(&gLoot);
    if (Now() - lastRender > 500) {
      gRedrawScreen = true;
      lastRender = Now();
    }
    next();
    return;
  }

  if (gLoot.open) {
    // gLoot.open = RADIO_IsSquelchOpen();
    RADIO_CheckAndListen();
    gRedrawScreen = true;
  } else {
    gLoot.f = radio.rxF;
    gLoot.rssi = measure(radio.rxF, !isAnalyserMode);

    if (!sqLevel && gLoot.rssi) {
      sqLevel = gLoot.rssi - 1;
    }

    if (sqLevel > gLoot.rssi) {
      uint16_t perc =
          (sqLevel - gLoot.rssi) * 100 / ((sqLevel + gLoot.rssi) / 2);
      if (perc >= 25) {
        sqLevel = gLoot.rssi - 1;
      }
    }

    gLoot.open = gLoot.rssi >= sqLevel;

    SP_AddPoint(&gLoot);
  }

  if (gSettings.skipGarbageFrequencies && (radio.rxF % 1300000 == 0)) {
    gLoot.open = false;
  }

  // really good level?
  if (gLoot.open && !gIsListening) {
    thinking = true;
    wasThinkingEarlier = true;
    SYS_DelayMs(SQL_DELAY);
    gLoot.open = RADIO_IsSquelchOpen();
    thinking = false;
    gRedrawScreen = true;
    if (!gLoot.open) {
      sqLevel++;
    }
  }

  LOOT_Update(&gLoot);

  // reset sql to noise floor when sql closed to check next freq better
  if (gIsListening && !gLoot.open) {
    sqLevel = SP_GetNoiseFloor();
  }
  RADIO_ToggleRX(gLoot.open);

  if (gLoot.open) {
    gRedrawScreen = true;
  }

  static uint8_t stepsPassed;

  if (!gLoot.open) {
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
