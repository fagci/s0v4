#ifndef LEVEL_H
#define LEVEL_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

void LEVEL_init();
void LEVEL_deinit();
void LEVEL_update();
bool LEVEL_key(KEY_Code_t key, Key_State_t state);
void LEVEL_render();

#endif /* end of include guard: LEVEL_H */
