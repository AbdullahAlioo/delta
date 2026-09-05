/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef SDCARD_H
#define SDCARD_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

extern bool sdCardAvailable;

bool sdcard_init();
bool sdcard_appendLog(const char* path, const String& line);
bool sdcard_writeFile(const char* path, const String& content);
String sdcard_readFile(const char* path);
int sdcard_readLines(const char* path, String* lines, int maxLines);
bool sdcard_ensureDir(const char* path);
String sdcard_getTimestamp();

#endif // SDCARD_H
