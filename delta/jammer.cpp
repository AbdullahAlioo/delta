/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "jammer.h"
#include <SPI.h>
#include <RF24.h>
#include "esp_bt.h"
#include "esp_wifi.h"

namespace jammer {

static byte bluetooth_even_channels[] = {
     2,  4,  6,  8, 10, 12, 14, 16, 18, 20,
    22, 24, 26, 28, 30, 32, 34, 36, 38, 40,
    42, 44, 46, 48, 50, 52, 54, 56, 58, 60,
    62, 64, 66, 68, 70, 72, 74, 76, 78, 80
};
static byte bluetooth_odd_channels[] = {
     1,  3,  5,  7,  9, 11, 13, 15, 17, 19,
    21, 23, 25, 27, 29, 31, 33, 35, 37, 39,
    41, 43, 45, 47, 49, 51, 53, 55, 57, 59,
    61, 63, 65, 67, 69, 71, 73, 75, 77, 79
};
static byte wifi_channels[] = {
     6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
    22, 24, 26, 28,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
    46, 48, 50, 52,
    55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68
};
static byte ble_channels[]  = {1, 2, 3, 25, 26, 27, 79, 80, 81};
static byte full_channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                        21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
                        38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50};

static const int num_bluetooth_even = sizeof(bluetooth_even_channels) / sizeof(bluetooth_even_channels[0]);
static const int num_bluetooth_odd  = sizeof(bluetooth_odd_channels)  / sizeof(bluetooth_odd_channels[0]);
static const int num_wifi           = sizeof(wifi_channels)           / sizeof(wifi_channels[0]);
static const int num_ble            = sizeof(ble_channels)            / sizeof(ble_channels[0]);
static const int num_full           = sizeof(full_channels)           / sizeof(full_channels[0]);

static RF24 radio(NRF_CE, NRF_CSN, 16000000);

static char mode = 'r';
static volatile bool jammersActive = false;
static bool nrfReady = false;
static volatile uint32_t hopCount = 0;
static uint32_t startTime = 0;
static volatile uint8_t lastChannel = 0;

static TaskHandle_t jammerTaskHandle = NULL;

static void configureRadio(RF24& r, byte channel) {
  r.setAutoAck(false);
  r.stopListening();
  r.setRetries(0, 0);
  r.setPALevel(RF24_PA_MAX, true);
  r.setDataRate(RF24_2MBPS);
  r.setCRCLength(RF24_CRC_DISABLED);
  r.startConstCarrier(RF24_PA_MAX, channel);
}

static void runCurrentMode() {
  byte newCh;
  switch (mode) {
    case 'r':
      if (random(2)) newCh = bluetooth_odd_channels[random(num_bluetooth_odd)];
      else           newCh = bluetooth_even_channels[random(num_bluetooth_even)];
      break;
    case 'w':  newCh = wifi_channels[random(num_wifi)];      break;
    case 'b':  newCh = ble_channels[random(num_ble)];        break;
    case 'f':  newCh = full_channels[random(num_full)];      break;
    default:   return;
  }
  if (newCh != radio.getChannel()) radio.setChannel(newCh);
  lastChannel = newCh;
  delayMicroseconds(random(60));
}

// FreeRTOS task on Core 0 for dedicated channel hopping
static void jammerTask(void* param) {
  for (;;) {
    if (jammersActive) {
      runCurrentMode();
      hopCount++;
    } else {
      vTaskDelay(1);
    }
  }
}

void init() {
  jammersActive = false;
  nrfReady = false;
  hopCount = 0;
}

void start() {
  if (jammersActive) return;

  // Deselect SD card to release shared MISO line
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  esp_bt_controller_deinit();
  if (esp_wifi_stop() == ESP_OK) esp_wifi_deinit();

  SPI.begin(18, 19, 23);

  if (!radio.begin(&SPI)) {
    nrfReady = false;
    Serial.println(F("[JAM] NRF24L01 FAILED — check wiring!"));
    return;
  }

  nrfReady = true;
  Serial.println(F("[JAM] NRF24L01 OK"));

  configureRadio(radio, 45);
  mode = 'r';
  hopCount = 0;
  startTime = millis();
  jammersActive = true;

  // Disable Core 0 watchdog for uninterrupted tight loop
  disableCore0WDT();

  if (jammerTaskHandle == NULL) {
    xTaskCreatePinnedToCore(
      jammerTask,
      "jammer",
      2048,
      NULL,
      1,
      &jammerTaskHandle,
      0
    );
  }

  Serial.println(F("[JAM] JAMMING ACTIVE (Core 0)"));
}

void stop() {
  if (jammersActive || jammerTaskHandle != NULL) {
    jammersActive = false;

    vTaskDelay(pdMS_TO_TICKS(10));

    if (jammerTaskHandle != NULL) {
      vTaskDelete(jammerTaskHandle);
      jammerTaskHandle = NULL;
    }

    radio.stopConstCarrier();
    radio.powerDown();

    Serial.printf("[JAM] Stopped after %lu hops\n", hopCount);
  }
}

bool isRunning() { return jammersActive; }
uint32_t getHopCount() { return hopCount; }

void update() {
}

void draw(U8G2& u8g2) {
  u8g2.setFont(u8g2_font_7x14_tr);

  if (!nrfReady) {
    u8g2.drawStr(0, 12, "NRF24 NOT FOUND");
    u8g2.drawStr(0, 28, "CHECK WIRING");
    return;
  }

  u8g2.drawStr(0, 12, "2.4GHz JAMMER");

  if (jammersActive) {
    int dots = (millis() / 300) % 4;
    char active[16] = "ACTIVE";
    for (int i = 0; i < dots; i++) strcat(active, ".");
    u8g2.drawStr(0, 26, active);
  } else {
    u8g2.drawStr(0, 26, "STOPPED");
  }

  char line[32];
  snprintf(line, 32, "HOPS:%lu", hopCount);
  u8g2.drawStr(0, 40, line);

  if (jammersActive) {
    uint32_t elapsed = (millis() - startTime) / 1000;
    uint32_t mins = elapsed / 60;
    uint32_t secs = elapsed % 60;
    snprintf(line, 32, "TIME:%02lu:%02lu CH:%d", mins, secs, lastChannel);
    u8g2.drawStr(0, 54, line);
  }

  int barY = 58;
  int barH = 6;
  u8g2.drawFrame(0, barY, 128, barH);

  if (jammersActive) {
    int pos = ((int)lastChannel * 124) / 80;
    if (pos < 0) pos = 0;
    if (pos > 124) pos = 124;
    u8g2.drawBox(pos + 2, barY + 1, 3, barH - 2);

    for (int i = 0; i < 8; i++) {
      int rch;
      if (random(2)) rch = bluetooth_odd_channels[random(num_bluetooth_odd)];
      else           rch = bluetooth_even_channels[random(num_bluetooth_even)];
      int rx = ((int)rch * 124) / 80 + 2;
      if (rx >= 1 && rx < 127) {
        u8g2.drawPixel(rx, barY + 2);
        u8g2.drawPixel(rx, barY + 3);
      }
    }
  }
}

} // namespace jammer
