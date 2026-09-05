/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef RFANALYZER_H
#define RFANALYZER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

namespace rfanalyzer {

void init();
void start();
void stop();
void update();
void draw(U8G2& u8g2);

bool isRunning();
uint8_t getPeakChannel();
uint8_t getMaxRSSI();

} // namespace rfanalyzer

#endif // RFANALYZER_H
