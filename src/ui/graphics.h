#ifndef GRAPHICS_H

#define GRAPHICS_H

#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "gfxfont.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
  POS_L,
  POS_C,
  POS_R,
} TextPos;

typedef enum {
  C_CLEAR,
  C_FILL,
  C_INVERT,
} Color;

typedef enum {
  SYM_CONVERTER = 0x30,
  SYM_CH = 0x31,
  SYM_BAND = 0x32,
  SYM_VFO = 0x33,
  SYM_SETTING = 0x34,
  SYM_FILE = 0x35,
  SYM_MELODY = 0x36,
  SYM_FOLDER = 0x37,
  SYM_MISC2 = 0x38,
  SYM_EEPROM_W = 0x39,
  SYM_BEEP = 0x3A,
  SYM_MONITOR = 0x3B,
  SYM_BROADCAST = 0x3C,
  SYM_SCAN = 0x3D,
  SYM_NO_LISTEN = 0x3E,
  SYM_LOCK = 0x3F,
  SYM_FC = 0x40,
  SYM_BEACON = 0x41,
  SYM_DW = 0x42,
  SYM_LOOT_FULL = 0x43,
} Symbol;

typedef struct {
  uint8_t x;
  uint8_t y;
} Cursor;

void UI_ClearStatus();
void UI_ClearScreen();

void PutPixel(uint8_t x, uint8_t y, uint8_t fill);
bool GetPixel(uint8_t x, uint8_t y);

void DrawVLine(int16_t x, int16_t y, int16_t h, Color color);
void DrawHLine(int16_t x, int16_t y, int16_t w, Color color);
void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color);
void DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color);
void FillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color);

void PrintSmall(uint8_t x, uint8_t y, const char *pattern, ...);
void PrintMedium(uint8_t x, uint8_t y, const char *pattern, ...);
void PrintMediumBold(uint8_t x, uint8_t y, const char *pattern, ...);
void PrintBigDigits(uint8_t x, uint8_t y, const char *pattern, ...);
void PrintBiggestDigits(uint8_t x, uint8_t y, const char *pattern, ...);

void PrintSmallEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                  const char *pattern, ...);
void PrintMediumEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                   const char *pattern, ...);
void PrintMediumBoldEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                       const char *pattern, ...);
void PrintBigDigitsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                      const char *pattern, ...);
void PrintBiggestDigitsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                          const char *pattern, ...);
void PrintSymbolsEx(uint8_t x, uint8_t y, TextPos posLCR, Color color,
                    const char *pattern, ...);

#endif /* end of include guard: GRAPHICS_H */
