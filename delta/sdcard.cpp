/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "sdcard.h"

bool sdCardAvailable = false;

bool sdcard_init() {
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1); // CS=-1 so hardware doesn't auto-pull on shared SPI bus
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("[SD] Card mount failed or not present");
    sdCardAvailable = false;
    // Deselect SD module to release shared MISO line
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    return false;
  }
  Serial.println("[SD] Card mounted successfully");
  sdCardAvailable = true;

  sdcard_ensureDir(LOG_DIR);
  return true;
}

bool sdcard_ensureDir(const char* path) {
  if (!sdCardAvailable) return false;
  if (!SD.exists(path)) {
    return SD.mkdir(path);
  }
  return true;
}

bool sdcard_appendLog(const char* path, const String& line) {
  if (!sdCardAvailable) return false;

  File f = SD.open(path, FILE_APPEND);
  if (!f) {
    Serial.printf("[SD] Failed to open %s for append\n", path);
    return false;
  }
  f.println(line);
  f.close();
  return true;
}

bool sdcard_writeFile(const char* path, const String& content) {
  if (!sdCardAvailable) return false;

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.printf("[SD] Failed to open %s for write\n", path);
    return false;
  }
  f.print(content);
  f.close();
  return true;
}

String sdcard_readFile(const char* path) {
  if (!sdCardAvailable) return "";

  File f = SD.open(path, FILE_READ);
  if (!f) {
    return "";
  }
  String content = f.readString();
  f.close();
  return content;
}

int sdcard_readLines(const char* path, String* lines, int maxLines) {
  if (!sdCardAvailable) return 0;

  File f = SD.open(path, FILE_READ);
  if (!f) return 0;

  int count = 0;
  while (f.available() && count < maxLines) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      lines[count++] = line;
    }
  }
  f.close();
  return count;
}

String sdcard_getTimestamp() {
  // Uptime timestamp calculation
  unsigned long sec = millis() / 1000;
  unsigned long mins = sec / 60;
  unsigned long hrs = mins / 60;
  char buf[32];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hrs, mins % 60, sec % 60);
  return String(buf);
}
