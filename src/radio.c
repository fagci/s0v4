#include "radio.h"
#include "board.h"
#include "dcs.h"
#include "driver/audio.h"
#include "driver/backlight.h"
#include "driver/bk1080.h"
#include "driver/bk4819-regs.h"
#include "driver/bk4819.h"
#include "driver/si473x.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/uart.h"
#include "external/FreeRTOS/include/FreeRTOS.h"
#include "external/FreeRTOS/include/projdefs.h"
#include "external/FreeRTOS/include/task.h"
#include "external/FreeRTOS/include/timers.h"
#include "external/printf/printf.h"
#include "helper/bands.h"
#include "helper/battery.h"
#include "helper/channels.h"
#include "helper/lootlist.h"
#include "helper/measurements.h"
#include "misc.h"
#include "scheduler.h"
#include "settings.h"
#include "ui/spectrum.h"
#include "ui/statusline.h"
#include <stdint.h>

CH *radio;
VFO gVFO[2];

Measurement gLoot[2] = {0};

bool gIsListening = false;
bool gMonitorMode = false;
uint8_t gCurrentTxPower = 0;
TXState gTxState = TX_UNKNOWN;
bool gShowAllRSSI = false;

static bool hasSi = false;
static bool hasSsbPatch = false;

static uint8_t oldRadio = 255;

const uint16_t StepFrequencyTable[15] = {
    2,   5,   50,  100,

    250, 500, 625, 833, 900, 1000, 1250, 2500, 5000, 10000, 50000,
};

const char *modulationTypeOptions[8] = {"FM",  "AM",  "LSB", "USB",
                                        "BYP", "RAW", "WFM"};
const char *powerNames[4] = {"ULOW, LOW", "MID", "HIGH"};
const char *bwNames[10] = {
    "U6K",  //
    "U7K",  //
    "N9k",  //
    "N10k", //
    "W12k", //
    "W14k", //
    "W17k", //
    "W20k", //
    "W23k", //
    "W26k", //
};
const char *bwNamesSiAMFM[7] = {
    [BK4819_FILTER_BW_6k] = "1k",  [BK4819_FILTER_BW_7k] = "1.8k",
    [BK4819_FILTER_BW_9k] = "2k",  [BK4819_FILTER_BW_10k] = "2.5k",
    [BK4819_FILTER_BW_12k] = "3k", [BK4819_FILTER_BW_14k] = "4k",
    [BK4819_FILTER_BW_17k] = "6k",
};
const char *bwNamesSiSSB[6] = {
    [BK4819_FILTER_BW_6k] = "0.5k", [BK4819_FILTER_BW_7k] = "1.0k",
    [BK4819_FILTER_BW_9k] = "1.2k", [BK4819_FILTER_BW_10k] = "2.2k",
    [BK4819_FILTER_BW_12k] = "3k",  [BK4819_FILTER_BW_14k] = "4k",

};
const char *radioNames[4] = {"BK4819", "BK1080", "SI4732"};
const char *shortRadioNames[3] = {"BK", "BC", "SI"};
const char *TX_STATE_NAMES[7] = {"TX Off",   "TX On",  "CHARGING", "BAT LOW",
                                 "DISABLED", "UPCONV", "HIGH POW"};

const SquelchType sqTypeValues[4] = {
    SQUELCH_RSSI_NOISE_GLITCH,
    SQUELCH_RSSI_GLITCH,
    SQUELCH_RSSI_NOISE,
    SQUELCH_RSSI,
};
const char *sqTypeNames[4] = {"RNG", "RG", "RN", "R"};
const char *deviationNames[] = {"", "+", "-"};

static const SI47XX_SsbFilterBW SI_BW_MAP_SSB[] = {
    [BK4819_FILTER_BW_6k] = SI47XX_SSB_BW_0_5_kHz,
    [BK4819_FILTER_BW_7k] = SI47XX_SSB_BW_1_0_kHz,
    [BK4819_FILTER_BW_9k] = SI47XX_SSB_BW_1_2_kHz,
    [BK4819_FILTER_BW_10k] = SI47XX_SSB_BW_2_2_kHz,
    [BK4819_FILTER_BW_12k] = SI47XX_SSB_BW_3_kHz,
    [BK4819_FILTER_BW_14k] = SI47XX_SSB_BW_4_kHz,
};
static const SI47XX_FilterBW SI_BW_MAP_AMFM[] = {
    [BK4819_FILTER_BW_6k] = SI47XX_BW_1_kHz,
    [BK4819_FILTER_BW_7k] = SI47XX_BW_1_8_kHz,
    [BK4819_FILTER_BW_9k] = SI47XX_BW_2_kHz,
    [BK4819_FILTER_BW_10k] = SI47XX_BW_2_5_kHz,
    [BK4819_FILTER_BW_12k] = SI47XX_BW_3_kHz,
    [BK4819_FILTER_BW_14k] = SI47XX_BW_4_kHz,
    [BK4819_FILTER_BW_17k] = SI47XX_BW_6_kHz,
};

static ModulationType MODS_BK4819[] = {
    MOD_FM,
    MOD_AM,
    MOD_USB,
    MOD_WFM,
};

static ModulationType MODS_BOTH_PATCH[] = {
    MOD_FM, MOD_AM, MOD_USB, MOD_LSB, MOD_BYP, MOD_RAW, MOD_WFM,
};

static ModulationType MODS_BOTH[] = {
    MOD_FM, MOD_AM, MOD_USB, MOD_BYP, MOD_RAW, MOD_WFM,
};

static ModulationType MODS_SI4732_PATCH[] = {
    MOD_AM,
    MOD_LSB,
    MOD_USB,
};

static ModulationType MODS_SI4732[] = {
    MOD_AM,
};

static ModulationType MODS_WFM[] = {
    MOD_WFM,
};

static void loadVFO(uint8_t num) {
  CHANNELS_Load(CHANNELS_GetCountMax() - 2 + num, &gVFO[num]);
}

static void saveVFO(uint8_t num) {
  CHANNELS_Save(CHANNELS_GetCountMax() - 2 + num, &gVFO[num]);
}

static uint8_t indexOfMod(const ModulationType *arr, uint8_t n,
                          ModulationType t) {
  for (uint8_t i = 0; i < n; ++i) {
    if (arr[i] == t) {
      return i;
    }
  }
  return 0;
}

static ModulationType getNextModulation(bool next, bool apply) {
  uint8_t sz = ARRAY_SIZE(MODS_BK4819);
  ModulationType *items = MODS_BK4819;

  if (radio->rxF >= 88 * MHZ && radio->rxF <= BK1080_F_MAX) {
    items = MODS_WFM;
    sz = ARRAY_SIZE(MODS_WFM);
  } else if (radio->rxF <= SI47XX_F_MAX && radio->rxF >= BK4819_F_MIN) {
    if (hasSsbPatch) {
      items = MODS_BOTH_PATCH;
      sz = ARRAY_SIZE(MODS_BOTH_PATCH);
    } else {
      items = MODS_BOTH;
      sz = ARRAY_SIZE(MODS_BOTH);
    }
  } else if (radio->rxF <= SI47XX_F_MAX) {
    if (hasSsbPatch) {
      items = MODS_SI4732_PATCH;
      sz = ARRAY_SIZE(MODS_SI4732_PATCH);
    } else {
      items = MODS_SI4732;
      sz = ARRAY_SIZE(MODS_SI4732);
    }
  }

  const uint8_t curIndex = indexOfMod(items, sz, radio->modulation);

  return items[apply ? IncDecU(curIndex, 0, sz, next) : curIndex];
}

Radio RADIO_Selector(uint32_t freq, ModulationType mod) {
  if (freq >= BK1080_F_MIN && freq <= BK1080_F_MAX) {
    return hasSi ? RADIO_SI4732 : RADIO_BK1080;
  }

  if (hasSi && freq <= SI47XX_F_MAX &&
      (mod == MOD_AM || (hasSsbPatch && RADIO_IsSSB()))) {
    return RADIO_SI4732;
  }

  return RADIO_BK4819;
}

inline Radio RADIO_GetRadio() { return radio->radio; }

ModulationType RADIO_GetModulation() { return radio->modulation; }

const char *RADIO_GetBWName(const VFO *vfo) {
  switch (vfo->radio) {
  case RADIO_SI4732:
    if (RADIO_IsSSB()) {
      return bwNamesSiSSB[vfo->bw];
    }
    return bwNamesSiAMFM[vfo->bw];
  default:
    return bwNames[vfo->bw];
  }
}

void RADIO_Init(void) {
  Log("RADIO_Init");
  hasSi = RADIO_HasSi();
  if (hasSi) {
    hasSsbPatch = SETTINGS_IsPatchPresent();
  }
  Log("RADIO hasSi=%u, hasPatch=%u", hasSi, hasSsbPatch);
  BK4819_Init();
}

static void setSI4732Modulation(ModulationType mod) {
  if (mod == MOD_AM) {
    SI47XX_SwitchMode(SI47XX_AM);
  } else if (mod == MOD_LSB) {
    SI47XX_SwitchMode(SI47XX_LSB);
  } else if (mod == MOD_USB) {
    SI47XX_SwitchMode(SI47XX_USB);
  } else {
    SI47XX_SwitchMode(SI47XX_FM);
  }
}

static StaticTimer_t saveCurrentVfoTimerBuffer;
static TimerHandle_t saveCurrentVfoTimer;
void RADIO_SaveCurrentVFODelayed(void) {
  /* Log("!!!VFO SAV delayed");
  return; */
  if (saveCurrentVfoTimer) {
    xTimerStop(saveCurrentVfoTimer, 0);
  }
  saveCurrentVfoTimer =
      xTimerCreateStatic("RS", pdMS_TO_TICKS(1000), pdFALSE, NULL,
                         RADIO_SaveCurrentVFO, &saveCurrentVfoTimerBuffer);
  xTimerStart(saveCurrentVfoTimer, 0);
}

static void setupToneDetection() {
  Log("setupToneDetection");
  // HACK? to enable STE RX
  Log("DC flt BW = 0");
  BK4819_WriteRegister(BK4819_REG_7E, 0x302E); // DC flt BW 0=BYP
  uint16_t InterruptMask = BK4819_REG_3F_CxCSS_TAIL;
  if (gSettings.dtmfdecode) {
    BK4819_EnableDTMF();
    InterruptMask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
  } else {
    BK4819_DisableDTMF();
  }
  switch (radio->code.rx.type) {
  case CODE_TYPE_DIGITAL:
  case CODE_TYPE_REVERSE_DIGITAL:
    // Log("DCS on");
    BK4819_SetCDCSSCodeWord(
        DCS_GetGolayCodeWord(radio->code.rx.type, radio->code.rx.value));
    InterruptMask |= BK4819_REG_3F_CDCSS_FOUND | BK4819_REG_3F_CDCSS_LOST;
    break;
  case CODE_TYPE_CONTINUOUS_TONE:
    // Log("CTCSS on");
    BK4819_SetCTCSSFrequency(CTCSS_Options[radio->code.rx.value]);
    InterruptMask |= BK4819_REG_3F_CTCSS_FOUND | BK4819_REG_3F_CTCSS_LOST;
    break;
  default:
    // Log("STE on");
    BK4819_SetCTCSSFrequency(670);
    BK4819_SetTailDetection(550);
    break;
  }
  BK4819_WriteRegister(BK4819_REG_3F, InterruptMask);
}

static void toggleBK4819(bool on) {
  // Log("Toggle bk4819 audio %u", on);
  if (on) {
    BK4819_ToggleAFDAC(true);
    BK4819_ToggleAFBit(true);
    SYS_DelayMs(8);
    AUDIO_ToggleSpeaker(true);
  } else {
    AUDIO_ToggleSpeaker(false);
    SYS_DelayMs(8);
    BK4819_ToggleAFDAC(false);
    BK4819_ToggleAFBit(false);
  }
}

static void toggleBK1080SI4732(bool on) {
  // Log("Toggle bk1080si audio %u", on);
  if (on) {
    SYS_DelayMs(8);
    AUDIO_ToggleSpeaker(true);
  } else {
    AUDIO_ToggleSpeaker(false);
    SYS_DelayMs(8);
  }
}

static uint8_t calculateOutputPower(uint32_t f) {
  uint8_t power_bias;
  PowerCalibration cal = BANDS_GetPowerCalib(f);

  switch (radio->power) {
  case TX_POW_LOW:
    power_bias = cal.s;
    break;

  case TX_POW_MID:
    power_bias = cal.m;
    break;

  case TX_POW_HIGH:
    power_bias = cal.e;
    break;

  default:
    power_bias = cal.s;
    if (power_bias > 10)
      power_bias -= 10; // 10mw if Low=500mw
  }

  return power_bias;
}

static void sendEOT() {
  BK4819_ExitSubAu();
  switch (gSettings.roger) {
  case 1:
    BK4819_PlayRogerTiny();
    break;
  default:
    break;
  }
  if (gSettings.ste) {
    SYS_DelayMs(50);
    BK4819_GenTail(4);
    BK4819_WriteRegister(BK4819_REG_51, 0x9033);
    SYS_DelayMs(200);
  }
  BK4819_ExitSubAu();
}

static void rxTurnOff(Radio r) {
  switch (r) {
  case RADIO_BK4819:
    BK4819_Idle();
    break;
  case RADIO_BK1080:
    BK1080_Mute(true);
    break;
  case RADIO_SI4732:
    if (gSettings.si4732PowerOff) {
      SI47XX_PowerDown();
    } else {
      SI47XX_SetVolume(0);
    }
    break;
  default:
    break;
  }
}

static void rxTurnOn(Radio r) {
  switch (r) {
  case RADIO_BK4819:
    BK4819_RX_TurnOn();
    break;
  case RADIO_BK1080:
    BK4819_Idle();
    BK1080_Mute(false);
    BK1080_Init(radio->rxF, true);
    break;
  case RADIO_SI4732:
    BK4819_Idle();
    if (gSettings.si4732PowerOff || !isSi4732On) {
      if (RADIO_IsSSB()) {
        SI47XX_PatchPowerUp();
      } else {
        SI47XX_PowerUp();
      }
    } else {
      SI47XX_SetVolume(63);
    }
    break;
  default:
    break;
  }
}

uint32_t GetScreenF(uint32_t f) { return f - gSettings.upconverter; }

uint32_t GetTuneF(uint32_t f) { return f + gSettings.upconverter; }

bool RADIO_IsSSB() {
  ModulationType mod = RADIO_GetModulation();
  return mod == MOD_LSB || mod == MOD_USB;
}

void RADIO_ToggleRX(bool on) {
  if (gIsListening == on) {
    return;
  }
  BOARD_ToggleGreen(on);
  Log("TOGGLE RX=%u", on);
  gRedrawScreen = true;

  gIsListening = on;

  if (on) {
    if (gSettings.backlightOnSquelch != BL_SQL_OFF) {
      BACKLIGHT_On();
    }
  } else {
    if (gSettings.backlightOnSquelch == BL_SQL_OPEN) {
      BACKLIGHT_Toggle(false);
    }
  }

  Radio r = RADIO_GetRadio();
  if (r == RADIO_BK4819) {
    toggleBK4819(on);
  } else {
    toggleBK1080SI4732(on);
  }
}

void RADIO_EnableCxCSS(void) {
  switch (radio->code.tx.type) {
  case CODE_TYPE_CONTINUOUS_TONE:
    BK4819_SetCTCSSFrequency(CTCSS_Options[radio->code.tx.value]);
    break;
  case CODE_TYPE_DIGITAL:
  case CODE_TYPE_REVERSE_DIGITAL:
    BK4819_SetCDCSSCodeWord(
        DCS_GetGolayCodeWord(radio->code.tx.type, radio->code.tx.value));
    break;
  default:
    BK4819_ExitSubAu();
    break;
  }
}

uint32_t RADIO_GetTXFEx(const VFO *vfo) {
  switch (vfo->offsetDir) {
  case OFFSET_FREQ:
    return vfo->txF;
  case OFFSET_PLUS:
    return vfo->rxF + vfo->txF;
  case OFFSET_MINUS:
    return vfo->rxF - vfo->txF;
  default:
    return vfo->rxF;
  }
}

uint32_t RADIO_GetTXF(void) { return RADIO_GetTXFEx(radio); }

TXState RADIO_GetTXState(uint32_t txF) {
  if (gSettings.upconverter) {
    return TX_DISABLED_UPCONVERTER;
  }

  if (RADIO_GetRadio() != RADIO_BK4819) {
    return TX_DISABLED;
  }

  Band txBand = BANDS_ByFrequency(txF);

  if (!txBand.allowTx && !(RADIO_IsChMode() && radio->allowTx)) {
    return TX_DISABLED;
  }

  if (gBatteryPercent == 0) {
    return TX_BAT_LOW;
  }
  if (gChargingWithTypeC || gBatteryVoltage > 880) {
    return TX_VOL_HIGH;
  }
  return TX_ON;
}

uint32_t RADIO_GetTxPower(uint32_t txF) {
  return Clamp(calculateOutputPower(txF), 0, 0x91);
}

void RADIO_ToggleTX(bool on) {
  uint32_t txF = RADIO_GetTXF();
  uint8_t power = RADIO_GetTxPower(txF);
  RADIO_ToggleTXEX(on, txF, power, true);
}

bool RADIO_IsChMode() { return radio->channel >= 0; }

void RADIO_ToggleTXEX(bool on, uint32_t txF, uint8_t power, bool paEnabled) {
  bool lastOn = gTxState == TX_ON;
  if (gTxState == on) {
    return;
  }

  gTxState = on ? RADIO_GetTXState(txF) : TX_UNKNOWN;

  if (gTxState == TX_ON) {
    RADIO_ToggleRX(false);

    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);

    BK4819_TuneTo(txF, true);

    BOARD_ToggleRed(gSettings.brightness > 1);
    BK4819_PrepareTransmit();

    SYS_DelayMs(10);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, paEnabled);
    SYS_DelayMs(5);
    gCurrentTxPower = power;
    BK4819_SetupPowerAmplifier(power, txF);
    SYS_DelayMs(10);

    RADIO_EnableCxCSS();

  } else if (lastOn) {
    BK4819_ExitDTMF_TX(true); // also prepares to tx ste

    sendEOT();
    toggleBK1080SI4732(false);
    BOARD_ToggleRed(false);
    BK4819_TurnsOffTones_TurnsOnRX();

    gCurrentTxPower = 0;
    BK4819_SetupPowerAmplifier(0, 0);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);

    setupToneDetection();
    BK4819_TuneTo(radio->rxF, true);
  }
}

void RADIO_TuneToPure(uint32_t f, bool precise) {
  uint32_t s = 100; // 1kHz
  if (f < SI47XX_F_MAX) {
    s = 50; // 500Hz
  } else if (f >= BK1080_F_MIN && f <= BK1080_F_MAX) {
    s = 1000; // 10kHz
  }
  f += gCurrentBand.ppm * s;
  LOOT_Replace(&gLoot[gSettings.activeVFO], f);
  Radio r = RADIO_GetRadio();
  // Log("Tune %s to %u", radioNames[r], f);
  switch (r) {
  case RADIO_BK4819:
    BK4819_TuneTo(f, precise);
    break;
  case RADIO_BK1080:
    BK1080_SetFrequency(f);
    break;
  case RADIO_SI4732:
    SI47XX_TuneTo(f);
    break;
  default:
    break;
  }
}

void RADIO_SwitchRadioPure() {
  if (oldRadio == radio->radio) {
    return;
  }
  rxTurnOff(oldRadio);
  rxTurnOn(radio->radio);
  oldRadio = radio->radio;
}

void RADIO_SwitchRadio() {
  radio->modulation = getNextModulation(true, false);
  radio->radio = RADIO_Selector(radio->rxF, radio->modulation);
  RADIO_SwitchRadioPure();
}

static void checkVisibleBand() {
  if (!BANDS_InRange(radio->rxF, gCurrentBand)) {
    BANDS_SelectByFrequency(radio->rxF, radio->fixedBoundsMode);
  }
}

void RADIO_SetupByCurrentVFO(void) {
  Log("RADIO setup by VFO");
  checkVisibleBand();

  RADIO_SwitchRadio();
  RADIO_Setup();
  RADIO_TuneToPure(radio->rxF, !gMonitorMode);
}

// USE CASE: set vfo temporary for current app
void RADIO_TuneTo(uint32_t f) {
  if (RADIO_IsChMode()) {
    radio->channel = -1;
    snprintf(radio->name, 5, "VFO-%c", 'A' + gSettings.activeVFO);
  }
  radio->txF = 0;
  radio->rxF = f;
  RADIO_SetupByCurrentVFO();
}

// USE CASE: set vfo and use in another app
void RADIO_TuneToSave(uint32_t f) {
  RADIO_TuneTo(f);
  gCurrentBand.misc.lastUsedFreq = f;
  RADIO_SaveCurrentVFO();
  BANDS_SaveCurrent();
}

void RADIO_SaveCurrentVFO(void) {
  int16_t vfoChNum = CHANNELS_GetCountMax() - 2 + gSettings.activeVFO;
  int16_t chToSave = radio->channel;
  if (chToSave >= 0) {
    // save only active channel number
    // to load it instead of full VFO
    // and to prevent overwrite VFO with MR
    VFO oldVfo;
    CHANNELS_Load(vfoChNum, &oldVfo);
    oldVfo.channel = chToSave;
    CHANNELS_Save(vfoChNum, &oldVfo);
    return;
  }
  CHANNELS_Save(vfoChNum, radio);
}

void RADIO_LoadCurrentVFO(void) {
  gMonitorMode = false;
  for (uint8_t i = 0; i < 2; ++i) {
    loadVFO(i);
    // Log("gVFO(%u)= (f=%u, radio=%u)", i + 1, gVFO[i].rxF, gVFO[i].radio);
    if (gVFO[i].channel >= 0) {
      RADIO_VfoLoadCH(i);
    }

    LOOT_Replace(&gLoot[i], gVFO[i].rxF);
  }
  radio = &gVFO[gSettings.activeVFO];

  // needed to select gCurrentBand & set band index in SL
  CHANNELS_LoadScanlist(RADIO_IsChMode() ? TYPE_FILTER_CH : TYPE_FILTER_BAND,
                        gSettings.currentScanlist);

  RADIO_SetupByCurrentVFO();
}

void RADIO_SetSquelch(uint8_t sq) {
  radio->squelch.value = sq;
  BK4819_Squelch(sq, gSettings.sqlOpenTime, gSettings.sqlCloseTime);
  RADIO_SaveCurrentVFODelayed();
}

void RADIO_SetSquelchType(SquelchType t) {
  radio->squelch.type = t;
  RADIO_SaveCurrentVFODelayed();
}

void RADIO_SetGain(uint8_t gainIndex) {
  radio->gainIndex = gainIndex;
  Log("GAIN: %+d", -gainTable[gainIndex].gainDb + 33);
  bool disableAGC;
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    BK4819_SetAGC(radio->modulation != MOD_AM, gainIndex);
    break;
  case RADIO_SI4732:
    // 0 - max gain
    // 26 - min gain
    disableAGC = gainIndex != AUTO_GAIN_INDEX;
    gainIndex = ARRAY_SIZE(gainTable) - 1 - gainIndex;
    gainIndex = ConvertDomain(gainIndex, 0, ARRAY_SIZE(gainTable) - 1, 0, 26);
    SI47XX_SetAutomaticGainControl(disableAGC, disableAGC ? gainIndex : 0);
    break;
  case RADIO_BK1080:
    break;
  default:
    break;
  }
}

void RADIO_SetFilterBandwidth(BK4819_FilterBandwidth_t bw) {
  Log("BW: %s", bwNames[bw]);
  ModulationType mod = RADIO_GetModulation();
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    BK4819_SetFilterBandwidth(bw);
    break;
  case RADIO_BK1080:
    break;
  case RADIO_SI4732:
    if (mod == MOD_USB || mod == MOD_LSB) {
      SI47XX_SetSsbBandwidth(SI_BW_MAP_SSB[bw]);
    } else {
      SI47XX_SetBandwidth(SI_BW_MAP_AMFM[bw], true);
    }
    break;
  default:
    break;
  }
}

void RADIO_Setup() {
  Log("---------- %s RADIO_Setup ----------", radioNames[RADIO_GetRadio()]);
  ModulationType mod = RADIO_GetModulation();
  RADIO_SetGain(radio->gainIndex);
  RADIO_SetFilterBandwidth(radio->bw);
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    Log("SQ %s,%u", sqTypeNames[radio->squelch.type], radio->squelch.value);
    BK4819_SquelchType(radio->squelch.type);
    BK4819_Squelch(radio->squelch.value, gSettings.sqlOpenTime,
                   gSettings.sqlCloseTime);
    Log("MOD: %s", modulationTypeOptions[mod]);
    BK4819_SetModulation(mod);
    BK4819_SetScrambler(radio->scrambler);

    setupToneDetection();
    break;
  case RADIO_BK1080:
    break;
  case RADIO_SI4732:
    if (mod == MOD_FM) {
      SI47XX_SetSeekFmLimits(gCurrentBand.rxF, gCurrentBand.txF);
      SI47XX_SetSeekFmSpacing(StepFrequencyTable[gCurrentBand.step]);
    } else if (mod == MOD_AM) {
      SI47XX_SetSeekAmLimits(gCurrentBand.rxF, gCurrentBand.txF);
      SI47XX_SetSeekAmSpacing(StepFrequencyTable[gCurrentBand.step]);
    }

    setSI4732Modulation(mod);

    break;
  default:
    break;
  }
}

uint16_t RADIO_GetRSSI(void) {
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    return BK4819_GetRSSI();
  case RADIO_BK1080:
    return gShowAllRSSI ? BK1080_GetRSSI() : 0;
  case RADIO_SI4732:
    if (gShowAllRSSI) {
      RSQ_GET();
      return ConvertDomain(rsqStatus.resp.RSSI, 0, 64, 30, 346);
    }
    return 0;
  default:
    return 128;
  }
}

uint8_t RADIO_GetSNR(void) {
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    return ConvertDomain(BK4819_GetSNR(), 24, 170, 0, 30);
  case RADIO_BK1080:
    return gShowAllRSSI ? BK1080_GetSNR() : 0;
  case RADIO_SI4732:
    if (gShowAllRSSI) {
      RSQ_GET();
      return rsqStatus.resp.SNR;
    }
    return 0;
  default:
    return 0;
  }
}

uint16_t RADIO_GetS() {
  uint8_t snr = RADIO_GetSNR();
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    return ConvertDomain(snr, 0, 137, 0, 13);
  case RADIO_BK1080:
    return ConvertDomain(snr, 0, 137, 0, 13);
  case RADIO_SI4732:
    return ConvertDomain(snr, 0, 30, 0, 13);
  default:
    return 0;
  }
}

bool RADIO_IsSquelchOpen() {
  if (gMonitorMode) {
    return true;
  }
  if (RADIO_GetRadio() == RADIO_BK4819) {
    return BK4819_IsSquelchOpen();
  }

  return gShowAllRSSI ? RADIO_GetSNR() > radio->squelch.value : true;
}

void RADIO_VfoLoadCH(uint8_t i) {
  int16_t chNum = gVFO[i].channel;
  CHANNELS_Load(gVFO[i].channel, &gVFO[i]);
  gVFO[i].meta.type = TYPE_VFO;
  gVFO[i].channel = chNum;
}

void RADIO_TuneToBand(int16_t num) {
  if (CHANNELS_GetMeta(num).type == TYPE_BAND) {
    BANDS_Select(num, true);
    // radio->allowTx = gCurrentBand.allowTx;
    if (BANDS_InRange(radio->rxF, gCurrentBand)) {
      return;
    }
    if (BANDS_InRange(gCurrentBand.misc.lastUsedFreq, gCurrentBand)) {
      RADIO_TuneToSave(gCurrentBand.misc.lastUsedFreq);
    } else {
      RADIO_TuneToSave(gCurrentBand.rxF);
    }
  }
}

void RADIO_TuneToCH(int16_t num) {
  if (CHANNELS_GetMeta(num).type == TYPE_CH) {
    radio->channel = num;
    RADIO_VfoLoadCH(gSettings.activeVFO);
    RADIO_SaveCurrentVFO();
    RADIO_SetupByCurrentVFO();
  }
}

bool RADIO_TuneToMR(int16_t num) {
  // Log("Tune to MR %u", num);
  if (CHANNELS_Existing(num)) {
    // Log("MR existing, type=%u", CHANNELS_GetMeta(num).type);
    switch (CHANNELS_GetMeta(num).type) {
    case TYPE_CH:
      RADIO_TuneToCH(num);
      return true;
    case TYPE_BAND:
      RADIO_TuneToBand(num);
      break;
    default:
      break;
    }
  }
  radio->channel = -1;
  return false;
}

void RADIO_NextVFO(void) {
  gSettings.activeVFO = !gSettings.activeVFO;
  radio = &gVFO[gSettings.activeVFO];
  RADIO_SetupByCurrentVFO();
  SETTINGS_Save();
}

void RADIO_ToggleVfoMR(void) {
  if (RADIO_IsChMode()) {
    loadVFO(gSettings.activeVFO);
    radio->channel += 1; // 0 -> 1
    radio->channel *= -1;
    saveVFO(gSettings.activeVFO);
    RADIO_SetupByCurrentVFO();
  } else {
    CHANNELS_LoadScanlist(TYPE_FILTER_CH, gSettings.currentScanlist);
    if (gScanlistSize == 0) {
      // Log("SL SIZE=0, skip");
      return;
    }
    radio->channel *= -1;
    radio->channel -= 1; // 1 -> 0
    // Log("radio->ch=%u", radio->channel);
    if (CHANNELS_Existing(radio->channel)) {
      RADIO_TuneToMR(radio->channel);
    } else {
      CHANNELS_Next(true);
      // Log("CH NEXT, radio->ch=%u", radio->channel);
    }
  }
  RADIO_SaveCurrentVFO();
}

void RADIO_UpdateSquelchLevel(bool next) {
  radio->squelch.value = IncDecU(radio->squelch.value, 0, 10, next);
  RADIO_SetSquelch(radio->squelch.value);
}

void RADIO_NextF(bool inc) {
  uint32_t step = StepFrequencyTable[radio->step];
  radio->rxF += inc ? step : -step;
  RADIO_TuneToPure(radio->rxF, !gIsListening);
}

void RADIO_UpdateStep(bool inc) {
  radio->step = IncDecU(radio->step, 0, STEP_500_0kHz, inc);
  radio->fixedBoundsMode = false;
  RADIO_SaveCurrentVFODelayed();
}

void RADIO_ToggleListeningBW(void) {
  if (radio->bw == BK4819_FILTER_BW_26k) {
    radio->bw = BK4819_FILTER_BW_6k;
  } else {
    ++radio->bw;
  }

  RADIO_SetFilterBandwidth(radio->bw);

  RADIO_SaveCurrentVFODelayed();
}

void RADIO_ToggleTxPower(void) {
  if (radio->power == TX_POW_HIGH) {
    radio->power = TX_POW_ULOW;
  } else {
    ++radio->power;
  }

  RADIO_SaveCurrentVFODelayed();
}

void RADIO_ToggleModulationEx(bool next) {
  if (radio->modulation == getNextModulation(next, true)) {
    return;
  }
  radio->modulation = getNextModulation(next, true);

  // NOTE: for right BW after switching from WFM to another
  RADIO_Setup();
  RADIO_SaveCurrentVFODelayed();
}

void RADIO_ToggleModulation(void) { RADIO_ToggleModulationEx(true); }

bool RADIO_HasSi() { return BK1080_ReadRegister(1) != 0x1080; }

void RADIO_SendDTMF(const char *pattern, ...) {
  char str[32] = {0};
  va_list args;
  va_start(args, pattern);
  vsnprintf(str, 31, pattern, args);
  va_end(args);
  RADIO_ToggleTX(true);
  if (gTxState == TX_ON) {
    SYS_DelayMs(200);
    BK4819_EnterDTMF_TX(true);
    BK4819_PlayDTMFString(str, true, 100, 100, 100, 100);
    RADIO_ToggleTX(false);
  }
}

void RADIO_GetGainString(char *String, uint8_t i) {
  if (i == AUTO_GAIN_INDEX) {
    sprintf(String, "AGC");
  } else {
    sprintf(String, "%+ddB", -gainTable[i].gainDb + 33);
  }
}

void RADIO_CheckAndListen() {
  Measurement m = {
      .f = radio->rxF,
      .rssi = RADIO_GetRSSI(),
      .snr = RADIO_GetSNR(),
      .noise = BK4819_GetNoise(),
      .glitch = BK4819_GetGlitch(),
  };
  m.open = RADIO_IsSquelchOpen();
  if (!gMonitorMode) {
    LOOT_Update(&m);
  }
  Log("RADIO_CheckAndListen f=%u, r=%u, o=%u", m.f, m.rssi, m.open);
  RADIO_ToggleRX(m.open);
  SP_ShiftGraph(-1);
  SP_AddGraphPoint(&m);
}
