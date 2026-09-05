/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "scanner.h"
#include "oui.h"
#include <WiFi.h>
#include "esp_wifi.h"

APInfo scannedAPs[MAX_APS];
int scannedAPCount = 0;
STInfo scannedSTs[MAX_STS];
int scannedSTCount = 0;
bool scanInProgress = false;
volatile uint32_t stPktsSeen = 0;

static ScanType currentScanType = SCAN_TYPE_NONE;
static int scanPhase = 0;
static unsigned long scanStartTime = 0;
static uint8_t currentChannel = 1;
static unsigned long lastChannelHop = 0;
static int scanProgress = 0;
static bool scanJustCompleted = false;

static EncryptionType mapEncryption(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN:            return ENC_OPEN;
    case WIFI_AUTH_WEP:             return ENC_WEP;
    case WIFI_AUTH_WPA_PSK:         return ENC_WPA;
    case WIFI_AUTH_WPA2_PSK:        return ENC_WPA2;
    case WIFI_AUTH_WPA_WPA2_PSK:    return ENC_WPA2;
    case WIFI_AUTH_WPA2_ENTERPRISE: return ENC_WPA2;
    case WIFI_AUTH_WPA3_PSK:        return ENC_WPA3;
    case WIFI_AUTH_WPA2_WPA3_PSK:   return ENC_WPA3;
    default:                        return ENC_UNKNOWN;
  }
}

const char* scanner_encStr(EncryptionType enc) {
  switch (enc) {
    case ENC_OPEN:    return "OPEN";
    case ENC_WEP:     return "WEP";
    case ENC_WPA:     return "WPA";
    case ENC_WPA2:    return "WPA2";
    case ENC_WPA3:    return "WPA3";
    default:          return "???";
  }
}

static void IRAM_ATTR stSnifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!buf || !scanInProgress) return;
  stPktsSeen++;
  
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* frame = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;
  if (len < 24) return;

  uint8_t frameType = (frame[0] >> 2) & 0x03;
  uint8_t frameSubtype = (frame[0] >> 4) & 0x0F;

  // 1. AP Discovery via Beacons (subtype 8) and Probe Responses (subtype 5)
  if (frameType == 0 && (frameSubtype == 8 || frameSubtype == 5)) {
    if (currentScanType == SCAN_TYPE_APS || currentScanType == SCAN_TYPE_ALL) {
      if (len >= 36) {
        const uint8_t* bssid = frame + 10;
        if (!(bssid[0] & 0x01) && !(bssid[0] == 0 && bssid[1] == 0 && bssid[2] == 0 && bssid[3] == 0 && bssid[4] == 0 && bssid[5] == 0)) {
          int existing = -1;
          for (int i = 0; i < scannedAPCount; i++) {
            if (memcmp(scannedAPs[i].bssid, bssid, 6) == 0) {
              existing = i;
              break;
            }
          }

          char ssid[MAX_SSID_LEN] = {0};
          uint8_t ch = currentChannel;
          uint16_t capInfo = frame[34] | (frame[35] << 8);
          EncryptionType enc = (capInfo & 0x0010) ? ENC_WEP : ENC_OPEN;

          uint16_t pos = 36;
          while (pos + 2 <= (uint16_t)len) {
            uint8_t tagNum = frame[pos];
            uint8_t tagLen = frame[pos + 1];
            if (pos + 2 + tagLen > (uint16_t)len) break;

            if (tagNum == 0) {
              if (tagLen > 0 && tagLen < MAX_SSID_LEN) {
                memcpy(ssid, frame + pos + 2, tagLen);
                ssid[tagLen] = '\0';
              }
            } else if (tagNum == 3 && tagLen >= 1) {
              ch = frame[pos + 2];
            } else if (tagNum == 48) {
              enc = ENC_WPA2;
            } else if (tagNum == 221 && tagLen >= 4) {
              if (frame[pos + 2] == 0x00 && frame[pos + 3] == 0x50 &&
                  frame[pos + 4] == 0xF2 && frame[pos + 5] == 0x01) {
                if (enc != ENC_WPA2) enc = ENC_WPA;
              }
            }
            pos += 2 + tagLen;
          }

          if (existing >= 0) {
            if (pkt->rx_ctrl.rssi > scannedAPs[existing].rssi) {
              scannedAPs[existing].rssi = pkt->rx_ctrl.rssi;
            }
            if (ch >= 1 && ch <= 14) scannedAPs[existing].channel = ch;
            if ((scannedAPs[existing].ssid[0] == '\0' || strcmp(scannedAPs[existing].ssid, "[Hidden]") == 0) && ssid[0] != '\0') {
              strncpy(scannedAPs[existing].ssid, ssid, MAX_SSID_LEN - 1);
              scannedAPs[existing].ssid[MAX_SSID_LEN - 1] = '\0';
            }
          } else if (scannedAPCount < MAX_APS) {
            int idx = scannedAPCount;
            if (ssid[0] != '\0') {
              strncpy(scannedAPs[idx].ssid, ssid, MAX_SSID_LEN - 1);
            } else {
              strncpy(scannedAPs[idx].ssid, "[Hidden]", MAX_SSID_LEN - 1);
            }
            scannedAPs[idx].ssid[MAX_SSID_LEN - 1] = '\0';
            memcpy(scannedAPs[idx].bssid, bssid, 6);
            scannedAPs[idx].channel = (ch >= 1 && ch <= 14) ? ch : currentChannel;
            scannedAPs[idx].rssi = pkt->rx_ctrl.rssi;
            scannedAPs[idx].encryption = enc;
            scannedAPs[idx].selected = false;
            oui_lookup(scannedAPs[idx].bssid, scannedAPs[idx].vendor, sizeof(scannedAPs[idx].vendor));
            scannedAPCount++;
          }
        }
      }
    }
  }

  // 2. Station Discovery via Data & Probe Request frames
  if (currentScanType == SCAN_TYPE_STS || currentScanType == SCAN_TYPE_ALL) {
    bool toDS = (frame[1] & 0x01);
    bool fromDS = (frame[1] & 0x02) >> 1;

    const uint8_t* addr1 = frame + 4;
    const uint8_t* addr2 = frame + 10;
    const uint8_t* addr3 = frame + 16;

    const uint8_t* stMac = nullptr;
    const uint8_t* apMac = nullptr;

    if (frameType == 2) {
      if (toDS == 0 && fromDS == 1) {
        stMac = addr1;
        apMac = addr2;
      } else if (toDS == 1 && fromDS == 0) {
        stMac = addr2;
        apMac = addr1;
      } else if (toDS == 0 && fromDS == 0) {
        stMac = addr2;
        apMac = addr3;
      } else {
        return;
      }
    } else if (frameType == 0) {
      if (frameSubtype == 4) {
        stMac = addr2;
        apMac = nullptr;
      } else if (frameSubtype == 0 || frameSubtype == 11) {
        stMac = addr2;
        apMac = addr3;
      } else {
        return;
      }
    } else {
      return;
    }

    if (!stMac || (stMac[0] & 0x01)) return;

    for (int i = 0; i < scannedAPCount; i++) {
      if (memcmp(scannedAPs[i].bssid, stMac, 6) == 0) return;
    }

    for (int i = 0; i < scannedSTCount; i++) {
      if (memcmp(scannedSTs[i].mac, stMac, 6) == 0) {
        scannedSTs[i].rssi = pkt->rx_ctrl.rssi;
        if (apMac && (scannedSTs[i].apMac[0] == 0 && scannedSTs[i].apMac[1] == 0)) {
          memcpy(scannedSTs[i].apMac, apMac, 6);
        }
        return;
      }
    }

    if (scannedSTCount < MAX_STS) {
      memcpy(scannedSTs[scannedSTCount].mac, stMac, 6);
      if (apMac) memcpy(scannedSTs[scannedSTCount].apMac, apMac, 6);
      else memset(scannedSTs[scannedSTCount].apMac, 0, 6);
      
      scannedSTs[scannedSTCount].rssi = pkt->rx_ctrl.rssi;
      scannedSTs[scannedSTCount].selected = false;
      oui_lookup(scannedSTs[scannedSTCount].mac, scannedSTs[scannedSTCount].vendor, sizeof(scannedSTs[scannedSTCount].vendor));
      
      scannedSTCount++;
    }
  }
}

void scanner_start(ScanType type) {
  scanner_stop();

  currentScanType = type;
  scanJustCompleted = false;
  scanProgress = 0;
  scanStartTime = millis();
  lastChannelHop = millis();
  currentChannel = 1;
  stPktsSeen = 0;

  if (type == SCAN_TYPE_APS || type == SCAN_TYPE_ALL) {
    scannedAPCount = 0;
  }
  if (type == SCAN_TYPE_STS || type == SCAN_TYPE_ALL) {
    scannedSTCount = 0;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(30);

  esp_wifi_set_promiscuous(false);
  wifi_promiscuous_filter_t filter;
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(stSnifferCallback);
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(true);

  scanPhase = 1;
  scanInProgress = true;
}

void scanner_stop() {
  if (scanInProgress) {
    scanInProgress = false;
    scanPhase = 0;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
  }
}

void scanner_update() {
  if (!scanInProgress) return;

  unsigned long now = millis();
  if (now - lastChannelHop >= 250) {
    currentChannel++;
    if (currentChannel <= 14) {
      esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
      lastChannelHop = now;
    } else {
      esp_wifi_set_promiscuous(false);
      esp_wifi_set_promiscuous_rx_cb(nullptr);
      if (scannedAPCount > 0) {
        scanner_sortByRSSI();
      }
      scanInProgress = false;
      scanPhase = 0;
      scanProgress = 100;
      scanJustCompleted = true;
      return;
    }
  }

  if (scanInProgress) {
    int pct = ((currentChannel - 1) * 100 + ((now - lastChannelHop) * 100) / 250) / 14;
    scanProgress = constrain(pct, 0, 99);
  }
}

bool scanner_isComplete() {
  return !scanInProgress;
}

bool scanner_justCompleted() {
  if (scanJustCompleted) {
    scanJustCompleted = false;
    return true;
  }
  return false;
}

ScanType scanner_getScanType() {
  return currentScanType;
}

uint8_t scanner_getCurrentChannel() {
  return currentChannel;
}

int scanner_getProgress() {
  return scanProgress;
}

void scanner_startScan() {
  scanner_start(SCAN_TYPE_APS);
}

void scanner_startSTScan() {
  scanner_start(SCAN_TYPE_STS);
}

bool scanner_checkComplete() {
  return !scanInProgress;
}

void scanner_sortByRSSI() {
  for (int i = 1; i < scannedAPCount; i++) {
    APInfo key = scannedAPs[i];
    int j = i - 1;
    while (j >= 0 && scannedAPs[j].rssi < key.rssi) {
      scannedAPs[j + 1] = scannedAPs[j];
      j--;
    }
    scannedAPs[j + 1] = key;
  }
}

void scanner_toggleSelect(int idx) {
  if (idx >= 0 && idx < scannedAPCount) {
    scannedAPs[idx].selected = !scannedAPs[idx].selected;
  }
}

void scanner_clearSelections() {
  for (int i = 0; i < scannedAPCount; i++) {
    scannedAPs[i].selected = false;
  }
}

int scanner_countSelected() {
  int count = 0;
  for (int i = 0; i < scannedAPCount; i++) {
    if (scannedAPs[i].selected) count++;
  }
  return count;
}

void scanner_formatBSSID(const uint8_t* bssid, char* buf, int bufLen) {
  snprintf(buf, bufLen, "%02X:%02X:%02X:%02X:%02X:%02X",
           bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}

void scanner_toggleSelectST(int idx) {
  if (idx >= 0 && idx < scannedSTCount) {
    scannedSTs[idx].selected = !scannedSTs[idx].selected;
  }
}

int scanner_countSelectedSTs() {
  int count = 0;
  for (int i = 0; i < scannedSTCount; i++) {
    if (scannedSTs[i].selected) count++;
  }
  return count;
}
