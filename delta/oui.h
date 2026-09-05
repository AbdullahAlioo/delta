/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef OUI_H
#define OUI_H

#include <Arduino.h>
#include "config.h"

void oui_init();
void oui_lookup(const uint8_t* mac, char* buf, int bufLen);

#endif // OUI_H
