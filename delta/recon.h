/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef RECON_H
#define RECON_H

#include <Arduino.h>
#include "config.h"

namespace recon {

void init();
void startScan();
void startHunt();
void startLive();
void stop();
void update();

bool isActive();
ReconMode getMode();
uint16_t getDeviceCount();
uint32_t getTotalPackets();
uint8_t getAlertCount();
uint8_t getCurrentChannel();
uint32_t getElapsed();
uint32_t getDuration();

ReconDevice* getDevice(uint8_t idx);
ReconAlert* getAlert(uint8_t idx);

int getDeviceIndexByMac(const uint8_t* mac);

uint8_t getSummaryRouters();
uint8_t getSummaryCameras();
uint8_t getSummaryPhones();
uint8_t getSummaryPrinters();
uint8_t getSummaryOpen();
uint8_t getSummaryEasyHack();

String macToStr(const uint8_t* mac);
const char* encToStr(EncryptionType enc);
const char* categoryToStr(DeviceCategory cat);
const char* riskToStr(uint8_t score);

} // namespace recon

#endif // RECON_H
