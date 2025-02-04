#include "keyboard.h"
#include "../inc/dp32g030/gpio.h"
#include "../misc.h"
#include "../system.h"
#include "FreeRTOS.h"
#include "gpio.h"
#include "systick.h"
#include "task.h"

static StaticTask_t mKeyTaskBuffer;
static StackType_t mKeyTaskStack[configMINIMAL_STACK_SIZE + 100];

static const uint32_t LONG_PRESS_TIME = 500;
static const uint32_t ROWS = 5;
static const uint32_t COLS = 4;

static bool mKeyPtt;
static KEY_Code_t mKeyPressed = KEY_INVALID;
static KEY_Code_t mPrevKeyPressed = KEY_INVALID;
static Key_State_t mPrevStatePtt;
static Key_State_t mPrevKeyState;
static TickType_t mLongPressTimer;

typedef const struct {
  uint16_t setToZeroMask;
  struct {
    uint8_t key : 5; // Key 23 is highest
    uint8_t pin : 3; // Pin 6 is highest
  } pins[4];
} Keyboard;

Keyboard keyboard[5] = {
    /* Zero row  */
    {// Set to zero to handle special case of nothing pulled down.
     .setToZeroMask = 0xffff,
     .pins =
         {
             {.key = KEY_SIDE1, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_SIDE2, .pin = GPIOA_PIN_KEYBOARD_1},
             /* Duplicate to fill the array with valid values */
             {.key = KEY_SIDE2, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_SIDE2, .pin = GPIOA_PIN_KEYBOARD_1},
         }},
    /* First row  */
    {.setToZeroMask = ~(1 << GPIOA_PIN_KEYBOARD_4) & 0xffff,
     .pins =
         {
             {.key = KEY_MENU, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_1, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_4, .pin = GPIOA_PIN_KEYBOARD_2},
             {.key = KEY_7, .pin = GPIOA_PIN_KEYBOARD_3},
         }},
    /* Second row */
    {.setToZeroMask = ~(1 << GPIOA_PIN_KEYBOARD_5) & 0xffff,
     .pins =
         {
             {.key = KEY_UP, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_2, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_5, .pin = GPIOA_PIN_KEYBOARD_2},
             {.key = KEY_8, .pin = GPIOA_PIN_KEYBOARD_3},
         }},
    /* Third row */
    {.setToZeroMask = ~(1 << GPIOA_PIN_KEYBOARD_6) & 0xffff,
     .pins =
         {
             {.key = KEY_DOWN, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_3, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_6, .pin = GPIOA_PIN_KEYBOARD_2},
             {.key = KEY_9, .pin = GPIOA_PIN_KEYBOARD_3},
         }},
    /* Fourth row */
    {.setToZeroMask = ~(1 << GPIOA_PIN_KEYBOARD_7) & 0xffff,
     .pins =
         {
             {.key = KEY_EXIT, .pin = GPIOA_PIN_KEYBOARD_0},
             {.key = KEY_STAR, .pin = GPIOA_PIN_KEYBOARD_1},
             {.key = KEY_0, .pin = GPIOA_PIN_KEYBOARD_2},
             {.key = KEY_F, .pin = GPIOA_PIN_KEYBOARD_3},
         }},
};

void KEYBOARD_Poll(void) {
  uint16_t reg, reg2;
  uint8_t ii;
  uint8_t k;

  // Handle PTT key
  mKeyPtt = !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT);
  if (mPrevStatePtt == KEY_PRESSED && !mKeyPtt) {
    SYSTEM_MsgKey(KEY_PTT, KEY_RELEASED);
    mPrevStatePtt = KEY_RELEASED;
    return;
  } else if (mPrevStatePtt == KEY_RELEASED && mKeyPtt) {
    SYSTEM_MsgKey(KEY_PTT, KEY_PRESSED);
    mPrevStatePtt = KEY_PRESSED;
    return;
  } else if (mPrevStatePtt == KEY_PRESSED && mKeyPtt) {
    return;
  }

  // Read matrix keyboard

  mKeyPressed = KEY_INVALID;
  // Scan main matrix
  for (uint8_t i = 0; i < ROWS; i++) {
    // Reset rows
    GPIOA->DATA |= (1u << GPIOA_PIN_KEYBOARD_4) | (1u << GPIOA_PIN_KEYBOARD_5) |
                   (1u << GPIOA_PIN_KEYBOARD_6) | (1u << GPIOA_PIN_KEYBOARD_7);

    GPIOA->DATA &= keyboard[i].setToZeroMask;

    for (ii = 0, k = 0, reg = 0; ii < 3 && k < 8; ii++, k++) {
      // SYSTICK_DelayUs(1);
      vTaskDelay(pdMS_TO_TICKS(0));
      reg2 = (uint16_t)GPIOA->DATA;
      if (reg != reg2) { // noise
        reg = reg2;
        ii = 0;
      }
    }
    if (ii < 3)
      break; // noise is too bad

    for (uint8_t j = 0; j < COLS; j++) {
      const uint16_t mask = 1u << keyboard[i].pins[j].pin;
      if (!(reg & mask)) {
        mKeyPressed = keyboard[i].pins[j].key;
        break;
      }
    }
    if (mKeyPressed != KEY_INVALID) {
      break;
    }
  }

  // Create I2C stop condition since we might have toggled I2C pins
  // This leaves GPIOA_PIN_KEYBOARD_4 and GPIOA_PIN_KEYBOARD_5 high
  // I2C_Stop();

  // Reset VOICE pins
  GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_KEYBOARD_6);
  GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_KEYBOARD_7);
}

void KEYBOARD_CheckKeys() {
  TickType_t currentTick = xTaskGetTickCount();

  if (mKeyPressed != KEY_INVALID) {
    if (mPrevKeyState == KEY_RELEASED) {
      SYSTEM_MsgKey(mKeyPressed, KEY_PRESSED);
      mPrevKeyState = KEY_PRESSED;

      mLongPressTimer = currentTick;
      mPrevKeyPressed = mKeyPressed;
    } else if (mPrevKeyState == KEY_PRESSED) {
      TickType_t elapsedTime = currentTick - mLongPressTimer;
      if (elapsedTime >= pdMS_TO_TICKS(LONG_PRESS_TIME)) {
        mLongPressTimer = 0;
        mPrevKeyState = KEY_LONG_PRESSED;
      }
    } else if (mPrevKeyState == KEY_LONG_PRESSED ||
               mPrevKeyState == KEY_LONG_PRESSED_CONT) {
      SYSTEM_MsgKey(mKeyPressed, mPrevKeyState);
      mPrevKeyState = KEY_LONG_PRESSED_CONT;
    }
  } else {
    if (mPrevKeyState != KEY_RELEASED) {
      if (mPrevKeyState != KEY_LONG_PRESSED &&
          mPrevKeyState != KEY_LONG_PRESSED_CONT) {
        SYSTEM_MsgKey(mPrevKeyPressed, KEY_RELEASED);
      }

      if (mPrevKeyState == KEY_LONG_PRESSED_CONT &&
          (mPrevKeyPressed == KEY_UP || mPrevKeyPressed == KEY_DOWN)) {
        SYSTEM_MsgKey(mPrevKeyPressed, KEY_RELEASED);
      }

      mLongPressTimer = 0;
      mPrevKeyState = KEY_RELEASED;
      mPrevKeyPressed = KEY_INVALID;
    }
  }
}

static void checkKeys(void *attr) {
  for (;;) {
    KEYBOARD_Poll();
    KEYBOARD_CheckKeys();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void KEYBOARD_Init() {
  Log("Kbd init start");
  xTaskCreateStatic(checkKeys, "KEY", ARRAY_SIZE(mKeyTaskStack), NULL, 1,
                    mKeyTaskStack, &mKeyTaskBuffer);
  Log("Kbd init end");
}
