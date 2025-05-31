#ifndef RADIO2_H
#define RADIO2_H

#include "driver/bk4819.h"
#include "driver/uart.h"
#include "helper/channels.h"
typedef struct {
  // Настройки приёмника
  uint32_t rxF;
  uint8_t squelch;
  uint8_t gain;
  ModulationType modulation;

  // Флаги изменений
  struct {
    bool frequency : 1;
    bool volume : 1;
    bool squelch : 1;
    bool modulation : 1;
    // ... другие флаги
  } dirty;
} RadioSettings;

typedef struct {
  uint32_t frequency;
  uint8_t gain;
  BK4819_FilterBandwidth_t bw;
  ModulationType modulation;
  Squelch squelch;
  uint8_t afc;
  uint16_t dev;
  uint8_t mic;
  XtalMode xtal;
  TXOutputPower power;
} HardwareState;

typedef struct {
  RadioSettings settings;
  RadioSettings shadow; // Для транзакций
  Radio radioType;
  bool inTransaction;
  HardwareState hwState; // Текущее состояние железа
} RadioContext;

typedef struct {
  uint32_t min_freq; // Нижняя граница диапазона (в Гц)
  uint32_t max_freq; // Верхняя граница диапазона
  const ModulationType *modulations; // Доступные модуляции
  size_t mods_count;
  const uint16_t *bandwidths; // Доступные полосы (кГц)
  size_t bw_count;
  // ... другие параметры (gain, step и т.д.)
} FreqBandCapabilities;

typedef struct {
  const char *name;
  int *value_ptr;
  struct {
    int min;
    int max;
    const int *values; // Для enum-параметров
    size_t values_count;
  } limits;
  void (*apply_fn)(int);
} RadioParamMeta;

// Структура для быстрого доступа к параметрам по типу
typedef enum {
  PARAM_VOLUME,
  PARAM_SQUELCH,
  PARAM_MODULATION,
  // ... другие параметры
  PARAM_COUNT
} ParamType;

typedef struct {
  const char *name;
  const FreqBandCapabilities *bands; // Массив диапазонов
  size_t bands_count;
} RadioCapabilities;

// Основные функции
void RADIO2_Init(RadioContext *ctx, Radio r);
void RADIO2_ApplySettings(RadioContext *ctx); // Применяет все изменения

// Транзакционное API
void RADIO2_BeginTransaction(RadioContext *ctx);
void RADIO2_CommitTransaction(RadioContext *ctx);
void RADIO2_RollbackTransaction(RadioContext *ctx);

const FreqBandCapabilities *RADIO_GetActiveBand(RadioContext *ctx);

// Макросы для удобного доступа к настройкам
#define RADIO2_SET(ctx, field, value)                                          \
  do {                                                                         \
    UART_printf("SET ctx->field = value");                                     \
    (ctx)->settings.field = (value);                                           \
    (ctx)->settings.dirty.field = true;                                        \
    if (!(ctx)->inTransaction)                                                 \
      RADIO2_ApplySettings(ctx);                                               \
  } while (0)

#define RADIO2_GET(ctx, field) ((ctx)->settings.field)

// Универсальный поиск в массиве структур по полю
#define ARRAY_FIND(arr, val, field)                                                                  \
  ({                                                                                                 \
    size_t index = 0;                                                                                \
    while (index < ARRAY_SIZE(arr) && arr[index].field != val) {                                     \
      index++;                                                                                       \
    }                                                                                                \
    (index < ARRAY_SIZE(arr)) ? index : 0; /* Возвращаем 0 если не найдено */ \
  })

// Получить параметр по имени (из таблицы)
#define PARAM_GET(name) (&params[ARRAY_FIND(params, name, .name)])

// Универсальное изменение
#define PARAM_ADJUST(name, delta) AdjustParam(PARAM_GET(name), delta)

#define PARAM_SET(name, value) SetParam(PARAM_GET(name), value)

#endif /* end of include guard: RADIO2_H */
