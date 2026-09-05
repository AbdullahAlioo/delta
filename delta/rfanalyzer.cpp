/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "rfanalyzer.h"
#include <SPI.h>
#include <RF24.h>

namespace rfanalyzer {

static RF24 radio(NRF_CE, NRF_CSN);
static bool running = false;
static bool nrfAvailable = false;

#define NUM_NRF_CHANNELS 126
static uint8_t spectrum[NUM_NRF_CHANNELS];
static uint8_t peakVals[NUM_NRF_CHANNELS];
static uint8_t sweepCount = 0;

#define SAMPLES_PER_SWEEP 2
static uint8_t sampleBuf[NUM_NRF_CHANNELS];

// Wi-Fi channels 1-14 center frequencies mapped to NRF channel indices
static const uint8_t wifiChannelCenter[] = {
    12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72, 84
};

static uint8_t peakChannel = 0;
static uint8_t maxRSSI = 0;

void init() {
    nrfAvailable = false;
    running = false;
    memset(spectrum, 0, sizeof(spectrum));
    memset(peakVals, 0, sizeof(peakVals));
}

void start() {
    if (running) return;

    // Deselect SD card to prevent MISO line bus contention on shared SPI
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    
    if (radio.begin(&SPI)) {
        if (!radio.isChipConnected()) {
            nrfAvailable = false;
            Serial.println(F("[RF] NRF24L01 not connected!"));
            return;
        }

        nrfAvailable = true;

        radio.setAutoAck(false);
        radio.stopListening();
        radio.setRetries(0, 0);
        radio.setDataRate(RF24_1MBPS);
        radio.setPALevel(RF24_PA_MIN);
        radio.setCRCLength(RF24_CRC_DISABLED);

        radio.openReadingPipe(0, 0xAABBCCDD01LL);
        radio.startListening();

        memset(spectrum, 0, sizeof(spectrum));
        memset(peakVals, 0, sizeof(peakVals));
        sweepCount = 0;

        running = true;
        Serial.println(F("[RF] Analyzer started"));
    } else {
        nrfAvailable = false;
        Serial.println(F("[RF] NRF24L01 not detected!"));
    }
}

void stop() {
    if (running) {
        radio.stopListening();
        radio.powerDown();
        running = false;
        Serial.println(F("[RF] Analyzer stopped"));
    }
}

bool isRunning() { return running; }
uint8_t getPeakChannel() { return peakChannel; }
uint8_t getMaxRSSI() { return maxRSSI; }

void update() {
    if (!running || !nrfAvailable) return;

    maxRSSI = 0;
    uint8_t peakIdx = 0;

    for (uint8_t ch = 0; ch < NUM_NRF_CHANNELS; ch++) {
        radio.setChannel(ch);
        radio.startListening();
        delayMicroseconds(150); // NRF24 settling time

        // Check Received Power Detector (RPD)
        bool rpd = radio.testRPD();
        radio.stopListening();

        if (rpd) {
            spectrum[ch] = 255;
        } else {
            if (spectrum[ch] > 15) {
                spectrum[ch] -= 15;
            } else {
                spectrum[ch] = 0;
            }
        }

        if (spectrum[ch] > peakVals[ch]) {
            peakVals[ch] = spectrum[ch];
        } else if (peakVals[ch] > 0) {
            if (sweepCount % 4 == 0) peakVals[ch]--;
        }

        if (spectrum[ch] > maxRSSI) {
            maxRSSI = spectrum[ch];
            peakIdx = ch;
        }
    }

    peakChannel = 0;
    uint8_t minDist = 255;
    for (uint8_t i = 0; i < 14; i++) {
        uint8_t dist = abs((int)peakIdx - (int)wifiChannelCenter[i]);
        if (dist < minDist) {
            minDist = dist;
            peakChannel = i + 1;
        }
    }

    sweepCount++;
}

void draw(U8G2& u8g2) {
    u8g2.setFont(u8g2_font_7x14_tr);

    char header[32];
    if (!nrfAvailable) {
        snprintf(header, 32, "NRF24 NOT FOUND");
        u8g2.drawStr(0, 14, header);
        return;
    }

    snprintf(header, 32, "RF CH%d PK:%d", peakChannel, maxRSSI);
    u8g2.drawStr(0, 12, header);

    int graphY = 16;
    int graphH = 48;

    for (uint8_t ch = 0; ch < NUM_NRF_CHANNELS && ch < 126; ch++) {
        int x = ch + 1;

        int barH = 0;
        if (maxRSSI > 0) {
            barH = ((int)spectrum[ch] * graphH) / 255;
            if (barH > graphH) barH = graphH;
        }

        if (barH > 0) {
            u8g2.drawVLine(x, graphY + graphH - barH, barH);
        }

        if (peakVals[ch] > 0) {
            int peakY = graphY + graphH - ((int)peakVals[ch] * graphH) / 255;
            if (peakY >= graphY && peakY < graphY + graphH) {
                u8g2.drawPixel(x, peakY);
            }
        }
    }

    u8g2.setFont(u8g2_font_4x6_tr);
    for (uint8_t i = 0; i < 14; i++) {
        int x = wifiChannelCenter[i] + 1;
        if (x >= 1 && x < 127) {
            u8g2.drawPixel(x, graphY + graphH - 1);
            u8g2.drawPixel(x, graphY + graphH - 2);
        }
    }
    u8g2.drawStr(wifiChannelCenter[0] - 1, graphY + graphH, "1");
    u8g2.drawStr(wifiChannelCenter[5] - 1, graphY + graphH, "6");
    u8g2.drawStr(wifiChannelCenter[10] - 3, graphY + graphH, "11");
}

} // namespace rfanalyzer
