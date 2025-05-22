#include "finput.h"
#include "../driver/bk4819.h"
#include "../driver/st7565.h"
#include "../driver/uart.h"
#include "../scheduler.h"
#include "../ui/graphics.h"
#include "apps.h"

uint32_t gFInputTempFreq;
void (*gFInputCallback)(uint32_t f);

#define MAX_FREQ_LENGTH 10

static char freqInputArr[MAX_FREQ_LENGTH] = "";

static uint8_t cursorPos = 0;
static uint8_t dotEntered = 0;
static uint8_t blinkState = 0;

static bool isManualDot = false;

static uint32_t getFrequencyHz() {
  uint32_t integerPart = 0;
  uint32_t fractionalPart = 0;
  uint8_t fractionalDigits = 0;
  bool isFractional = false;

  for (uint8_t i = 0; freqInputArr[i] != '\0'; i++) {
    if (freqInputArr[i] == '.') {
      isFractional = true;
    } else if (freqInputArr[i] >= '0' && freqInputArr[i] <= '9') {
      if (isFractional) {
        fractionalPart = fractionalPart * 10 + (freqInputArr[i] - '0');
        fractionalDigits++;
      } else {
        integerPart = integerPart * 10 + (freqInputArr[i] - '0');
      }
    }
  }

  uint32_t frequencyHz = integerPart * 1000000ULL;
  if (fractionalDigits > 0) {
    uint32_t multiplier = 1;
    for (uint8_t i = 0; i < 6 - fractionalDigits; i++) {
      multiplier *= 10;
    }
    frequencyHz += fractionalPart * multiplier;
  }

  return frequencyHz;
}

static void input(KEY_Code_t key) {
  if (key <= KEY_9) {
    if (cursorPos < MAX_FREQ_LENGTH) {
      freqInputArr[cursorPos++] = '0' + (key - KEY_0);
      freqInputArr[cursorPos] = '\0';
    }
  } else if (key == KEY_STAR && !dotEntered) {
    if (cursorPos < MAX_FREQ_LENGTH) {
      freqInputArr[cursorPos++] = '.';
      isManualDot = true;
      freqInputArr[cursorPos] = '\0';
      dotEntered = 1;
    }
  } else if (key == KEY_EXIT) {
    if (cursorPos > 0) {
      if (freqInputArr[cursorPos - 1] == '.') {
        dotEntered = 0;
        if (!isManualDot) {
          freqInputArr[--cursorPos] = '\0';
        }
      }
      freqInputArr[--cursorPos] = '\0';
    }
  }

  gFInputTempFreq = getFrequencyHz() / 10;
}

static void reset() {
  cursorPos = 0;
  dotEntered = 0;
  blinkState = 0;
  memset(freqInputArr, 0, MAX_FREQ_LENGTH);
}

void fillFromTempFreq() {
  if (!gFInputTempFreq) {
    return;
  }
  uint32_t integerPart = gFInputTempFreq / MHZ;
  uint32_t fractionalPart = gFInputTempFreq % (MHZ / 10);

  if (fractionalPart) {
    while (fractionalPart % 10 == 0) {
      fractionalPart /= 10;
    }
    snprintf(freqInputArr, MAX_FREQ_LENGTH + 1, "%u.%u", integerPart,
             fractionalPart);
  } else {
    snprintf(freqInputArr, MAX_FREQ_LENGTH + 1, "%u", integerPart);
  }
  cursorPos = strlen(freqInputArr);
}

void FINPUT_init(void) {
  reset();
  fillFromTempFreq();
}

static uint32_t lastUpdate;

void FINPUT_update() {
  if (Now() - lastUpdate >= 500) {
    if (!dotEntered) {
      blinkState = !blinkState;
      gRedrawScreen = true;
    } else {
      blinkState = true;
    }
    lastUpdate = Now();
  }
}

void FINPUT_deinit(void) {}

bool FINPUT_key(KEY_Code_t key, Key_State_t state) {
  if (state == KEY_LONG_PRESSED && key == KEY_EXIT) {
    reset();
    return true;
  }
  if (state == KEY_RELEASED) {
    switch (key) {
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
    case KEY_STAR:
      input(key);
      if (gFInputTempFreq > 13000000) {
        input(KEY_STAR);
        isManualDot = false;
      }
      gRedrawScreen = true;
      return true;
    case KEY_EXIT:
      if (cursorPos == 0) {
        gFInputCallback = NULL;
        APPS_exit();
        return true;
      }
      input(key);
      gRedrawScreen = true;
      return true;
    case KEY_MENU:
    case KEY_F:
    case KEY_PTT:
      if (gFInputTempFreq <= BK4819_F_MAX && gFInputCallback) {
        APPS_exit();
        gFInputCallback(gFInputTempFreq);
        gFInputCallback = NULL;
        gFInputTempFreq = 0;
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

void FINPUT_render(void) {
  const uint8_t BASE_Y = 24;

  uint8_t offset = dotEntered ? 1 : 5;
  if (!dotEntered && blinkState) {
    PrintBigDigitsEx(LCD_WIDTH - 1, BASE_Y, POS_R, C_FILL, ".");
  }
  PrintBigDigitsEx(LCD_WIDTH - 1 - offset, BASE_Y, POS_R, C_FILL, "%s",
                   freqInputArr);
}
