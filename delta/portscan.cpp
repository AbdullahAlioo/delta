/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#include "portscan.h"
#include <WiFi.h>

bool portscanRunning = false;
bool portscanComplete = false;
int portscanProgress = 0;
int portscanOpenCount = 0;

static PortResult results[20];
static int resultCount = 0;
static int currentPortIdx = 0;
static IPAddress targetIP;

void portscan_start(IPAddress target) {
  targetIP = target;
  currentPortIdx = 0;
  resultCount = 0;
  portscanOpenCount = 0;
  portscanProgress = 0;
  portscanComplete = false;
  portscanRunning = true;

  Serial.printf("[PORTSCAN] Starting scan on %s\n", target.toString().c_str());
}

bool portscan_scanNext() {
  if (!portscanRunning || portscanComplete) return portscanComplete;
  if (currentPortIdx >= commonPortCount) {
    portscanComplete = true;
    portscanRunning = false;
    Serial.printf("[PORTSCAN] Complete. Open ports: %d\n", portscanOpenCount);
    return true;
  }

  uint16_t port = commonPorts[currentPortIdx].port;
  const char* service = commonPorts[currentPortIdx].service;

  WiFiClient client;
  client.setTimeout(PORT_SCAN_TIMEOUT);

  bool isOpen = client.connect(targetIP, port);

  results[resultCount].port = port;
  results[resultCount].service = service;
  results[resultCount].open = isOpen;
  resultCount++;

  if (isOpen) {
    portscanOpenCount++;
    Serial.printf("[PORTSCAN] Port %d (%s) OPEN\n", port, service);
    client.stop();
  }

  currentPortIdx++;
  portscanProgress = (currentPortIdx * 100) / commonPortCount;

  return portscanComplete;
}

PortResult* portscan_getResults() {
  return results;
}

int portscan_getResultCount() {
  return resultCount;
}

void portscan_stop() {
  portscanRunning = false;
  portscanComplete = true;
}
