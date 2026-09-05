/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Buttons (active LOW, INPUT_PULLUP)
#define BTN_SELECT  13
#define BTN_DOWN    14
#define BTN_UP      26
#define BTN_LEFT    32
#define BTN_RIGHT   33

// I2C OLED (SH1106 128x64)
#define I2C_SDA     21
#define I2C_SCL     22
#define OLED_ADDR   0x3C

// SD Card & NRF24L01 (shared SPI bus)
#define SD_SCK      18
#define SD_MISO     19
#define SD_MOSI     23
#define SD_CS       16
#define NRF_CE      17
#define NRF_CSN     5

#ifndef IR_RX_PIN
#define IR_RX_PIN   34
#endif
#ifndef IR_TX_PIN
#define IR_TX_PIN   25
#endif

#define SCREEN_W    128
#define SCREEN_H    64

#define DEBOUNCE_MS        40
#define SCAN_TIMEOUT_MS    10000
#define CONNECT_TIMEOUT    20000
#define PORT_SCAN_TIMEOUT  500
#define DEAUTH_BURST_DELAY 1
#define DEAUTH_PKTS_BURST  25
#define BEACON_INTERVAL    10
#define PKTMON_SAMPLE_MS   100

#define MAX_APS            32
#define MAX_SSID_LEN       33
#define MAX_SSIDS_LIST     50
#define MAX_OUI_ENTRIES    2000
#define PKTMON_GRAPH_W     128
#define MAX_STS            32
#define PKTMON_GRAPH_H     42
#define MAX_FUNNY_SSIDS    30

#define MAX_RECON_DEVICES   32
#define MAX_RECON_ALERTS    16
#define MAX_PROBE_SSIDS     4
#define MAX_HIDDEN_SSIDS    8
#define RECON_CHANNEL_COUNT 14

#define OUI_FILE       "/oui.csv"
#define SSID_LIST_FILE "/ssids.txt"
#define CONFIG_FILE    "/config.txt"
#define LOG_DIR        "/logs"
#define DEAUTH_LOG     "/logs/deauth_log.txt"
#define EVILTWIN_LOG   "/logs/eviltwin_log.txt"
#define PKTMON_LOG     "/logs/pktmon_log.txt"
#define BEACON_LOG     "/logs/beacon_log.txt"

enum AppState {
  STATE_MAIN_MENU,
  STATE_SCAN_MENU,
  STATE_SCANNING,
  STATE_SELECT_MENU,
  STATE_SELECT_APS,
  STATE_SELECT_STS,
  STATE_AP_INFO,
  STATE_ST_INFO,
  STATE_ATTACK_MENU,
  STATE_ATTACK_RUNNING,
  STATE_PACKET_MONITOR,
  STATE_WIFI_CONNECT,
  STATE_PORTS,
  STATE_PORT_RESULTS,
  STATE_KEYBOARD,
  STATE_IR_CLONER,
  STATE_RECON_MENU,
  STATE_RECON_SCANNING,
  STATE_RECON_RESULTS,
  STATE_RECON_DEVICE,
  STATE_RECON_ALERTS,
  STATE_RF_ANALYZER,
  STATE_JAMMER_RUNNING
};

#define LONG_PRESS_MS 600

enum ButtonID {
  BTN_NONE = 0,
  BTN_ID_SELECT,
  BTN_ID_UP,
  BTN_ID_DOWN,
  BTN_ID_LEFT,
  BTN_ID_RIGHT
};

enum EncryptionType {
  ENC_OPEN,
  ENC_WEP,
  ENC_WPA,
  ENC_WPA2,
  ENC_WPA3,
  ENC_UNKNOWN
};

enum BeaconMode {
  BEACON_RANDOM,
  BEACON_CUSTOM_LIST,
  BEACON_FUNNY
};

enum ReconMode : uint8_t {
  RECON_OFF = 0,
  RECON_SCAN,
  RECON_HUNT,
  RECON_LIVE
};

enum DeviceCategory : uint8_t {
  CAT_UNKNOWN = 0,
  CAT_ROUTER,
  CAT_PHONE,
  CAT_LAPTOP,
  CAT_CAMERA,
  CAT_PRINTER,
  CAT_TV,
  CAT_IOT,
  CAT_GAMING,
  CAT_BABY_MON,
  CAT_VOICE_AST,
  CAT_SMART_LOCK,
  CAT_SMART_PLUG,
  CAT_THERMOSTAT,
  CAT_MEDICAL,
  CAT_POS,
  CAT_ACCESS_CTRL,
  CAT_VEHICLE,
  CAT_COUNT
};

enum AlertType : uint8_t {
  ALERT_ROGUE_AP = 0,
  ALERT_EVIL_TWIN,
  ALERT_DEAUTH,
  ALERT_HIDDEN_NET,
  ALERT_VULN_DEVICE
};

struct ReconDevice {
  uint8_t mac[6];
  char ssid[25];
  char vendor[16];
  char model[16];
  int8_t rssi;
  uint8_t channel;
  EncryptionType encryption;
  DeviceCategory category;
  uint8_t riskScore;
  uint16_t packetCount;
  uint16_t dataPackets;
  uint16_t mgmtPackets;
  uint32_t firstSeen;
  uint32_t lastSeen;
  bool isAP;
  bool wpsEnabled;
  bool isRandomMAC;
  bool httpDetected;
  bool defaultCreds;
  bool fingerprinted;
  bool alerted;
  uint8_t knownCVECount;
  uint8_t openPorts;
  uint8_t probeCount;
  char probeSSIDs[MAX_PROBE_SSIDS][20];
};

struct ReconAlert {
  AlertType type;
  uint8_t mac[6];
  char detail[50];
  uint32_t timestamp;
  bool active;
};

struct APInfo {
  char ssid[MAX_SSID_LEN];
  uint8_t bssid[6];
  uint8_t channel;
  int32_t rssi;
  EncryptionType encryption;
  char vendor[24];
  bool selected;
};

struct STInfo {
  uint8_t mac[6];
  uint8_t apMac[6];
  int32_t rssi;
  char vendor[24];
  bool selected;
};

struct PortResult {
  uint16_t port;
  const char* service;
  bool open;
};

// 802.11 Frame Templates
// Offsets: [4-9]=Dest, [10-15]=Source, [16-21]=BSSID
static uint8_t deauthFrame[26] = {
  0xC0, 0x00,                         // Deauth
  0x3A, 0x01,                         // Duration
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Broadcast destination
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source BSSID (patched at runtime)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID (patched at runtime)
  0x00, 0x00,                         // Sequence number
  0x07, 0x00                          // Reason: Class 3 frame
};

static uint8_t disassocFrame[26] = {
  0xA0, 0x00,                         // Disassoc
  0x3A, 0x01,                         // Duration
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Broadcast destination
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source BSSID
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
  0x00, 0x00,                         // Sequence number
  0x08, 0x00                          // Reason
};

// Minimal 802.11 beacon header (36 bytes)
static const uint8_t beaconFrameHeader[36] = {
  0x80, 0x00,
  0x00, 0x00,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x64, 0x00,
  0x31, 0x04
};

static const uint8_t beaconTaggedParams[] = {
  0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x24, 0x30, 0x48, 0x6C, // Supported Rates
  0x03, 0x01, 0x01,                                           // DS Parameter Set (channel patched at [2])
  0x05, 0x04, 0x00, 0x01, 0x00, 0x00                          // TIM
};

// ESP32 raw frame transmission bypass
extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
  return 0;
}

static const char* funnySSIDs[] = {
  "FBI Surveillance Van",
  "NSA Listening Post",
  "Pretty Fly for a WiFi",
  "Bill Wi the Science Fi",
  "The LAN Before Time",
  "Wu-Tang LAN",
  "Abraham Linksys",
  "Benjamin FrankLAN",
  "Martin Router King",
  "John Wilkes Bluetooth",
  "Drop It Like Its Hotspot",
  "It Hurts When IP",
  "Silence of the LANs",
  "LAN of the Free",
  "The Promised LAN",
  "Nacho WiFi",
  "Get Off My LAN",
  "Hide Yo Kids Hide Yo WiFi",
  "Loading...",
  "Searching...",
  "Virus Infected WiFi",
  "Free Public WiFi",
  "Definitely Not WiFi",
  "404 Network Unavailable",
  "No Free WiFi Here",
  "Yell Password For WiFi",
  "Tell My WiFi Love Her",
  "Router? I Barely Know Her",
  "The Creep Next Door",
  "Winternet Is Coming"
};
static const int funnySSIDCount = 30;

struct PortDef {
  uint16_t port;
  const char* service;
};

static const PortDef commonPorts[] = {
  {21,    "FTP"},
  {22,    "SSH"},
  {23,    "Telnet"},
  {25,    "SMTP"},
  {53,    "DNS"},
  {80,    "HTTP"},
  {110,   "POP3"},
  {143,   "IMAP"},
  {443,   "HTTPS"},
  {445,   "SMB"},
  {993,   "IMAPS"},
  {995,   "POP3S"},
  {3306,  "MySQL"},
  {3389,  "RDP"},
  {5432,  "PgSQL"},
  {5900,  "VNC"},
  {6379,  "Redis"},
  {8080,  "HTTP-Alt"},
  {8443,  "HTTPS-A"},
  {27017, "MongoDB"}
};
static const int commonPortCount = 20;

#endif // CONFIG_H
