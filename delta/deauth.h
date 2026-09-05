/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef DEAUTH_H
#define DEAUTH_H

#include <Arduino.h>
#include "config.h"

extern bool deauthRunning;
extern uint32_t deauthPacketsSent;
extern unsigned long deauthStartTime;

void deauth_init();
void deauth_start();
void deauth_stop();
int deauth_sendBurst();
const char* deauth_getCurrentTarget();
float deauth_getPPS();

#endif // DEAUTH_H
