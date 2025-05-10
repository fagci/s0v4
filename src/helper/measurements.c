#include "../helper/measurements.h"
#include <stdint.h>

long long Clamp(long long v, long long min, long long max) {
  return v <= min ? min : (v >= max ? max : v);
}

int ConvertDomain(int aValue, int aMin, int aMax, int bMin, int bMax) {
  const int aRange = aMax - aMin;
  const int bRange = bMax - bMin;
  aValue = Clamp(aValue, aMin, aMax);
  return ((aValue - aMin) * bRange + aRange / 2) / aRange + bMin;
}

uint32_t ClampF(uint32_t v, uint32_t min, uint32_t max) {
  return v <= min ? min : (v >= max ? max : v);
}

uint32_t ConvertDomainF(uint32_t aValue, uint32_t aMin, uint32_t aMax,
                        uint32_t bMin, uint32_t bMax) {
  if (aMin == aMax) {
    return bMin;
  }

  const uint64_t aRange = (uint64_t)aMax - aMin;
  const uint64_t bRange = (uint64_t)bMax - bMin;

  aValue = ClampF(aValue, aMin, aMax);

  uint64_t scaledValue = (uint64_t)(aValue - aMin) * bRange;
  uint64_t result = (scaledValue + aRange / 2) / aRange + bMin;

  return (uint32_t)ClampF(result, bMin, bMax);
}

uint8_t DBm2S(int dbm, bool isVHF) {
  uint8_t i = 0;
  dbm *= -1;
  for (i = 0; i < 15; i++) {
    if (dbm >= rssi2s[isVHF][i]) {
      return i;
    }
  }
  return i;
}

int Rssi2DBm(uint16_t rssi) { return (rssi >> 1) - 160; }

// applied x2 to prevent initial rounding
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax) {
  return ConvertDomain(rssi - 320, -260, -120, pxMin, pxMax);
}

uint16_t Mid(const uint16_t *array, size_t n) {
  if (array == NULL || n == 0) {
    return 0;
  }
  int32_t sum = array[0];
  for (size_t i = 1; i < n; ++i) {
    sum += array[i];
  }
  return sum / n;
}

uint16_t Min(const uint16_t *array, size_t n) {
  if (array == NULL || n == 0) {
    return 0;
  }
  uint16_t min = array[0];
  for (size_t i = 1; i < n; ++i) {
    if (array[i] < min) {
      min = array[i];
    }
  }
  return min;
}

uint16_t Max(const uint16_t *array, size_t n) {
  if (array == NULL || n == 0) {
    return 0;
  }
  uint16_t max = array[0];
  for (size_t i = 1; i < n; ++i) {
    if (array[i] > max) {
      max = array[i];
    }
  }
  return max;
}

uint16_t Mean(const uint16_t *array, size_t n) {
  if (array == NULL || n == 0) {
    return 0;
  }
  uint32_t sum = array[0];
  for (size_t i = 1; i < n; ++i) {
    sum += array[i];
  }
  return sum / n;
}

uint16_t Sqrt(uint32_t v) {
  uint16_t res = 0;
  for (uint32_t i = 0; i < v; ++i) {
    if (i * i <= v) {
      res = i;
    } else {
      break;
    }
  }
  return res;
}

uint16_t Std(const uint16_t *data, size_t n) {
  if (data == NULL || n == 0) {
    return 0;
  }
  uint32_t sumDev = 0;
  for (uint8_t i = 0; i < n; ++i) {
    sumDev += data[i] * data[i];
  }
  return Sqrt(sumDev / n);
}

int32_t AdjustI(int32_t val, int32_t min, int32_t max, int32_t inc) {
  if (inc > 0) {
    return val == max - inc ? min : val + inc;
  } else {
    return val > min ? val + inc : max + inc;
  }
}

uint32_t AdjustU(uint32_t val, uint32_t min, uint32_t max, int32_t inc) {
  if (inc > 0) {
    return val == max - inc ? min : val + inc;
  } else {
    return val > min ? val + inc : max + inc;
  }
}

int32_t IncDecI(int32_t val, int32_t min, int32_t max, bool inc) {
  return AdjustI(val, min, max, inc ? 1 : -1);
}

uint32_t IncDecU(uint32_t val, uint32_t min, uint32_t max, bool inc) {
  return AdjustU(val, min, max, inc ? 1 : -1);
}

bool IsReadable(char *name) { return name[0] >= 32 && name[0] < 127; }

SQL GetSql(uint8_t level) {
  SQL sq = {0, 0, 255, 255, 255, 255};
  if (level == 0) {
    return sq;
  }

  sq.ro = ConvertDomain(level, 0, 10, 10, 180);
  sq.no = ConvertDomain(level, 0, 10, 64, 12);
  sq.go = ConvertDomain(level, 0, 10, 150, 0); // was 32, 6

  sq.rc = sq.ro - 4;
  sq.nc = sq.no + 4;
  sq.gc = sq.go + 4;
  return sq;
}

uint32_t DeltaF(uint32_t f1, uint32_t f2) {
  return f1 > f2 ? f1 - f2 : f2 - f1;
}

uint32_t RoundToStep(uint32_t f, uint32_t step) {
  uint32_t sd = f % step;
  if (sd > step / 2) {
    f += step - sd;
  } else {
    f -= sd;
  }
  return f;
}
