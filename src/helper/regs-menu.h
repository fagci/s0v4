#ifndef REGS_MENU_H
#define REGS_MENU_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

void REGSMENU_Draw();
bool REGSMENU_Key(KEY_Code_t key, Key_State_t state);

#endif /* end of include guard: REGS_MENU_H */
