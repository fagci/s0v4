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

uint32_t gAppUpdateInterval = pdMS_TO_TICKS(1);

static char notificationMessage[16] = "";
static uint32_t notificationTimeoutAt;

static TimerHandle_t sysTimer;
static StaticTimer_t sysTimerBuffer;

static QueueHandle_t systemMessageQueue; // Message queue handle
static StaticQueue_t systemTasksQueue;   // Static queue storage area
static uint8_t systemQueueStorageArea[queueLen * itemSize];

StaticTask_t appUpdateTaskBuffer;
StackType_t appUpdateTaskStack[configMINIMAL_STACK_SIZE + 100];

StaticTask_t appRenderTaskBuffer;
StackType_t appRenderTaskStack[configMINIMAL_STACK_SIZE + 100];

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
      STATUSLINE_render();

      APPS_render();

      if (notificationMessage[0]) {
        FillRect(0, 32 - 5, 128, 9, C_FILL);
        PrintMediumBoldEx(64, 32 + 2, POS_C, C_CLEAR, notificationMessage);
      }

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

static uint32_t lastUartDataTime;

void SYS_Main(void *params) {
  BOARD_Init();

  uint8_t buf[2];
  uint8_t deadBuf[] = {0xDE, 0xAD};
  EEPROM_ReadBuffer(0, buf, 2);

  if (memcmp(buf, deadBuf, 2) == 0) {
    gSettings.batteryCalibration = 2000;
    gSettings.backlight = 5;
    APPS_run(APP_RESET);
  } else {
    SETTINGS_Load();
    if (gSettings.batteryCalibration > 2154 ||
        gSettings.batteryCalibration < 1900) {
      gSettings.batteryCalibration = 0;
      EEPROM_WriteBuffer(0, deadBuf, 2);
      NVIC_SystemReset();
    }

    ST7565_Init(false);
    BACKLIGHT_Init();

    Log("Load bands");
    BANDS_Load();
    Log("INIT RADIO");
    RADIO_Init();
    Log("RUN DEFAULT APP");
    APPS_run(gSettings.mainApp);
  }

  BATTERY_UpdateBatteryInfo();

  systemMessageQueue = xQueueCreateStatic(
      queueLen, itemSize, systemQueueStorageArea, &systemTasksQueue);
  sysTimer = xTimerCreateStatic("sysT", pdMS_TO_TICKS(1000), pdTRUE, NULL,
                                systemUpdate, &sysTimerBuffer);
  xTimerStart(sysTimer, 0);

  xTaskCreateStatic(appUpdate, "appU", ARRAY_SIZE(appUpdateTaskStack), NULL, 3,
                    appUpdateTaskStack, &appUpdateTaskBuffer);
  xTaskCreateStatic(appRender, "appR", ARRAY_SIZE(appRenderTaskStack), NULL, 2,
                    appRenderTaskStack, &appRenderTaskBuffer);
  /* xTaskCreateStatic(uart, "uart", ARRAY_SIZE(uartTaskStack), NULL, 2,
                    uartTaskStack, &uartTaskBuffer); */

  SystemMessages n;

  for (;;) {
    if (xQueueReceive(systemMessageQueue, &n, pdMS_TO_TICKS(5))) {
      // Process system notifications
      // Log("MSG: m:%u, k:%u, st:%u", n.message, n.key, n.state);
      if (n.message == MSG_KEYPRESSED && Now() - lastUartDataTime >= 1000) {
        BACKLIGHT_On();
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
    }

    while (UART_IsCommandAvailable()) {
      UART_HandleCommand();
      lastUartDataTime = Now();
    }

    STATUSLINE_update();

    if (Now() >= notificationTimeoutAt) {
      notificationMessage[0] = '\0';
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
