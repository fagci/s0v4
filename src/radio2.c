#include "radio2.h"
#include "driver/bk1080.h"
#include "driver/bk4819-regs.h"
#include "driver/bk4819.h"
#include "driver/si473x.h"
#include "driver/uart.h"
#include "helper/channels.h"
#include "helper/measurements.h"
#include "misc.h"
#include <string.h>

RadioContext ctxBK;
RadioContext ctxBC;

// Глобальная таблица параметров (инициализируется один раз)
static RadioParamMeta g_param_table[PARAM_COUNT];

// Инициализация таблицы параметров
void InitParamTable(RadioContext *ctx) {
  g_param_table[PARAM_VOLUME] = (RadioParamMeta){
      .name = "Volume",
      .value_ptr = &ctx->settings.volume,
      .limits = {.values = RADIO_CAPS[ctx->radio_type].gains,
                 .values_count = RADIO_CAPS[ctx->radio_type].gains_count},
      .apply_fn = BK4819_SetVolume};
  // ... аналогично для других параметров
}

static int FindValueIndex(const int *values, size_t count, int target) {
  for (size_t i = 0; i < count; i++) {
    if (values[i] == target) {
      return i;
    }
  }
  return 0; // Возвращаем 0 если не найдено (безопасное значение по умолчанию)
}

static RadioParamMeta *GetParamMeta(ParamType type) {
  if (type >= PARAM_COUNT) {
    return &g_param_table[PARAM_VOLUME]; // Возвращаем параметр по умолчанию
  }
  return &g_param_table[type];
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

// Переключение между приёмниками
void RADIO2_SwitchActive(RadioContext *ctx) {
  bool hasSi = BK1080_ReadRegister(1) != 0x1080;
  if (ctx == &ctxBK) {
    rxTurnOff(hasSi ? RADIO_SI4732 : RADIO_BK1080);
    rxTurnOn(RADIO_BK4819);
  } else {
    rxTurnOff(RADIO_BK4819);
    rxTurnOn(hasSi ? RADIO_SI4732 : RADIO_BK1080);
  }
}

// Диапазон FM (88-108 МГц)
static const ModulationType si4732_fm_mods[] = {MOD_WFM};
static const uint16_t si4732_fm_bandwidths[] = {6, 8, 10, 12, 14};

// Диапазон AM/SSB (0.15-30 МГц)
static const ModulationType si4732_am_mods[] = {MOD_AM, MOD_LSB, MOD_USB};
static const uint16_t si4732_am_bandwidths[] = {3, 4, 6, 8};

static const FreqBandCapabilities si4732_bands[] = {
    {
        .min_freq = SI47XX_FM_F_MIN,
        .max_freq = SI47XX_FM_F_MAX,
        .modulations = si4732_fm_mods,
        .mods_count = ARRAY_SIZE(si4732_fm_mods),
        .bandwidths = si4732_fm_bandwidths,
        .bw_count = ARRAY_SIZE(si4732_fm_bandwidths),
    },
    {
        .min_freq = SI47XX_F_MIN,
        .max_freq = SI47XX_F_MAX,
        .modulations = si4732_am_mods,
        .mods_count = ARRAY_SIZE(si4732_am_mods),
        .bandwidths = si4732_am_bandwidths,
        .bw_count = ARRAY_SIZE(si4732_am_bandwidths),
    },
};

// Итоговая конфигурация SI4732
static const RadioCapabilities RADIO_CAPS[] = {
    {.name = "SI4732",
     .bands = si4732_bands,
     .bands_count = ARRAY_SIZE(si4732_bands)},
    // ... другие приёмники (BK4819 и т.д.)
};

void RADIO_UpdateHardwareState(RadioContext *ctx) {
  switch (ctx->radioType) {
  case RADIO_BK4819:
    ctx->hwState.frequency = BK4819_GetFrequency();
    ctx->hwState.modulation = BK4819_GetModulation();
    ctx->hwState.gain = BK4819_GetRegValue(RS_AF_RX_GAIN);
    ctx->hwState.modulation = BK4819_GetModulation();
    ctx->hwState.afc = BK4819_GetAFC();
    ctx->hwState.dev = BK4819_GetRegValue(RS_DEV);
    ctx->hwState.mic = BK4819_GetRegValue(RS_MIC);
    ctx->hwState.squelch.type = BK4819_GetRegValue(RS_SQ_TYPE);
    ctx->hwState.xtal = BK4819_GetRegValue(RS_XTAL_MODE);
    // ctx->hwState.squelch.value = ;
    // ctx->hwState.power = ;
    // ctx->hwState.bw = ;
    break;

  case RADIO_SI4732:
    bool valid = false;
    ctx->hwState.frequency = SI47XX_getFrequency(&valid);
    // ctx->hwState.modulation = SI47XX_MODE();
    // ctx->hwState.volume = SI473X_GetVolume();
    break;

  default:
    break;
  }
}

void RADIO2_Init(RadioContext *ctx, Radio r) {
  memset(ctx, 0, sizeof(RadioContext));
  ctx->radioType = r;

  // Инициализация железа
  switch (r) {
  case RADIO_BK4819:
    BK4819_Init();
    break;
  case RADIO_SI4732:
    if (gSettings.si4732PowerOff || !isSi4732On) {
      if (ctx->settings.modulation == MOD_LSB ||
          ctx->settings.modulation == MOD_USB) {
        SI47XX_PatchPowerUp();
      } else {
        SI47XX_PowerUp();
      }
    } else {
      SI47XX_SetVolume(63);
    }
    break;
  case RADIO_BK1080:
    BK1080_Init(ctx->settings.rxF, false);
    break;
  }

  // Запрашиваем актуальное состояние
  RADIO_UpdateHardwareState(ctx);

  // Синхронизируем настройки
  ctx->settings.rxF = ctx->hwState.frequency;
  ctx->settings.modulation = ctx->hwState.modulation;
}

void RADIO2_ApplySettings(RadioContext *ctx) {
  if (ctx->settings.dirty.frequency) {
    BK4819_SetFrequency(ctx->settings.rxF);
    ctx->hwState.frequency = ctx->settings.rxF;
    ctx->settings.dirty.frequency = false;
  }

  if (ctx->settings.dirty.modulation) {
    BK4819_SetModulation(ctx->settings.modulation);
    ctx->hwState.modulation = ctx->settings.modulation;
    ctx->settings.dirty.modulation = false;
  }

  // ... обработка других изменений
}

void RADIO2_BeginTransaction(RadioContext *ctx) {
  if (ctx->inTransaction)
    return;
  memcpy(&ctx->shadow, &ctx->settings, sizeof(RadioSettings));
  ctx->inTransaction = true;
}

void RADIO2_CommitTransaction(RadioContext *ctx) {
  if (!ctx->inTransaction)
    return;
  ctx->inTransaction = false;
  RADIO2_ApplySettings(ctx);
}

void RADIO2_RollbackTransaction(RadioContext *ctx) {
  if (!ctx->inTransaction)
    return;
  memcpy(&ctx->settings, &ctx->shadow, sizeof(RadioSettings));
  ctx->inTransaction = false;
  // Не применяем настройки - возвращаем старые значения
}

const FreqBandCapabilities *RADIO2_GetActiveBand(RadioContext *ctx) {
  const RadioCapabilities *caps = &RADIO_CAPS[ctx->radioType];

  for (size_t i = 0; i < caps->bands_count; i++) {
    if (ctx->settings.rxF >= caps->bands[i].min_freq &&
        ctx->settings.rxF <= caps->bands[i].max_freq) {
      return &caps->bands[i];
    }
  }
  return NULL; // Частота вне допустимых диапазонов
}

bool RADIO2_IsModulationValid(RadioContext *ctx, ModulationType mod) {
  const FreqBandCapabilities *band = RADIO_GetActiveBand(ctx);
  if (!band)
    return false;

  for (size_t i = 0; i < band->mods_count; i++) {
    if (band->modulations[i] == mod)
      return true;
  }
  return false;
}

void RADIO2_AdjustRadioParam(RadioContext *ctx, ParamType param_type,
                             int delta) {
  RadioParamMeta *param = GetParamMeta(ctx, param_type);
  const RadioCapabilities *caps = &RADIO_CAPS[ctx->radioType];

  if (param->limits.values) {
    // Для enum-параметров
    int index = FindValueIndex(param->limits.values, *param->value_ptr,
                               param->limits.values_count);
    index = IncDecU(index, 0, param->limits.values_count - 1, delta > 0);
    *param->value_ptr = param->limits.values[index];
  } else {
    // Для числовых параметров
    *param->value_ptr =
        Clamp(*param->value_ptr + delta, param->limits.min, param->limits.max);
  }

  if (param->apply_fn)
    param->apply_fn(*param->value_ptr);
}

// Установить точное значение
void RADIO2_SetParam(RadioParamMeta *param, int value) {
  *param->value_ptr = Clamp(value, param->limits.min, param->limits.max);
  if (param->apply_fn) {
    param->apply_fn(*param->value_ptr);
  }
}
