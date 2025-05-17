#include "systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"

static const uint32_t TICK_MULTIPLIER = 48;

void SYSTICK_Init(void) { SysTick_Config(48000); }

void SYSTICK_DelayTicks(const uint32_t ticks) {
  uint32_t elapsed_ticks = 0;
  uint32_t Start = SysTick->LOAD;
  uint32_t Previous = SysTick->VAL;

  do {
    uint32_t Current = SysTick->VAL;
    if (Current != Previous) {
      uint32_t Delta = (Current < Previous) ? (Previous - Current)
                                            : (Start - Current + Previous);

      elapsed_ticks += Delta;
      Previous = Current;
    }
  } while (elapsed_ticks < ticks);
}

void SYSTICK_DelayUs(const uint32_t Delay) {
  SYSTICK_DelayTicks(Delay * TICK_MULTIPLIER);
}

void SYSTICK_Delay250ns(const uint32_t Delay) {
  SYSTICK_DelayTicks(Delay * TICK_MULTIPLIER / 4);
}
