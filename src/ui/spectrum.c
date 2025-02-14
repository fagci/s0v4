#include "spectrum.h"
#include "../driver/uart.h"
#include "../helper/measurements.h"
#include "components.h"
#include "graphics.h"
#include <stdint.h>
#include <stdlib.h>

#define MAX_POINTS 128

typedef struct {
  uint16_t vMin;
  uint16_t vMax;
} VMinMax;

uint8_t SPECTRUM_Y = 8;
uint8_t SPECTRUM_H = 42;

static uint8_t S_BOTTOM;

static uint16_t rssiHistory[MAX_POINTS] = {0};
static uint16_t rssiGraphHistory[MAX_POINTS] = {0};

static uint8_t x = 0;
static uint8_t ox = UINT8_MAX;
static uint8_t filledPoints;

static Band *range;
static uint32_t step;

static uint16_t minRssi(const uint16_t *array, uint8_t n) {
  uint16_t min = UINT16_MAX;
  for (uint8_t i = 0; i < n; ++i) {
    if (array[i] && array[i] < min) {
      min = array[i];
    }
  }
  return min;
}

static uint8_t maxNoise(const uint8_t *array, uint8_t n) {
  uint16_t max = 0;
  for (uint8_t i = 0; i < n; ++i) {
    if (array[i] != UINT8_MAX && array[i] > max) {
      max = array[i];
    }
    /* if (array[i] == UINT8_MAX) {
      Log("!!! NOISE=255 at %u", i); // appears when switching bands
    } */
  }
  return max;
}

void SP_ResetHistory(void) {
  filledPoints = 0;
  for (uint8_t i = 0; i < MAX_POINTS; ++i) {
    rssiHistory[i] = 0;
  }
}

void SP_Begin(void) {
  x = 0;
  ox = UINT8_MAX;
}

void SP_Init(Band *b) {
  S_BOTTOM = SPECTRUM_Y + SPECTRUM_H;
  range = b;
  step = StepFrequencyTable[b->step];
  SP_ResetHistory();
  SP_Begin();
}

uint8_t SP_F2X(uint32_t f) {
  return ConvertDomainF(f, range->rxF, range->txF, 0, MAX_POINTS - 1);
}

uint32_t SP_X2F(uint8_t x) {
  return ConvertDomainF(x, 0, MAX_POINTS - 1, range->rxF, range->txF);
}

void SP_AddPoint(const Measurement *msm) {
  uint32_t xs = SP_F2X(msm->f);
  uint32_t xe = SP_F2X(msm->f + step);

  if (xe > MAX_POINTS) {
    xe = MAX_POINTS;
  }
  uint16_t v = msm->rssi ? msm->rssi : msm->rssi;
  // TODO: debug this range
  for (x = xs; x <= xe; ++x) {
    if (ox != x) {
      ox = x;
      rssiHistory[x] = 0;
    }
    if (v > rssiHistory[x]) {
      rssiHistory[x] = v;
    }
  }
  if (x + 1 > filledPoints) {
    filledPoints = x + 1;
  }
  if (filledPoints > MAX_POINTS) {
    filledPoints = MAX_POINTS;
  }
}

static VMinMax getV() {
  const uint16_t rssiMin = minRssi(rssiHistory, filledPoints);
  const uint16_t rssiMax = Max(rssiHistory, filledPoints);
  const uint16_t rssiDiff = rssiMax - rssiMin;
  return (VMinMax){
      .vMin = rssiMin,
      .vMax = rssiMax + Clamp(rssiDiff, 20, rssiDiff),
  };
}

uint16_t peaks[MAX_POINTS];

void SP_Render(const Band *p) {
  const VMinMax v = getV();

  if (p) {
    UI_DrawTicks(S_BOTTOM, p);
  }

  DrawHLine(0, S_BOTTOM, MAX_POINTS, C_FILL);
  // DrawHLine(0, SPECTRUM_Y, LCD_WIDTH, C_FILL);

  for (uint8_t i = 0; i < filledPoints; ++i) {
    uint8_t yVal = ConvertDomain(rssiHistory[i], v.vMin, v.vMax, 0, SPECTRUM_H);
    DrawVLine(i, S_BOTTOM - yVal, yVal, C_FILL);
  }

  for (uint8_t x = 0; x < MAX_POINTS; ++x) {
    if (peaks[x]) {
      DrawVLine(x, SPECTRUM_Y + 1, 4, C_FILL);
    }
  }
}

void SP_RenderArrow(const Band *p, uint32_t f) {
  uint8_t cx = SP_F2X(f);
  DrawVLine(cx, SPECTRUM_Y + SPECTRUM_H + 1, 1, C_FILL);
  FillRect(cx - 1, SPECTRUM_Y + SPECTRUM_H + 2, 3, 1, C_FILL);
  FillRect(cx - 2, SPECTRUM_Y + SPECTRUM_H + 3, 5, 1, C_FILL);
}

void SP_RenderRssi(uint16_t rssi, char *text, bool top) {
  const VMinMax v = getV();

  uint8_t yVal = ConvertDomain(rssi, v.vMin, v.vMax, 0, SPECTRUM_H);
  DrawHLine(0, S_BOTTOM - yVal, filledPoints, C_FILL);
  PrintSmallEx(0, S_BOTTOM - yVal + (top ? -2 : 6), POS_L, C_FILL, "%s %d",
               text, Rssi2DBm(rssi));
}

void SP_RenderLine(uint16_t rssi) {
  const VMinMax v = getV();

  uint8_t yVal = ConvertDomain(rssi, v.vMin, v.vMax, 0, SPECTRUM_H);
  DrawHLine(0, S_BOTTOM - yVal, filledPoints, C_FILL);
}

uint16_t SP_GetNoiseFloor() { return Std(rssiHistory, filledPoints); }
uint16_t SP_GetRssiMax() { return Max(rssiHistory, filledPoints); }

void SP_RenderGraph() {
  const VMinMax v = {
      /* .vMin = 78,
      .vMax = 274, */
      .vMin = RSSI_MIN,
      .vMax = RSSI_MAX,
  };
  S_BOTTOM = SPECTRUM_Y + SPECTRUM_H; // TODO: mv to separate function

  uint8_t oVal =
      ConvertDomain(rssiGraphHistory[0], v.vMin, v.vMax, 0, SPECTRUM_H);

  for (uint8_t i = 1; i < MAX_POINTS; ++i) {
    uint8_t yVal =
        ConvertDomain(rssiGraphHistory[i], v.vMin, v.vMax, 0, SPECTRUM_H);
    DrawLine(i - 1, S_BOTTOM - oVal, i, S_BOTTOM - yVal, C_FILL);
    oVal = yVal;
  }
  DrawHLine(0, SPECTRUM_Y, LCD_WIDTH, C_FILL);
  DrawHLine(0, S_BOTTOM, LCD_WIDTH, C_FILL);

  for (uint8_t x = 0; x < LCD_WIDTH; x += 4) {
    DrawHLine(x, SPECTRUM_Y + SPECTRUM_H / 2, 2, C_FILL);
  }
}

void SP_AddGraphPoint(const Measurement *msm) {
  rssiGraphHistory[MAX_POINTS - 1] = msm->rssi;
  filledPoints = MAX_POINTS;
}

static void shiftEx(uint16_t *history, uint16_t n, int16_t shift) {
  if (shift == 0) {
    return;
  }
  if (shift > 0) {
    while (shift-- > 0) {
      for (int16_t i = n - 2; i >= 0; --i) {
        history[i + 1] = history[i];
      }
      history[0] = 0;
    }
  } else {
    while (shift++ < 0) {
      for (int16_t i = 0; i < n - 1; ++i) {
        history[i] = history[i + 1];
      }
      history[MAX_POINTS - 1] = 0;
    }
  }
}

void SP_Shift(int16_t n) { shiftEx(rssiHistory, MAX_POINTS, n); }
void SP_ShiftGraph(int16_t n) { shiftEx(rssiGraphHistory, MAX_POINTS, n); }

static uint8_t curX = MAX_POINTS / 2;
static uint8_t curSbWidth = 16;

void CUR_Render() {
  for (uint8_t y = SPECTRUM_Y + 10; y < S_BOTTOM; y += 4) {
    DrawVLine(curX - curSbWidth, y, 2, C_INVERT);
    DrawVLine(curX + curSbWidth, y, 2, C_INVERT);
  }
  for (uint8_t y = SPECTRUM_Y + 10; y < S_BOTTOM; y += 1) {
    DrawVLine(curX, y, 1, C_INVERT);
  }
}

bool CUR_Move(bool up) {
  if (up) {
    if (curX + curSbWidth < MAX_POINTS - 1) {
      curX++;
      return true;
    }
  } else {
    if (curX - curSbWidth > 0) {
      curX--;
      return true;
    }
  }
  return false;
}

bool CUR_Size(bool up) {
  if (up) {
    if (curX + curSbWidth < MAX_POINTS - 1 && curX - curSbWidth > 0) {
      curSbWidth++;
      return true;
    }
  } else {
    if (curSbWidth > 1) {
      curSbWidth--;
      return true;
    }
  }
  return false;
}

static uint32_t roundToStep(uint32_t f, uint32_t step) {
  uint32_t sd = f % step;
  if (sd > step / 2) {
    f += step - sd;
  } else {
    f -= sd;
  }
  return f;
}

Band CUR_GetRange(Band *p, uint32_t step) {
  Band range = *p;
  range.rxF = SP_X2F(curX - curSbWidth);
  range.txF = SP_X2F(curX + curSbWidth),
  range.rxF = roundToStep(range.rxF, step);
  range.txF = roundToStep(range.txF, step);
  return range;
}

uint32_t CUR_GetCenterF(Band *p, uint32_t step) {
  return roundToStep(SP_X2F(curX), step);
}

void CUR_Reset() {
  curX = MAX_POINTS / 2;
  curSbWidth = 16;
}
