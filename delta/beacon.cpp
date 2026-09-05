/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "beacon.h"
#include "sdcard.h"
#include <WiFi.h>
#include "esp_wifi.h"

bool beaconRunning = false;
uint32_t beaconPacketsSent = 0;
int beaconSSIDCount = 0;
BeaconMode beaconCurrentMode = BEACON_RANDOM;

static String customSSIDs[MAX_SSIDS_LIST];
static int customSSIDCount = 0;

static char randomSSIDs[MAX_SSIDS_LIST][MAX_SSID_LEN];
static int randomSSIDCount = 0;

static int currentSSIDIndex = 0;
static unsigned long lastBeaconTime = 0;

static uint8_t fakeMACs[MAX_SSIDS_LIST][6];
static uint8_t beaconFrame[128];

static void generateRandomMAC(uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    mac[i] = random(256);
  }
  // Set locally administered bit (bit 1), clear multicast bit (bit 0)
  mac[0] = (mac[0] | 0x02) & 0xFE;
}

static void generateRandomSSID(char* ssid, int maxLen) {
  int len = random(6, 16);
  if (len >= maxLen) len = maxLen - 1;
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (int i = 0; i < len; i++) {
    ssid[i] = charset[random(sizeof(charset) - 1)];
  }
  ssid[len] = '\0';
}

static int buildBeaconFrame(const char* ssid, const uint8_t* srcMAC, uint8_t channel) {
  int ssidLen = strlen(ssid);
  if (ssidLen > 32) ssidLen = 32;

  int frameLen = 0;

  memcpy(beaconFrame, beaconFrameHeader, 36);
  memcpy(beaconFrame + 10, srcMAC, 6);
  memcpy(beaconFrame + 16, srcMAC, 6);
  frameLen = 36;

  beaconFrame[frameLen++] = 0x00;
  beaconFrame[frameLen++] = ssidLen;
  memcpy(beaconFrame + frameLen, ssid, ssidLen);
  frameLen += ssidLen;

  memcpy(beaconFrame + frameLen, beaconTaggedParams, sizeof(beaconTaggedParams));
  // DS Parameter channel offset within tagged params
  beaconFrame[frameLen + 12] = channel;
  frameLen += sizeof(beaconTaggedParams);

  return frameLen;
}

int beacon_loadSSIDs() {
  customSSIDCount = sdcard_readLines(SSID_LIST_FILE, customSSIDs, MAX_SSIDS_LIST);
  Serial.printf("[BEACON] Loaded %d custom SSIDs from SD\n", customSSIDCount);
  return customSSIDCount;
}

void beacon_start(BeaconMode mode) {
  beaconCurrentMode = mode;
  beaconPacketsSent = 0;
  currentSSIDIndex = 0;
  lastBeaconTime = 0;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);
  esp_wifi_set_promiscuous(true);

  switch (mode) {
    case BEACON_RANDOM:
      randomSSIDCount = MAX_SSIDS_LIST;
      for (int i = 0; i < randomSSIDCount; i++) {
        generateRandomSSID(randomSSIDs[i], MAX_SSID_LEN);
        generateRandomMAC(fakeMACs[i]);
      }
      beaconSSIDCount = randomSSIDCount;
      break;

    case BEACON_CUSTOM_LIST:
      if (customSSIDCount == 0) {
        beacon_loadSSIDs();
      }
      if (customSSIDCount == 0) {
        Serial.println("[BEACON] No custom SSIDs, falling back to random");
        beaconCurrentMode = BEACON_RANDOM;
        randomSSIDCount = 20;
        for (int i = 0; i < randomSSIDCount; i++) {
          generateRandomSSID(randomSSIDs[i], MAX_SSID_LEN);
          generateRandomMAC(fakeMACs[i]);
        }
        beaconSSIDCount = randomSSIDCount;
      } else {
        for (int i = 0; i < customSSIDCount; i++) {
          generateRandomMAC(fakeMACs[i]);
        }
        beaconSSIDCount = customSSIDCount;
      }
      break;

    case BEACON_FUNNY:
      beaconSSIDCount = funnySSIDCount;
      for (int i = 0; i < funnySSIDCount; i++) {
        generateRandomMAC(fakeMACs[i]);
      }
      break;
  }

  beaconRunning = true;
  sdcard_appendLog(BEACON_LOG, sdcard_getTimestamp() + " BEACON_START mode=" + String(mode) + " count=" + String(beaconSSIDCount));
  Serial.printf("[BEACON] Started with %d SSIDs, mode=%d\n", beaconSSIDCount, mode);
}

void beacon_stop() {
  beaconRunning = false;
  esp_wifi_set_promiscuous(false);

  sdcard_appendLog(BEACON_LOG, sdcard_getTimestamp() + " BEACON_STOP pkts=" + String(beaconPacketsSent));
  Serial.printf("[BEACON] Stopped. Packets: %u\n", beaconPacketsSent);
}

void beacon_sendNext() {
  if (!beaconRunning || beaconSSIDCount == 0) return;

  unsigned long now = millis();
  if (now - lastBeaconTime < BEACON_INTERVAL) return;
  lastBeaconTime = now;

  const char* ssid = nullptr;

  switch (beaconCurrentMode) {
    case BEACON_RANDOM:
      ssid = randomSSIDs[currentSSIDIndex];
      break;
    case BEACON_CUSTOM_LIST:
      ssid = customSSIDs[currentSSIDIndex].c_str();
      break;
    case BEACON_FUNNY:
      ssid = funnySSIDs[currentSSIDIndex];
      break;
  }

  if (!ssid) return;

  uint8_t channel = (currentSSIDIndex % 11) + 1;
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  int frameLen = buildBeaconFrame(ssid, fakeMACs[currentSSIDIndex], channel);
  esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, beaconFrame, frameLen, false);
  if (err == ESP_OK) {
    beaconPacketsSent++;
  }

  currentSSIDIndex = (currentSSIDIndex + 1) % beaconSSIDCount;
}

const char* beacon_getCurrentSSID() {
  if (beaconSSIDCount == 0) return "None";

  switch (beaconCurrentMode) {
    case BEACON_RANDOM:
      return randomSSIDs[currentSSIDIndex];
    case BEACON_CUSTOM_LIST:
      return customSSIDs[currentSSIDIndex].c_str();
    case BEACON_FUNNY:
      return funnySSIDs[currentSSIDIndex];
  }
  return "None";
}
