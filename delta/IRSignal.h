/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */
#ifndef IRSIGNAL_H
#define IRSIGNAL_H

#include <Arduino.h>

namespace ir {

enum class Company : uint8_t {
    Haier = 0, Gree, Panasonic, Mitsubishi, Samsung, LG, TCL, Sony, Midea, Daikin, Nobel,
    Toshiba, Philips, Sharp, JVC, Hitachi, Sanyo, Hisense, Vizio, COUNT
};

enum class DeviceType : uint8_t {
    AC = 0, TV, COUNT
};

enum class Task : uint8_t {
    On = 0, Off, Set16C, SwingOn, SwingOff, FanOff, COUNT
};

enum class Mode {
    Idle, Analyse, Record, SaveConfirm, SendResult, Flooder
};

enum class Screen {
    RootList, TopList, DeviceList, TaskList, ProtocolList, CustomList, SignalReady
};

void begin();
void update();
void startAnalyse();
void startRecord();
void stop();
void startFlooder();
void stopFlooder();
Mode getMode();
String getStatus();
String getDetailLine();
String getPendingSummary();
String getSendResult();
uint8_t getGraphSize();
uint8_t getGraphValue(uint8_t index);
bool hasSignals();
uint8_t getSignalCount();
String getSignalName(uint8_t index);
uint8_t getPresetCount();
String getPresetName(uint8_t index);
bool sendPreset(uint8_t index);
bool sendSignal(uint8_t index);
bool acceptSave(String name);
void rejectSave();
bool deleteSignal(uint8_t index);
String nextSignalName();
void refreshCachedNames();
void handleSerial();

uint8_t getCompanyCount();
String getCompanyName(uint8_t companyIndex);
uint8_t getDeviceTypeCount();
String getDeviceTypeName(uint8_t deviceIndex);
uint8_t getTaskCount();
String getTaskName(uint8_t taskIndex);

uint8_t getFilteredSignalCount(Company company, DeviceType device, Task task);
String getFilteredSignalName(Company company, DeviceType device, Task task, uint8_t index);
bool sendFilteredSignal(Company company, DeviceType device, Task task, uint8_t index);

void setPendingCompany(Company company);
void setPendingDevice(DeviceType device);
void setPendingTask(Task task);

Company getPendingCompany();
DeviceType getPendingDevice();
Task getPendingTask();

void menuEnter();
Screen menuGetScreen();
String menuGetCurrentSignalName();
bool menuIsSent();
String menuGetSendResultText();
String menuGetTitle();
uint8_t menuGetItemCount();
String menuGetItemName(uint8_t index);
bool menuSend();
void menuSelect(uint8_t index);
bool menuCanGoBack();
void menuBack();

} // namespace ir

#endif // IRSIGNAL_H
