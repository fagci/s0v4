#include "board.h"
#include "config/FreeRTOSConfig.h"
#include "driver/crc.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/uart.h"
#include "external/FreeRTOS/include/FreeRTOS.h"
#include "external/FreeRTOS/portable/GCC/ARM_CM0/portmacro.h"
#include "misc.h"
#include "system.h"

void Main(void) {
  SYSTICK_Init();
  SYS_ConfigureSysCon();
  SYS_ConfigureClocks();
  BOARD_GPIO_Init();
  BOARD_PORTCON_Init();
  BOARD_ADC_Init();
  CRC_Init();
  UART_Init();

  Log("s0v4");

  StaticTask_t tBuf;
  StackType_t tStack[configMINIMAL_STACK_SIZE + 200];

  xTaskCreateStatic(SYS_Main, "sys", ARRAY_SIZE(tStack), NULL, 1, tStack,
                    &tBuf);

  vTaskStartScheduler();

  for (;;) {
  }
}
