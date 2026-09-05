/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef SCANNER_H
#define SCANNER_H

#include <Arduino.h>
#include "config.h"

enum ScanType {
  SCAN_TYPE_NONE = 0,
  SCAN_TYPE_APS  = 1,
  SCAN_TYPE_STS  = 2,
  SCAN_TYPE_ALL  = 3
};

extern APInfo scannedAPs[];
extern int scannedAPCount;
extern STInfo scannedSTs[];
extern int scannedSTCount;
extern bool scanInProgress;
extern volatile uint32_t stPktsSeen;

void scanner_start(ScanType type);
void scanner_stop();
void scanner_update();
bool scanner_isComplete();
bool scanner_justCompleted();
ScanType scanner_getScanType();
uint8_t scanner_getCurrentChannel();
int scanner_getProgress();

void scanner_startScan();
void scanner_startSTScan();
bool scanner_checkComplete();

const char* scanner_encStr(EncryptionType enc);
void scanner_sortByRSSI();
void scanner_toggleSelect(int idx);
void scanner_toggleSelectST(int idx);
void scanner_clearSelections();
int scanner_countSelected();
int scanner_countSelectedSTs();
void scanner_formatBSSID(const uint8_t* bssid, char* buf, int bufLen);

#endif // SCANNER_H
