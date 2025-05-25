#include "vfo1.h"
#include "../dcs.h"
#include "../driver/gpio.h"
#include "../helper/bands.h"
#include "../helper/channels.h"
#include "../helper/measurements.h"
#include "../helper/numnav.h"
#include "../helper/regs-menu.h"
#include "../helper/vfo.h"
#include "../inc/dp32g030/gpio.h"
#include "../radio.h"
#include "../scheduler.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/menu.h"
#include "../ui/spectrum.h"
#include "../ui/statusline.h"
#include "apps.h"
#include "chcfg.h"
#include "chlist.h"
#include "finput.h"

static char String[16];

static void setChannel(uint16_t v) { RADIO_TuneToCH(v); }

static void tuneTo(uint32_t f) {
  RADIO_TuneToSave(GetTuneF(f));
  radio.fixedBoundsMode = false;
  RADIO_SaveCurrentVFO();
}

void VFO1_init(void) {
  VFO_LoadScanlist(0);
  RADIO_LoadCurrentVFO();
  gLastActiveLoot = NULL;
}

static uint32_t lastUpdate;
static uint32_t lastRender;

static const Step liveStep = STEP_5_0kHz;

static uint8_t mWatchVfoIndex = 0;

static VFO *shadowVfo;
static VFO *mWatchVfo;

void VFO1_update(void) {
  if (Now() - lastUpdate >= SQL_DELAY) {
    RADIO_CheckAndListen();
    lastUpdate = Now();

    // multiwatch
    if (gSettings.mWatch && gTxState != TX_ON) {
      if (gIsListening) {
        if (gSettings.mWatch == MW_SWITCH) {
          VFO_Select(mWatchVfoIndex);
          mWatchVfo = NULL;
        } else if (mWatchVfoIndex != gSettings.activeVFO) {
          mWatchVfo = shadowVfo;
        }
      } else {
        mWatchVfoIndex = IncDecU(mWatchVfoIndex, 0, VFO_GetSize(), true);
        shadowVfo = VFO_Get(mWatchVfoIndex);
        RADIO_SetupEx(*shadowVfo);
        BK4819_TuneTo(shadowVfo->rxF, true);
      }

      if (!gMonitorMode) {
        gLoot.f = shadowVfo->rxF;
        gLoot.open = gIsListening;
        LOOT_Update(&gLoot);
      }
    }
  }

  if (Now() - lastRender >= 1000) {
    lastRender = Now();
    gRedrawScreen = true;
  }
}

bool VFO1_key(KEY_Code_t key, Key_State_t state) {
  if (!gIsNumNavInput && state == KEY_RELEASED && REGSMENU_Key(key, state)) {
    return true;
  }

  if (state == KEY_RELEASED && RADIO_IsChMode()) {
    if (!gIsNumNavInput && key <= KEY_9) {
      NUMNAV_Init(radio.channel, 0, CHANNELS_GetCountMax() - 1);
      gNumNavCallback = setChannel;
    }
    if (gIsNumNavInput) {
      NUMNAV_Input(key);
      return true;
    }
  }

  if (key == KEY_PTT && !gIsNumNavInput) {
    RADIO_ToggleTX(state == KEY_PRESSED);
    return true;
  }

  // pressed or hold continue
  if (state == KEY_RELEASED || state == KEY_LONG_PRESSED_CONT) {
    bool isSsb = RADIO_IsSSB();
    switch (key) {
    case KEY_UP:
    case KEY_DOWN:
      if (RADIO_IsChMode()) {
        CHANNELS_Next(key == KEY_UP);
      } else {
        RADIO_NextF(key == KEY_UP);
      }
      RADIO_SaveCurrentVFODelayed();
      return true;
    case KEY_SIDE1:
    case KEY_SIDE2:
      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio.rxF + (key == KEY_SIDE1 ? 5 : -5));
        return true;
      }
      break;
    default:
      break;
    }
  }

  if (state == KEY_LONG_PRESSED) {
    switch (key) {
    case KEY_1:
      gChListFilter = TYPE_FILTER_BAND;
      APPS_run(APP_CH_LIST);
      return true;
    case KEY_2:
      if (gCurrentApp == APP_VFO1) {
        gSettings.iAmPro = !gSettings.iAmPro;
        SETTINGS_Save();
        return true;
      }
      return false;
    case KEY_3:
      RADIO_ToggleVfoMR();
      VFO1_init();
      return true;
    case KEY_4:
      gShowAllRSSI = !gShowAllRSSI;
      return true;
    case KEY_5:
      return true;
    case KEY_6:
      RADIO_ToggleTxPower();
      return true;
    case KEY_7:
      RADIO_UpdateStep(true);
      return true;
    case KEY_8:
      radio.offsetDir = IncDecU(radio.offsetDir, 0, OFFSET_MINUS, true);
      return true;
    case KEY_0:
      RADIO_ToggleModulation();
      return true;
    case KEY_STAR:
      APPS_run(APP_SCANER);
      return true;
    case KEY_SIDE1:
    case KEY_SIDE2:
      SP_NextGraphUnit(key == KEY_SIDE1);
      return true;
    case KEY_EXIT:
      if (gLastActiveLoot->f != radio.rxF) {
        for (uint8_t i = 0; i < VFO_GetSize(); ++i) {
          if (VFO_Get(i)->rxF == gLastActiveLoot->f) {
            VFO_Select(i);
            mWatchVfo = NULL;
            return true;
          }
        }
        RADIO_TuneToSave(gLastActiveLoot->f);
        return true;
      }
      break;
    default:
      break;
    }
  }

  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
      gFInputCallback = tuneTo;
      APPS_run(APP_FINPUT);
      APPS_key(key, state);
      return true;
    case KEY_F:
      gChEd = radio;
      if (RADIO_IsChMode()) {
        gChEd.meta.type = TYPE_CH;
      }
      APPS_run(APP_CH_CFG);
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_SIDE1:
      gMonitorMode = !gMonitorMode;
      return true;
    case KEY_SIDE2:
      GPIO_FlipBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
      break;
    case KEY_EXIT:
      if (!APPS_exit()) {
        mWatchVfo = NULL;
        VFO_Next(true);
        RADIO_SetupByCurrentVFO();
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

static void renderTxRxState(uint8_t y, bool isTx) {
  if (isTx && gTxState != TX_ON) {
    PrintMediumBoldEx(LCD_XCENTER, y, POS_C, C_FILL, "%s",
                      TX_STATE_NAMES[gTxState]);
  }
}

static void renderChannelName(uint8_t y, uint16_t channel) {
  FillRect(0, y - 14, 30, 7, C_FILL);
  PrintSmallEx(15, y - 9, POS_C, C_INVERT, "VFO %u/%u", gSettings.activeVFO + 1,
               VFO_GetSize());
  if (RADIO_IsChMode()) {
    PrintSmallEx(32, y - 9, POS_L, C_FILL, "MR %03u", channel);
    UI_Scanlists(LCD_WIDTH - 25, y - 13, gSettings.currentScanlist);
  }
}

static void renderProModeInfo(uint8_t y) {
  if (radio.radio == RADIO_BK4819) {
    PrintSmall(0, LCD_HEIGHT - 10, "R %+3u N %+3u G %+3u SNR %+2u", gLoot.rssi,
               gLoot.noise, gLoot.glitch, gLoot.snr);
  } else {
    PrintSmall(0, LCD_HEIGHT - 10, "R %+3u SNR %+2u", gLoot.rssi, gLoot.snr);
  }
}

void VFO1_render(void) {
  const uint8_t BASE = 40;

  if (gIsNumNavInput) {
    STATUSLINE_SetText("Select: %s", gNumNavInput);
  } else if (gSettings.iAmPro &&
             (!gSettings.mWatch || gIsListening)) { // NOTE mwatch is temporary
    STATUSLINE_RenderRadioSettings();
  } else {
    STATUSLINE_SetText(radio.name);
  }

  uint32_t f = gTxState == TX_ON ? RADIO_GetTXF() : GetScreenF(radio.rxF);
  const char *mod = modulationTypeOptions[radio.modulation];

  if (RADIO_IsChMode()) {
    PrintMediumEx(LCD_XCENTER, BASE - 16, POS_C, C_FILL, radio.name);
  } else {
    if (gCurrentBand.meta.type == TYPE_BAND_DETACHED) {
      PrintSmallEx(32, 12, POS_L, C_FILL, "*%s", gCurrentBand.name);
    } else {
      PrintSmallEx(
          32, 12, POS_L, C_FILL, radio.fixedBoundsMode ? "=%s:%u" : "%s:%u",
          gCurrentBand.name, CHANNELS_GetChannel(&gCurrentBand, radio.rxF) + 1);
    }
  }

  // Шаг, полоса, уровень SQL, мощность, субтоны, названия каналов.

  renderTxRxState(BASE, gTxState == TX_ON);
  UI_BigFrequency(BASE, f);
  PrintMediumEx(LCD_WIDTH - 1, BASE - 12, POS_R, C_FILL, mod);
  renderChannelName(21, radio.channel);
  const uint32_t step = StepFrequencyTable[radio.step];
  if (potentialTxState == TX_ON) {
    PrintSmallEx(LCD_XCENTER, BASE + 6, POS_C, C_FILL, "%s",
                 TX_POWER_NAMES[radio.power]);
  }
  PrintSmallEx(LCD_WIDTH, BASE + 6, POS_R, C_FILL, "%d.%02d", step / KHZ,
               step % KHZ);

  if (radio.code.rx.type) {
    PrintRTXCode(String, radio.code.rx.type, radio.code.rx.value);
    PrintSmallEx(0, BASE - 6, POS_L, C_FILL, "R%s", String);
  }
  if (radio.code.tx.type) {
    PrintRTXCode(String, radio.code.tx.type, radio.code.tx.value);
    PrintSmallEx(0, BASE, POS_L, C_FILL, "T%s", String);
  }

  if (gSettings.iAmPro) {
    uint32_t lambda = 29979246 / (radio.rxF / 100);
    PrintSmallEx(0, BASE - 14, POS_L, C_FILL, "L=%u/%ucm", lambda, lambda / 4);
  }

  if (gMonitorMode) {
    SPECTRUM_Y = BASE + 2;
    SPECTRUM_H = LCD_HEIGHT - SPECTRUM_Y;
    if (gSettings.showLevelInVFO) {
      char *graphMeasurementNames[] = {
          [GRAPH_RSSI] = "RSSI",           //
          [GRAPH_PEAK_RSSI] = "Peak RSSI", //
          [GRAPH_AGC_RSSI] = "AGC RSSI",   //
          [GRAPH_NOISE] = "Noise",         //
          [GRAPH_GLITCH] = "Glitch",       //
          [GRAPH_SNR] = "SNR",             //
      };
      switch (graphMeasurement) {
      case GRAPH_RSSI:
      case GRAPH_COUNT:
        SP_RenderGraph(RSSI_MIN, RSSI_MAX);
        break;
      case GRAPH_NOISE:
      case GRAPH_GLITCH:
        SP_RenderGraph(0, 256);
        break;
      case GRAPH_SNR:
        SP_RenderGraph(0, 30);
        break;
      case GRAPH_PEAK_RSSI:
        SP_RenderGraph(15, 88);
        break;
      case GRAPH_AGC_RSSI:
        SP_RenderGraph(25, 128);
        break;
      }
      PrintSmallEx(0, SPECTRUM_Y + 5, POS_L, C_FILL, "%s %+3u",
                   graphMeasurementNames[graphMeasurement],
                   SP_GetLastGraphValue());
    } else {
      UI_RSSIBar(BASE + 1);
    }
  } else {
    if (gIsListening || gSettings.iAmPro) {
      UI_RSSIBar(BASE + 1);
    }
    if (gTxState == TX_ON) {
      UI_TxBar(BASE + 1);
    }
    if (gSettings.iAmPro) {
      renderProModeInfo(BASE);
    }
  }

  if (gLastActiveLoot) {
    const uint32_t ago = (Now() - gLastActiveLoot->lastTimeOpen) / 1000;
    if (gLastActiveLoot->ct != 255) {
      PrintRTXCode(String, CODE_TYPE_CONTINUOUS_TONE, gLastActiveLoot->ct);
      PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%s", String);
    } else if (gLastActiveLoot->cd != 255) {
      PrintRTXCode(String, CODE_TYPE_DIGITAL, gLastActiveLoot->cd);
      PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%s", String);
    }
    UI_DrawLoot(gLastActiveLoot, LCD_XCENTER, LCD_HEIGHT - 1, POS_C);
    if (ago) {
      PrintSmallEx(LCD_WIDTH, LCD_HEIGHT - 1, POS_R, C_FILL, "%u:%02u",
                   ago / 60, ago % 60);
    }
  }

  REGSMENU_Draw();
}
