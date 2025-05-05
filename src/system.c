#include "system.h"
#include "apps/apps.h"
#include "board.h"
#include "config/FreeRTOSConfig.h"
#include "driver/backlight.h"
#include "driver/eeprom.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/uart.h"
#include "external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "external/FreeRTOS/include/FreeRTOS.h"
#include "external/FreeRTOS/include/projdefs.h"
#include "external/FreeRTOS/include/queue.h"
#include "external/FreeRTOS/include/task.h"
#include "external/FreeRTOS/include/timers.h"
#include "external/FreeRTOS/portable/GCC/ARM_CM0/portmacro.h"
#include "helper/bands.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "scheduler.h"
#include "settings.h"
#include "ui/graphics.h"
#include "ui/statusline.h"

#define queueLen 20
#define itemSize sizeof(SystemMessages)

typedef enum {
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
  uint32_t payload;
  KEY_Code_t key;
  Key_State_t state;
} SystemMessages;

static uint8_t DEAD_BUF[] = {0xDE, 0xAD};

uint32_t gAppUpdateInterval = pdMS_TO_TICKS(1);

static char notificationMessage[16] = "";
static uint32_t notificationTimeoutAt;

static TimerHandle_t sysTimer;
static StaticTimer_t sysTimerBuffer;

static QueueHandle_t systemMessageQueue; // Message queue handle
static StaticQueue_t systemTasksQueue;   // Static queue storage area
static uint8_t systemQueueStorageArea[queueLen * itemSize];

StaticTask_t appUpdateTaskBuffer;
StackType_t appUpdateTaskStack[configMINIMAL_STACK_SIZE + 120];

StaticTask_t appRenderTaskBuffer;
StackType_t appRenderTaskStack[configMINIMAL_STACK_SIZE + 120];

static uint32_t lastUartDataTime;

static bool isUartWaiting() {
  return lastUartDataTime && Now() - lastUartDataTime < 5000;
}

static void appUpdate(void *arg) {
  for (;;) {
    APPS_update();
    vTaskDelay(gAppUpdateInterval);
  }
}

static void appRender(void *arg) {
  for (;;) {
    if (gRedrawScreen) {
      UI_ClearScreen();

      APPS_render();

      if (notificationMessage[0]) {
        FillRect(0, 32 - 5, 128, 9, C_FILL);
        PrintMediumEx(64, 32 + 2, POS_C, C_CLEAR, notificationMessage);
      }

      STATUSLINE_render(); // coz of APPS_render calls STATUSLINE_SetText

      ST7565_Blit();
      gRedrawScreen = false;
    }
    vTaskDelay(pdMS_TO_TICKS(40)); // 25 fps
  }
}

static void systemUpdate() {
  BATTERY_UpdateBatteryInfo();
  BACKLIGHT_Update();
}

static bool resetNeeded() {
  uint8_t buf[2];
  EEPROM_ReadBuffer(0, buf, 2);

  return memcmp(buf, DEAD_BUF, 2) == 0;
}

static void loadSettingsOrReset() {
  SETTINGS_Load();
  if (gSettings.batteryCalibration > 2154 ||
      gSettings.batteryCalibration < 1900) {
    gSettings.batteryCalibration = 0;
    EEPROM_WriteBuffer(0, DEAD_BUF, 2);
    NVIC_SystemReset();
  }
}

static bool checkKeylock(Key_State_t state, KEY_Code_t key) {
  if (state == KEY_LONG_PRESSED && key == KEY_F) {
    gSettings.keylock = !gSettings.keylock;
    SETTINGS_Save();
    gRedrawScreen = true;
    return true;
  }

  /* if (gSettings.keylock && state == KEY_LONG_PRESSED && key == KEY_8) {
    captureScreen();
    return true;
  } */

  if (gSettings.keylock && (gSettings.pttLock ? true : key != KEY_PTT) &&
      !(state == KEY_LONG_PRESSED && key == KEY_F)) {
    return true;
  }

  return false;
}

void SYS_Main(void *params) {
  BOARD_Init();
  BATTERY_UpdateBatteryInfo();

  // run updates & render tasks to keep user informed
  sysTimer = xTimerCreateStatic("sysT", pdMS_TO_TICKS(1000), pdTRUE, NULL,
                                systemUpdate, &sysTimerBuffer);
  xTimerStart(sysTimer, 0);

  xTaskCreateStatic(appUpdate, "appU", ARRAY_SIZE(appUpdateTaskStack), NULL, 3,
                    appUpdateTaskStack, &appUpdateTaskBuffer);
  xTaskCreateStatic(appRender, "appR", ARRAY_SIZE(appRenderTaskStack), NULL, 2,
                    appRenderTaskStack, &appRenderTaskBuffer);
  systemMessageQueue = xQueueCreateStatic(
      queueLen, itemSize, systemQueueStorageArea, &systemTasksQueue);

  SystemMessages n;

  if (resetNeeded()) {
    gSettings.batteryCalibration = 2000;
    gSettings.backlight = 5;
    APPS_run(APP_RESET);
  } else {
    loadSettingsOrReset();
    BATTERY_UpdateBatteryInfo();

    ST7565_Init(false);
    BACKLIGHT_Init();

    SYS_MsgNotify("LOAD BANDS", 1000);
    Log("LOAD BANDS");
    BANDS_Load();

    SYS_MsgNotify("INIT RADIO", 1000);
    Log("INIT RADIO");
    RADIO_Init();
    SYS_MsgNotify("", 0);

    Log("RUN DEFAULT APP");
    APPS_run(gSettings.mainApp);
  }

  for (;;) {
    if (xQueueReceive(systemMessageQueue, &n, pdMS_TO_TICKS(40))) {
      // Process system notifications
      if (n.message == MSG_KEYPRESSED && !isUartWaiting()) {
        BACKLIGHT_On();

        if (checkKeylock(n.state, n.key)) {
          continue;
        }

        if (APPS_key(n.key, n.state)) {
          gRedrawScreen = true;
        } else {
          // Log("Process keys external");
          if (n.key == KEY_MENU) {
            if (n.state == KEY_LONG_PRESSED) {
              APPS_run(APP_SETTINGS);
            } else if (n.state == KEY_RELEASED) {
              APPS_run(APP_APPS_LIST);
            }
          }
          if (n.key == KEY_EXIT) {
            if (n.state == KEY_RELEASED) {
              APPS_exit();
            }
          }
        }
      }
      if (n.message == MSG_NOTIFY) {
        gRedrawScreen = true;
      }
    }

    while (UART_IsCommandAvailable()) {
      UART_HandleCommand();
      lastUartDataTime = Now();
      vTaskDelay(1);
    }

    if (!isUartWaiting()) {
      STATUSLINE_update();
      if (Now() >= notificationTimeoutAt) {
        notificationMessage[0] = '\0';
      }
    }
    // vTaskDelay(1);
  }
}

void SYS_MsgKey(KEY_Code_t key, Key_State_t state) {
  SystemMessages appMSG = {MSG_KEYPRESSED, 0, key, state};
  BaseType_t xHigherPriorityTaskWoken = pdTRUE;
  xQueueSendFromISR(systemMessageQueue, (void *)&appMSG,
                    &xHigherPriorityTaskWoken);
}

void SYS_MsgNotify(const char *message, uint32_t ms) {
  SystemMessages appMSG = {MSG_NOTIFY};
  BaseType_t xHigherPriorityTaskWoken = pdTRUE;
  xQueueSendFromISR(systemMessageQueue, (void *)&appMSG,
                    &xHigherPriorityTaskWoken);
  notificationTimeoutAt = Now() + ms;
  strncpy(notificationMessage, message, 16);
}
