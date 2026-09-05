/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef PORTSCAN_H
#define PORTSCAN_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

extern bool portscanRunning;
extern bool portscanComplete;
extern int portscanProgress;
extern int portscanOpenCount;

void portscan_start(IPAddress target);
bool portscan_scanNext();
PortResult* portscan_getResults();
int portscan_getResultCount();
void portscan_stop();

#endif // PORTSCAN_H
