#ifndef CHLIST_H
#define CHLIST_H

#include "../driver/keyboard.h"
#include "../helper/channels.h"
#include <stdbool.h>
#include <stdint.h>

void CHLIST_init();
void CHLIST_deinit();
bool CHLIST_key(KEY_Code_t key, bool bKeyPressed, bool bKeyHeld);
void CHLIST_render();

extern bool gChSaveMode;
extern CHTypeFilter gChListFilter;

#endif /* end of include guard: CHLIST_H */
