/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "pktmon.h"
#include "sdcard.h"
#include <WiFi.h>
#include "esp_wifi.h"

bool pktmonRunning = false;
uint8_t pktmonChannel = 1;

volatile uint32_t pktmon_totalCount = 0;
volatile uint32_t pktmon_beaconCount = 0;
volatile uint32_t pktmon_dataCount = 0;
volatile uint32_t pktmon_deauthCount = 0;
volatile uint32_t pktmon_probeCount = 0;
volatile uint32_t pktmon_mgmtCount = 0;

static uint16_t graphData[PKTMON_GRAPH_W];
static int graphWriteIdx = 0;
static uint16_t graphMax = 1;

static uint32_t dispTotal = 0;
static uint32_t dispBeacon = 0;
static uint32_t dispData = 0;
static uint32_t dispDeauth = 0;

static unsigned long lastSampleTime = 0;
static uint32_t totalAllTime = 0;
static uint32_t deauthAllTime = 0;

static void IRAM_ATTR promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!buf) return;

  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* frame = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;
  if (len < 2) return;

  pktmon_totalCount++;

  // 802.11 Frame Control type (bits 2-3) and subtype (bits 4-7)
  uint8_t frameType = (frame[0] >> 2) & 0x03;
  uint8_t frameSubtype = (frame[0] >> 4) & 0x0F;

  switch (frameType) {
    case 0:
      pktmon_mgmtCount++;
      switch (frameSubtype) {
        case 0x08:
          pktmon_beaconCount++;
          break;
        case 0x04:
        case 0x05:
          pktmon_probeCount++;
          break;
        case 0x0C:
        case 0x0A:
          pktmon_deauthCount++;
          break;
      }
      break;
    case 1:
      break;
    case 2:
      pktmon_dataCount++;
      break;
  }
}

void pktmon_start(uint8_t channel) {
  memset(graphData, 0, sizeof(graphData));
  graphWriteIdx = 0;
  graphMax = 1;
  pktmon_totalCount = 0;
  pktmon_beaconCount = 0;
  pktmon_dataCount = 0;
  pktmon_deauthCount = 0;
  pktmon_probeCount = 0;
  pktmon_mgmtCount = 0;
  totalAllTime = 0;
  deauthAllTime = 0;
  lastSampleTime = millis();

  pktmonChannel = channel;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  wifi_promiscuous_filter_t filter;
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                       WIFI_PROMIS_FILTER_MASK_DATA |
                       WIFI_PROMIS_FILTER_MASK_CTRL;
  esp_wifi_set_promiscuous_filter(&filter);

  esp_wifi_set_promiscuous_rx_cb(promiscuousCallback);
  esp_wifi_set_promiscuous(true);

  pktmonRunning = true;
  Serial.printf("[PKTMON] Started on channel %d\n", channel);
}

void pktmon_stop() {
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  pktmonRunning = false;
  Serial.println("[PKTMON] Stopped");
}

void pktmon_setChannel(uint8_t ch) {
  if (ch < 1) ch = 1;
  if (ch > 14) ch = 14;
  pktmonChannel = ch;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void pktmon_update() {
  if (!pktmonRunning) return;

  unsigned long now = millis();
  if (now - lastSampleTime < PKTMON_SAMPLE_MS) return;
  lastSampleTime = now;

  uint32_t total = pktmon_totalCount;
  uint32_t beacon = pktmon_beaconCount;
  uint32_t data = pktmon_dataCount;
  uint32_t deauth = pktmon_deauthCount;

  pktmon_totalCount = 0;
  pktmon_beaconCount = 0;
  pktmon_dataCount = 0;
  pktmon_deauthCount = 0;
  pktmon_probeCount = 0;
  pktmon_mgmtCount = 0;

  float scale = 1000.0f / PKTMON_SAMPLE_MS;
  dispTotal = (uint32_t)(total * scale);
  dispBeacon = (uint32_t)(beacon * scale);
  dispData = (uint32_t)(data * scale);
  dispDeauth = (uint32_t)(deauth * scale);

  totalAllTime += total;
  deauthAllTime += deauth;

  graphData[graphWriteIdx] = total;
  graphWriteIdx = (graphWriteIdx + 1) % PKTMON_GRAPH_W;

  graphMax = 1;
  for (int i = 0; i < PKTMON_GRAPH_W; i++) {
    if (graphData[i] > graphMax) graphMax = graphData[i];
  }
}

void pktmon_draw(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_7x14_tr);

  char leftStr[16];
  snprintf(leftStr, 16, "CH[%d]", pktmonChannel);
  char rightStr[16];
  snprintf(rightStr, 16, "PKT[%lu/s]", (unsigned long)dispTotal);

  u8g2.drawStr(0, 14, leftStr);
  int rw = u8g2.getStrWidth(rightStr);
  u8g2.drawStr(128 - rw, 14, rightStr);

  int graphStartY = 16;
  int graphH = 48;

  for (int x = 0; x < PKTMON_GRAPH_W; x++) {
    int idx = (graphWriteIdx + x) % PKTMON_GRAPH_W;
    uint16_t val = graphData[idx];

    int barH = 0;
    if (graphMax > 0) {
      barH = (val * graphH) / graphMax;
      if (barH > graphH) barH = graphH;
    }

    if (barH > 0) {
      u8g2.drawVLine(x, graphStartY + graphH - barH, barH);
    }
  }
}

void pktmon_logSession() {
  String entry = sdcard_getTimestamp() + " PKTMON ch=" + String(pktmonChannel) +
                 " total=" + String(totalAllTime) +
                 " deauth=" + String(deauthAllTime);
  sdcard_appendLog(PKTMON_LOG, entry);
}
