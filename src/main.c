#include "board.h"
#include "driver/crc.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "driver/uart.h"
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

  SYS_Main();
}
