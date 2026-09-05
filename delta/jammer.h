/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef JAMMER_H
#define JAMMER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

namespace jammer {

void init();
void start();
void stop();
void update();
void draw(U8G2& u8g2);

bool isRunning();
uint32_t getHopCount();

} // namespace jammer

#endif // JAMMER_H
