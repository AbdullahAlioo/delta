/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C display;

void ui_init();
void ui_clear();
void ui_flush();
void ui_drawScrollBar(int current, int total, int visible);
void ui_drawProgressBar(int x, int y, int w, int h, int percent);
void ui_drawRSSIBar(int x, int y, int rssi);
void ui_drawCentered(int y, const char* text);
void ui_drawSplash();

void ui_drawMainMenu(int selectedIdx, bool wifiConnected);
void ui_drawScanMenu(int selectedIdx);
void ui_drawScanProgress(const char* title, uint8_t channel, int apCount, int stCount, uint32_t pktCount, int percent);
void ui_drawScanProgress(int apCount, int stCount, int pktCount, int percent);
void ui_drawSelectMenu(int selectedIdx, bool pgSignup);
void ui_drawSelectAPs(int selectedIdx, int scrollOffset);
void ui_drawSelectSTs(int selectedIdx, int scrollOffset);
void ui_drawAPInfo(int apIdx, int selectedOption);
void ui_drawSTInfo(int stIdx, int selectedOption);
void ui_drawAttackMenu(int selectedAttack, int selectedIdx);
void ui_drawAttackRunning(int attackType);
void ui_drawScanning();
void ui_drawIRCloner(int selectedIdx, int scrollOffset);
void ui_drawReconMenu(int selectedIdx);
void ui_drawReconScanning();
void ui_drawReconResults(int selectedIdx, int scrollOffset);
void ui_drawReconDevice(int deviceIdx);
void ui_drawReconAlerts(int selectedIdx, int scrollOffset);
void ui_drawWifiConnect(int status, const char* ssid, uint32_t elapsedMs, uint32_t timeoutMs);
void ui_drawPortScan(const char* targetIP, int progress, int openCount);
void ui_drawPortResults(int selectedIdx, int scrollOffset);

#endif // UI_H
