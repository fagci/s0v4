#ifndef DRIVER_KEYBOARD_H
#define DRIVER_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  KEY_0 = 0,
  KEY_1 = 1,
  KEY_2 = 2,
  KEY_3 = 3,
  KEY_4 = 4,
  KEY_5 = 5,
  KEY_6 = 6,
  KEY_7 = 7,
  KEY_8 = 8,
  KEY_9 = 9,
  KEY_MENU = 10,
  KEY_UP = 11,
  KEY_DOWN = 12,
  KEY_EXIT = 13,
  KEY_STAR = 14,
  KEY_F = 15,
  KEY_PTT = 21,
  KEY_SIDE2 = 22,
  KEY_SIDE1 = 23,
  KEY_INVALID = 255,
} KEY_Code_t;

typedef enum {
  KEY_RELEASED,
  KEY_PRESSED,
  KEY_LONG_PRESSED,
  KEY_LONG_PRESSED_CONT
} Key_State_t;

typedef enum {
  MSG_NONE,
  MSG_NOTIFY,
  MSG_BKCLIGHT,
  MSG_KEYPRESSED,
  MSG_PLAY_BEEP,
  MSG_RADIO_RX,
  MSG_RADIO_TX,
  MSG_APP_LOAD,
} SystemMSG;

typedef struct {
  SystemMSG message;
  KEY_Code_t key;
  Key_State_t state;
} SystemMessages;

void KEYBOARD_Poll(void);
void KEYBOARD_CheckKeys();
SystemMessages KEYBOARD_GetKey();

#endif
