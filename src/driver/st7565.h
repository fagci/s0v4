#ifndef DRIVER_ST7565_H
#define DRIVER_ST7565_H

#include <stdbool.h>
#include <stdint.h>

#define LCD_WIDTH 128
#define LCD_HEIGHT 64
#define LCD_XCENTER 64
#define LCD_YCENTER 32

extern bool gRedrawScreen;
extern uint8_t gFrameBuffer[8][LCD_WIDTH];

void ST7565_Blit(void);
void ST7565_Init(void);
void ST7565_WriteByte(uint8_t Value);

#endif
