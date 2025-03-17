#include "misc.h"
#include "driver/uart.h"

char IsPrintable(char ch) { return (ch < 32 || 126 < ch) ? ' ' : ch; }

// return square root of 'value'
unsigned int SQRT16(unsigned int value) {
  unsigned int shift = 16; // number of bits supplied in 'value' .. 2 ~ 32
  unsigned int bit = 1u << --shift;
  unsigned int sqrti = 0;
  while (bit) {
    const unsigned int temp = ((sqrti << 1) | bit) << shift--;
    if (value >= temp) {
      value -= temp;
      sqrti |= bit;
    }
    bit >>= 1;
  }
  return sqrti;
}

void _putchar(char c) { UART_Send((uint8_t *)&c, 1); }
void vAssertCalled(__attribute__((unused)) unsigned long ulLine,
                   __attribute__((unused)) const char *const pcFileName) {

  taskENTER_CRITICAL();
  { Log("[ASSERT ERROR] %s %s: line=%lu\r\n", __func__, pcFileName, ulLine); }
  taskEXIT_CRITICAL();
}

void vApplicationStackOverflowHook(__attribute__((unused)) TaskHandle_t pxTask,
                                   __attribute__((unused)) char *pcTaskName) {

  taskENTER_CRITICAL();
  {
    unsigned int stackWm = uxTaskGetStackHighWaterMark(pxTask);
    Log("[STACK ERROR] %s task=%s : %i\r\n", __func__, pcTaskName, stackWm);
  }
  taskEXIT_CRITICAL();
}

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
  static StaticTask_t xIdleTaskTCB;
  static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
  static StaticTask_t xTimerTaskTCB;
  static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
