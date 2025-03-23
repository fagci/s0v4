#include "level.h"
#include "../radio.h"
#include "../ui/graphics.h"
#include "../ui/spectrum.h"

void LEVEL_init() {
  SPECTRUM_Y = 6;
  SPECTRUM_H = LCD_HEIGHT - SPECTRUM_Y;
  BK4819_WriteRegister(0x13, (3 << 8) | (7 << 5) | (0 << 3) | (0 << 0));
  BK4819_TuneTo(1340 * MHZ, true);
}

void LEVEL_deinit() {}

void LEVEL_update() {
  Measurement msm = {
      .rssi = BK4819_GetRegValue(RS_PEAK_RSSI),
  };
  SP_ShiftGraph(-1);
  SP_AddGraphPoint(&msm);
  gRedrawScreen = true;

  vTaskDelay(pdMS_TO_TICKS(60));
}

bool LEVEL_key(KEY_Code_t key, Key_State_t state) {
  switch (key) {
  default:
    break;
  }
  return false;
}

void LEVEL_render() { SP_RenderGraph(48, 88); }
