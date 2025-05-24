#ifndef STATUSLINE_H
#define STATUSLINE_H

#include "../radio.h"

void STATUSLINE_update();
void STATUSLINE_render();
void STATUSLINE_SetText(const char *pattern, ...);
void STATUSLINE_SetTickerText(const char *pattern, ...);
void STATUSLINE_RenderRadioSettings();

#endif /* end of include guard: STATUSLINE_H */
