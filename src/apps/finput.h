#ifndef FINPUT_H
#define FINPUT_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

void FINPUT_init();
bool FINPUT_key(KEY_Code_t key, bool bKeyPressed, bool bKeyHeld);
void FINPUT_render();
void FINPUT_deinit();

extern uint32_t gFInputTempFreq;
extern void (*gFInputCallback)(uint32_t f);

#endif /* end of include guard: FINPUT_H */
