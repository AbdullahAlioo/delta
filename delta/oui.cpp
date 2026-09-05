/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "oui.h"
#include "sdcard.h"
#include <SD.h>

struct OUIEntry {
  uint8_t oui[3];
  const char* vendor;
};

static const OUIEntry builtinOUI[] PROGMEM = {
  {{0x00,0x03,0x93}, "Apple"},
  {{0x00,0x0C,0xE7}, "MediaTek"},
  {{0x00,0x0F,0xAC}, "Sony"},
  {{0x00,0x11,0x32}, "Synology"},
  {{0x00,0x13,0x46}, "D-Link"},
  {{0x00,0x14,0x22}, "Dell"},
  {{0x00,0x15,0x5D}, "Microsoft"},
  {{0x00,0x17,0xC4}, "Quanta"},
  {{0x00,0x19,0xE3}, "Apple"},
  {{0x00,0x1A,0x11}, "Google"},
  {{0x00,0x1B,0x63}, "Apple"},
  {{0x00,0x1D,0x43}, "Cisco"},
  {{0x00,0x1E,0x58}, "D-Link"},
  {{0x00,0x1F,0x3C}, "Intel"},
  {{0x00,0x21,0x6A}, "Intel"},
  {{0x00,0x22,0x6B}, "Cisco"},
  {{0x00,0x23,0x69}, "Cisco"},
  {{0x00,0x24,0xD7}, "Intel"},
  {{0x00,0x25,0x00}, "Apple"},
  {{0x00,0x25,0x9C}, "Cisco"},
  {{0x00,0x26,0xBB}, "Apple"},
  {{0x00,0x50,0x56}, "VMware"},
  {{0x00,0xE0,0x4C}, "Realtek"},
  {{0x04,0xF0,0x21}, "Xiaomi"},
  {{0x08,0x00,0x27}, "VirtualBox"},
  {{0x10,0x68,0x3F}, "LG"},
  {{0x14,0x13,0x46}, "Huawei"},
  {{0x18,0xFE,0x34}, "Espressif"},
  {{0x1C,0xBF,0xCE}, "Shenzhen"},
  {{0x24,0x0A,0xC4}, "Espressif"},
  {{0x28,0x6C,0x07}, "Xiaomi"},
  {{0x2C,0x4D,0x54}, "ASUSTek"},
  {{0x30,0xAE,0xA4}, "Espressif"},
  {{0x34,0xAB,0x37}, "Apple"},
  {{0x3C,0x5A,0xB4}, "Google"},
  {{0x40,0x4E,0x36}, "HTC"},
  {{0x44,0x65,0x0D}, "Amazon"},
  {{0x48,0x5D,0x60}, "AzureWave"},
  {{0x4C,0xEB,0xD6}, "Espressif"},
  {{0x54,0x60,0x09}, "Google"},
  {{0x5C,0xCF,0x7F}, "Espressif"},
  {{0x60,0x01,0x94}, "Espressif"},
  {{0x68,0xC6,0x3A}, "Samsung"},
  {{0x70,0xEE,0x50}, "Netatmo"},
  {{0x74,0xDA,0x38}, "Edimax"},
  {{0x7C,0x9E,0xBD}, "Samsung"},
  {{0x80,0x7D,0x3A}, "Apple"},
  {{0x84,0x0D,0x8E}, "Espressif"},
  {{0x84,0xCC,0xA8}, "Espressif"},
  {{0x8C,0xAA,0xB5}, "Samsung"},
  {{0x90,0x97,0xD5}, "Espressif"},
  {{0x94,0xB9,0x7E}, "Espressif"},
  {{0x98,0xD3,0x32}, "Espressif"},
  {{0xA0,0x20,0xA6}, "Espressif"},
  {{0xA4,0x7B,0x9D}, "Espressif"},
  {{0xA4,0xCF,0x12}, "Espressif"},
  {{0xAC,0x67,0xB2}, "Espressif"},
  {{0xAC,0xD0,0x74}, "Espressif"},
  {{0xB4,0xE6,0x2D}, "Espressif"},
  {{0xB8,0x27,0xEB}, "Raspberry"},
  {{0xBC,0xDD,0xC2}, "Espressif"},
  {{0xC4,0x4F,0x33}, "Espressif"},
  {{0xC8,0x2B,0x96}, "Espressif"},
  {{0xCC,0x50,0xE3}, "Espressif"},
  {{0xD0,0xEF,0x76}, "Espressif"},
  {{0xD8,0xA0,0x1D}, "Espressif"},
  {{0xDC,0x4F,0x22}, "Espressif"},
  {{0xE0,0x98,0x06}, "Espressif"},
  {{0xE8,0xDB,0x84}, "Espressif"},
  {{0xEC,0xFA,0xBC}, "Espressif"},
  {{0xF0,0x08,0xD1}, "TP-Link"},
  {{0xF4,0xCF,0xA2}, "Espressif"},
  {{0xFC,0xF5,0xC4}, "Espressif"},
};
static const int builtinOUICount = sizeof(builtinOUI) / sizeof(builtinOUI[0]);

struct DynOUIEntry {
  uint8_t oui[3];
  char vendor[24];
};

static DynOUIEntry* dynOUI = nullptr;
static int dynOUICount = 0;
static bool ouiLoaded = false;

static int oui_compare(const uint8_t* a, const uint8_t* b) {
  for (int i = 0; i < 3; i++) {
    if (a[i] < b[i]) return -1;
    if (a[i] > b[i]) return 1;
  }
  return 0;
}

static uint8_t hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

void oui_init() {
  if (!sdCardAvailable) {
    Serial.println("[OUI] No SD card, using built-in table only");
    return;
  }

  File f = SD.open(OUI_FILE, FILE_READ);
  if (!f) {
    Serial.println("[OUI] No oui.csv on SD, using built-in table");
    return;
  }

  int lineCount = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() >= 8) lineCount++;
  }
  f.seek(0);

  if (lineCount > MAX_OUI_ENTRIES) lineCount = MAX_OUI_ENTRIES;

  dynOUI = (DynOUIEntry*)malloc(lineCount * sizeof(DynOUIEntry));
  if (!dynOUI) {
    Serial.println("[OUI] Failed to allocate memory for OUI table");
    f.close();
    return;
  }

  dynOUICount = 0;
  while (f.available() && dynOUICount < lineCount) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() < 8) continue;

    int commaIdx = line.indexOf(',');
    if (commaIdx < 6) continue;

    String hexStr = line.substring(0, commaIdx);
    hexStr.trim();
    if (hexStr.length() < 6) continue;

    dynOUI[dynOUICount].oui[0] = (hexVal(hexStr[0]) << 4) | hexVal(hexStr[1]);
    dynOUI[dynOUICount].oui[1] = (hexVal(hexStr[2]) << 4) | hexVal(hexStr[3]);
    dynOUI[dynOUICount].oui[2] = (hexVal(hexStr[4]) << 4) | hexVal(hexStr[5]);

    String vendorStr = line.substring(commaIdx + 1);
    vendorStr.trim();
    strncpy(dynOUI[dynOUICount].vendor, vendorStr.c_str(), 23);
    dynOUI[dynOUICount].vendor[23] = '\0';

    dynOUICount++;
  }
  f.close();

  for (int i = 0; i < dynOUICount - 1; i++) {
    for (int j = i + 1; j < dynOUICount; j++) {
      if (oui_compare(dynOUI[i].oui, dynOUI[j].oui) > 0) {
        DynOUIEntry tmp = dynOUI[i];
        dynOUI[i] = dynOUI[j];
        dynOUI[j] = tmp;
      }
    }
  }

  ouiLoaded = true;
  Serial.printf("[OUI] Loaded %d entries from SD card\n", dynOUICount);
}

void oui_lookup(const uint8_t* mac, char* buf, int bufLen) {
  // Locally administered bit indicates randomized MAC
  if (mac[0] & 0x02) {
    strncpy(buf, "Random", bufLen - 1);
    buf[bufLen - 1] = '\0';
    return;
  }

  // Binary search on dynamic OUI table
  if (dynOUI && dynOUICount > 0) {
    int lo = 0, hi = dynOUICount - 1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      int cmp = oui_compare(mac, dynOUI[mid].oui);
      if (cmp == 0) {
        strncpy(buf, dynOUI[mid].vendor, bufLen - 1);
        buf[bufLen - 1] = '\0';
        return;
      } else if (cmp < 0) {
        hi = mid - 1;
      } else {
        lo = mid + 1;
      }
    }
  }

  // Linear scan on built-in fallback table
  for (int i = 0; i < builtinOUICount; i++) {
    if (oui_compare(mac, builtinOUI[i].oui) == 0) {
      strncpy(buf, builtinOUI[i].vendor, bufLen - 1);
      buf[bufLen - 1] = '\0';
      return;
    }
  }

  strncpy(buf, "Unknown", bufLen - 1);
  buf[bufLen - 1] = '\0';
}
