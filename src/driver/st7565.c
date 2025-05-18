#include "st7565.h"
#include "../inc/dp32g030/gpio.h"
#include "../inc/dp32g030/spi.h"
#include "../misc.h"
#include "../settings.h"
#include "gpio.h"
#include "spi.h"
#include "system.h"
#include "systick.h"
#include "uart.h"
#include <stdint.h>
#include <string.h>

static void waitToSend() {
  while ((SPI0->FIFOST & SPI_FIFOST_TFF_MASK) != SPI_FIFOST_TFF_BITS_NOT_FULL) {
    continue;
  }
}

uint8_t gFrameBuffer[8][LCD_WIDTH];
static uint8_t frameBufferSecond[8][LCD_WIDTH];

bool gRedrawScreen = true;

static void ST7565_Configure_GPIO_B11(void) {
  GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_ST7565_RES);
  SYS_DelayMs(1);
  GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_ST7565_RES);
  SYS_DelayMs(20);
  GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_ST7565_RES);
  SYS_DelayMs(120);
}

static void ST7565_SelectColumnAndLine(uint8_t Column, uint8_t Line) {
  GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_ST7565_A0);
  waitToSend();
  SPI0->WDR = Line + 0xB0;
  waitToSend();
  SPI0->WDR = ((Column >> 4) & 0x0F) | 0x10;
  waitToSend();
  SPI0->WDR = ((Column >> 0) & 0x0F);
  SPI_WaitForUndocumentedTxFifoStatusBit();
}

static void ST7565_FillScreen(uint8_t Value) {
  SPI_ToggleMasterMode(&SPI0->CR, false);

  for (uint8_t i = 0; i < 8; i++) {
    ST7565_SelectColumnAndLine(0, i);
    GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_ST7565_A0);
    for (uint8_t j = 0; j < 132; j++) {
      waitToSend();
      SPI0->WDR = Value;
    }
    SPI_WaitForUndocumentedTxFifoStatusBit();
  }

  SPI_ToggleMasterMode(&SPI0->CR, true);
}

void ST7565_Blit(void) {
  uint8_t Line;
  uint8_t Column;

  SPI_ToggleMasterMode(&SPI0->CR, false);
  ST7565_WriteByte(0x40);

  for (Line = 0; Line < ARRAY_SIZE(gFrameBuffer); Line++) {
    if (memcmp(gFrameBuffer[Line], frameBufferSecond[Line],
               ARRAY_SIZE(frameBufferSecond[Line])) == 0) {
      continue;
    }

    ST7565_SelectColumnAndLine(4U, Line);
    GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_ST7565_A0);
    for (Column = 0; Column < ARRAY_SIZE(gFrameBuffer[0]); Column++) {
      waitToSend();
      SPI0->WDR = gFrameBuffer[Line][Column];
    }
    SPI_WaitForUndocumentedTxFifoStatusBit();

    memcpy(frameBufferSecond[Line], gFrameBuffer[Line],
           ARRAY_SIZE(frameBufferSecond[Line]));
  }

  SPI_ToggleMasterMode(&SPI0->CR, true);
}

void ST7565_Init(bool full) {
  if (full) {
    SPI0_Init();
    ST7565_Configure_GPIO_B11();
    SPI_ToggleMasterMode(&SPI0->CR, false);
    ST7565_WriteByte(0xE2);
    SYS_DelayMs(120);
  } else {
    SPI_ToggleMasterMode(&SPI0->CR, false);
  }

  ST7565_WriteByte(0xA2);
  ST7565_WriteByte(0xC0);
  ST7565_WriteByte(0xA1);
  ST7565_WriteByte(0xA6);
  ST7565_WriteByte(0xA4);
  ST7565_WriteByte(0x24);
  ST7565_WriteByte(0x81);
  ST7565_WriteByte(23 + gSettings.contrast); // default 31

  if (full) {
    // setup power circuit
    ST7565_WriteByte(0x2B);
    SYS_DelayMs(1);
    ST7565_WriteByte(0x2E);
    SYS_DelayMs(1);

    // power circuit v1 v2 v3 v4
    for (uint8_t i = 0; i < 4; ++i) {
      ST7565_WriteByte(0x2F);
    }
    SYS_DelayMs(40);
  }

  ST7565_WriteByte(0x40);
  ST7565_WriteByte(0xAF);
  SPI_WaitForUndocumentedTxFifoStatusBit();
  SPI_ToggleMasterMode(&SPI0->CR, true);

  if (full) {
    ST7565_FillScreen(0x00);
  }
}

void ST7565_WriteByte(uint8_t Value) {
  GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_ST7565_A0);
  waitToSend();
  SPI0->WDR = Value;
}
