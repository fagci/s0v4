#ifndef BANDSCAN_H
#define BANDSCAN_H

#include "../driver/keyboard.h"
#include <stdbool.h>
#include <stdint.h>

bool BANDSCAN_key(KEY_Code_t Key, Key_State_t state);
void BANDSCAN_init(void);
void BANDSCAN_deinit(void);
void BANDSCAN_update(void);
void BANDSCAN_render(void);

#endif /* end of include guard: BANDSCAN_H */
