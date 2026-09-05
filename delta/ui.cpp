/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "ui.h"
#include "scanner.h"
#include "deauth.h"
#include "beacon.h"
#include "pktmon.h"
#include "eviltwin.h"
#include "portscan.h"
#include "sdcard.h"
#include "recon.h"
#include <WiFi.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void ui_init() {
  display.begin();
  display.setContrast(255);
  display.clearBuffer();
}

void ui_clear() {
  display.clearBuffer();
}

void ui_flush() {
  display.sendBuffer();
}

void ui_drawScrollBar(int current, int total, int visible) {
  if (total <= visible) return;
  int barH = 64;
  int thumbH = max(4, (barH * visible) / total);
  int thumbY = ((barH - thumbH) * current) / (total - visible);
  display.drawVLine(127, 0, barH);
  display.drawBox(126, thumbY, 2, thumbH);
}

void ui_drawProgressBar(int x, int y, int w, int h, int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  display.drawFrame(x, y, w, h);
  int fillW = ((w - 2) * percent) / 100;
  if (fillW > 0) {
    display.drawBox(x + 1, y + 1, fillW, h - 2);
  }
}

void ui_drawRSSIBar(int x, int y, int rssi) {
  int bars = 0;
  if (rssi >= -50) bars = 4;
  else if (rssi >= -60) bars = 3;
  else if (rssi >= -70) bars = 2;
  else if (rssi >= -80) bars = 1;
  for (int i = 0; i < 4; i++) {
    int bh = 2 + i * 2;
    int bx = x + i * 4;
    int by = y + (8 - bh);
    if (i < bars) display.drawBox(bx, by, 3, bh);
    else display.drawFrame(bx, by, 3, bh);
  }
}

void ui_drawCentered(int y, const char* text) {
  int w = display.getStrWidth(text);
  display.drawStr((128 - w) / 2, y, text);
}

const unsigned char logo[] PROGMEM = {
  0x00,0x00,0x00,0x00,0xF8,0x1F,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x80,0xFF,0xFF,0x01,0x00,0x00,0x00,
  0x00,0x00,0x00,0xE0,0xFF,0xFF,0x07,0x00,0x00,0x00,
  0x00,0x00,0x00,0xF8,0x03,0xC0,0x1F,0x00,0x00,0x00,
  0x00,0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00,
  0x00,0x00,0x00,0x1E,0x00,0x00,0x78,0x00,0x00,0x00,
  0x00,0x00,0x00,0x04,0x00,0x00,0x20,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0xF8,0x3F,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0xC0,0x3F,0xFC,0x03,0x00,0x00,0x00,

  0x00,0x00,0x00,0xC0,0x03,0xC0,0x07,0x00,0x00,0x00,
  0x80,0x1F,0x00,0xC0,0x00,0x00,0x03,0x00,0xF8,0x01,
  0xE0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0xFC,0x07,
  0xC0,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x03,
  0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,
  0x00,0xFC,0x01,0x00,0x7C,0x3E,0x00,0x80,0x7F,0x00,
  0x00,0xF8,0x01,0x00,0x7F,0xFE,0x01,0x80,0x1F,0x00,
  0x00,0xF8,0x03,0xC0,0x7F,0xFE,0x03,0xC0,0x1F,0x00,
  0x00,0xF8,0x03,0xF0,0x7F,0xFE,0x0F,0xC0,0x1F,0x00,
  0x00,0xF8,0x03,0xF8,0x7F,0xFE,0x1F,0xC0,0x1F,0x00,

  0x02,0xF8,0x03,0xFC,0x7F,0xFE,0x3F,0xC0,0x1F,0x40,
  0x0E,0xFC,0x07,0xFE,0x7F,0xFE,0x7F,0xE0,0x3F,0x70,
  0x3E,0xFF,0x1F,0xFE,0x7F,0xFE,0xFF,0xF0,0xFF,0x7C,
  0xFC,0xFF,0x3F,0xFF,0x7F,0xFE,0xFF,0xFC,0xFF,0x3F,
  0xFC,0xFF,0x9F,0xFF,0x7F,0xFE,0xFF,0xF9,0xFF,0x3F,
  0xF8,0xFF,0x9F,0xFF,0x7F,0xFE,0xFF,0xF9,0xFF,0x1F,
  0xF0,0xFF,0xDF,0xFF,0x7F,0xFE,0xFF,0xF3,0xFF,0x0F,
  0xE0,0xFF,0xCF,0xFF,0x7F,0xFE,0xFF,0xF3,0xFF,0x07,
  0x00,0xC0,0xCF,0xFF,0x7F,0xFE,0xFF,0xF3,0x03,0x00,
  0x00,0x80,0xEF,0xFF,0x7F,0xFE,0xFF,0xF7,0x01,0x00,

  0x00,0x00,0xEE,0xFF,0x7F,0xFE,0xFF,0x67,0x00,0x00,
  0x00,0x00,0xE4,0xFF,0x7F,0xFE,0xFF,0x27,0x00,0x00,
  0x00,0x00,0xE0,0xFF,0x7F,0xFE,0xFF,0x07,0x00,0x00,
  0x00,0x00,0xE0,0xFB,0x7F,0xFE,0x9F,0x07,0x00,0x00,
  0x00,0x00,0xE0,0xC1,0x7F,0xFE,0x83,0x07,0x00,0x00,
  0x00,0x00,0xE0,0x01,0x7F,0xFE,0x80,0x07,0x00,0x00,
  0x00,0x00,0xE0,0x01,0x7C,0x3E,0x80,0x07,0x00,0x00,
  0x00,0x00,0xE0,0x03,0x70,0x0E,0xC0,0x07,0x00,0x00,
  0x00,0x00,0xC0,0x03,0x70,0x0E,0xC0,0x03,0x00,0x00,
  0x00,0x00,0xC0,0x07,0x70,0x0E,0xE0,0x03,0x00,0x00,

  0x00,0x00,0xC0,0x07,0x78,0x1E,0xE0,0x03,0x00,0x00,
  0x00,0x00,0x80,0x1F,0x7C,0x3E,0xF8,0x01,0x00,0x00,
  0x00,0x00,0x90,0xFF,0x7F,0xFE,0xFF,0x09,0x00,0x00,
  0x00,0x00,0x38,0xFF,0x7F,0xFE,0xFF,0x1C,0x00,0x00,
  0x00,0x00,0x7C,0xFE,0x7F,0xFE,0x7F,0x7C,0x00,0x00,
  0x00,0x00,0x7F,0xFC,0x7F,0xFE,0x7F,0xFE,0x00,0x00,
  0x00,0x80,0xFF,0xF8,0x7F,0xFE,0x3F,0xFF,0x01,0x00,
  0x80,0xCF,0xFF,0xF1,0x7F,0xFE,0x9F,0xFF,0xF3,0x01,
  0xE0,0xFF,0xFF,0xE1,0x7F,0xFE,0x87,0xFF,0xFF,0x07,
  0xF0,0xFF,0xFF,0xC0,0x7F,0xFE,0x03,0xFF,0xFF,0x1F,

  0xF8,0xFF,0x7F,0x80,0x7F,0xFE,0x03,0xFE,0xFF,0x1F,
  0xFC,0xFF,0x3F,0x80,0x7F,0xFE,0x03,0xFC,0xFF,0x3F,
  0x7C,0xFF,0x0F,0x80,0x7F,0xFE,0x03,0xF0,0xFF,0x3E,
  0x3C,0xFC,0x07,0x80,0x77,0xEE,0x03,0xE0,0x3F,0x7C,
  0x0E,0xF8,0x03,0x80,0x73,0xCE,0x03,0xC0,0x1F,0x70,
  0x06,0xF8,0x03,0x80,0x77,0xCE,0x03,0xC0,0x1F,0x40,
  0x00,0xF8,0x03,0x80,0x77,0xCE,0x03,0xC0,0x1F,0x00,
  0x00,0xF8,0x03,0x80,0x77,0xCE,0x01,0xC0,0x1F,0x00,
  0x00,0xF8,0x01,0x00,0x73,0xCE,0x00,0x80,0x1F,0x00,
  0x00,0xFC,0x01,0x00,0x76,0x4E,0x00,0x80,0x3F,0x00,

  0x00,0xFF,0x00,0x00,0x70,0x0E,0x00,0x00,0xFF,0x00,
  0x80,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x03,
  0xC0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0xFC,0x07,
  0x80,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x01
};

void ui_drawSplash() {
  display.clearBuffer();

  display.drawXBMP(24, 0, 80, 64, logo);

  display.sendBuffer();
}

void ui_drawScanning() {
  display.setFont(u8g2_font_7x14_tr);
  display.drawStr(0, 12, "SCANNING...");
  if ((millis() / 500) % 2) {
    display.drawDisc(124, 6, 2);
  }
  display.drawStr(0, 32, "Starting radio...");
  ui_drawProgressBar(0, 48, 128, 8, (millis() / 40) % 100);
}

// Main Menu: SCAN / SELECT / ATTACK / PKT MON / IR CLONER
//            + PORTS (only when wifiConnected)
void ui_drawMainMenu(int selectedIdx, bool wifiConnected) {
  display.setFont(u8g2_font_7x14_tr);

  const char* items[8];
  int count = 7;
  items[0] = "SCAN";
  items[1] = "SELECT";
  items[2] = "ATTACK";
  items[3] = "PKT MON";
  items[4] = "IR CLONER";
  items[5] = "RF ANALYZER";
  items[6] = "JAMMER";
  if (wifiConnected) {
    items[7] = "PORTS";
    count = 8;
  }

  int visible = 5;
  int scrollOffset = 0;
  if (selectedIdx >= visible) scrollOffset = selectedIdx - visible + 1;

  for (int i = 0; i < visible && (scrollOffset + i) < count; i++) {
    int idx = scrollOffset + i;
    int y = 12 + i * 12;
    char line[32];
    if (idx == selectedIdx) {
      snprintf(line, 32, "|%s", items[idx]);
    } else {
      snprintf(line, 32, " %s", items[idx]);
    }
    display.drawStr(0, y, line);
  }

  ui_drawScrollBar(scrollOffset, count, visible);
}

void ui_drawScanMenu(int selectedIdx) {
  display.setFont(u8g2_font_7x14_tr);

  const char* items[] = {
    "[BACK]",
    "SCAN APs",
    "SCAN STs",
    "SCAN APs + STs",
    "RECON HUNT"
  };
  int count = 5;

  for (int i = 0; i < count; i++) {
    int y = 12 + i * 12;
    char line[32];
    if (i == selectedIdx) {
      snprintf(line, 32, "|%s", items[i]);
    } else {
      snprintf(line, 32, " %s", items[i]);
    }
    display.drawStr(0, y, line);
  }
}

void ui_drawScanProgress(const char* title, uint8_t channel, int apCount, int stCount, uint32_t pktCount, int percent) {
  display.setFont(u8g2_font_7x14_tr);

  display.drawStr(0, 12, title ? title : "SCANNING");

  if (channel > 0) {
    char chStr[16];
    snprintf(chStr, 16, "CH:%d", channel);
    int rw = display.getStrWidth(chStr);
    display.drawStr(128 - rw - 8, 12, chStr);
  }

  if ((millis() / 500) % 2) {
    display.drawDisc(124, 6, 2);
  }

  display.drawHLine(0, 14, 128);

  char line1[32];
  if (title && strstr(title, "AP") && !strstr(title, "ALL")) {
    snprintf(line1, 32, "APs: %-3d", apCount);
  } else if (title && strstr(title, "ST") && !strstr(title, "ALL")) {
    snprintf(line1, 32, "STs: %-3d", stCount);
  } else {
    snprintf(line1, 32, "APs:%-3d  STs:%d", apCount, stCount);
  }
  display.drawStr(0, 29, line1);

  char line2[28];
  snprintf(line2, 28, "PKTS: %lu", (unsigned long)pktCount);
  display.drawStr(0, 43, line2);

  ui_drawProgressBar(0, 52, 128, 8, percent);
}

void ui_drawScanProgress(int apCount, int stCount, int pktCount, int percent) {
  ui_drawScanProgress("SCANNING", 0, apCount, stCount, (uint32_t)pktCount, percent);
}

void ui_drawSelectMenu(int selectedIdx, bool pgSignup) {
  display.setFont(u8g2_font_7x14_tr);

  const char* items[4];
  items[0] = "[BACK]";
  items[1] = "SELECT APs";
  items[2] = "SELECT STs";
  items[3] = pgSignup ? "PG SIGNUP" : "PG UPDATE";

  for (int i = 0; i < 4; i++) {
    int y = 12 + i * 12;
    char line[32];
    if (i == selectedIdx) {
      snprintf(line, 32, "|%s", items[i]);
    } else {
      snprintf(line, 32, " %s", items[i]);
    }
    display.drawStr(0, y, line);
  }
}

void ui_drawSelectAPs(int selectedIdx, int scrollOffset);
void ui_drawSelectSTs(int selectedIdx, int scrollOffset);
void ui_drawAPInfo(int apIdx, int selectedOption);
void ui_drawSTInfo(int stIdx, int selectedOption);

void ui_drawSelectAPs(int selectedIdx, int scrollOffset) {
  display.setFont(u8g2_font_7x14_tr);

  int totalItems = scannedAPCount + 1;
  int visible = 5;

  for (int i = 0; i < visible && (scrollOffset + i) < totalItems; i++) {
    int idx = scrollOffset + i;
    int y = 12 + i * 12;

    if (idx == 0) {
      char line[32];
      if (idx == selectedIdx) snprintf(line, 32, "|[BACK]");
      else snprintf(line, 32, " [BACK]");
      display.drawStr(0, y, line);
    } else {
      APInfo& ap = scannedAPs[idx - 1];

      char ssid[16];
      strncpy(ssid, ap.ssid, 15);
      ssid[15] = '\0';
      for (int c = 0; ssid[c]; c++) ssid[c] = toupper(ssid[c]);

      char line[30];
      if (idx == selectedIdx) {
        snprintf(line, 30, "|%c%s", ap.selected ? '*' : ' ', ssid);
      } else {
        snprintf(line, 30, " %c%s", ap.selected ? '*' : ' ', ssid);
      }
      display.drawStr(0, y, line);
    }
  }

  ui_drawScrollBar(scrollOffset, totalItems, visible);
}

void ui_drawSelectSTs(int selectedIdx, int scrollOffset) {
  display.setFont(u8g2_font_7x14_tr);

  int totalItems = scannedSTCount + 1;
  int visible = 5;

  for (int i = 0; i < visible && (scrollOffset + i) < totalItems; i++) {
    int idx = scrollOffset + i;
    int y = 12 + i * 12;

    if (idx == 0) {
      char line[32];
      if (idx == selectedIdx) snprintf(line, 32, "|[BACK]");
      else snprintf(line, 32, " [BACK]");
      display.drawStr(0, y, line);
    } else {
      STInfo& st = scannedSTs[idx - 1];

      char vendor[16];
      strncpy(vendor, st.vendor, 15);
      vendor[15] = '\0';
      if (strlen(vendor) == 0) strcpy(vendor, "UNKNOWN");

      char line[30];
      if (idx == selectedIdx) {
        snprintf(line, 30, "|%c%s", st.selected ? '*' : ' ', vendor);
      } else {
        snprintf(line, 30, " %c%s", st.selected ? '*' : ' ', vendor);
      }
      display.drawStr(0, y, line);
    }
  }

  ui_drawScrollBar(scrollOffset, totalItems, visible);
}

void ui_drawAPInfo(int apIdx, int selectedOption) {
  if (apIdx < 0 || apIdx >= scannedAPCount) return;

  APInfo& ap = scannedAPs[apIdx];
  display.setFont(u8g2_font_7x14_tr);

  char ssid[19];
  strncpy(ssid, ap.ssid, 18);
  ssid[18] = '\0';
  display.drawStr(0, 12, ssid);

  char bssidStr[20];
  scanner_formatBSSID(ap.bssid, bssidStr, sizeof(bssidStr));
  display.drawStr(0, 24, bssidStr);

  char info[32];
  snprintf(info, 32, "CH:%d %ddBm %s", ap.channel, ap.rssi, scanner_encStr(ap.encryption));
  display.drawStr(0, 36, info);

  // Options (drawn side by side or just stacked)
  // Let's draw them stacked: [BACK] then CONNECT
  int yOpts = 50;
  
  if (selectedOption == 0) {
    display.drawStr(0, yOpts, "|[BACK]  CONNECT");
  } else {
    display.drawStr(0, yOpts, " [BACK] |CONNECT");
  }
}

void ui_drawSTInfo(int stIdx, int selectedOption) {
  if (stIdx < 0 || stIdx >= scannedSTCount) return;

  STInfo& st = scannedSTs[stIdx];
  display.setFont(u8g2_font_7x14_tr);

  char macStr[20];
  scanner_formatBSSID(st.mac, macStr, sizeof(macStr));
  display.drawStr(0, 12, macStr);

  char vendorStr[20];
  snprintf(vendorStr, 20, "V:%.17s", st.vendor);
  display.drawStr(0, 24, vendorStr);

  // AP Network (BSSID or SSID if we have it)
  char netStr[20];
  if (st.apMac[0] == 0 && st.apMac[1] == 0 && st.apMac[2] == 0) {
    strcpy(netStr, "AP:NONE/HIDDEN");
  } else {
    // Try to find the name of the AP in our AP list
    bool found = false;
    for (int i = 0; i < scannedAPCount; i++) {
      if (memcmp(scannedAPs[i].bssid, st.apMac, 6) == 0) {
        snprintf(netStr, 20, "AP:%.16s", scannedAPs[i].ssid);
        found = true;
        break;
      }
    }
    if (!found) {
      char apMacStr[20];
      scanner_formatBSSID(st.apMac, apMacStr, sizeof(apMacStr));
      snprintf(netStr, 20, "AP:%s", apMacStr);
    }
  }
  display.drawStr(0, 36, netStr);

  char info[32];
  snprintf(info, 32, "RSSI: %ddBm", st.rssi);
  display.drawStr(0, 48, info);

  // Options (only BACK for STs currently)
  display.drawStr(0, 60, "|[BACK]");
}

void ui_drawAttackMenu(int selectedAttack, int selectedIdx) {
  display.setFont(u8g2_font_7x14_tr);

  const char* items[] = { "[BACK]", "DEAUTH", "BEACON", "EVIL TWIN", "START" };

  for (int i = 0; i < 5; i++) {
    int y = 12 + i * 12;
    char line[32];
    
    if (i == 0 || i == 4) {
      if (i == selectedIdx) snprintf(line, 32, "|%s", items[i]);
      else snprintf(line, 32, " %s", items[i]);
    } else {
      // Attacks: index 1 to 3 map to attack 0 to 2
      char marker = ((i - 1) == selectedAttack) ? 'X' : ' ';
      if (i == selectedIdx) {
        snprintf(line, 32, "|[%c] %s", marker, items[i]);
      } else {
        snprintf(line, 32, " [%c] %s", marker, items[i]);
      }
    }
    display.drawStr(0, y, line);
  }
}

void ui_drawAttackRunning(int attackType) {
  display.setFont(u8g2_font_7x14_tr);

  if (attackType == 0) {
    display.drawStr(0, 12, "DEAUTH ACTIVE");

    char line2[28];
    snprintf(line2, 28, "TGT:%.13s", deauth_getCurrentTarget());
    display.drawStr(0, 24, line2);

    char line3[28];
    snprintf(line3, 28, "PKTS:%lu", (unsigned long)deauthPacketsSent);
    display.drawStr(0, 36, line3);

    char line4[28];
    snprintf(line4, 28, "PPS:%.0f", deauth_getPPS());
    display.drawStr(0, 48, line4);

  } else if (attackType == 1) {
    display.drawStr(0, 12, "BEACON ACTIVE");

    char line2[28];
    snprintf(line2, 28, "SSIDS:%d", beaconSSIDCount);
    display.drawStr(0, 24, line2);

    char line3[28];
    snprintf(line3, 28, "PKTS:%lu", (unsigned long)beaconPacketsSent);
    display.drawStr(0, 36, line3);

    char ssidBuf[19];
    strncpy(ssidBuf, beacon_getCurrentSSID(), 18);
    ssidBuf[18] = '\0';
    display.drawStr(0, 48, ssidBuf);

  } else if (attackType == 2) {
    display.drawStr(0, 12, "EVIL TWIN LIVE");

    char line2[28];
    snprintf(line2, 28, "AP:%.14s", eviltwin_getSSID());
    display.drawStr(0, 24, line2);

    char line3[28];
    snprintf(line3, 28, "C:%d P:%d", eviltwinClients, eviltwinCaptured);
    display.drawStr(0, 36, line3);

    if (eviltwinCaptured > 0) {
      char passLine[28];
      snprintf(passLine, 28, "U:%.15s", eviltwinLastUser);
      display.drawStr(0, 48, passLine);
    } else {
      display.drawStr(0, 48, "WAITING...");
    }
  }

  if ((millis() / 500) % 2) {
    display.drawDisc(124, 6, 2);
  }
}

#include "IRSignal.h"

void ui_drawIRCloner(int selectedIdx, int scrollOffset) {
  display.setFont(u8g2_font_7x14_tr);
  
  if (ir::getMode() == ir::Mode::Flooder) {
    display.drawStr(0, 12, "IR FLOODER");
    display.drawHLine(0, 14, 128);
    if ((millis() / 500) % 2) display.drawDisc(124, 6, 2);
    ui_drawCentered(29, "BLASTING OFF");
    char st[24];
    snprintf(st, sizeof(st), "%.20s", ir::getStatus().c_str());
    ui_drawCentered(43, st);
    display.drawHLine(0, 52, 128);
    display.drawStr(0, 63, "< [LEFT] STOP");
    return;
  }

  if (ir::getMode() == ir::Mode::Record || ir::getMode() == ir::Mode::Analyse) {
    display.drawStr(0, 12, "IR LEARNING");
    display.drawHLine(0, 14, 128);
    if ((millis() / 500) % 2) display.drawDisc(124, 6, 2);
    display.drawStr(0, 29, "Aim remote at sensor");
    display.drawStr(0, 43, "Press button...");
    display.drawHLine(0, 52, 128);
    display.drawStr(0, 63, "< [LEFT] CANCEL");
    return;
  }

  if (ir::getMode() == ir::Mode::SendResult && ir::menuGetScreen() != ir::Screen::SignalReady) {
    display.drawStr(0, 12, "IR STATUS");
    display.drawHLine(0, 14, 128);
    char res[24];
    snprintf(res, sizeof(res), "%.20s", ir::getSendResult().c_str());
    ui_drawCentered(36, res);
    display.drawHLine(0, 52, 128);
    display.drawStr(0, 63, "< [BACK]");
    return;
  }
  
  if (ir::menuGetScreen() == ir::Screen::SignalReady) {
    display.drawStr(0, 12, "TRANSMIT");
    display.drawHLine(0, 14, 128);

    String sig = ir::menuGetCurrentSignalName();
    if (sig.length() == 0) sig = "Signal";

    char nameBuf[24];
    snprintf(nameBuf, sizeof(nameBuf), "%.18s", sig.c_str());
    ui_drawCentered(30, nameBuf);

    if (ir::menuIsSent()) {
      ui_drawCentered(44, "* SENT *");
    } else {
      ui_drawCentered(44, "[ PRESS OK ]");
    }

    display.drawHLine(0, 52, 128);
    display.drawStr(0, 63, "< BACK");

    const char* rPrompt = "SEND [OK]";
    int rw = display.getStrWidth(rPrompt);
    display.drawStr(128 - rw, 63, rPrompt);
    return;
  }

  int count = ir::menuGetItemCount();
  bool hasBack = true;
  int totalItems = count + 1;

  if (totalItems == 0) {
    ui_drawCentered(28, "EMPTY");
    return;
  }

  int visible = 5;
  for (int i = 0; i < visible && (scrollOffset + i) < totalItems; i++) {
    int idx = scrollOffset + i;
    int y = 12 + i * 12;
    char line[32];
    
    if (hasBack && idx == 0) {
      if (idx == selectedIdx) snprintf(line, 32, "|[BACK]");
      else snprintf(line, 32, " [BACK]");
    } else {
      int dataIdx = hasBack ? idx - 1 : idx;
      String name = ir::menuGetItemName(dataIdx);
      if (idx == selectedIdx) {
        snprintf(line, 32, "|%.30s", name.c_str());
      } else {
        snprintf(line, 32, " %.30s", name.c_str());
      }
    }
    display.drawStr(0, y, line);
  }
  ui_drawScrollBar(scrollOffset, totalItems, visible);
}

#include "recon.h"

void ui_drawReconMenu(int selectedIdx) {
  display.setFont(u8g2_font_7x14_tr);

  const char* items[] = {
    "[BACK]",
    "SCAN (10s)",
    "HUNT (25s)",
    "LIVE"
  };
  int count = 4;

  for (int i = 0; i < count; i++) {
    int y = 12 + i * 12;
    char line[32];
    if (i == selectedIdx) {
      snprintf(line, 32, "|%s", items[i]);
    } else {
      snprintf(line, 32, " %s", items[i]);
    }
    display.drawStr(0, y, line);
  }
}

void ui_drawReconScanning() {
  display.setFont(u8g2_font_7x14_tr);

  const char* modeStr = "SCANNING";
  if (recon::getMode() == RECON_HUNT) modeStr = "HUNTING";
  else if (recon::getMode() == RECON_LIVE) modeStr = "LIVE";

  display.drawStr(0, 12, modeStr);

  char ch[16];
  snprintf(ch, 16, "CH:%d", recon::getCurrentChannel());
  int rw = display.getStrWidth(ch);
  display.drawStr(128 - rw, 12, ch);

  char l2[28];
  snprintf(l2, 28, "DEV:%d PKT:%lu", recon::getDeviceCount(), (unsigned long)recon::getTotalPackets());
  display.drawStr(0, 28, l2);

  char l3[28];
  snprintf(l3, 28, "ALERTS:%d", recon::getAlertCount());
  display.drawStr(0, 42, l3);

  uint32_t dur = recon::getDuration();
  if (dur > 0) {
    uint32_t elapsed = recon::getElapsed();
    int pct = (int)((elapsed * 100) / dur);
    if (pct > 100) pct = 100;
    ui_drawProgressBar(0, 52, 128, 8, pct);
  } else {
    display.drawStr(0, 56, "PRESS LEFT TO STOP");
  }

  if ((millis() / 500) % 2) {
    display.drawDisc(124, 6, 2);
  }
}

void ui_drawReconResults(int selectedIdx, int scrollOffset) {
  display.setFont(u8g2_font_7x14_tr);

  uint16_t count = recon::getDeviceCount();
  // Items: [BACK], ALERTS(N), then devices
  int totalItems = count + 2;
  int visible = 5;

  for (int i = 0; i < visible && (scrollOffset + i) < totalItems; i++) {
    int idx = scrollOffset + i;
    int y = 12 + i * 12;
    char line[32];

    if (idx == 0) {
      if (idx == selectedIdx) snprintf(line, 32, "|[BACK]");
      else snprintf(line, 32, " [BACK]");
    } else if (idx == 1) {
      if (idx == selectedIdx) snprintf(line, 32, "|ALERTS(%d)", recon::getAlertCount());
      else snprintf(line, 32, " ALERTS(%d)", recon::getAlertCount());
    } else {
      int devIdx = idx - 2;
      ReconDevice* d = recon::getDevice(devIdx);
      if (d) {
        const char* cat = recon::categoryToStr(d->category);
        if (idx == selectedIdx) {
          if (d->ssid[0]) snprintf(line, 32, "|%.10s %s", d->ssid, cat);
          else snprintf(line, 32, "|%02X:%02X:%02X %s", d->mac[3], d->mac[4], d->mac[5], cat);
        } else {
          if (d->ssid[0]) snprintf(line, 32, " %.10s %s", d->ssid, cat);
          else snprintf(line, 32, " %02X:%02X:%02X %s", d->mac[3], d->mac[4], d->mac[5], cat);
        }
      }
    }
    display.drawStr(0, y, line);
  }
  ui_drawScrollBar(scrollOffset, totalItems, visible);
}

void ui_drawReconDevice(int deviceIdx) {
  display.setFont(u8g2_font_7x14_tr);

  ReconDevice* d = recon::getDevice(deviceIdx);
  if (!d) {
    ui_drawCentered(28, "NO DEVICE");
    return;
  }

  char l1[22];
  snprintf(l1, 22, "%02X:%02X:%02X:%02X:%02X:%02X",
           d->mac[0], d->mac[1], d->mac[2], d->mac[3], d->mac[4], d->mac[5]);
  display.drawStr(0, 12, l1);

  char l2[28];
  snprintf(l2, 28, "%s [%s]", d->vendor[0] ? d->vendor : "???", recon::categoryToStr(d->category));
  display.drawStr(0, 24, l2);

  if (d->ssid[0]) {
    char l3[28];
    snprintf(l3, 28, "%.24s", d->ssid);
    display.drawStr(0, 36, l3);
  }

  char l4[28];
  snprintf(l4, 28, "%s R:%d %s CH:%d",
           recon::encToStr(d->encryption),
           d->riskScore, recon::riskToStr(d->riskScore),
           d->channel);
  display.drawStr(0, 48, l4);

  char l5[28];
  snprintf(l5, 28, "%s%s%s%s P:%d",
           d->isAP ? "AP " : "",
           d->wpsEnabled ? "WPS " : "",
           d->isRandomMAC ? "RND " : "",
           d->httpDetected ? "HTTP " : "",
           d->packetCount);
  display.drawStr(0, 60, l5);
}

void ui_drawReconAlerts(int selectedIdx, int scrollOffset) {
  display.setFont(u8g2_font_7x14_tr);

  uint8_t count = recon::getAlertCount();
  int totalItems = count + 1; // [BACK] + alerts
  int visible = 5;

  if (count == 0) {
    display.drawStr(0, 12, selectedIdx == 0 ? "|[BACK]" : " [BACK]");
    ui_drawCentered(36, "NO ALERTS");
    return;
  }

  for (int i = 0; i < visible && (scrollOffset + i) < totalItems; i++) {
    int idx = scrollOffset + i;
    int y = 12 + i * 12;
    char line[32];

    if (idx == 0) {
      if (idx == selectedIdx) snprintf(line, 32, "|[BACK]");
      else snprintf(line, 32, " [BACK]");
    } else {
      int alertIdx = idx - 1;
      ReconAlert* a = recon::getAlert(alertIdx);
      if (a) {
        if (idx == selectedIdx) snprintf(line, 32, "|%.30s", a->detail);
        else snprintf(line, 32, " %.30s", a->detail);
      }
    }
    display.drawStr(0, y, line);
  }
  ui_drawScrollBar(scrollOffset, totalItems, visible);
}

void ui_drawWifiConnect(int status, const char* ssid, uint32_t elapsedMs, uint32_t timeoutMs) {
  display.setFont(u8g2_font_7x14_tr);

  if (status == 1) {
    display.drawStr(0, 12, "CONNECTED!");

    char l2[28];
    String s = WiFi.SSID();
    if (s.length() == 0 && ssid && strlen(ssid) > 0) s = ssid;
    snprintf(l2, sizeof(l2), "SSID:%.17s", s.c_str());
    display.drawStr(0, 24, l2);

    char l3[28];
    snprintf(l3, sizeof(l3), "IP:%s", WiFi.localIP().toString().c_str());
    display.drawStr(0, 36, l3);

    char l4[28];
    snprintf(l4, sizeof(l4), "RSSI:%ddBm CH:%d", WiFi.RSSI(), WiFi.channel());
    display.drawStr(0, 48, l4);

    display.drawStr(0, 60, "|[BACK]");
  } else if (status == -1) {
    display.drawStr(0, 12, "CONN FAILED");

    char l2[28];
    if (ssid && strlen(ssid) > 0) {
      snprintf(l2, sizeof(l2), "SSID:%.17s", ssid);
    } else {
      snprintf(l2, sizeof(l2), "SSID:Unknown");
    }
    display.drawStr(0, 24, l2);

    display.drawStr(0, 36, "Check Password/AP");
    display.drawStr(0, 48, "Timeout / Signal");
    display.drawStr(0, 60, "|[BACK]");
  } else {
    display.drawStr(0, 12, "CONNECTING...");

    char l2[28];
    if (ssid && strlen(ssid) > 0) {
      snprintf(l2, sizeof(l2), "SSID:%.17s", ssid);
    } else {
      snprintf(l2, sizeof(l2), "SSID:Connecting");
    }
    display.drawStr(0, 24, l2);

    uint32_t elSec = elapsedMs / 1000;
    uint32_t toSec = timeoutMs / 1000;
    char l3[28];
    snprintf(l3, sizeof(l3), "TIME:%lus / %lus", (unsigned long)elSec, (unsigned long)toSec);
    display.drawStr(0, 36, l3);

    int pct = (timeoutMs > 0) ? (int)((elapsedMs * 100) / timeoutMs) : 100;
    if (pct > 100) pct = 100;
    ui_drawProgressBar(0, 42, 128, 6, pct);

  }
}

void ui_drawPortScan(const char* targetIP, int progress, int openCount) {
  display.setFont(u8g2_font_7x14_tr);
  display.drawStr(0, 12, "PORT SCANNER");
  if ((millis() / 500) % 2) {
    display.drawDisc(124, 6, 2);
  }
  char l1[28];
  snprintf(l1, 28, "TGT:%.20s", targetIP ? targetIP : "0.0.0.0");
  display.drawStr(0, 26, l1);

  char l2[28];
  snprintf(l2, 28, "OPEN:%-3d  %d%%", openCount, progress);
  display.drawStr(0, 40, l2);

  ui_drawProgressBar(0, 50, 128, 8, progress);
}

void ui_drawPortResults(int selectedIdx, int scrollOffset) {
  display.setFont(u8g2_font_7x14_tr);
  int count = portscan_getResultCount();
  PortResult* results = portscan_getResults();
  if (count == 0) {
    display.drawStr(0, 12, selectedIdx == 0 ? "|[BACK]" : " [BACK]");
    ui_drawCentered(36, "NO OPEN PORTS");
    return;
  }

  int totalItems = count + 1;
  int visible = 5;
  for (int i = 0; i < visible && (scrollOffset + i) < totalItems; i++) {
    int idx = scrollOffset + i;
    int y = 12 + i * 12;
    char line[32];
    if (idx == 0) {
      if (idx == selectedIdx) snprintf(line, 32, "|[BACK]");
      else snprintf(line, 32, " [BACK]");
    } else {
      int rIdx = idx - 1;
      if (idx == selectedIdx) {
        snprintf(line, 32, "|%5d %-6.6s %s",
                 results[rIdx].port, results[rIdx].service,
                 results[rIdx].open ? "OPN" : "---");
      } else {
        snprintf(line, 32, " %5d %-6.6s %s",
                 results[rIdx].port, results[rIdx].service,
                 results[rIdx].open ? "OPN" : "---");
      }
    }
    display.drawStr(0, y, line);
  }
  ui_drawScrollBar(scrollOffset, totalItems, visible);
}

