#include "system.h"
#include "../inc/dp32g030/pmu.h"
#include "../inc/dp32g030/syscon.h"
#include "systick.h"

void SYS_DelayMs(uint32_t Delay) { SYSTICK_DelayUs(Delay * 1000); }

void SYS_ConfigureClocks(void) {
  // Set source clock from external crystal
  PMU_SRC_CFG =
      (PMU_SRC_CFG & ~(PMU_SRC_CFG_RCHF_SEL_MASK | PMU_SRC_CFG_RCHF_EN_MASK)) |
      PMU_SRC_CFG_RCHF_SEL_BITS_48MHZ | PMU_SRC_CFG_RCHF_EN_BITS_ENABLE;

  // Divide by 2
  SYSCON_CLK_SEL = SYSCON_CLK_SEL_DIV_BITS_2;

  // Disable division clock gate
  SYSCON_DIV_CLK_GATE =
      (SYSCON_DIV_CLK_GATE & ~SYSCON_DIV_CLK_GATE_DIV_CLK_GATE_MASK) |
      SYSCON_DIV_CLK_GATE_DIV_CLK_GATE_BITS_DISABLE;
}

void SYS_ConfigureSysCon() {
  // Enable clock gating of blocks we need.
  SYSCON_DEV_CLK_GATE = 0 | SYSCON_DEV_CLK_GATE_GPIOA_BITS_ENABLE |
                        SYSCON_DEV_CLK_GATE_GPIOB_BITS_ENABLE |
                        SYSCON_DEV_CLK_GATE_GPIOC_BITS_ENABLE |
                        SYSCON_DEV_CLK_GATE_UART1_BITS_ENABLE |
                        SYSCON_DEV_CLK_GATE_SPI0_BITS_ENABLE |
                        SYSCON_DEV_CLK_GATE_SARADC_BITS_ENABLE |
                        SYSCON_DEV_CLK_GATE_CRC_BITS_ENABLE |
                        // SYSCON_DEV_CLK_GATE_AES_BITS_ENABLE |
                        SYSCON_DEV_CLK_GATE_PWM_PLUS0_BITS_ENABLE;
}
