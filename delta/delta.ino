/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */

#include <WiFi.h>
#include "esp_wifi.h"
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>

#include "config.h"
#include "ui.h"
#include "keyboard.h"
#include "sdcard.h"
#include "oui.h"
#include "scanner.h"
#include "deauth.h"
#include "beacon.h"
#include "pktmon.h"
#include "eviltwin.h"
#include "portscan.h"
#include "IRSignal.h"
#include "recon.h"
#include "rfanalyzer.h"
#include "jammer.h"

Preferences prefs;

static AppState currentState = STATE_MAIN_MENU;
static AppState keyboardReturnState = STATE_MAIN_MENU;

static int mainMenuIdx = 0;
static int scanMenuIdx = 0;
static int selectMenuIdx = 0;
static int attackMenuIdx = 0;
static int selectedAttackType = 0;
static int selectAPIdx = 0;
static int selectAPScroll = 0;
static int selectSTIdx = 0;
static int selectSTScroll = 0;
static int apInfoOption = 0;
static int apInfoIdx = 0;
static int stInfoIdx = 0;
static int portResultIdx = 0;
static int portResultScroll = 0;

static int irClonerIdx = 0;
static int irClonerScroll = 0;

static int reconMenuIdx = 0;
static int reconResultIdx = 0;
static int reconResultScroll = 0;
static int reconDeviceIdx = 0;
static int reconAlertIdx = 0;
static int reconAlertScroll = 0;

static int connectAPIdx = -1;
static unsigned long connectStartTime = 0;
static bool wifiConnected = false;
static int connectStatus = 0;
static char connectTargetSSID[33] = "";

static uint8_t pktmonCh = 1;

static bool pgSignup = false;

static int scanProgress = 0;
static bool scanDone = false;

static unsigned long selectPressStart = 0;
static bool selectHeld = false;
static bool longPressHandled = false;

struct ButtonState {
  uint8_t pin;
  bool lastState;
  unsigned long lastDebounce;
  bool pressed;
};

static ButtonState buttons[] = {
  {BTN_SELECT, HIGH, 0, false},
  {BTN_UP,     HIGH, 0, false},
  {BTN_DOWN,   HIGH, 0, false},
  {BTN_LEFT,   HIGH, 0, false},
  {BTN_RIGHT,  HIGH, 0, false},
};
static const int buttonCount = 5;

static void buttons_init() {
  for (int i = 0; i < buttonCount; i++) {
    pinMode(buttons[i].pin, INPUT_PULLUP);
    buttons[i].lastState = HIGH;
    buttons[i].lastDebounce = 0;
    buttons[i].pressed = false;
  }
}

static ButtonID buttons_read() {
  unsigned long now = millis();
  for (int i = 0; i < buttonCount; i++) {
    bool reading = digitalRead(buttons[i].pin);

    if (reading != buttons[i].lastState) {
      buttons[i].lastDebounce = now;
      buttons[i].lastState = reading;
    }

    if ((now - buttons[i].lastDebounce) > DEBOUNCE_MS) {
      if (reading == LOW && !buttons[i].pressed) {
        buttons[i].pressed = true;
        switch (i) {
          case 0: return BTN_ID_SELECT;
          case 1: return BTN_ID_UP;
          case 2: return BTN_ID_DOWN;
          case 3: return BTN_ID_LEFT;
          case 4: return BTN_ID_RIGHT;
        }
      } else if (reading == HIGH) {
        buttons[i].pressed = false;
      }
    }
  }
  return BTN_NONE;
}

// Check if SELECT button is currently physically held
static bool isSelectHeld() {
  return digitalRead(BTN_SELECT) == LOW;
}

static void adjustScroll(int& selectedIdx, int& scrollOffset, int totalItems, int visible, ButtonID btn) {
  if (btn == BTN_ID_UP) {
    if (selectedIdx > 0) selectedIdx--;
  } else if (btn == BTN_ID_DOWN) {
    if (selectedIdx < totalItems - 1) selectedIdx++;
  }
  if (selectedIdx < scrollOffset) scrollOffset = selectedIdx;
  if (selectedIdx >= scrollOffset + visible) scrollOffset = selectedIdx - visible + 1;
}

static int getMainMenuCount() {
  return wifiConnected ? 9 : 8;
}


static void handleMainMenu(ButtonID btn) {
  int count = getMainMenuCount();
  if (btn == BTN_ID_UP && mainMenuIdx > 0) mainMenuIdx--;
  if (btn == BTN_ID_DOWN && mainMenuIdx < count - 1) mainMenuIdx++;

  if (btn == BTN_ID_SELECT) {
    switch (mainMenuIdx) {
      case 0:
        scanMenuIdx = 0;
        currentState = STATE_SCAN_MENU;
        break;
      case 1:
        selectMenuIdx = 0;
        currentState = STATE_SELECT_MENU;
        break;
      case 2:
        attackMenuIdx = 0;
        currentState = STATE_ATTACK_MENU;
        break;
      case 3:
        pktmonCh = 1;
        pktmon_start(pktmonCh);
        currentState = STATE_PACKET_MONITOR;
        break;
      case 4:
        irClonerIdx = 0;
        irClonerScroll = 0;
        ir::menuEnter();
        currentState = STATE_IR_CLONER;
        break;
      case 5:
        rfanalyzer::start();
        currentState = STATE_RF_ANALYZER;
        break;
      case 6:
        jammer::start();
        currentState = STATE_JAMMER_RUNNING;
        break;
      default:
        if (wifiConnected && mainMenuIdx == 7) {
          portscan_start(WiFi.gatewayIP());
          currentState = STATE_PORTS;
        } else if (mainMenuIdx == (wifiConnected ? 8 : 7)) {
          currentState = STATE_ABOUT;
        }
        break;
    }
  }
}


static void handleScanMenu(ButtonID btn) {
  if (btn == BTN_ID_UP && scanMenuIdx > 0) scanMenuIdx--;
  if (btn == BTN_ID_DOWN && scanMenuIdx < 4) scanMenuIdx++;

  if (btn == BTN_ID_SELECT) {
    switch (scanMenuIdx) {
      case 0:
        currentState = STATE_MAIN_MENU;
        break;
      case 1:
        scanner_start(SCAN_TYPE_APS);
        currentState = STATE_SCANNING;
        break;
      case 2:
        scanner_start(SCAN_TYPE_STS);
        currentState = STATE_SCANNING;
        break;
      case 3:
        scanner_start(SCAN_TYPE_ALL);
        currentState = STATE_SCANNING;
        break;
      case 4:
        reconMenuIdx = 0;
        currentState = STATE_RECON_MENU;
        break;
    }
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_MAIN_MENU;
  }
}

static void handleScanning(ButtonID btn) {
  if (btn == BTN_ID_LEFT) {
    scanner_stop();
    currentState = STATE_SCAN_MENU;
    return;
  }

  if (scanner_justCompleted() || (scanner_isComplete() && btn == BTN_ID_SELECT)) {
    ScanType t = scanner_getScanType();
    if (t == SCAN_TYPE_STS) {
      selectSTIdx = 0;
      selectSTScroll = 0;
      currentState = STATE_SELECT_STS;
    } else {
      selectAPIdx = 0;
      selectAPScroll = 0;
      currentState = STATE_SELECT_APS;
    }
  }
}


static void handleSelectMenu(ButtonID btn) {
  if (btn == BTN_ID_UP && selectMenuIdx > 0) selectMenuIdx--;
  if (btn == BTN_ID_DOWN && selectMenuIdx < 3) selectMenuIdx++;

  if (btn == BTN_ID_SELECT) {
    switch (selectMenuIdx) {
      case 0:
        currentState = STATE_MAIN_MENU;
        break;
      case 1:
        selectAPIdx = 0;
        selectAPScroll = 0;
        currentState = STATE_SELECT_APS;
        break;
      case 2:
        selectSTIdx = 0;
        selectSTScroll = 0;
        currentState = STATE_SELECT_STS;
        break;
      case 3:
        pgSignup = !pgSignup;
        prefs.begin("delta", false);
        prefs.putBool("pgSignup", pgSignup);
        prefs.end();
        break;
    }
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_MAIN_MENU;
  }
}

static void handleSelectAPs(ButtonID btn) {
  adjustScroll(selectAPIdx, selectAPScroll, (scannedAPCount > 0 ? scannedAPCount : 0) + 1, 5, btn);

  if (scannedAPCount == 0) {
    if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) currentState = STATE_SELECT_MENU;
    return;
  }

  if (btn == BTN_ID_SELECT) {
    if (selectAPIdx == 0) {
      currentState = STATE_SELECT_MENU;
      return;
    }
    selectPressStart = millis();
    selectHeld = true;
    longPressHandled = false;
  }

  if (selectHeld && isSelectHeld()) {
    if (!longPressHandled && (millis() - selectPressStart >= LONG_PRESS_MS)) {
      longPressHandled = true;
      apInfoIdx = selectAPIdx - 1;
      apInfoOption = 0;
      currentState = STATE_AP_INFO;
      selectHeld = false;
    }
  } else if (selectHeld && !isSelectHeld()) {
    if (!longPressHandled) {
      scanner_toggleSelect(selectAPIdx - 1);
    }
    selectHeld = false;
    longPressHandled = false;
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_SELECT_MENU;
  }
}

static void handleSelectSTs(ButtonID btn) {
  adjustScroll(selectSTIdx, selectSTScroll, (scannedSTCount > 0 ? scannedSTCount : 0) + 1, 5, btn);

  if (scannedSTCount == 0) {
    if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) currentState = STATE_SELECT_MENU;
    return;
  }

  if (btn == BTN_ID_SELECT) {
    if (selectSTIdx == 0) {
      currentState = STATE_SELECT_MENU;
      return;
    }
    selectPressStart = millis();
    selectHeld = true;
    longPressHandled = false;
  }

  if (selectHeld && isSelectHeld()) {
    if (!longPressHandled && (millis() - selectPressStart >= LONG_PRESS_MS)) {
      longPressHandled = true;
      stInfoIdx = selectSTIdx - 1;
      currentState = STATE_ST_INFO;
      selectHeld = false;
    }
  } else if (selectHeld && !isSelectHeld()) {
    if (!longPressHandled) {
      scanner_toggleSelectST(selectSTIdx - 1);
    }
    selectHeld = false;
    longPressHandled = false;
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_SELECT_MENU;
  }
}

static void handleAPInfo(ButtonID btn) {
  if ((btn == BTN_ID_UP || btn == BTN_ID_LEFT) && apInfoOption > 0) apInfoOption = 0;
  if ((btn == BTN_ID_DOWN || btn == BTN_ID_RIGHT) && apInfoOption < 1) apInfoOption = 1;

  if (btn == BTN_ID_SELECT) {
    if (apInfoOption == 0) {
      currentState = STATE_SELECT_APS;
    } else {
      connectAPIdx = apInfoIdx;
      strncpy(connectTargetSSID, scannedAPs[apInfoIdx].ssid, 32);
      connectTargetSSID[32] = '\0';
      if (scannedAPs[apInfoIdx].encryption == ENC_OPEN) {
        prefs.begin("delta", false);
        prefs.putString("wifi_ssid", scannedAPs[apInfoIdx].ssid);
        prefs.putString("wifi_pass", "");
        prefs.end();
        WiFi.persistent(false);
        WiFi.disconnect(false);
        delay(100);
        WiFi.mode(WIFI_STA);
        WiFi.begin(scannedAPs[apInfoIdx].ssid);
        connectStartTime = millis();
        connectStatus = 0;
        currentState = STATE_WIFI_CONNECT;
      } else {
        keyboard_init("WiFi Password:", "", 63, true);
        keyboardReturnState = STATE_WIFI_CONNECT;
        currentState = STATE_KEYBOARD;
      }
    }
  }
}

static void handleSTInfo(ButtonID btn) {
  if (btn == BTN_ID_SELECT || btn == BTN_ID_LEFT) {
    currentState = STATE_SELECT_STS;
  }
}


static void handleAttackMenu(ButtonID btn) {
  if (btn == BTN_ID_UP && attackMenuIdx > 0) attackMenuIdx--;
  if (btn == BTN_ID_DOWN && attackMenuIdx < 4) attackMenuIdx++;

  if (btn == BTN_ID_SELECT) {
    if (attackMenuIdx == 0) {
      currentState = STATE_MAIN_MENU;
    } else if (attackMenuIdx < 4) {
      // Selecting attack type (1=DEAUTH, 2=BEACON, 3=EVIL TWIN)
      selectedAttackType = attackMenuIdx - 1;
    } else {
      // START — launch the selected attack
      if (selectedAttackType == 0) {
        if (scanner_countSelected() > 0) {
          deauth_start();
          currentState = STATE_ATTACK_RUNNING;
        }
      } else if (selectedAttackType == 1) {
        beacon_start(BEACON_RANDOM);
        currentState = STATE_ATTACK_RUNNING;
      } else if (selectedAttackType == 2) {
        int tgtIdx = -1;
        for (int i = 0; i < scannedAPCount; i++) {
          if (scannedAPs[i].selected) { tgtIdx = i; break; }
        }
        if (tgtIdx >= 0) {
          APInfo& ap = scannedAPs[tgtIdx];
          eviltwin_start(ap.ssid, ap.channel, ap.bssid, ap.channel);
          currentState = STATE_ATTACK_RUNNING;
        }
      }
    }
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_MAIN_MENU;
  }
}

static void handleAttackRunning(ButtonID btn) {
  if (selectedAttackType == 0 && deauthRunning) {
    deauth_sendBurst();
  } else if (selectedAttackType == 1 && beaconRunning) {
    beacon_sendNext();
  } else if (selectedAttackType == 2) {
    eviltwin_loop();
  }

  if (btn == BTN_ID_SELECT || btn == BTN_ID_LEFT) {
    if (selectedAttackType == 0 && deauthRunning) deauth_stop();
    if (selectedAttackType == 1 && beaconRunning) beacon_stop();
    if (selectedAttackType == 2) eviltwin_stop();
    currentState = STATE_ATTACK_MENU;
  }
}


static void handlePacketMonitor(ButtonID btn) {
  if (pktmonRunning) {
    pktmon_update();
  }

  if (btn == BTN_ID_LEFT) {
    if (pktmonRunning) {
      pktmon_logSession();
      pktmon_stop();
    }
    currentState = STATE_MAIN_MENU;
  }

  if (btn == BTN_ID_RIGHT) {
    pktmonCh++;
    if (pktmonCh > 14) pktmonCh = 1;
    pktmon_setChannel(pktmonCh);
  }

  if (btn == BTN_ID_UP) {
    pktmonCh--;
    if (pktmonCh < 1) pktmonCh = 14;
    pktmon_setChannel(pktmonCh);
  }

  if (btn == BTN_ID_SELECT) {
    if (pktmonRunning) pktmon_stop();
    else pktmon_start(pktmonCh);
  }
}


static void handleKeyboard(ButtonID btn) {
  if (btn == BTN_NONE) return;

  bool done = keyboard_handleButton(btn);

  if (done) {
    if (keyboard_wasConfirmed()) {
      if (keyboardReturnState == STATE_WIFI_CONNECT) {
        if (connectAPIdx >= 0 && connectAPIdx < scannedAPCount) {
          strncpy(connectTargetSSID, scannedAPs[connectAPIdx].ssid, 32);
          connectTargetSSID[32] = '\0';
          prefs.begin("delta", false);
          prefs.putString("wifi_ssid", scannedAPs[connectAPIdx].ssid);
          prefs.putString("wifi_pass", keyboard_getResult());
          prefs.end();
          WiFi.persistent(false);
          WiFi.disconnect(false);
          delay(100);
          WiFi.mode(WIFI_STA);
          WiFi.begin(scannedAPs[connectAPIdx].ssid, keyboard_getResult());
          connectStartTime = millis();
          connectStatus = 0;
          currentState = STATE_WIFI_CONNECT;
        } else {
          currentState = STATE_MAIN_MENU;
        }
      } else if (keyboardReturnState == STATE_IR_CLONER) {
        ir::acceptSave(keyboard_getResult());
        currentState = STATE_IR_CLONER;
      } else {
        currentState = keyboardReturnState;
      }
    } else {
      if (keyboardReturnState == STATE_IR_CLONER) {
        ir::rejectSave();
        currentState = STATE_IR_CLONER;
      } else {
        currentState = STATE_SELECT_APS;
      }
    }
  }
}


static void handleWifiConnect(ButtonID btn) {
  if (connectStatus == 0) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      WiFi.setSleep(false);
      esp_wifi_set_ps(WIFI_PS_NONE);
      WiFi.setTxPower(WIFI_POWER_19_5dBm);
      esp_wifi_set_max_tx_power(78);
      connectStatus = 1;
      return;
    }

    if (millis() - connectStartTime > CONNECT_TIMEOUT) {
      WiFi.disconnect(false);
      connectStatus = -1;
      return;
    }

    if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) {
      WiFi.disconnect(false);
      currentState = STATE_AP_INFO;
    }
  } else {
    // Result screen shown (Connected or Failed)
    if (btn == BTN_ID_SELECT || btn == BTN_ID_LEFT) {
      if (connectStatus == 1) {
        currentState = STATE_MAIN_MENU;
      } else {
        currentState = STATE_AP_INFO;
      }
    }
  }
}


static void handlePorts(ButtonID btn) {
  if (portscanRunning && !portscanComplete) {
    portscan_scanNext();
  }

  if (portscanComplete) {
    currentState = STATE_PORT_RESULTS;
    portResultIdx = 0;
    portResultScroll = 0;
  }

  if (btn == BTN_ID_LEFT) {
    portscan_stop();
    currentState = STATE_MAIN_MENU;
  }
}

static void handlePortResults(ButtonID btn) {
  int count = portscan_getResultCount();
  adjustScroll(portResultIdx, portResultScroll, (count > 0 ? count : 0) + 1, 5, btn);

  if (btn == BTN_ID_SELECT && portResultIdx == 0) {
    currentState = STATE_MAIN_MENU;
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_MAIN_MENU;
  }
}


static void handleIrCloner(ButtonID btn) {
  if (ir::getMode() == ir::Mode::Flooder) {
    if (btn == BTN_ID_SELECT || btn == BTN_ID_LEFT) {
      ir::stopFlooder();
    }
    return;
  }
  
  if (ir::getMode() == ir::Mode::Record || ir::getMode() == ir::Mode::Analyse) {
    if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) {
      ir::stop();
    }
    return;
  }

  if (ir::getMode() == ir::Mode::SaveConfirm) {
    keyboard_init("Signal Name:", ir::nextSignalName().c_str(), 15, false);
    keyboardReturnState = STATE_IR_CLONER;
    currentState = STATE_KEYBOARD;
    return;
  }

  if (ir::getMode() == ir::Mode::SendResult && ir::menuGetScreen() != ir::Screen::SignalReady) {
    if (btn != BTN_NONE) {
      ir::stop();
    }
    return;
  }

  if (ir::menuGetScreen() == ir::Screen::SignalReady) {
    if (btn == BTN_ID_SELECT) {
      ir::menuSelect(0);
    } else if (btn == BTN_ID_LEFT) {
      ir::menuBack();
      irClonerIdx = 0;
      irClonerScroll = 0;
    }
    return;
  }

  int count = ir::menuGetItemCount();

  bool hasBack = true;
  int totalItems = count + 1;

  adjustScroll(irClonerIdx, irClonerScroll, totalItems, 5, btn);

  if (btn == BTN_ID_SELECT) {
    if (irClonerIdx == 0) {
      if (ir::menuCanGoBack()) {
        ir::menuBack();
      } else {
        currentState = STATE_MAIN_MENU;
      }
      irClonerIdx = 0;
      irClonerScroll = 0;
    } else {
      ir::menuSelect(irClonerIdx - 1);
      irClonerIdx = 0;
      irClonerScroll = 0;
    }
  } else if (btn == BTN_ID_LEFT) {
    if (ir::menuCanGoBack()) {
      ir::menuBack();
      irClonerIdx = 0;
      irClonerScroll = 0;
    } else {
      currentState = STATE_MAIN_MENU;
    }
  }
}



static void handleReconMenu(ButtonID btn) {
  if (btn == BTN_ID_UP && reconMenuIdx > 0) reconMenuIdx--;
  if (btn == BTN_ID_DOWN && reconMenuIdx < 3) reconMenuIdx++;

  if (btn == BTN_ID_SELECT) {
    switch (reconMenuIdx) {
      case 0: // BACK
        currentState = STATE_SCAN_MENU;
        break;
      case 1:
        recon::startScan();
        currentState = STATE_RECON_SCANNING;
        break;
      case 2:
        recon::startHunt();
        currentState = STATE_RECON_SCANNING;
        break;
      case 3:
        recon::startLive();
        currentState = STATE_RECON_SCANNING;
        break;
    }
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_SCAN_MENU;
  }
}

static void handleReconScanning(ButtonID btn) {
  if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) {
    recon::stop();
    reconResultIdx = 0;
    reconResultScroll = 0;
    currentState = STATE_RECON_RESULTS;
  }
  
  if (!recon::isActive() && recon::getDuration() > 0) {
    // Timed scan finished naturally
    reconResultIdx = 0;
    reconResultScroll = 0;
    currentState = STATE_RECON_RESULTS;
  }
}

static void handleReconResults(ButtonID btn) {
  uint16_t count = recon::getDeviceCount();
  int totalItems = count + 2;

  adjustScroll(reconResultIdx, reconResultScroll, totalItems, 5, btn);

  if (btn == BTN_ID_SELECT) {
    if (reconResultIdx == 0) {
      currentState = STATE_RECON_MENU;
    } else if (reconResultIdx == 1) {
      reconAlertIdx = 0;
      reconAlertScroll = 0;
      currentState = STATE_RECON_ALERTS;
    } else {
      reconDeviceIdx = reconResultIdx - 2;
      currentState = STATE_RECON_DEVICE;
    }
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_RECON_MENU;
  }
}

static void handleReconDevice(ButtonID btn) {
  if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) {
    currentState = STATE_RECON_RESULTS;
  }
}

static void handleReconAlerts(ButtonID btn) {
  uint8_t count = recon::getAlertCount();
  int totalItems = count + 1;

  adjustScroll(reconAlertIdx, reconAlertScroll, totalItems, 5, btn);

  if (btn == BTN_ID_SELECT) {
    if (reconAlertIdx == 0) {
      currentState = STATE_RECON_RESULTS;
    } else {
      int alertIdx = reconAlertIdx - 1;
      ReconAlert* a = recon::getAlert(alertIdx);
      if (a) {
        int devIdx = recon::getDeviceIndexByMac(a->mac);
        if (devIdx >= 0) {
          reconDeviceIdx = devIdx;
          currentState = STATE_RECON_DEVICE;
        }
      }
    }
  }

  if (btn == BTN_ID_LEFT) {
    currentState = STATE_RECON_RESULTS;
  }
}


static void handleRfAnalyzer(ButtonID btn) {
  if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) {
    rfanalyzer::stop();
    currentState = STATE_MAIN_MENU;
  }
}


static void handleJammerRunning(ButtonID btn) {
  // Keep the jammer running (channel hopping happens in jammer::update)
  if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) {
    jammer::stop();
    currentState = STATE_MAIN_MENU;
  }
}

static void handleAbout(ButtonID btn) {
  if (btn == BTN_ID_LEFT || btn == BTN_ID_SELECT) {
    currentState = STATE_MAIN_MENU;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[DELTA] ESP32 Deauther v1.0 starting...");

  Wire.begin(I2C_SDA, I2C_SCL);
  ui_init();
  ui_drawSplash();

  buttons_init();

  // Deselect SPI devices before initializing anything on the shared bus
  // This prevents the NRF24 from listening while the SD card initializes
  pinMode(NRF_CSN, OUTPUT);
  digitalWrite(NRF_CSN, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  sdcard_init();

  oui_init();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  deauth_init();

  randomSeed(esp_random());

  beacon_loadSSIDs();

  prefs.begin("delta", true);
  pgSignup = prefs.getBool("pgSignup", false);
  prefs.end();

  ir::begin();
  
  recon::init();
  
  rfanalyzer::init();
  
  jammer::init();

  Serial.println("[DELTA] Init complete");
  delay(1000);

  currentState = STATE_MAIN_MENU;
}

void loop() {
  ir::handleSerial();

  // Fast-path polling capture for IR Cloner to prevent I2C display latency from dropping pulses
  if (currentState == STATE_IR_CLONER && (ir::getMode() == ir::Mode::Record || ir::getMode() == ir::Mode::Analyse)) {
    static bool recordDrawn = false;
    if (!recordDrawn) {
      ui_clear();
      ui_drawIRCloner(irClonerIdx, irClonerScroll);
      ui_flush();
      recordDrawn = true;
    }

    ir::update();

    ButtonID b = buttons_read();
    if (b == BTN_ID_LEFT || b == BTN_ID_SELECT) {
      ir::stop();
      recordDrawn = false;
    }

    if (ir::getMode() != ir::Mode::Record && ir::getMode() != ir::Mode::Analyse) {
      recordDrawn = false;
    }

    yield();
    return;
  }

  ir::update();
  scanner_update();
  recon::update();
  rfanalyzer::update();
  jammer::update();

  ButtonID btn = buttons_read();

  switch (currentState) {
    case STATE_MAIN_MENU:       handleMainMenu(btn);       break;
    case STATE_SCAN_MENU:       handleScanMenu(btn);       break;
    case STATE_SCANNING:        handleScanning(btn);       break;
    case STATE_SELECT_MENU:     handleSelectMenu(btn);     break;
    case STATE_SELECT_APS:      handleSelectAPs(btn);      break;
    case STATE_SELECT_STS:      handleSelectSTs(btn);      break;
    case STATE_AP_INFO:         handleAPInfo(btn);         break;
    case STATE_ST_INFO:         handleSTInfo(btn);         break;
    case STATE_ATTACK_MENU:     handleAttackMenu(btn);     break;
    case STATE_ATTACK_RUNNING:  handleAttackRunning(btn);  break;
    case STATE_PACKET_MONITOR:  handlePacketMonitor(btn);  break;
    case STATE_WIFI_CONNECT:    handleWifiConnect(btn);    break;
    case STATE_PORTS:           handlePorts(btn);          break;
    case STATE_PORT_RESULTS:    handlePortResults(btn);    break;
    case STATE_KEYBOARD:        handleKeyboard(btn);       break;
    case STATE_IR_CLONER:       handleIrCloner(btn);       break;
    case STATE_RECON_MENU:      handleReconMenu(btn);      break;
    case STATE_RECON_SCANNING:  handleReconScanning(btn);  break;
    case STATE_RECON_RESULTS:   handleReconResults(btn);   break;
    case STATE_RECON_DEVICE:    handleReconDevice(btn);    break;
    case STATE_RECON_ALERTS:    handleReconAlerts(btn);    break;
    case STATE_RF_ANALYZER:     handleRfAnalyzer(btn);     break;
    case STATE_JAMMER_RUNNING:   handleJammerRunning(btn);  break;
    case STATE_ABOUT:            handleAbout(btn);          break;
  }

  ui_clear();

  switch (currentState) {
    case STATE_MAIN_MENU:
      ui_drawMainMenu(mainMenuIdx, wifiConnected);
      break;
    case STATE_SCAN_MENU:
      ui_drawScanMenu(scanMenuIdx);
      break;
    case STATE_SCANNING: {
      const char* title = "SCANNING";
      ScanType st = scanner_getScanType();
      if (st == SCAN_TYPE_APS) title = "SCAN APs";
      else if (st == SCAN_TYPE_STS) title = "SCAN STs";
      else if (st == SCAN_TYPE_ALL) title = "SCAN ALL";

      ui_drawScanProgress(title,
                          scanner_getCurrentChannel(),
                          scannedAPCount,
                          scannedSTCount,
                          stPktsSeen,
                          scanner_getProgress());
      break;
    }
    case STATE_SELECT_MENU:
      ui_drawSelectMenu(selectMenuIdx, pgSignup);
      break;
    case STATE_SELECT_APS:
      ui_drawSelectAPs(selectAPIdx, selectAPScroll);
      break;
    case STATE_SELECT_STS:
      ui_drawSelectSTs(selectSTIdx, selectSTScroll);
      break;
    case STATE_AP_INFO:
      ui_drawAPInfo(apInfoIdx, apInfoOption);
      break;
    case STATE_ST_INFO:
      ui_drawSTInfo(stInfoIdx, 0);
      break;
    case STATE_ATTACK_MENU:
      ui_drawAttackMenu(selectedAttackType, attackMenuIdx);
      break;
    case STATE_ATTACK_RUNNING:
      ui_drawAttackRunning(selectedAttackType);
      break;
    case STATE_PACKET_MONITOR:
      pktmon_draw(display);
      break;
    case STATE_WIFI_CONNECT:
      ui_drawWifiConnect(connectStatus, connectTargetSSID,
                         millis() - connectStartTime, CONNECT_TIMEOUT);
      break;
    case STATE_PORTS:
      ui_drawPortScan(WiFi.gatewayIP().toString().c_str(), portscanProgress, portscanOpenCount);
      break;
    case STATE_PORT_RESULTS:
      ui_drawPortResults(portResultIdx, portResultScroll);
      break;
    case STATE_KEYBOARD:
      keyboard_draw(display);
      break;
    case STATE_IR_CLONER:
      ui_drawIRCloner(irClonerIdx, irClonerScroll);
      break;
    case STATE_RECON_MENU:
      ui_drawReconMenu(reconMenuIdx);
      break;
    case STATE_RECON_SCANNING:
      ui_drawReconScanning();
      break;
    case STATE_RECON_RESULTS:
      ui_drawReconResults(reconResultIdx, reconResultScroll);
      break;
    case STATE_RECON_DEVICE:
      ui_drawReconDevice(reconDeviceIdx);
      break;
    case STATE_RECON_ALERTS:
      ui_drawReconAlerts(reconAlertIdx, reconAlertScroll);
      break;
    case STATE_RF_ANALYZER:
      rfanalyzer::draw(display);
      break;
    case STATE_JAMMER_RUNNING:
      jammer::draw(display);
      break;
    case STATE_ABOUT:
      ui_drawAbout();
      break;
  }

  ui_flush();

  // Tight delay during attacks, normal otherwise
  if (deauthRunning || beaconRunning || eviltwinRunning) {
    delay(1);
  } else {
    delay(10);
  }
}
