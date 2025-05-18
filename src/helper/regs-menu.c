#include "regs-menu.h"
#include "../radio.h"
#include "../ui/graphics.h"
#include "channels.h"
#include "measurements.h"

typedef enum {
  REG_GAIN,
  REG_BW,
  REG_SQL,
  REG_MOD,
  REG_STEP,
  REG_RADIO,
  REG_AFC,
  REG_DEV,
  REG_MIC,
  REG_XTAL,
  REG_AF_DAC_GAIN,
  REG_TX_POWER,

  REG_COUNT,
} MenuReg;

static const uint8_t MENU_Y = 6;
static const uint8_t MENU_ITEM_H = 7;

static MenuReg currentIndex;
static bool inMenu;
static char buf[16];

static char *MENU_NAMES[] = {
    [REG_GAIN] = "Gain",             //
    [REG_BW] = "BW",                 //
    [REG_MOD] = "Mod",               //
    [REG_STEP] = "Step",             //
    [REG_SQL] = "SQL",               //
    [REG_RADIO] = "Radio",           //
    [REG_AFC] = "AFC",               //
    [REG_DEV] = "DEV",               //
    [REG_MIC] = "MIC",               //
    [REG_XTAL] = "XTAL",             //
    [REG_AF_DAC_GAIN] = "AF_DAC_GAIN", //
    [REG_TX_POWER] = "TX POW",       //
};

static void updateValue(bool inc) {
  uint16_t v;
  switch (currentIndex) {
  case REG_GAIN:
    RADIO_SetGain(IncDecU(radio.gainIndex, 0, ARRAY_SIZE(gainTable), inc));
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_STEP:
    RADIO_UpdateStep(inc);
    break;
  case REG_SQL:
    RADIO_UpdateSquelchLevel(inc);
    break;
  case REG_MOD:
    RADIO_ToggleModulationEx(inc);
    break;
  case REG_RADIO:
    if (radio.radio == RADIO_BK4819) {
      radio.radio = RADIO_HasSi() ? RADIO_SI4732 : RADIO_BK1080;
    } else {
      radio.radio = RADIO_BK4819;
    }
    RADIO_SwitchRadio();
    RADIO_SaveCurrentVFO();
    RADIO_SetupByCurrentVFO();
    break;
  case REG_BW:
    RADIO_SetFilterBandwidth(radio.bw =
                                 IncDecU(radio.bw, 0, RADIO_GetBWCount(), inc));
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_AFC:
    BK4819_SetAFC(IncDecU(BK4819_GetAFC(), 0, 8 + 1, inc));
    break;
  case REG_XTAL:
    BK4819_XtalSet(IncDecU(BK4819_XtalGet(), 0, XTAL_3_38_4M + 1, inc));
    break;
  case REG_DEV:
    v = AdjustU(BK4819_GetRegValue(RS_DEV), 0, 1450, inc ? 10 : -10);
    BK4819_SetRegValue(RS_DEV, v);
    gSettings.deviation = v / 10;
    SETTINGS_DelayedSave();
    break;
  case REG_MIC:
    v = IncDecU(BK4819_GetRegValue(RS_MIC), 0, 16, inc);
    BK4819_SetRegValue(RS_MIC, v);
    gSettings.mic = v;
    SETTINGS_DelayedSave();
    break;
  case REG_AF_DAC_GAIN:
    BK4819_SetRegValue(RS_AF_DAC_GAIN, IncDecU(BK4819_GetRegValue(RS_AF_DAC_GAIN),
                                              0, 0b111111 + 1, inc));
    break;
  case REG_TX_POWER:
    radio.power = IncDecU(radio.power, 0, TX_POW_HIGH + 1, inc);
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_COUNT:
    break;
  }
}

static void updateValueAlt(bool inc) {
  uint16_t v;
  switch (currentIndex) {
  case REG_GAIN:
    // TODO: доделать
    switch (radio.gainIndex) {
    case AUTO_GAIN_INDEX:
      radio.gainIndex = PLUS2_GAIN_INDEX;
      break;
    case PLUS2_GAIN_INDEX:
      radio.gainIndex = PLUS10_GAIN_INDEX;
      break;
    case PLUS10_GAIN_INDEX:
      radio.gainIndex = PLUS33_GAIN_INDEX;
      break;
    default:
      radio.gainIndex = AUTO_GAIN_INDEX;
    }
    RADIO_SetGain(radio.gainIndex);
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_SQL:
    radio.squelch.type = IncDecU(radio.squelch.type, 0, SQUELCH_RSSI + 1, inc);
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_BW:
    switch (radio.bw) {
    case BK4819_FILTER_BW_6k:
      radio.bw = BK4819_FILTER_BW_12k;
      break;
    case BK4819_FILTER_BW_12k:
      radio.bw = BK4819_FILTER_BW_26k;
      break;
    default:
      radio.bw = BK4819_FILTER_BW_6k;
      break;
    }
    RADIO_SetFilterBandwidth(radio.bw);
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_DEV:
    v = AdjustU(BK4819_GetRegValue(RS_DEV), 0, 1450, inc ? 100 : -100);
    BK4819_SetRegValue(RS_DEV, v);
    gSettings.deviation = v / 10;
    SETTINGS_DelayedSave();
    break;
  default:
    break;
  }
}

static void getValue(MenuReg reg) {
  switch (reg) {
  case REG_GAIN:
    RADIO_GetGainString(buf, radio.radio, radio.gainIndex);
    break;
  case REG_BW:
    snprintf(buf, 16, "%s", RADIO_GetBWName());
    break;
  case REG_RADIO:
    snprintf(buf, 16, "%s", radioNames[radio.radio]);
    break;
  case REG_TX_POWER:
    snprintf(buf, 16, "%s", powerNames[radio.power]);
    break;
  case REG_MOD:
    snprintf(buf, 16, "%s", modulationTypeOptions[radio.modulation]);
    break;
  case REG_STEP:
    snprintf(buf, 16, "%u.%02ukHz", StepFrequencyTable[radio.step] / KHZ,
             StepFrequencyTable[radio.step] % KHZ);
    break;
  case REG_AFC:
    snprintf(buf, 16, "%u", BK4819_GetAFC());
    break;
  case REG_XTAL:
    snprintf(buf, 16, "%u", BK4819_XtalGet());
    break;
  case REG_DEV:
    snprintf(buf, 16, "%u", BK4819_GetRegValue(RS_DEV));
    break;
  case REG_MIC:
    snprintf(buf, 16, "%u", BK4819_GetRegValue(RS_MIC));
    break;
  case REG_SQL:
    snprintf(buf, 16, "%s %u", sqTypeNames[radio.squelch.type],
             radio.squelch.value);
    break;
  case REG_AF_DAC_GAIN:
    snprintf(buf, 16, "%u", BK4819_GetRegValue(RS_AF_DAC_GAIN));
    break;
  case REG_COUNT:
    break;
  }
}

const uint8_t MENU_LINES_TO_SHOW = 7;

void REGSMENU_Draw() {
  if (inMenu) {

    const uint8_t size = REG_COUNT;
    const uint16_t maxItems =
        size < MENU_LINES_TO_SHOW ? size : MENU_LINES_TO_SHOW;
    const uint16_t offset = Clamp(currentIndex - 2, 0, size - maxItems);

    FillRect(0, MENU_Y, LCD_XCENTER, maxItems * MENU_ITEM_H + 2, C_CLEAR);
    DrawRect(0, MENU_Y, LCD_XCENTER, maxItems * MENU_ITEM_H + 2, C_FILL);

    for (uint8_t i = 0; i < maxItems; ++i) {
      const uint16_t itemIndex = i + offset;
      const bool isCurrent = itemIndex == currentIndex;
      const Color color = isCurrent ? C_INVERT : C_FILL;
      const uint8_t ry = MENU_Y + 1 + i * MENU_ITEM_H;
      const uint8_t y = ry + 5;

      if (isCurrent) {
        FillRect(0, ry, LCD_XCENTER, MENU_ITEM_H, C_FILL);
      }

      getValue(itemIndex);
      PrintSmallEx(5, y, POS_L, color, "%s", MENU_NAMES[itemIndex]);
      PrintSmallEx(LCD_XCENTER - 5, y, POS_R, color, "%s", buf);
    }
  }
}

bool REGSMENU_Key(KEY_Code_t key, Key_State_t state) {
  switch (key) {
  case KEY_0:
    inMenu = !inMenu;
    return true;
  case KEY_UP:
  case KEY_DOWN:
    if (inMenu) {
      currentIndex = IncDecU(currentIndex, 0, REG_COUNT, key == KEY_DOWN);
      return true;
    }
    break;
  case KEY_2:
  case KEY_8:
    if (inMenu) {
      radio.fixedBoundsMode = false;
      updateValue(key == KEY_2);
      return true;
    }
    break;
  case KEY_1:
  case KEY_7:
    if (inMenu) {
      radio.fixedBoundsMode = false;
      updateValueAlt(key == KEY_1);
      return true;
    }
    break;
  case KEY_EXIT:
    if (inMenu) {
      inMenu = false;
      return true;
    }
    break;
  default:
    break;
  }

  return false;
}
