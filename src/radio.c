#include "radio.h"
#include "apps/vfo1.h"
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
#include "external/printf/printf.h"
#include "helper/bands.h"
#include "helper/battery.h"
#include "helper/channels.h"
#include "helper/lootlist.h"
#include "helper/measurements.h"
#include "helper/vfo.h"
#include "misc.h"
#include "scheduler.h"
#include "settings.h"
#include "system.h"
#include "ui/spectrum.h"
#include "ui/statusline.h"
#include <stdint.h>
#include <string.h>

CH radio;

Measurement gLoot = {0};

bool gIsListening = false;
bool gMonitorMode = false;
uint8_t gCurrentTxPower = 0;
TXState gTxState = TX_UNKNOWN;
bool gShowAllRSSI = false;
TXState potentialTxState;

static bool hasSi = false;
static bool hasSsbPatch = false;

static uint8_t oldRadio = 255;

const uint16_t StepFrequencyTable[15] = {
    2,   5,   50,  100,

    250, 500, 625, 833, 900, 1000, 1250, 2500, 5000, 10000, 50000,
};

const char *modulationTypeOptions[8] = {"FM",  "AM",  "LSB", "USB",
                                        "BYP", "RAW", "WFM"};
const char *powerNames[4] = {"ULOW", "LOW", "MID", "HIGH"};
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

static const Band SI4732_LB = {.rxF = SI47XX_F_MIN, .txF = SI47XX_F_MAX};
static const Band SI4732_FM = {.rxF = SI47XX_FM_F_MIN, .txF = SI47XX_FM_F_MAX};
static const Band BK1080_FM = {.rxF = BK1080_F_MIN, .txF = BK1080_F_MAX};
static const Band BK4819_RANGE = {.rxF = BK4819_F_MIN, .txF = BK4819_F_MAX};

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

static uint16_t getVfoChannel() { return VFO_GetCh(gSettings.activeVFO); }

static void loadVFO() { CHANNELS_Load(getVfoChannel(), &radio); }

static void saveVFO() { CHANNELS_Save(getVfoChannel(), &radio); }

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

  if (radio.rxF >= 88 * MHZ && radio.rxF <= BK1080_F_MAX) {
    items = MODS_WFM;
    sz = ARRAY_SIZE(MODS_WFM);
  } else if (radio.rxF <= SI47XX_F_MAX && radio.rxF >= BK4819_F_MIN) {
    if (hasSsbPatch) {
      items = MODS_BOTH_PATCH;
      sz = ARRAY_SIZE(MODS_BOTH_PATCH);
    } else {
      items = MODS_BOTH;
      sz = ARRAY_SIZE(MODS_BOTH);
    }
  } else if (BANDS_InRange(radio.rxF, SI4732_LB)) {
    if (hasSsbPatch) {
      items = MODS_SI4732_PATCH;
      sz = ARRAY_SIZE(MODS_SI4732_PATCH);
    } else {
      items = MODS_SI4732;
      sz = ARRAY_SIZE(MODS_SI4732);
    }
  }

  uint8_t curIndex = indexOfMod(items, sz, radio.modulation);

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

inline Radio RADIO_GetRadio() { return radio.radio; }

ModulationType RADIO_GetModulation() { return radio.modulation; }

const char *RADIO_GetBWName() {
  switch (radio.radio) {
  case RADIO_SI4732:
    if (RADIO_IsSSB()) {
      return bwNamesSiSSB[radio.bw];
    }
    return bwNamesSiAMFM[radio.bw];
  default:
    return bwNames[radio.bw];
  }
}

uint8_t RADIO_GetBWCount() {
  switch (radio.radio) {
  case RADIO_SI4732:
    if (RADIO_IsSSB()) {
      return ARRAY_SIZE(bwNamesSiSSB);
    }
    return ARRAY_SIZE(bwNamesSiAMFM);
  default:
    return ARRAY_SIZE(bwNames);
  }
}

void RADIO_GetGainString(char *buf, Radio radio, uint8_t i) {
  if (i == AUTO_GAIN_INDEX) {
    snprintf(buf, 16, "AGC");
    return;
  }
  switch (radio) {
  case RADIO_BK4819:
    snprintf(buf, 16, "%+ddB", -gainTable[i].gainDb + 33);
    break;
  default:
    snprintf(buf, 16, "%d", -i);
    break;
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
  BK4819_SetAFC(7);
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

static uint32_t saveVfoTime;
void RADIO_Update() {
  if (saveVfoTime && Now() > saveVfoTime) {
    RADIO_SaveCurrentVFO();
    saveVfoTime = 0;
  }
}

void RADIO_SaveCurrentVFODelayed(void) { saveVfoTime = Now() + 1000; }

static void setupToneDetection() {
  // Log("setupToneDetection");
  // HACK? to enable STE RX
  // Log("DC flt BW = 0");
  // BK4819_WriteRegister(BK4819_REG_7E, 0x302E); // DC flt BW 0=BYP
  uint16_t InterruptMask = BK4819_REG_3F_CxCSS_TAIL;
  if (gSettings.dtmfdecode) {
    BK4819_EnableDTMF();
    InterruptMask |= BK4819_REG_3F_DTMF_5TONE_FOUND;
  } else {
    BK4819_DisableDTMF();
  }
  switch (radio.code.rx.type) {
  case CODE_TYPE_DIGITAL:
  case CODE_TYPE_REVERSE_DIGITAL:
    // Log("DCS on");
    BK4819_SetCDCSSCodeWord(
        DCS_GetGolayCodeWord(radio.code.rx.type, radio.code.rx.value));
    InterruptMask |= BK4819_REG_3F_CDCSS_FOUND | BK4819_REG_3F_CDCSS_LOST;
    break;
  case CODE_TYPE_CONTINUOUS_TONE:
    // Log("CTCSS on");
    BK4819_SetCTCSSFrequency(CTCSS_Options[radio.code.rx.value]);
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

  switch (radio.power) {
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
    SYS_DelayMs(10);
    BK4819_GenTail(4);
    BK4819_WriteRegister(BK4819_REG_51, 0x9033);
    SYS_DelayMs(250);
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
    BK1080_Init(radio.rxF, true);
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
  switch (radio.code.tx.type) {
  case CODE_TYPE_CONTINUOUS_TONE:
    BK4819_SetCTCSSFrequency(CTCSS_Options[radio.code.tx.value]);
    break;
  case CODE_TYPE_DIGITAL:
  case CODE_TYPE_REVERSE_DIGITAL:
    BK4819_SetCDCSSCodeWord(
        DCS_GetGolayCodeWord(radio.code.tx.type, radio.code.tx.value));
    break;
  default:
    BK4819_ExitSubAu();
    break;
  }
}

uint32_t RADIO_GetTXF() {
  switch (radio.offsetDir) {
  case OFFSET_FREQ:
    return radio.txF;
  case OFFSET_PLUS:
    return radio.rxF + radio.txF;
  case OFFSET_MINUS:
    return radio.rxF - radio.txF;
  default:
    return radio.rxF;
  }
}

TXState RADIO_GetTXState(uint32_t txF) {
  if (gSettings.upconverter) {
    return TX_DISABLED_UPCONVERTER;
  }

  if (RADIO_GetRadio() != RADIO_BK4819) {
    return TX_DISABLED;
  }

  Band txBand = BANDS_ByFrequency(txF);

  if (!txBand.allowTx && !(RADIO_IsChMode() && radio.allowTx)) {
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

bool RADIO_IsChMode() { return radio.isChMode; }

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
    // toggleBK1080SI4732(false);
    BK4819_TurnsOffTones_TurnsOnRX();

    gCurrentTxPower = 0;
    BK4819_SetupPowerAmplifier(0, 0);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
    BOARD_ToggleRed(false);
    BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);

    setupToneDetection();
    BK4819_TuneTo(radio.rxF, true);
  }
}

void RADIO_TuneToPure(uint32_t f, bool precise) {
  uint32_t s = 100; // 1kHz
  if (f < SI47XX_F_MAX) {
    s = 25; // 250Hz
  } else if (f >= BK1080_F_MIN && f <= BK1080_F_MAX) {
    s = 1000; // 10kHz
  }
  f += gCurrentBand.ppm * s;
  LOOT_Replace(&gLoot, f);
  Radio r = RADIO_GetRadio();
  Log("Tune %s to %u", radioNames[r], f);
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
  if (oldRadio == radio.radio) {
    return;
  }
  rxTurnOff(oldRadio);
  rxTurnOn(radio.radio);
  oldRadio = radio.radio;
}

void RADIO_SwitchRadio() {
  bool si4732Support = (BANDS_InRange(radio.rxF, SI4732_LB) ||
                        BANDS_InRange(radio.rxF, SI4732_FM));
  bool bk4819Support = BANDS_InRange(radio.rxF, BK4819_RANGE);
  bool bk1080Support = BANDS_InRange(radio.rxF, BK1080_FM);

  switch (radio.radio) {
  case RADIO_BK4819:
    if (!bk4819Support) {
      if (hasSi && si4732Support) {
        radio.radio = RADIO_SI4732;
      } else if (!hasSi && bk1080Support) {
        radio.radio = RADIO_BK1080;
      }
    }
    break;
  case RADIO_BK1080:
    if (!bk1080Support && bk4819Support) {
      radio.radio = RADIO_BK4819;
    }
    break;
  case RADIO_SI4732:
    if (!si4732Support && bk4819Support) {
      radio.radio = RADIO_BK4819;
    }
    break;
  }

  radio.modulation = getNextModulation(true, false);

  if (radio.radio != MOD_AM && !RADIO_IsSSB() && radio.radio == RADIO_SI4732 &&
      BANDS_InRange(radio.rxF, SI4732_LB)) {
    radio.modulation = MOD_AM;
  }

  RADIO_SwitchRadioPure();
}

static void checkVisibleBand() {
  if (!BANDS_InRange(radio.rxF, gCurrentBand)) {
    Log("band %s not in r of %u", gCurrentBand.name, radio.rxF);
    BANDS_SelectByFrequency(radio.rxF, radio.fixedBoundsMode);
  }
}

void RADIO_SetupByCurrentVFO(void) {
  Log("RADIO setup by VFO");

  checkVisibleBand();

  RADIO_SwitchRadio();
  RADIO_Setup();
  RADIO_TuneToPure(radio.rxF, !gMonitorMode);
  potentialTxState = RADIO_GetTXState(RADIO_GetTXF());
}

// USE CASE: set vfo temporary for current app
void RADIO_TuneTo(uint32_t f) {
  if (RADIO_IsChMode()) {
    radio.isChMode = false;
  }
  radio.txF = 0;
  radio.rxF = f;
  RADIO_SetupByCurrentVFO();
}

// USE CASE: set vfo and use in another app
void RADIO_TuneToSave(uint32_t f) {
  Log("Tune to save");
  gCurrentBand.misc.lastUsedFreq = f;
  radio.rxF = f; // TEST: save freq in vfo
  RADIO_SaveCurrentVFO();
  BANDS_SaveCurrent();
  RADIO_TuneTo(f);
}

void RADIO_SaveCurrentVFO(void) {
  if (RADIO_IsChMode()) {
    // save only active channel number
    // to load it instead of full VFO
    // and to prevent overwrite VFO with MR
    VFO oldVfo;
    int16_t vfoChNum = getVfoChannel();
    int16_t chToSave = radio.channel;
    CHANNELS_Load(vfoChNum, &oldVfo);
    oldVfo.channel = chToSave;
    oldVfo.isChMode = true;
    CHANNELS_Save(vfoChNum, &oldVfo);
    return;
  }
  VFO_SaveCurrent();
}
static bool initialized = false;
void RADIO_LoadCurrentVFO(void) {
  gMonitorMode = false;
  if (!initialized || radio.fixedBoundsMode) {
    loadVFO();
    initialized = true;
  }
  if (RADIO_IsChMode()) {
    RADIO_VfoLoadCH();
  }

  LOOT_Replace(&gLoot, radio.rxF);

  // needed to select gCurrentBand & set band index in SL
  CHANNELS_LoadScanlist(RADIO_IsChMode() ? TYPE_FILTER_CH : TYPE_FILTER_BAND,
                        gSettings.currentScanlist);

  RADIO_SetupByCurrentVFO();
}

void RADIO_SetSquelch(uint8_t sq) {
  radio.squelch.value = sq;
  BK4819_Squelch(sq, gSettings.sqlOpenTime, gSettings.sqlCloseTime);
  RADIO_SaveCurrentVFODelayed();
}

void RADIO_SetSquelchType(SquelchType t) {
  radio.squelch.type = t;
  RADIO_SaveCurrentVFODelayed();
}

void RADIO_SetGain(uint8_t gainIndex) {
  radio.gainIndex = gainIndex;
  // Log("GAIN: %+d", -gainTable[gainIndex].gainDb + 33);
  bool disableAGC;
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    BK4819_SetAGC(radio.modulation != MOD_AM, gainIndex);
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
  // Log("BW: %s", bwNames[bw]);
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
  // Log("---------- %s RADIO_Setup ----------", radioNames[RADIO_GetRadio()]);
  ModulationType mod = RADIO_GetModulation();
  RADIO_SetGain(radio.gainIndex);
  RADIO_SetFilterBandwidth(radio.bw);
  switch (RADIO_GetRadio()) {
  case RADIO_BK4819:
    // Log("SQ %s,%u", sqTypeNames[radio.squelch.type], radio.squelch.value);
    BK4819_SquelchType(radio.squelch.type);
    BK4819_Squelch(radio.squelch.value, gSettings.sqlOpenTime,
                   gSettings.sqlCloseTime);
    // Log("MOD: %s", modulationTypeOptions[mod]);
    BK4819_SetModulation(mod);

    setupToneDetection();
    BK4819_SetScrambler(radio.scrambler);
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

  return gShowAllRSSI ? RADIO_GetSNR() > radio.squelch.value : true;
}

void RADIO_VfoLoadCH() {
  uint16_t chNum = radio.channel;
  CHANNELS_Load(radio.channel, &radio);

  // NOTE: coz it modified by CHANNELS_Load
  radio.meta.type = TYPE_VFO;
  radio.channel = chNum;
  radio.isChMode = true;
}

void RADIO_TuneToBand(uint16_t num) {
  Log("Tune to band");
  if (CHANNELS_GetMeta(num).type == TYPE_BAND) {
    BANDS_Select(num, true);
    if (BANDS_InRange(radio.rxF, gCurrentBand)) {
      return;
    }
    if (BANDS_InRange(gCurrentBand.misc.lastUsedFreq, gCurrentBand)) {
      RADIO_TuneToSave(gCurrentBand.misc.lastUsedFreq);
    } else {
      RADIO_TuneToSave(gCurrentBand.rxF);
    }
  }
}

void RADIO_TuneToCH(uint16_t num) {
  if (CHANNELS_GetMeta(num).type == TYPE_CH) {
    radio.channel = num;
    radio.isChMode = true;
    RADIO_VfoLoadCH();
    RADIO_SaveCurrentVFO();
    RADIO_SetupByCurrentVFO();
    CHANNELS_SetScanlistIndexFromRadio();
  }
}

bool RADIO_TuneToMR(uint16_t num) {
  Log("Tune to MR %u", num);
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
  radio.isChMode = false;
  return false;
}

void RADIO_ToggleVfoMR(void) {
  if (RADIO_IsChMode()) {
    // loadVFO();
    radio.isChMode = false;
    saveVFO();
    RADIO_SetupByCurrentVFO();
  } else {
    CHANNELS_LoadScanlist(TYPE_FILTER_CH, gSettings.currentScanlist);
    if (gScanlistSize == 0) {
      return;
    }
    // loadVFO();
    Log("radio.ch=%u", radio.channel);
    if (CHANNELS_GetMeta(radio.channel).type == TYPE_CH) {
      RADIO_TuneToMR(radio.channel);
      Log("CH TUNE, radio.ch=%u", radio.channel);
    } else {
      CHANNELS_Next(true);
      Log("CH NEXT, radio.ch=%u", radio.channel);
      saveVFO();
    }
  }
  RADIO_SaveCurrentVFO();
}

void RADIO_UpdateSquelchLevel(bool next) {
  radio.squelch.value = IncDecU(radio.squelch.value, 0, 10, next);
  RADIO_SetSquelch(radio.squelch.value);
}

void RADIO_NextF(bool inc) {
  uint32_t step = StepFrequencyTable[radio.step];
  radio.rxF += inc ? step : -step;
  RADIO_TuneToPure(radio.rxF, !gIsListening);
}

void RADIO_UpdateStep(bool inc) {
  radio.step = IncDecU(radio.step, 0, STEP_500_0kHz, inc);
  radio.fixedBoundsMode = false;
  RADIO_SaveCurrentVFODelayed();
}

void RADIO_ToggleListeningBW(void) {
  if (radio.bw == BK4819_FILTER_BW_26k) {
    radio.bw = BK4819_FILTER_BW_6k;
  } else {
    ++radio.bw;
  }

  RADIO_SetFilterBandwidth(radio.bw);

  RADIO_SaveCurrentVFODelayed();
}

void RADIO_ToggleTxPower(void) {
  if (radio.power == TX_POW_HIGH) {
    radio.power = TX_POW_ULOW;
  } else {
    ++radio.power;
  }

  RADIO_SaveCurrentVFODelayed();
}

void RADIO_ToggleModulationEx(bool next) {
  ModulationType nextMod = getNextModulation(next, true);
  if (radio.modulation == nextMod) {
    return;
  }
  radio.modulation = nextMod;

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

static uint32_t lastCloseToneFound;

// TODO: переделать на что-то более гибкое
static void checkTone(Measurement *m) {
  if (RADIO_GetRadio() != RADIO_BK4819) {
    return;
  }
  while (BK4819_ReadRegister(BK4819_REG_0C) & 1) {
    BK4819_WriteRegister(BK4819_REG_02, 0);

    uint16_t intBits = BK4819_ReadRegister(BK4819_REG_02);

    if ((intBits & BK4819_REG_02_CxCSS_TAIL) ||
        (intBits & BK4819_REG_02_CTCSS_FOUND) ||
        (intBits & BK4819_REG_02_CDCSS_FOUND)) {
      Log("Tail tone or ctcss/dcs found");
      lastCloseToneFound = Now();
      m->open = false;
    }
    if ((intBits & BK4819_REG_02_CTCSS_LOST) ||
        (intBits & BK4819_REG_02_CDCSS_LOST)) {
      Log("ctcss/dcs lost");
      lastCloseToneFound = 0;
      m->open = true;
    }

    // to keep it closed while STE transmitting
    if (Now() - lastCloseToneFound < 250) {
      m->open = false;
    }

    /* if (intBits & BK4819_REG_02_DTMF_5TONE_FOUND) {
      uint8_t code = BK4819_GetDTMF_5TONE_Code();
      Log("DTMF: %u", code);
    } */
  }
}

void RADIO_CheckAndListen() {
  gLoot.f = radio.rxF;
  gLoot.rssi = RADIO_GetRSSI();

  if (gSettings.iAmPro || graphMeasurement == GRAPH_SNR) {
    gLoot.snr = RADIO_GetSNR();
  }
  if (gSettings.iAmPro || graphMeasurement == GRAPH_NOISE) {
    gLoot.noise = BK4819_GetNoise();
  }
  if (gSettings.iAmPro || graphMeasurement == GRAPH_GLITCH) {
    gLoot.glitch = BK4819_GetGlitch();
  }
  if (graphMeasurement == GRAPH_PEAK_RSSI) {
    gLoot.lnaPeakRssi = BK4819_GetLnaPeakRSSI();
  }
  if (graphMeasurement == GRAPH_AGC_RSSI) {
    gLoot.rssiAgc = BK4819_GetAgcRSSI();
  }

  if (radio.code.rx.type == CODE_TYPE_OFF) {
    gLoot.open = RADIO_IsSquelchOpen();
    if (gLoot.open) {
      checkTone(&gLoot);
    }
  } else {
    checkTone(&gLoot);
  }

  bool opn = gLoot.open;
  if (!gMonitorMode && radio.radio == RADIO_BK4819) {
    LOOT_Update(&gLoot);
  }
  if (gLoot.open) {
    gLoot.open = opn; // TEST: maybe there STE broken
  }
  RADIO_ToggleRX(gLoot.open);
  SP_ShiftGraph(-1);
  SP_AddGraphPoint(&gLoot);
}
