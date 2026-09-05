/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef PKTMON_H
#define PKTMON_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

extern bool pktmonRunning;
extern uint8_t pktmonChannel;

extern volatile uint32_t pktmon_totalCount;
extern volatile uint32_t pktmon_beaconCount;
extern volatile uint32_t pktmon_dataCount;
extern volatile uint32_t pktmon_deauthCount;
extern volatile uint32_t pktmon_probeCount;
extern volatile uint32_t pktmon_mgmtCount;

void pktmon_start(uint8_t channel);
void pktmon_stop();
void pktmon_update();
void pktmon_draw(U8G2& u8g2);
void pktmon_setChannel(uint8_t ch);
void pktmon_logSession();

#endif // PKTMON_H
