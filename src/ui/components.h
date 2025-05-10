#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "../radio.h"
#include "graphics.h"
#include <stdbool.h>
#include <stdint.h>

#define BATTERY_W 13

void UI_Battery(uint8_t Level);
void UI_TxBar(uint8_t y);
void UI_RSSIBar(uint8_t y);
void UI_DrawScrollBar(const uint16_t size, const uint16_t currentIndex,
                      const uint8_t linesCount);
void UI_DrawTicks(uint8_t y, const Band *band);
void UI_ShowWait();
void UI_Scanlists(uint8_t baseX, uint8_t baseY, uint16_t sl);
void UI_DrawLoot(const Loot *loot, uint8_t x, uint8_t y, TextPos pos);
void UI_BigFrequency(uint8_t y, uint32_t f);
void UI_DisplayScanlists(uint32_t y);

void UI_RenderScanScreen();

#endif /* end of include guard: COMPONENTS_H */
