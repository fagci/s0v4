#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include "../misc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const uint16_t RSSI_MIN = 28;
static const uint16_t RSSI_MAX = 226;

typedef struct {
  uint16_t ro;
  uint16_t rc;
  uint8_t no;
  uint8_t nc;
  uint8_t go;
  uint8_t gc;
} SQL;

static const uint8_t rssi2s[2][15] = {
    {121, 115, 109, 103, 97, 91, 85, 79, 73, 63, 53, 43, 33, 23, 13},
    {141, 135, 129, 123, 117, 111, 105, 99, 93, 83, 73, 63, 53, 43, 33},
};

long long Clamp(long long v, long long min, long long max);
int ConvertDomain(int aValue, int aMin, int aMax, int bMin, int bMax);
uint32_t ClampF(uint32_t v, uint32_t min, uint32_t max);
uint32_t ConvertDomainF(uint32_t aValue, uint32_t aMin, uint32_t aMax,
                        uint32_t bMin, uint32_t bMax);
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax);
uint8_t DBm2S(int dbm, bool isVHF);
int Rssi2DBm(uint16_t rssi);
uint16_t Mid(const uint16_t *array, size_t n);
uint16_t Min(const uint16_t *array, size_t n);
uint16_t Max(const uint16_t *array, size_t n);
uint16_t Mean(const uint16_t *array, size_t n);
uint16_t Std(const uint16_t *data, size_t n);

int32_t AdjustI(int32_t val, int32_t min, int32_t max, int32_t inc);
uint32_t AdjustU(uint32_t val, uint32_t min, uint32_t max, int32_t inc);
int32_t IncDecI(int32_t val, int32_t min, int32_t max, bool inc);
uint32_t IncDecU(uint32_t val, uint32_t min, uint32_t max, bool inc);

bool IsReadable(char *name);
SQL GetSql(uint8_t level);
uint32_t DeltaF(uint32_t f1, uint32_t f2);
uint32_t RoundToStep(uint32_t f, uint32_t step);

#endif /* end of include guard: MEASUREMENTS_H */
