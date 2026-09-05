/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef BEACON_H
#define BEACON_H

#include <Arduino.h>
#include "config.h"

extern bool beaconRunning;
extern uint32_t beaconPacketsSent;
extern int beaconSSIDCount;
extern BeaconMode beaconCurrentMode;

int beacon_loadSSIDs();
void beacon_start(BeaconMode mode);
void beacon_stop();
void beacon_sendNext();
const char* beacon_getCurrentSSID();

#endif // BEACON_H
