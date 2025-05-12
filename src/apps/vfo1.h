#ifndef VFO1_APP_H
#define VFO1_APP_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

void VFO1_init();
void VFO1_update();
bool VFO1_keyEx(KEY_Code_t key, Key_State_t state,
                bool isProMode);
bool VFO1_key(KEY_Code_t key, Key_State_t state);
void VFO1_render();

#endif /* end of include guard: VFO1_APP_H */
