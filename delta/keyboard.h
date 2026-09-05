/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

void keyboard_init(const char* prompt, const char* initialValue = "", int maxLen = 32, bool isPassword = false);
bool keyboard_handleButton(ButtonID btn);
void keyboard_draw(U8G2& u8g2);
const char* keyboard_getResult();
bool keyboard_wasConfirmed();

#endif // KEYBOARD_H
