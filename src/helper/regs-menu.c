#include "regs-menu.h"
#include "../radio.h"
#include "../ui/graphics.h"
#include "channels.h"
#include "measurements.h"

typedef enum {
  REG_GAIN,
  REG_BW,
  REG_STEP,
  REG_SQL,
  REG_RADIO,
  REG_AFC,
  REG_DEV,

  REG_COUNT,
} MenuReg;

static const uint8_t MENU_Y = 6;
static const uint8_t MENU_ITEM_H = 7;

static MenuReg currentParam;
static bool inMenu;
static char buf[16];

static char *MENU_NAMES[] = {
    [REG_GAIN] = "Gain",   //
    [REG_BW] = "BW",       //
    [REG_STEP] = "Step",   //
    [REG_SQL] = "SQL",     //
    [REG_RADIO] = "Radio", //
    [REG_AFC] = "AFC",     //
    [REG_DEV] = "DEV",     //
};

static void updateValue(bool inc) {
  uint16_t v;
  switch (currentParam) {
  case REG_GAIN:
    RADIO_SetGain(IncDecU(radio->gainIndex, 0, ARRAY_SIZE(gainTable), inc));
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_STEP:
    RADIO_UpdateStep(inc);
    break;
  case REG_SQL:
    RADIO_UpdateSquelchLevel(inc);
    break;
  case REG_RADIO:
    if (radio->radio == RADIO_BK4819) {
      radio->radio = RADIO_HasSi() ? RADIO_SI4732 : RADIO_BK1080;
    } else {
      radio->radio = RADIO_BK4819;
    }
    RADIO_Setup();
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_BW:
    RADIO_SetFilterBandwidth(
        radio->bw = IncDecU(radio->bw, 0, RADIO_GetBWCount(radio), inc));
    RADIO_SaveCurrentVFODelayed();
    break;
  case REG_AFC:
    BK4819_SetAFC(IncDecU(BK4819_GetAFC(), 0, 8 + 1, inc));
    break;
  case REG_DEV:
    v = AdjustU(BK4819_GetRegValue(RS_DEV), 0, 1450, inc ? 100 : -100);
    BK4819_SetRegValue(RS_DEV, v);
    gSettings.deviation = v / 10;
    SETTINGS_DelayedSave();
    break;
  case REG_COUNT:
    break;
  }
}

static void getValue(MenuReg reg) {
  switch (reg) {
  case REG_GAIN:
    RADIO_GetGainString(buf, radio->radio, radio->gainIndex);
    break;
  case REG_BW:
    snprintf(buf, 16, "%s", RADIO_GetBWName(radio));
    break;
  case REG_RADIO:
    snprintf(buf, 16, "%s", radioNames[radio->radio]);
    break;
  case REG_STEP:
    snprintf(buf, 16, "%u.%02ukHz", StepFrequencyTable[radio->step] / KHZ,
             StepFrequencyTable[radio->step] % KHZ);
    break;
  case REG_AFC:
    snprintf(buf, 16, "%u", BK4819_GetAFC());
    break;
  case REG_DEV:
    snprintf(buf, 16, "%u", gSettings.deviation * 10);
    break;
  case REG_SQL:
    snprintf(buf, 16, "%s %u", sqTypeNames[radio->squelch.type],
             radio->squelch.value);
    break;
  case REG_COUNT:
    break;
  }
}

void REGSMENU_Draw() {
  if (inMenu) {
    FillRect(0, MENU_Y, LCD_XCENTER, REG_COUNT * MENU_ITEM_H + 2, C_CLEAR);
    DrawRect(0, MENU_Y, LCD_XCENTER, REG_COUNT * MENU_ITEM_H + 2, C_FILL);
    for (uint8_t i = 0; i < REG_COUNT; ++i) {
      bool isCurrent = i == currentParam;
      getValue(i);
      if (isCurrent) {
        FillRect(0, MENU_Y + 1 + i * MENU_ITEM_H, LCD_XCENTER, MENU_ITEM_H,
                 C_FILL);
      }
      PrintSmallEx(5, MENU_Y + 1 + i * MENU_ITEM_H + 5, POS_L,
                   isCurrent ? C_INVERT : C_FILL, "%s", MENU_NAMES[i]);
      PrintSmallEx(LCD_XCENTER - 5, MENU_Y + 1 + i * MENU_ITEM_H + 5, POS_R,
                   isCurrent ? C_INVERT : C_FILL, "%s", buf);
    }
  }
}

bool REGSMENU_Key(KEY_Code_t key, Key_State_t state) {
  switch (key) {
  case KEY_4:
    inMenu = !inMenu;
    return true;
  case KEY_UP:
  case KEY_DOWN:
    if (inMenu) {
      currentParam = IncDecU(currentParam, 0, REG_COUNT, key == KEY_DOWN);
      return true;
    }
    break;
  case KEY_2:
  case KEY_8:
    if (inMenu) {
      updateValue(key == KEY_2);
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
