/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "recon.h"
#include "oui.h"
#include <WiFi.h>
#include "esp_wifi.h"

namespace recon {

static ReconDevice devices[MAX_RECON_DEVICES];
static ReconAlert  alerts[MAX_RECON_ALERTS];
static uint16_t deviceCount   = 0;
static uint8_t  alertCount    = 0;
static ReconMode mode         = RECON_OFF;
static uint32_t totalPackets  = 0;
static uint32_t deauthCount   = 0;
static uint32_t deauthWindowStart = 0;
static uint8_t  currentChannel = 1;
static uint32_t scanStartTime = 0;
static uint32_t scanDuration  = 0;
static uint32_t lastChannelHop = 0;
static uint32_t lastOutput     = 0;
static uint16_t channelPkts[RECON_CHANNEL_COUNT];
static uint8_t  deauthTargetMAC[6];

// Hidden SSID tracking
static uint8_t  hiddenSSIDCount = 0;
static char     hiddenSSIDs[MAX_HIDDEN_SSIDS][25];

// SSID-BSSID map for rogue AP detection
struct SSIDBSSIDPair {
    char ssid[25];
    uint8_t bssid[6];
    EncryptionType enc;
    int8_t rssi;
};
static SSIDBSSIDPair ssidMap[8];
static uint8_t ssidMapCount = 0;

// Summary counters
static uint8_t summaryRouters  = 0;
static uint8_t summaryCameras  = 0;
static uint8_t summaryPhones   = 0;
static uint8_t summaryPrinters = 0;
static uint8_t summaryOpen     = 0;
static uint8_t summaryEasyHack = 0;

static int  findDevice(const uint8_t* mac);
static int  addDevice(const uint8_t* mac);
static void fingerprint(int idx);
static void categorizeDevice(int idx);
static void evaluateDeviceAlerts(int idx);
static void calculateRiskScore(int idx);
static void parseBeacon(const uint8_t* frame, uint16_t len, int8_t rssi);
static void parseProbeRequest(const uint8_t* frame, uint16_t len, int8_t rssi);
static void parseDataFrame(const uint8_t* frame, uint16_t len, int8_t rssi);
static void parseDeauth(const uint8_t* frame, uint16_t len);
static void checkRogueAP(const char* ssid, const uint8_t* bssid, EncryptionType enc, int8_t rssi);
static void checkDeauthRate();
static void checkHiddenSSID(const char* ssid);
static void channelHop();
static bool isRandomMAC(const uint8_t* mac);
static void addAlert(AlertType type, const uint8_t* mac, const char* detail);

static EncryptionType mapAuth(uint8_t authByte);

static void IRAM_ATTR snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!buf || mode == RECON_OFF) return;

    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const uint8_t* frame = pkt->payload;
    int len = pkt->rx_ctrl.sig_len;
    int8_t rssi = pkt->rx_ctrl.rssi;
    if (len < 24) return;

    totalPackets++;

    // Track channel utilization
    if (currentChannel >= 1 && currentChannel <= 14) {
        channelPkts[currentChannel - 1]++;
    }

    uint8_t frameType    = (frame[0] >> 2) & 0x03;
    uint8_t frameSubtype = (frame[0] >> 4) & 0x0F;

    if (frameType == 0) {
        switch (frameSubtype) {
            case 0x08: parseBeacon(frame, len, rssi); break;
            case 0x05: parseBeacon(frame, len, rssi); break;
            case 0x04: parseProbeRequest(frame, len, rssi); break;
            case 0x0C: parseDeauth(frame, len); break;
            case 0x0A: parseDeauth(frame, len); break;
        }
    } else if (frameType == 2) {
        parseDataFrame(frame, len, rssi);
    }
}

static void startCommon(ReconMode m, uint32_t dur) {
    stop();
    mode          = m;
    scanStartTime = millis();
    scanDuration  = dur;
    totalPackets  = 0;
    deviceCount   = 0;
    alertCount    = 0;
    hiddenSSIDCount = 0;
    ssidMapCount  = 0;
    currentChannel = 1;
    lastChannelHop = millis();
    lastOutput     = millis();
    deauthCount    = 0;
    deauthWindowStart = millis();

    summaryRouters = 0;
    summaryCameras = 0;
    summaryPhones  = 0;
    summaryPrinters = 0;
    summaryOpen    = 0;
    summaryEasyHack = 0;

    memset(devices, 0, sizeof(devices));
    memset(alerts, 0, sizeof(alerts));
    memset(channelPkts, 0, sizeof(channelPkts));
    memset(hiddenSSIDs, 0, sizeof(hiddenSSIDs));
    memset(ssidMap, 0, sizeof(ssidMap));
    memset(deauthTargetMAC, 0, 6);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

    wifi_promiscuous_filter_t filter;
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                         WIFI_PROMIS_FILTER_MASK_DATA |
                         WIFI_PROMIS_FILTER_MASK_CTRL;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(snifferCallback);
    esp_wifi_set_promiscuous(true);
}

void init() {
    mode = RECON_OFF;
    deviceCount = 0;
    alertCount = 0;
    totalPackets = 0;
}

void startScan() {
    startCommon(RECON_SCAN, 10000);
    Serial.println(F("[RECON] SCAN started (10s)"));
}

void startHunt() {
    startCommon(RECON_HUNT, 25000);
    Serial.println(F("[RECON] HUNT started (25s)"));
}

void startLive() {
    if (mode == RECON_LIVE) return;
    startCommon(RECON_LIVE, 0);
    Serial.println(F("[RECON] LIVE mode started"));
}

void stop() {
    if (mode != RECON_OFF) {
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        mode = RECON_OFF;
        Serial.println(F("[RECON] Stopped"));
    }
}

bool isActive()           { return mode != RECON_OFF; }
ReconMode getMode()       { return mode; }
uint16_t getDeviceCount() { return deviceCount; }
uint32_t getTotalPackets(){ return totalPackets; }
uint8_t getAlertCount()   { return alertCount; }
uint8_t getCurrentChannel() { return currentChannel; }
uint32_t getElapsed()     { return (mode != RECON_OFF) ? (millis() - scanStartTime) : 0; }
uint32_t getDuration()    { return scanDuration; }

ReconDevice* getDevice(uint8_t idx) {
    return (idx < deviceCount) ? &devices[idx] : nullptr;
}

ReconAlert* getAlert(uint8_t idx) {
    return (idx < alertCount) ? &alerts[idx] : nullptr;
}

int getDeviceIndexByMac(const uint8_t* mac) {
    for (uint8_t i = 0; i < deviceCount; i++) {
        if (memcmp(devices[i].mac, mac, 6) == 0) return (int)i;
    }
    return -1;
}

uint8_t getSummaryRouters()  { return summaryRouters; }
uint8_t getSummaryCameras()  { return summaryCameras; }
uint8_t getSummaryPhones()   { return summaryPhones; }
uint8_t getSummaryPrinters() { return summaryPrinters; }
uint8_t getSummaryOpen()     { return summaryOpen; }
uint8_t getSummaryEasyHack() { return summaryEasyHack; }

void update() {
    if (mode == RECON_OFF) return;
    uint32_t now = millis();

    // Channel hopping every 250ms
    if (now - lastChannelHop > 250) {
        lastChannelHop = now;
        channelHop();
    }

    // Deauth rate check every second
    if (now - deauthWindowStart > 1000) {
        checkDeauthRate();
        deauthCount = 0;
        deauthWindowStart = now;
    }

    // Check scan completion for timed modes
    if (scanDuration > 0 && (now - scanStartTime > scanDuration)) {
        // Calculate final scores and summaries
        summaryRouters = 0;
        summaryCameras = 0;
        summaryPhones  = 0;
        summaryPrinters = 0;
        summaryOpen    = 0;
        summaryEasyHack = 0;

        for (uint8_t i = 0; i < deviceCount; i++) {
            calculateRiskScore(i);

            if (devices[i].category == CAT_ROUTER)  summaryRouters++;
            else if (devices[i].category == CAT_CAMERA)  summaryCameras++;
            else if (devices[i].category == CAT_PHONE)   summaryPhones++;
            else if (devices[i].category == CAT_PRINTER)  summaryPrinters++;

            bool easyHack = false;
            if (devices[i].isAP && devices[i].encryption == ENC_OPEN) {
                summaryOpen++;
                easyHack = true;
            }
            if (devices[i].wpsEnabled || devices[i].defaultCreds || devices[i].knownCVECount > 0) {
                easyHack = true;
            }
            if (easyHack) summaryEasyHack++;

            yield();
        }

        uint8_t oldMode = mode;
        stop();

        Serial.print(oldMode == RECON_SCAN ? F("[RECON] SCAN complete: ") : F("[RECON] HUNT complete: "));
        Serial.print(deviceCount);
        Serial.print(F(" devices, "));
        Serial.print(totalPackets);
        Serial.println(F(" packets"));
    }

    // Periodic score recalc for live mode
    if (mode == RECON_LIVE && (now - lastOutput > 5000)) {
        lastOutput = now;
        for (uint8_t i = 0; i < deviceCount; i++) {
            calculateRiskScore(i);
        }
    }
}

static void parseBeacon(const uint8_t* frame, uint16_t len, int8_t rssi) {
    if (len < 36) return;

    const uint8_t* bssid = &frame[10]; // BSSID in beacon

    char ssid[33] = {0};
    EncryptionType enc = ENC_OPEN;
    bool wps = false;

    // Capability info at offset 34
    uint16_t capInfo = 0;
    if (len > 35) {
        capInfo = frame[34] | (frame[35] << 8);
    }

    // Parse tagged parameters starting at offset 36
    uint16_t pos = 36;
    while (pos + 2 <= len) {
        uint8_t tagNum = frame[pos];
        uint8_t tagLen = frame[pos + 1];
        if (pos + 2 + tagLen > len) break;

        switch (tagNum) {
            case 0:
                if (tagLen > 0 && tagLen <= 32) {
                    memcpy(ssid, &frame[pos + 2], tagLen);
                    ssid[tagLen] = 0;
                }
                break;
            case 48:
                enc = ENC_WPA2;
                break;
            case 221:
                if (tagLen >= 4) {
                    if (frame[pos+2] == 0x00 && frame[pos+3] == 0x50 &&
                        frame[pos+4] == 0xF2 && frame[pos+5] == 0x01) {
                        if (enc < ENC_WPA) enc = ENC_WPA;
                    }
                    if (frame[pos+2] == 0x00 && frame[pos+3] == 0x50 &&
                        frame[pos+4] == 0xF2 && frame[pos+5] == 0x04) {
                        wps = true;
                    }
                }
                break;
        }
        pos += 2 + tagLen;
    }

    // WEP from capability info
    if (enc == ENC_OPEN && (capInfo & 0x0010)) {
        enc = ENC_WEP;
    }

    int idx = findDevice(bssid);
    if (idx < 0) {
        idx = addDevice(bssid);
        if (idx < 0) return;
    }

    devices[idx].isAP = true;
    devices[idx].rssi = rssi;
    devices[idx].channel = currentChannel;
    devices[idx].encryption = enc;
    devices[idx].wpsEnabled = wps;
    devices[idx].lastSeen = millis();
    devices[idx].packetCount++;
    devices[idx].mgmtPackets++;

    if (ssid[0] != 0) {
        strncpy(devices[idx].ssid, ssid, 24);
        devices[idx].ssid[24] = 0;
    }

    if (!devices[idx].fingerprinted && devices[idx].packetCount >= 3) {
        fingerprint(idx);
    }

    evaluateDeviceAlerts(idx);

    if (ssid[0] != 0) {
        checkRogueAP(ssid, bssid, enc, rssi);
    }
}

static void parseProbeRequest(const uint8_t* frame, uint16_t len, int8_t rssi) {
    if (len < 24) return;

    const uint8_t* srcMAC = &frame[10];

    char ssid[33] = {0};
    uint16_t pos = 24;
    while (pos + 2 <= len) {
        uint8_t tagNum = frame[pos];
        uint8_t tagLen = frame[pos + 1];
        if (pos + 2 + tagLen > len) break;
        if (tagNum == 0 && tagLen > 0 && tagLen <= 32) {
            memcpy(ssid, &frame[pos + 2], tagLen);
            ssid[tagLen] = 0;
        }
        pos += 2 + tagLen;
    }

    int idx = findDevice(srcMAC);
    if (idx < 0) {
        idx = addDevice(srcMAC);
        if (idx < 0) return;
    }

    devices[idx].rssi = rssi;
    devices[idx].channel = currentChannel;
    devices[idx].lastSeen = millis();
    devices[idx].packetCount++;
    devices[idx].mgmtPackets++;
    devices[idx].isRandomMAC = isRandomMAC(srcMAC);

    if (ssid[0] != 0 && devices[idx].probeCount < MAX_PROBE_SSIDS) {
        bool exists = false;
        for (uint8_t i = 0; i < devices[idx].probeCount; i++) {
            if (strcmp(devices[idx].probeSSIDs[i], ssid) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            strncpy(devices[idx].probeSSIDs[devices[idx].probeCount], ssid, 19);
            devices[idx].probeSSIDs[devices[idx].probeCount][19] = 0;
            devices[idx].probeCount++;
            checkHiddenSSID(ssid);
        }
    }

    if (!devices[idx].fingerprinted && devices[idx].packetCount >= 3) {
        fingerprint(idx);
    }

    evaluateDeviceAlerts(idx);
}

static void parseDataFrame(const uint8_t* frame, uint16_t len, int8_t rssi) {
    if (len < 24) return;

    const uint8_t* macFrom = &frame[10];

    // Skip broadcast/multicast
    if ((macFrom[0] & 0x01) || macFrom[0] == 0xFF) return;

    int idx = findDevice(macFrom);
    if (idx < 0) {
        idx = addDevice(macFrom);
        if (idx < 0) return;
    }

    devices[idx].rssi = rssi;
    devices[idx].channel = currentChannel;
    devices[idx].lastSeen = millis();
    devices[idx].packetCount++;
    devices[idx].dataPackets++;
    devices[idx].isRandomMAC = isRandomMAC(macFrom);

    // Detect unencrypted data (Protected Frame bit in FC byte 1)
    if ((frame[1] & 0x40) == 0) {
        devices[idx].httpDetected = true;
    }

    if (!devices[idx].fingerprinted && devices[idx].packetCount >= 3) {
        fingerprint(idx);
    }

    evaluateDeviceAlerts(idx);
}

static void parseDeauth(const uint8_t* frame, uint16_t len) {
    deauthCount++;
    if (len >= 10) {
        memcpy(deauthTargetMAC, &frame[4], 6);
    }
}

static int findDevice(const uint8_t* mac) {
    for (uint8_t i = 0; i < deviceCount; i++) {
        if (memcmp(devices[i].mac, mac, 6) == 0) return i;
    }
    return -1;
}

static int addDevice(const uint8_t* mac) {
    if (deviceCount >= MAX_RECON_DEVICES) {
        // Replace weakest old device
        int weakest = 0;
        int8_t weakestRSSI = 0;
        for (uint8_t i = 0; i < deviceCount; i++) {
            if (devices[i].rssi < weakestRSSI) {
                weakestRSSI = devices[i].rssi;
                weakest = i;
            }
        }
        if (millis() - devices[weakest].lastSeen > 10000) {
            memset(&devices[weakest], 0, sizeof(ReconDevice));
            memcpy(devices[weakest].mac, mac, 6);
            devices[weakest].firstSeen = millis();
            devices[weakest].lastSeen  = millis();
            return weakest;
        }
        return -1;
    }

    int idx = deviceCount;
    memset(&devices[idx], 0, sizeof(ReconDevice));
    deviceCount++;
    memcpy(devices[idx].mac, mac, 6);
    devices[idx].firstSeen = millis();
    devices[idx].lastSeen  = millis();
    return idx;
}

static void fingerprint(int idx) {
    if (idx < 0 || idx >= deviceCount) return;
    devices[idx].fingerprinted = true;
    devices[idx].isRandomMAC = isRandomMAC(devices[idx].mac);

    // OUI vendor lookup using existing oui module
    char vendorBuf[24];
    oui_lookup(devices[idx].mac, vendorBuf, 24);
    if (vendorBuf[0] != 0) {
        strncpy(devices[idx].vendor, vendorBuf, 15);
        devices[idx].vendor[15] = 0;
    }

    categorizeDevice(idx);
}

static void categorizeDevice(int idx) {
    if (devices[idx].category != CAT_UNKNOWN) return;

    if (devices[idx].isAP) {
        devices[idx].category = CAT_ROUTER;
        return;
    }

    char v[20];
    strncpy(v, devices[idx].vendor, sizeof(v) - 1);
    v[sizeof(v) - 1] = 0;
    for (char* p = v; *p; p++) *p = tolower(*p);

    if (strstr(v, "apple") || strstr(v, "samsung") ||
        strstr(v, "huawei") || strstr(v, "xiaomi") ||
        strstr(v, "oneplus") || strstr(v, "google") ||
        strstr(v, "oppo") || strstr(v, "vivo") ||
        strstr(v, "motorola")) {
        devices[idx].category = CAT_PHONE;
    }
    else if (strstr(v, "intel") || strstr(v, "dell") ||
             strstr(v, "lenovo") || strstr(v, "hp ") ||
             strstr(v, "microsoft") || strstr(v, "acer") ||
             strstr(v, "asus")) {
        devices[idx].category = CAT_LAPTOP;
    }
    else if (strstr(v, "brother") || strstr(v, "canon") ||
             strstr(v, "epson") || strstr(v, "xerox") ||
             strstr(v, "ricoh") || strstr(v, "lexmark")) {
        devices[idx].category = CAT_PRINTER;
    }
    else if (strstr(v, "hikvision") || strstr(v, "dahua") ||
             strstr(v, "axis") || strstr(v, "ring") ||
             strstr(v, "wyze") || strstr(v, "amcrest") ||
             strstr(v, "reolink")) {
        devices[idx].category = CAT_CAMERA;
    }
    else if (strstr(v, "roku") || strstr(v, "lg") ||
             strstr(v, "tcl") || strstr(v, "vizio")) {
        devices[idx].category = CAT_TV;
    }
    else if (strstr(v, "amazon") || strstr(v, "echo")) {
        devices[idx].category = CAT_VOICE_AST;
    }
    else if (strstr(v, "sony") || strstr(v, "nintendo") ||
             strstr(v, "xbox") || strstr(v, "valve")) {
        devices[idx].category = CAT_GAMING;
    }
    else if (strstr(v, "tenda") || strstr(v, "cisco") ||
             strstr(v, "netgear") || strstr(v, "linksys") ||
             strstr(v, "d-link") || strstr(v, "ubiquiti") ||
             strstr(v, "tp-link")) {
        devices[idx].category = CAT_ROUTER;
    }
    else if (strstr(v, "espressif") || strstr(v, "tuya") ||
             strstr(v, "shenzhen") || strstr(v, "sonos")) {
        devices[idx].category = CAT_IOT;
    }
    // Leave as CAT_UNKNOWN if no match
}

static void evaluateDeviceAlerts(int idx) {
    if (idx < 0 || idx >= deviceCount) return;
    ReconDevice& d = devices[idx];

    if (d.alerted || d.packetCount < 5) return;
    d.alerted = true;

    char detail[50];

    if (d.wpsEnabled) {
        snprintf(detail, sizeof(detail), "[WPS] %s crackable", d.ssid[0] ? d.ssid : d.vendor);
        addAlert(ALERT_VULN_DEVICE, d.mac, detail);
        return;
    }

    switch (d.category) {
        case CAT_PRINTER:
            if (d.encryption == ENC_OPEN) {
                snprintf(detail, sizeof(detail), "[PRT] OPEN: %s", d.vendor);
            } else {
                snprintf(detail, sizeof(detail), "[PRT] Found: %s", d.vendor);
            }
            addAlert(ALERT_VULN_DEVICE, d.mac, detail);
            break;
        case CAT_CAMERA:
            if (d.httpDetected) {
                snprintf(detail, sizeof(detail), "[CAM] HTTP: %s", d.vendor);
            } else {
                snprintf(detail, sizeof(detail), "[CAM] Found: %s", d.vendor);
            }
            addAlert(ALERT_VULN_DEVICE, d.mac, detail);
            break;
        case CAT_ROUTER:
            if (d.isAP && d.encryption == ENC_OPEN) {
                snprintf(detail, sizeof(detail), "[RTR] OPEN: %s", d.ssid[0] ? d.ssid : d.vendor);
                addAlert(ALERT_VULN_DEVICE, d.mac, detail);
            }
            break;
        default:
            if (d.httpDetected && d.dataPackets > 50) {
                snprintf(detail, sizeof(detail), "[!] CLEARTEXT: %s", d.vendor[0] ? d.vendor : "Device");
                addAlert(ALERT_VULN_DEVICE, d.mac, detail);
            }
            break;
    }
}

static void calculateRiskScore(int idx) {
    if (idx < 0 || idx >= deviceCount) return;

    uint8_t score = 0;

    if (devices[idx].isAP) {
        switch (devices[idx].encryption) {
            case ENC_OPEN:  score += 25; break;
            case ENC_WEP:   score += 20; break;
            case ENC_WPA:   score += 15; break;
            case ENC_WPA2:  score += 5;  break;
            case ENC_WPA3:  score += 0;  break;
            default: break;
        }
    }

    score += min((uint8_t)20, (uint8_t)(devices[idx].knownCVECount * 5));

    if (devices[idx].defaultCreds) score += 15;
    if (devices[idx].wpsEnabled) score += 10;
    if (devices[idx].httpDetected) score += 10;

    if (devices[idx].isRandomMAC && score > 15) score -= 15;
    if (score > 100) score = 100;

    devices[idx].riskScore = score;
}

static void checkRogueAP(const char* ssid, const uint8_t* bssid, EncryptionType enc, int8_t rssi) {
    for (uint8_t i = 0; i < ssidMapCount; i++) {
        if (strcmp(ssidMap[i].ssid, ssid) == 0) {
            if (memcmp(ssidMap[i].bssid, bssid, 6) != 0) {
                if (ssidMap[i].enc != enc) {
                    char detail[50];
                    snprintf(detail, sizeof(detail), "Evil Twin: %.20s", ssid);
                    addAlert(ALERT_EVIL_TWIN, bssid, detail);
                } else if (enc == ENC_OPEN || ssidMap[i].enc == ENC_OPEN) {
                    char detail[50];
                    snprintf(detail, sizeof(detail), "Rogue AP: %.20s", ssid);
                    addAlert(ALERT_ROGUE_AP, bssid, detail);
                } else if (rssi > ssidMap[i].rssi + 10) {
                    char detail[50];
                    snprintf(detail, sizeof(detail), "Rogue AP: %.20s (sig)", ssid);
                    addAlert(ALERT_ROGUE_AP, bssid, detail);
                }
            }
            ssidMap[i].rssi = rssi;
            return;
        }
    }

    if (ssidMapCount < 8) {
        strncpy(ssidMap[ssidMapCount].ssid, ssid, 24);
        ssidMap[ssidMapCount].ssid[24] = 0;
        memcpy(ssidMap[ssidMapCount].bssid, bssid, 6);
        ssidMap[ssidMapCount].enc  = enc;
        ssidMap[ssidMapCount].rssi = rssi;
        ssidMapCount++;
    }
}

static void checkDeauthRate() {
    if (deauthCount > 5) {
        char detail[50];
        snprintf(detail, sizeof(detail), "Deauth flood: %lu/s", (unsigned long)deauthCount);
        addAlert(ALERT_DEAUTH, deauthTargetMAC, detail);
    }
}

static void checkHiddenSSID(const char* ssid) {
    if (!ssid || ssid[0] == 0) return;

    for (uint8_t i = 0; i < ssidMapCount; i++) {
        if (strcmp(ssidMap[i].ssid, ssid) == 0) return;
    }
    for (uint8_t i = 0; i < hiddenSSIDCount; i++) {
        if (strcmp(hiddenSSIDs[i], ssid) == 0) return;
    }

    if (hiddenSSIDCount < MAX_HIDDEN_SSIDS) {
        strncpy(hiddenSSIDs[hiddenSSIDCount], ssid, 24);
        hiddenSSIDs[hiddenSSIDCount][24] = 0;
        hiddenSSIDCount++;

        char detail[50];
        snprintf(detail, sizeof(detail), "Hidden: %.30s", ssid);
        addAlert(ALERT_HIDDEN_NET, nullptr, detail);
    }
}

static bool isRandomMAC(const uint8_t* mac) {
    return (mac[0] & 0x02) != 0;
}

static void addAlert(AlertType type, const uint8_t* mac, const char* detail) {
    for (uint8_t i = 0; i < alertCount; i++) {
        if (alerts[i].type == type && strcmp(alerts[i].detail, detail) == 0) {
            alerts[i].timestamp = millis();
            return;
        }
    }

    if (alertCount >= MAX_RECON_ALERTS) {
        memmove(&alerts[0], &alerts[1], sizeof(ReconAlert) * (MAX_RECON_ALERTS - 1));
        alertCount = MAX_RECON_ALERTS - 1;
    }

    alerts[alertCount].type = type;
    alerts[alertCount].timestamp = millis();
    alerts[alertCount].active = true;
    if (mac) memcpy(alerts[alertCount].mac, mac, 6);
    else memset(alerts[alertCount].mac, 0, 6);
    strncpy(alerts[alertCount].detail, detail, 49);
    alerts[alertCount].detail[49] = 0;
    alertCount++;

    Serial.print(F("[RECON ALERT] "));
    Serial.println(detail);
}

static void channelHop() {
    currentChannel++;
    if (currentChannel > 14) currentChannel = 1;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    delay(1);
    esp_wifi_set_promiscuous(true);
}

String macToStr(const uint8_t* mac) {
    char buf[18];
    snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

const char* encToStr(EncryptionType enc) {
    switch (enc) {
        case ENC_OPEN:  return "Open";
        case ENC_WEP:   return "WEP";
        case ENC_WPA:   return "WPA";
        case ENC_WPA2:  return "WPA2";
        case ENC_WPA3:  return "WPA3";
        default:        return "???";
    }
}

const char* categoryToStr(DeviceCategory cat) {
    switch (cat) {
        case CAT_ROUTER:     return "Router";
        case CAT_PHONE:      return "Phone";
        case CAT_LAPTOP:     return "Laptop";
        case CAT_CAMERA:     return "Camera";
        case CAT_PRINTER:    return "Printer";
        case CAT_TV:         return "TV";
        case CAT_IOT:        return "IoT";
        case CAT_GAMING:     return "Gaming";
        case CAT_BABY_MON:   return "BabyMon";
        case CAT_VOICE_AST:  return "Alexa";
        case CAT_SMART_LOCK: return "Lock";
        case CAT_SMART_PLUG: return "Plug";
        case CAT_THERMOSTAT: return "Thermo";
        case CAT_MEDICAL:    return "Medical";
        case CAT_POS:        return "POS";
        case CAT_ACCESS_CTRL:return "Access";
        case CAT_VEHICLE:    return "Vehicle";
        default:             return "Unknown";
    }
}

const char* riskToStr(uint8_t score) {
    if (score >= 80) return "CRIT";
    if (score >= 60) return "HIGH";
    if (score >= 30) return "MED";
    return "LOW";
}

} // namespace recon
