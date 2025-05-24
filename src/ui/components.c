#include "components.h"
#include "../driver/st7565.h"
#include "../helper/channels.h"
#include "../helper/measurements.h"
#include <stdint.h>

void UI_Battery(uint8_t Level) {
  DrawRect(LCD_WIDTH - 13, 0, 12, 5, C_FILL);
  FillRect(LCD_WIDTH - 12, 1, Level, 3, C_FILL);
  DrawVLine(LCD_WIDTH - 1, 1, 3, C_FILL);

  if (Level > 10) {
    DrawHLine(LCD_WIDTH - 1 - 3, 1, 3, C_INVERT);
    DrawHLine(LCD_WIDTH - 1 - 7, 1, 5, C_INVERT);
    DrawHLine(LCD_WIDTH - 1 - 3, 3, 3, C_INVERT);
  }
}

void UI_TxBar(uint8_t y) {
  const uint8_t BAR_LEFT_MARGIN = 0;
  const uint8_t BAR_WIDTH = LCD_WIDTH - BAR_LEFT_MARGIN - 22;
  const uint8_t BAR_BASE = y + 7;
  FillRect(0, y, LCD_WIDTH, 8, C_CLEAR);
  PrintMediumEx(LCD_WIDTH - 1, BAR_BASE, 2, true, "%u", gCurrentTxPower);
  const unsigned int level = MIN(BK4819_GetVoiceAmplitude() * 8, 65535u);
  uint8_t audioW =
      ConvertDomain(MIN(SQRT16(level), 124u), 0, 124, 0, BAR_WIDTH);
  FillRect(BAR_LEFT_MARGIN, y + 2, audioW, 4, C_FILL);
}

void UI_RSSIBar(uint8_t y) {
  uint16_t rssi = gLoot.rssi;
  if (rssi == 0) {
    return;
  }

  const uint8_t BAR_LEFT_MARGIN = 0;
  const uint8_t BAR_WIDTH = LCD_WIDTH - BAR_LEFT_MARGIN - 22;
  const uint8_t BAR_BASE = y + 7;

  FillRect(0, y, LCD_WIDTH, 8, C_CLEAR);

  const uint16_t SNR_MIN = 0;
  const uint16_t SNR_MAX = 30;

  uint8_t rssiW = ConvertDomain(rssi, RSSI_MIN, RSSI_MAX, 0, BAR_WIDTH);
  uint8_t snrW = ConvertDomain(gLoot.snr, SNR_MIN, SNR_MAX, 0, BAR_WIDTH);

  FillRect(BAR_LEFT_MARGIN, y + 2, rssiW, 4, C_FILL);
  FillRect(BAR_LEFT_MARGIN, y + 7, snrW, 1, C_FILL);

  DrawHLine(0, y + 5, BAR_WIDTH - 2, C_FILL);
  for (int16_t r = Rssi2DBm(RSSI_MIN); r < Rssi2DBm(RSSI_MAX); r++) {
    if (r % 10 == 0) {
      FillRect(ConvertDomain(r, Rssi2DBm(RSSI_MIN), Rssi2DBm(RSSI_MAX), 0,
                             BAR_WIDTH),
               y + 5 - (r % 50 == 0 ? 2 : 1), 1, r % 50 == 0 ? 2 : 1, C_INVERT);
    }
  }

  PrintMediumEx(LCD_WIDTH - 1, BAR_BASE, 2, true, "%d", Rssi2DBm(rssi));
}

void drawTicks(uint8_t y, uint32_t fs, uint32_t fe, uint32_t div, uint8_t h) {
  for (uint32_t f = fs - (fs % div) + div; f < fe; f += div) {
    uint8_t x = ConvertDomainF(f, fs, fe, 0, LCD_WIDTH - 1);
    DrawVLine(x, y, h, C_FILL);
  }
}

void UI_DrawTicks(uint8_t y, const Band *band) {
#if 1
  uint32_t fs = band->rxF;
  uint32_t fe = band->txF;
  uint32_t bw = fe - fs;

  for (uint32_t p = 100000000; p >= 10; p /= 10) {
    if (p < bw) {
      drawTicks(y, fs, fe, p / 2, 2);
      drawTicks(y, fs, fe, p, 3);
      return;
    }
  }
#endif
}

void UI_Scanlists(uint8_t baseX, uint8_t baseY, uint16_t sl) {
#if 1
  for (uint8_t i = 0; i < 16; ++i) {
    bool isActive = (sl >> i) & 1;
    uint8_t xi = i % 8;
    uint8_t yi = i / 8;
    uint8_t x = baseX + xi * 3 + (xi > 3) * 2;
    uint8_t y = baseY + yi * 3 + (yi && !isActive);
    FillRect(x, y, 2, 1 + isActive, C_INVERT);
  }
#endif
}

void UI_DrawLoot(const Loot *loot, uint8_t x, uint8_t y, TextPos pos) {
  char c = ' ';
  if (loot->blacklist) {
    c = '-';
  }
  if (loot->whitelist) {
    c = '+';
  }

  PrintMediumEx(x, y, pos, C_INVERT, "%c%u.%05u %c", c, loot->f / MHZ,
                loot->f % MHZ, loot->open ? '!' : ' ');
}

void UI_BigFrequency(uint8_t y, uint32_t f) {
  uint16_t fp1 = f / MHZ;
  uint16_t fp2 = f / 100 % 1000;
  uint8_t fp3 = f % 100;

  PrintBiggestDigitsEx(LCD_WIDTH - 22, y, POS_R, C_FILL, "%4u.%03u", fp1, fp2);
  PrintBigDigitsEx(LCD_WIDTH - 1, y, POS_R, C_FILL, "%02u", fp3);
}

/* void UI_DisplayScanlists(uint32_t y) {
  uint16_t sl = gSettings.currentScanlist;

  PrintMediumEx(LCD_XCENTER, y, POS_C, C_FILL, "%s %s %s %s %s %s %s %s",
                (sl >> 0) & 1 ? "01" : "__", //
                (sl >> 1) & 1 ? "02" : "__", //
                (sl >> 2) & 1 ? "03" : "__", //
                (sl >> 3) & 1 ? "04" : "__", //
                (sl >> 4) & 1 ? "05" : "__", //
                (sl >> 5) & 1 ? "06" : "__", //
                (sl >> 6) & 1 ? "07" : "__", //
                (sl >> 7) & 1 ? "08" : "__"  //
  );
  PrintMediumEx(LCD_XCENTER, y + 8, POS_C, C_FILL, "%s %s %s %s %s %s %s %s",
                (sl >> 8) & 1 ? "09" : "__",  //
                (sl >> 9) & 1 ? "10" : "__",  //
                (sl >> 10) & 1 ? "11" : "__", //
                (sl >> 11) & 1 ? "12" : "__", //
                (sl >> 12) & 1 ? "13" : "__", //
                (sl >> 13) & 1 ? "14" : "__", //
                (sl >> 14) & 1 ? "15" : "__", //
                (sl >> 15) & 1 ? "16" : "__"  //
  );
} */

void UI_DisplayScanlists(uint32_t y) {
#if 1
  uint16_t sl = gSettings.currentScanlist;
  char buf[17] = {0}; // 16 бит + нуль-терминатор

  for (uint8_t i = 0; i < 16; i++) {
    bool sel = sl & (1 << i);
    if (i < 8) {
      buf[i] = sel ? '1' + i : '_';
    } else {
      buf[i] = sel ? 'A' + (i - 8) : '_';
    }
  }

  PrintMediumEx(LCD_XCENTER, y, POS_C, C_FILL, "%.4s %.4s %.4s %.4s", buf,
                buf + 4, buf + 8, buf + 12);
#endif
}

void UI_RenderScanScreen() {
  if (gScanlistSize) {
    PrintMediumEx(LCD_XCENTER, 26, POS_C, C_FILL, "%u.%05u", radio.rxF / MHZ,
                  radio.rxF % MHZ);
  } else {
    PrintMediumBoldEx(LCD_XCENTER, 18, POS_C, C_FILL, "Scanlist empty");
  }

  if (gIsListening) {
    UI_RSSIBar(28);
  }

  if (gLastActiveLoot) {
    UI_DrawLoot(gLastActiveLoot, LCD_XCENTER, 50, POS_C);
  }
  UI_DisplayScanlists(60);
}
