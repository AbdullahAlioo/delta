/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef EVILTWIN_H
#define EVILTWIN_H

#include <Arduino.h>
#include "config.h"

extern bool eviltwinRunning;
extern int eviltwinClients;
extern int eviltwinCaptured;
extern char eviltwinLastUser[64];
extern char eviltwinLastPass[64];
extern uint32_t eviltwinDeauthPkts;

void eviltwin_start(const char* ssid, uint8_t apChannel, const uint8_t* realBSSID, uint8_t realChannel);
void eviltwin_stop();
void eviltwin_loop();
const char* eviltwin_getSSID();

#endif // EVILTWIN_H
