/*
 * This firmware is under MIT License
 * Author: Abdullah Ali
 */

#include "IRSignal.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>
#include <ir_Mitsubishi.h>
#include <ir_Haier.h>
#include <ir_Gree.h>
#include <ir_Panasonic.h>
#include <ir_Samsung.h>
#include <ir_LG.h>
#include <ir_Midea.h>
#include <ir_Tcl.h>
#include <ir_Daikin.h>
#include <IRsend.h>

#ifndef IR_RX_PIN
#define IR_RX_PIN 34    // IR receiver data pin (e.g. TSOP1738)
#endif

#ifndef IR_TX_PIN
#define IR_TX_PIN 25    // IR LED pin (drive via transistor)
#endif

#define IR_RESOLUTION     1        // store raw microseconds (1:1, no loss)
#define IR_MAX_PULSES     300      // max mark/space pairs (300*2*2=1200 bytes on stack, safe for ESP8266)
#define IR_TIMEOUT        150000   // 150 ms silence = end of signal
#define IR_NOISE_FILTER   50       // ignore transitions < 50 µs
#define IR_SEND_REPEATS   3        // send raw signal this many times for reliability

namespace ir {

    static void updateMenuCache();

    // ── Flooder state ──
    static uint8_t flooderIndex = 0;       // which preset/signal we're on
    static uint32_t flooderLastSend = 0;   // last time we sent
    static bool flooderRunning = false;    // is flooder actively cycling
    static bool flooderInitialSend = false;// used to trigger first send immediately
    // ── Per-company A/C protocol instances ──
    static IRMitsubishiAC  acStd(IR_TX_PIN);   // 144-bit
    static IRMitsubishi136 ac136(IR_TX_PIN);   // 136-bit
    static IRMitsubishi112 ac112(IR_TX_PIN);   // 112-bit
    static IRHaierAC       haierAc(IR_TX_PIN);      // 9-byte HSU07-HEA03
    static IRMideaAC mideaAc(IR_TX_PIN);
    
    static IRHaierACYRW02  haierYrw02(IR_TX_PIN);   // YR-W02
    static IRHaierAC176    haier176(IR_TX_PIN);     // 176-bit
    static IRHaierAC160    haier160(IR_TX_PIN);     // 160-bit
    
    static IRGreeAC greeYaw1f(IR_TX_PIN, gree_ac_remote_model_t::YAW1F);
    static IRGreeAC greeYbofb(IR_TX_PIN, gree_ac_remote_model_t::YBOFB);
    static IRGreeAC greeYx1fsf(IR_TX_PIN, gree_ac_remote_model_t::YX1FSF);
    static IRPanasonicAc   panasonicAc(IR_TX_PIN);    // 48-byte "big" AC protocol
    static IRPanasonicAc32 panasonicAc32(IR_TX_PIN);  // 32-bit "short" AC protocol
    static IRSamsungAc     samsungAc(IR_TX_PIN);
    static IRLgAc lgGe6711(IR_TX_PIN);       // set model after construction
    static IRLgAc lg6711a(IR_TX_PIN);
    static IRLgAc lgAkb75215403(IR_TX_PIN);
    static IRLgAc lgAkb74955603(IR_TX_PIN);
    static IRLgAc lgAkb73757604(IR_TX_PIN);
    static IRTcl112Ac      tclAc(IR_TX_PIN);
    static IRDaikinESP     daikinAc(IR_TX_PIN);      // 280-bit
    static IRDaikin2       daikin2Ac(IR_TX_PIN);      // 312-bit
    static IRDaikin216     daikin216Ac(IR_TX_PIN);    // 216-bit
    static IRDaikin160     daikin160Ac(IR_TX_PIN);    // 160-bit
    static IRDaikin176     daikin176Ac(IR_TX_PIN);    // 176-bit
    static IRDaikin128     daikin128Ac(IR_TX_PIN);    // 128-bit
    static IRDaikin152     daikin152Ac(IR_TX_PIN);    // 152-bit
    static IRDaikin64      daikin64Ac(IR_TX_PIN);     // 64-bit

    // Plain sender for simple/TV-style codes that don't need a full A/C
    // state object (Samsung TV, Sony TV).
    static IRsend rawSend(IR_TX_PIN);

    static void haierOn()      { haierAc.setCommand(kHaierAcCmdOn); haierAc.send(); }
    static void haierOff()     { haierAc.setCommand(kHaierAcCmdOff); haierAc.send(); }
    static void haierYrw02On() { haierYrw02.on();  haierYrw02.send(); }
    static void haierYrw02Off(){ haierYrw02.off(); haierYrw02.send(); }
    static void haier176On()   { haier176.on();    haier176.send(); }
    static void panasonicOn()   { panasonicAc.on();  panasonicAc.send(); }
    static void panasonicOff()  { panasonicAc.off(); panasonicAc.send(); }
    static void panasonic32On() { panasonicAc32.setPowerToggle(true); panasonicAc32.send(); }
    static void panasonic32Off(){ panasonicAc32.setPowerToggle(false); panasonicAc32.send(); }
    static void greeYaw1fOn()  { greeYaw1f.on();  greeYaw1f.send(); }
    static void greeYaw1fOff() { greeYaw1f.off(); greeYaw1f.send(); }
    static void greeYbofbOn()  { greeYbofb.on();  greeYbofb.send(); }
    static void greeYbofbOff() { greeYbofb.off(); greeYbofb.send(); }
    static void greeYx1fsfOn() { greeYx1fsf.on();  greeYx1fsf.send(); }
    static void greeYx1fsfOff(){ greeYx1fsf.off(); greeYx1fsf.send(); }
    static void lgGe6711On()  { lgGe6711.on();  lgGe6711.send(); }
    static void lgGe6711Off() { lgGe6711.off(); lgGe6711.send(); }

    static void lg6711aOn()  { lg6711a.on();  lg6711a.send(); }
    static void lg6711aOff() { lg6711a.off(); lg6711a.send(); }

    static void lgAkb75215403On()  { lgAkb75215403.on();  lgAkb75215403.send(); }
    static void lgAkb75215403Off() { lgAkb75215403.off(); lgAkb75215403.send(); }

    static void lgAkb74955603On()  { lgAkb74955603.on();  lgAkb74955603.send(); }
    static void lgAkb74955603Off() { lgAkb74955603.off(); lgAkb74955603.send(); }

    static void lgAkb73757604On()  { lgAkb73757604.on();  lgAkb73757604.send(); }
    static void lgAkb73757604Off() { lgAkb73757604.off(); lgAkb73757604.send(); }
    static void haier176Off()  { haier176.off();   haier176.send(); }
    static void haier160On()   { haier160.on();    haier160.send(); }
    static void haier160Off()  { haier160.off();   haier160.send(); }
    static void sendAcStdOn()  { acStd.setMode(kMitsubishiAcCool); acStd.setTemp(16); acStd.setFan(kMitsubishiAcFanMax); acStd.on(); acStd.send(); }
    static void sendAcStdOff() { acStd.off(); acStd.send(); }

    static void send136On()    { ac136.setMode(kMitsubishi136Cool); ac136.setTemp(16); ac136.setFan(kMitsubishi136FanMax); ac136.on(); ac136.send(); }
    static void send136Off() { ac136.off(); ac136.send(); }

    static void send112On()    { ac112.setMode(kMitsubishi112Cool); ac112.setTemp(16); ac112.setFan(kMitsubishi112FanMax); ac112.on(); ac112.send(); }
    static void send112Off() { ac112.off(); ac112.send(); }



    static void samsungAcOn()  { samsungAc.on();  samsungAc.send(); }
    static void samsungAcOff() { samsungAc.off(); samsungAc.send(); }


    static void tclOn()  { tclAc.on();  tclAc.send(); }
    static void tclOff() { tclAc.off(); tclAc.send(); }
    // Haier — Set 16C
    static void haierSet16C()      { haierAc.setTemp(16); haierAc.send(); }
    static void haierYrw02Set16C() { haierYrw02.setTemp(16, false); haierYrw02.send(); }
    static void haier176Set16C()   { haier176.setTemp(16, false); haier176.send(); }
    static void haier160Set16C()   { haier160.setTemp(16, false); haier160.send(); }

    // Haier — Swing On/Off
    static void haierSwingOn()      { haierAc.setSwingV(kHaierAcSwingVChg); haierAc.send(); }
    static void haierSwingOff()     { haierAc.setSwingV(kHaierAcSwingVOff); haierAc.send(); }
    static void haierYrw02SwingOn() { haierYrw02.setSwingV(kHaierAcYrw02SwingVAuto); haierYrw02.send(); }
    static void haierYrw02SwingOff(){ haierYrw02.setSwingV(kHaierAcYrw02SwingVOff); haierYrw02.send(); }
    static void haier176SwingOn()   { haier176.setSwingV(kHaierAcYrw02SwingVAuto); haier176.send(); }
    static void haier176SwingOff()  { haier176.setSwingV(kHaierAcYrw02SwingVOff); haier176.send(); }
    static void haier160SwingOn()   { haier160.setSwingV(kHaierAc160SwingVAuto); haier160.send(); }
    static void haier160SwingOff()  { haier160.setSwingV(kHaierAc160SwingVOff); haier160.send(); }

    // Mitsubishi — Set 16C
    static void sendAcStdSet16C() { acStd.setTemp(16.0); acStd.send(); }
    static void send136Set16C()   { ac136.setTemp(16); ac136.send(); }
    static void send112Set16C()   { ac112.setTemp(16); ac112.send(); }

    // Mitsubishi — Swing On/Off
    static void sendAcStdSwingOn()  { acStd.setVane(kMitsubishiAcVaneSwing); acStd.send(); }
    static void sendAcStdSwingOff() { acStd.setVane(kMitsubishiAcVaneAuto); acStd.send(); }
    static void send136SwingOn()    { ac136.setSwingV(kMitsubishi136SwingVAuto); ac136.send(); }
    static void send136SwingOff()   { ac136.setSwingV(kMitsubishi136SwingVLow); ac136.send(); }
    static void send112SwingOn()    { ac112.setSwingV(kMitsubishi112SwingVAuto); ac112.send(); }
    static void send112SwingOff()   { ac112.setSwingV(kMitsubishi112SwingVLow); ac112.send(); }
    // ── Gree — Set 16C ──
    static void greeYaw1fSet16C()  { greeYaw1f.setTemp(16);  greeYaw1f.send(); }
    static void greeYbofbSet16C()  { greeYbofb.setTemp(16);  greeYbofb.send(); }
    static void greeYx1fsfSet16C() { greeYx1fsf.setTemp(16); greeYx1fsf.send(); }

    // ── Gree — Swing On/Off ──
    static void greeYaw1fSwingOn()   { greeYaw1f.setSwingVertical(true, kGreeSwingAuto); greeYaw1f.send(); }
    static void greeYaw1fSwingOff()  { greeYaw1f.setSwingVertical(false, kGreeSwingLastPos); greeYaw1f.send(); }
    static void greeYbofbSwingOn()   { greeYbofb.setSwingVertical(true, kGreeSwingAuto); greeYbofb.send(); }
    static void greeYbofbSwingOff()  { greeYbofb.setSwingVertical(false, kGreeSwingLastPos); greeYbofb.send(); }
    static void greeYx1fsfSwingOn()  { greeYx1fsf.setSwingVertical(true, kGreeSwingAuto); greeYx1fsf.send(); }
    static void greeYx1fsfSwingOff() { greeYx1fsf.setSwingVertical(false, kGreeSwingLastPos); greeYx1fsf.send(); }

    // ── Panasonic (48-byte) — Set 16C ──
    static void panasonicSet16C() { panasonicAc.setTemp(16); panasonicAc.send(); }

    // ── Panasonic (48-byte) — Swing On/Off ──
    static void panasonicSwingOn()  { panasonicAc.setSwingVertical(kPanasonicAcSwingVAuto); panasonicAc.send(); }
    static void panasonicSwingOff() { panasonicAc.setSwingVertical(kPanasonicAcSwingVMiddle); panasonicAc.send(); }

    // ── Panasonic (32-bit) — Set 16C ──
    static void panasonic32Set16C() { panasonicAc32.setTemp(16); panasonicAc32.send(); }

    // ── Panasonic (32-bit) — Swing On/Off ──
    static void panasonic32SwingOn()  { panasonicAc32.setSwingVertical(kPanasonicAc32SwingVAuto); panasonicAc32.send(); }
    static void panasonic32SwingOff() { panasonicAc32.setSwingVertical(kPanasonicAcSwingVMiddle); panasonicAc32.send(); }

    // ── Samsung — Set 16C ──
    static void samsungAcSet16C() { samsungAc.setTemp(16); samsungAc.send(); }

    // ── Samsung — Swing On/Off ──
    static void samsungAcSwingOn()  { samsungAc.setSwing(true);  samsungAc.send(); }
    static void samsungAcSwingOff() { samsungAc.setSwing(false); samsungAc.send(); }

    // ── LG — Set 16C ──
    static void lgGe6711Set16C()      { lgGe6711.setTemp(16);      lgGe6711.send(); }
    static void lg6711aSet16C()       { lg6711a.setTemp(16);       lg6711a.send(); }
    static void lgAkb75215403Set16C() { lgAkb75215403.setTemp(16); lgAkb75215403.send(); }
    static void lgAkb74955603Set16C() { lgAkb74955603.setTemp(16); lgAkb74955603.send(); }
    static void lgAkb73757604Set16C() { lgAkb73757604.setTemp(16); lgAkb73757604.send(); }

    // ── LG — Swing On/Off ──
    static void lgGe6711SwingOn()       { lgGe6711.setSwingV(kLgAcSwingVAuto); lgGe6711.send(); }
    static void lgGe6711SwingOff()      { lgGe6711.setSwingV(kLgAcSwingVOff);  lgGe6711.send(); }
    static void lg6711aSwingOn()        { lg6711a.setSwingV(kLgAcSwingVAuto);  lg6711a.send(); }
    static void lg6711aSwingOff()       { lg6711a.setSwingV(kLgAcSwingVOff);   lg6711a.send(); }
    static void lgAkb75215403SwingOn()  { lgAkb75215403.setSwingV(kLgAcSwingVAuto); lgAkb75215403.send(); }
    static void lgAkb75215403SwingOff() { lgAkb75215403.setSwingV(kLgAcSwingVOff);  lgAkb75215403.send(); }
    static void lgAkb74955603SwingOn()  { lgAkb74955603.setSwingV(kLgAcSwingVAuto); lgAkb74955603.send(); }
    static void lgAkb74955603SwingOff() { lgAkb74955603.setSwingV(kLgAcSwingVOff);  lgAkb74955603.send(); }
    static void lgAkb73757604SwingOn()  { lgAkb73757604.setSwingV(kLgAcSwingVAuto); lgAkb73757604.send(); }
    static void lgAkb73757604SwingOff() { lgAkb73757604.setSwingV(kLgAcSwingVOff);  lgAkb73757604.send(); }
    static void mideaOn()  { mideaAc.setMode(kMideaACAuto); mideaAc.setFan(kMideaACFanAuto); mideaAc.on();  mideaAc.send(); }
    static void mideaOff() { mideaAc.off(); mideaAc.send(); }

    // Optional: 16C set / swing, matching your other brands' pattern
    static void mideaSet16C()   { mideaAc.setTemp(16); mideaAc.send(); }
    static void mideaSwingOn()  { mideaAc.setSwingVToggle(true);  mideaAc.send(); }
    static void mideaSwingOff() { mideaAc.setSwingVToggle(false); mideaAc.send(); }
    // ── TCL — Set 16C ──
    static void tclSet16C() { tclAc.setTemp(16.0); tclAc.send(); }

    // ── TCL — Swing On/Off ──
    static void tclSwingOn()  { tclAc.setSwingVertical(kTcl112AcSwingVOn);  tclAc.send(); }
    static void tclSwingOff() { tclAc.setSwingVertical(kTcl112AcSwingVOff); tclAc.send(); }

    // ── Daikin 280-bit — On/Off / Set16C / Swing ──
    static void daikinOn()  { daikinAc.setMode(kDaikinCool); daikinAc.setTemp(16); daikinAc.setFan(kDaikinFanAuto); daikinAc.on();  daikinAc.send(); }
    static void daikinOff() { daikinAc.off(); daikinAc.send(); }
    static void daikinSet16C()    { daikinAc.setTemp(16); daikinAc.send(); }
    static void daikinSwingOn()   { daikinAc.setSwingVertical(true);  daikinAc.send(); }
    static void daikinSwingOff()  { daikinAc.setSwingVertical(false); daikinAc.send(); }

    // ── Daikin2 312-bit — On/Off / Set16C / Swing ──
    static void daikin2On()  { daikin2Ac.setMode(kDaikinCool); daikin2Ac.setTemp(16); daikin2Ac.setFan(kDaikinFanAuto); daikin2Ac.on();  daikin2Ac.send(); }
    static void daikin2Off() { daikin2Ac.off(); daikin2Ac.send(); }
    static void daikin2Set16C()    { daikin2Ac.setTemp(16); daikin2Ac.send(); }
    static void daikin2SwingOn()   { daikin2Ac.setSwingVertical(kDaikin2SwingVAuto); daikin2Ac.send(); }
    static void daikin2SwingOff()  { daikin2Ac.setSwingVertical(kDaikin2SwingVOff);  daikin2Ac.send(); }

    // ── Daikin216 216-bit — On/Off / Set16C / Swing ──
    static void daikin216On()  { daikin216Ac.setMode(kDaikinCool); daikin216Ac.setTemp(16); daikin216Ac.setFan(kDaikinFanAuto); daikin216Ac.on();  daikin216Ac.send(); }
    static void daikin216Off() { daikin216Ac.off(); daikin216Ac.send(); }
    static void daikin216Set16C()    { daikin216Ac.setTemp(16); daikin216Ac.send(); }
    static void daikin216SwingOn()   { daikin216Ac.setSwingVertical(true);  daikin216Ac.send(); }
    static void daikin216SwingOff()  { daikin216Ac.setSwingVertical(false); daikin216Ac.send(); }

    // ── Daikin160 160-bit — On/Off / Set16C / Swing ──
    static void daikin160On()  { daikin160Ac.setMode(kDaikinCool); daikin160Ac.setTemp(16); daikin160Ac.setFan(kDaikinFanAuto); daikin160Ac.on();  daikin160Ac.send(); }
    static void daikin160Off() { daikin160Ac.off(); daikin160Ac.send(); }
    static void daikin160Set16C()    { daikin160Ac.setTemp(16); daikin160Ac.send(); }
    static void daikin160SwingOn()   { daikin160Ac.setSwingVertical(kDaikin160SwingVAuto); daikin160Ac.send(); }
    static void daikin160SwingOff()  { daikin160Ac.setSwingVertical(kDaikin160SwingVLowest); daikin160Ac.send(); }

    // ── Daikin176 176-bit — On/Off / Set16C / Swing (horizontal only) ──
    static void daikin176On()  { daikin176Ac.setMode(kDaikinCool); daikin176Ac.setTemp(16); daikin176Ac.setFan(kDaikinFanAuto); daikin176Ac.on();  daikin176Ac.send(); }
    static void daikin176Off() { daikin176Ac.off(); daikin176Ac.send(); }
    static void daikin176Set16C()    { daikin176Ac.setTemp(16); daikin176Ac.send(); }
    static void daikin176SwingOn()   { daikin176Ac.setSwingHorizontal(kDaikin176SwingHAuto); daikin176Ac.send(); }
    static void daikin176SwingOff()  { daikin176Ac.setSwingHorizontal(kDaikin176SwingHOff);  daikin176Ac.send(); }

    // ── Daikin128 128-bit — On/Off (toggle) / Set16C / Swing ──
    static void daikin128On()  { daikin128Ac.setPowerToggle(true); daikin128Ac.setMode(kDaikin128Cool); daikin128Ac.setTemp(16); daikin128Ac.setFan(kDaikin128FanAuto); daikin128Ac.send(); }
    static void daikin128Off() { daikin128Ac.setPowerToggle(true); daikin128Ac.send(); }
    static void daikin128Set16C()    { daikin128Ac.setTemp(16); daikin128Ac.send(); }
    static void daikin128SwingOn()   { daikin128Ac.setSwingVertical(true);  daikin128Ac.send(); }
    static void daikin128SwingOff()  { daikin128Ac.setSwingVertical(false); daikin128Ac.send(); }

    // ── Daikin152 152-bit — On/Off / Set16C / Swing ──
    static void daikin152On()  { daikin152Ac.setMode(kDaikinCool); daikin152Ac.setTemp(16); daikin152Ac.setFan(kDaikinFanAuto); daikin152Ac.on();  daikin152Ac.send(); }
    static void daikin152Off() { daikin152Ac.off(); daikin152Ac.send(); }
    static void daikin152Set16C()    { daikin152Ac.setTemp(16); daikin152Ac.send(); }
    static void daikin152SwingOn()   { daikin152Ac.setSwingV(true);  daikin152Ac.send(); }
    static void daikin152SwingOff()  { daikin152Ac.setSwingV(false); daikin152Ac.send(); }

    // ── Daikin64 64-bit — On/Off (toggle) / Set16C / Swing ──
    static void daikin64On()  { daikin64Ac.setPowerToggle(true); daikin64Ac.setMode(kDaikinCool); daikin64Ac.setTemp(16); daikin64Ac.setFan(kDaikinFanAuto); daikin64Ac.send(); }
    static void daikin64Off() { daikin64Ac.setPowerToggle(true); daikin64Ac.send(); }
    static void daikin64Set16C()    { daikin64Ac.setTemp(16); daikin64Ac.send(); }
    static void daikin64SwingOn()   { daikin64Ac.setSwingVertical(true);  daikin64Ac.send(); }
    static void daikin64SwingOff()  { daikin64Ac.setSwingVertical(false); daikin64Ac.send(); }

    // TVs are normally driven by a single toggle "power" button rather than
    // distinct ON/OFF codes, so the same signal covers both menu entries.
    static void samsungTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendSAMSUNG(0xE0E040BFUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    static void sonyTvToggle() {
        // Standard Sony SIRC power code: 12-bit, address 1, command 0x15.
        uint32_t code = rawSend.encodeSony(12, 0x15, 0x1, 0);
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendSony(code, 12, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }

    // ── Nobel TV power-off raw IR signal (captured via IR Cloner) ──
    static const uint16_t nobelTvRaw[600] PROGMEM = {
        4543, 4506, 621, 1641, 621, 1661, 621, 1641,
        621, 521, 601, 562, 581, 561, 561, 581,
        561, 581, 557, 1701, 581, 1702, 561, 1721,
        561, 561, 561, 581, 561, 581, 561, 561,
        582, 561, 561, 581, 561, 1721, 561, 561,
        581, 561, 561, 581, 561, 581, 562, 561,
        581, 561, 561, 1721, 557, 581, 561, 1701,
        562, 1721, 561, 1701, 581, 1701, 561, 1721,
        561, 1701, 561, 46258,
        8961, 4522, 557, 581, 561, 601, 561, 581,
        561, 581, 581, 582, 561, 581, 561, 601,
        561, 581, 561, 1721, 561, 1702, 561, 1721,
        561, 1701, 561, 1721, 561, 1701, 581, 1698,
        561, 601, 561, 581, 561, 1721, 561, 581,
        562, 1721, 561, 581, 561, 581, 581, 581,
        561, 581, 581, 1701, 561, 582, 561, 1721,
        561, 581, 581, 1701, 561, 1701, 581, 1702,
        561, 1721, 561, 40318,
        4501, 4557, 561, 1701, 582, 1701, 561, 1701,
        581, 561, 561, 581, 561, 581, 561, 562,
        581, 561, 581, 1701, 561, 1701, 581, 1701,
        561, 581, 561, 578, 561, 581, 561, 561,
        581, 561, 561, 581, 561, 1701, 581, 561,
        561, 582, 561, 581, 561, 561, 581, 561,
        561, 581, 561, 1702, 581, 561, 561, 1721,
        561, 1701, 581, 1701, 561, 1722, 561, 1701,
        561, 1721, 561, 46274,
        8941, 4501, 582, 581, 561, 581, 561, 601,
        561, 581, 561, 601, 562, 581, 561, 581,
        581, 577, 561, 1721, 561, 1702, 561, 1721,
        561, 1701, 581, 1701, 561, 1701, 581, 1701,
        562, 601, 561, 581, 561, 1721, 561, 581,
        561, 1721, 561, 582, 561, 581, 581, 581,
        561, 581, 561, 1721, 561, 581, 562, 1721,
        561, 581, 561, 1721, 561, 1701, 581, 1701,
        561, 1701, 582, 40294,
        4521, 4541, 581, 1701, 561, 1721, 561, 1698,
        581, 561, 561, 581, 561, 581, 561, 561,
        581, 562, 561, 1721, 561, 1701, 561, 1721,
        561, 581, 561, 562, 581, 561, 561, 581,
        561, 561, 581, 561, 581, 1702, 561, 581,
        561, 573, 561, 561, 581, 562, 561, 581,
        561, 581, 561, 1701, 581, 561, 561, 1721,
        562, 1701, 561, 1721, 561, 1701, 581, 1701,
        561, 1721, 561, 46254,
        8938, 4521, 561, 601, 561, 581, 561, 581,
        582, 581, 561, 581, 561, 601, 561, 581,
        561, 581, 582, 1701, 561, 1721, 561, 1701,
        561, 1721, 561, 1701, 581, 1701, 561, 1701,
        582, 581, 561, 581, 581, 1701, 561, 581,
        581, 1701, 562, 581, 581, 581, 561, 581,
        561, 601, 561, 1701, 582, 581, 561, 1716,
        561, 581, 561, 1723, 561, 1701, 561, 1722,
        561, 1701, 581, 40293,
        4501, 4562, 581, 1701, 561, 1701, 581, 1701,
        561, 581, 561, 582, 561, 561, 581, 561,
        561, 581, 561, 1701, 582, 1701, 561, 1721,
        561, 561, 581, 561, 561, 581, 562, 581,
        561, 561, 581, 561, 561, 1721, 561, 561,
        582, 561, 561, 581, 561, 581, 561, 561,
        581, 561, 562, 1721, 561, 561, 576, 1701,
        561, 1723, 561, 1701, 581, 1701, 561, 1702,
        581, 1701, 561, 46253,
        8961, 4501, 581, 582, 561, 581, 561, 601,
        561, 581, 561, 581, 581, 581, 561, 581,
        581, 581, 561, 1702, 561, 1721, 561, 1701,
        581, 1701, 561, 1721, 561, 1701, 562, 1721,
        561, 581, 576, 581, 561, 1701, 581, 583,
        561, 1701, 562, 601, 561, 581, 561, 581,
        581, 581, 561, 1701, 582, 581, 561, 1701,
        581, 581, 561, 1701, 581, 1701, 561, 1721,
        558, 1701, 581, 40297,
        4501, 4562, 561, 1701, 582, 1701, 561, 1721,
        561, 561, 581, 561, 561, 581, 562, 581,
        561, 561, 581, 1701, 561, 1701, 581, 1701,
        561, 581, 562, 581, 561, 561, 576, 561,
        581, 561, 561, 581, 561, 1703, 581, 561,
        562, 581, 561, 581, 561, 561, 581, 561,
        561, 581, 561, 1701, 581, 562, 561, 1721
    };

    static void nobelTvToggle() {
        // Nobel TV power toggle — raw captured signal, stored in PROGMEM.
        uint16_t buf[600];
        memcpy_P(buf, nobelTvRaw, sizeof(nobelTvRaw));
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendRaw(buf, 600, 38);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }

    // ── LG TV power toggle ──
    // NEC 32-bit, code 0x20DF10EF — universally confirmed across IRDB,
    // Flipper-IRDB, and thousands of LG TV models worldwide.
    static void lgTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x20DF10EFUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Panasonic TV power toggle ──
    // Panasonic 48-bit Kaseikyo protocol. Manufacturer ID 0x4004,
    // command 0x0100BCBD. Source: IRremoteESP8266 examples, IRDB.
    static void panasonicTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendPanasonic64(0x40040100BCBDULL, 48, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Toshiba TV power toggle ──
    // NEC 32-bit, code 0x02FD48B7. Source: IRDB, TV-B-Gone, Flipper-IRDB.
    // Works on most Toshiba/Regza models.
    static void toshibaTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x02FD48B7UL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Philips TV power toggle ──
    // RC5 13-bit protocol. Device 0, command 12 (standby/power).
    // 0x100C = toggle=0, address=0, command=12. Source: IRDB.
    static void philipsTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendRC5(0x100CUL, 13, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Sharp TV power toggle ──
    // Sharp 15-bit protocol, code 0x4148. Source: IRDB, TV-B-Gone.
    static void sharpTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendSharpRaw(0x4148UL, 15, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── JVC TV power toggle ──
    // JVC 16-bit protocol, code 0xC5E8. Source: IRDB, Flipper-IRDB.
    static void jvcTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendJVC(0xC5E8UL, 16, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Hitachi TV power toggle ──
    // NEC 32-bit, code 0x0AF5D02F. Source: IRDB, Flipper-IRDB.
    static void hitachiTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x0AF5D02FUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Sanyo TV power toggle ──
    // NEC 32-bit, code 0x1CE348B7. Source: IRDB, TV-B-Gone.
    static void sanyoTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x1CE348B7UL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Hisense TV power toggle (primary) ──
    // NEC 32-bit, code 0x20DF10EF — many Hisense models use the same
    // NEC address 0x20DF as LG (shared IR chipset). Source: IRDB.
    static void hisenseTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x20DF10EFUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Hisense TV power toggle (alternate remote) ──
    // NEC 32-bit, code 0x00FF45BA. Source: Flipper-IRDB (EN2B27 remote).
    static void hisenseTvToggle2() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x00FF45BAUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Vizio TV power toggle (primary) ──
    // NEC 32-bit, code 0x20DF10EF — Vizio commonly uses same NEC
    // address as LG. Source: IRDB.
    static void vizioTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x20DF10EFUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Vizio TV power toggle (alternate XRT remote) ──
    // NEC 32-bit, code 0x04FB40BF. Source: Flipper-IRDB.
    static void vizioTvToggle2() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x04FB40BFUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Haier TV power toggle ──
    // NEC 32-bit, code 0x19E6E21D. Source: IRDB.
    static void haierTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x19E6E21DUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── TCL TV power toggle ──
    // NEC 32-bit, code 0x40BF12ED. Source: IRDB, Flipper-IRDB.
    static void tclTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x40BF12EDUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Midea TV power toggle ──
    // NEC 32-bit, code 0x807F02FD. Source: IRDB.
    static void mideaTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x807F02FDUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Mitsubishi TV power toggle ──
    // NEC 32-bit, code 0xE2D648B7. Source: IRDB, TV-B-Gone.
    static void mitsubishiTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0xE2D648B7UL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Daikin TV power toggle ──
    // NEC 32-bit, code 0x04FB40BF. Daikin rarely makes TVs but some
    // branded models exist in Asian markets. Source: IRDB.
    static void daikinTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x04FB40BFUL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    // ── Gree TV power toggle ──
    // NEC 32-bit, code 0x10EF08F7. Source: IRDB.
    static void greeTvToggle() {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            rawSend.sendNEC(0x10EF08F7UL, 32, 0);
            if (r + 1 < IR_SEND_REPEATS) delay(40);
        }
    }
    

    struct PresetSignal {
        const char* name;
        Company     company;
        DeviceType  device;
        Task        task;
        void (*sendFunc)();
    };

    static const PresetSignal presets[] = {
        {"MITSU144 ON",    Company::Mitsubishi, DeviceType::AC, Task::On,  sendAcStdOn},
        {"MITSU144 OFF",   Company::Mitsubishi, DeviceType::AC, Task::Off, sendAcStdOff},
        {"MITSU136 ON",    Company::Mitsubishi, DeviceType::AC, Task::On,  send136On},
        {"MITSU136 OFF",   Company::Mitsubishi, DeviceType::AC, Task::Off, send136Off},
        {"MITSU112 ON",    Company::Mitsubishi, DeviceType::AC, Task::On,  send112On},
        {"MITSU112 OFF",   Company::Mitsubishi, DeviceType::AC, Task::Off, send112Off},

        {"HAIER9 ON",       Company::Haier, DeviceType::AC, Task::On,  haierOn},
        {"HAIER9 OFF",      Company::Haier, DeviceType::AC, Task::Off, haierOff},
        {"HAIER YRW02 ON",  Company::Haier, DeviceType::AC, Task::On,  haierYrw02On},
        {"HAIER YRW02 OFF", Company::Haier, DeviceType::AC, Task::Off, haierYrw02Off},
        {"HAIER176 ON",     Company::Haier, DeviceType::AC, Task::On,  haier176On},
        {"HAIER176 OFF",    Company::Haier, DeviceType::AC, Task::Off, haier176Off},
        {"HAIER160 ON",     Company::Haier, DeviceType::AC, Task::On,  haier160On},
        {"HAIER160 OFF",    Company::Haier, DeviceType::AC, Task::Off, haier160Off},

        {"GREE YAW1F ON",   Company::Gree, DeviceType::AC, Task::On,  greeYaw1fOn},
        {"GREE YAW1F OFF",  Company::Gree, DeviceType::AC, Task::Off, greeYaw1fOff},
        {"GREE YBOFB ON",   Company::Gree, DeviceType::AC, Task::On,  greeYbofbOn},
        {"GREE YBOFB OFF",  Company::Gree, DeviceType::AC, Task::Off, greeYbofbOff},
        {"GREE YX1FSF ON",  Company::Gree, DeviceType::AC, Task::On,  greeYx1fsfOn},
        {"GREE YX1FSF OFF", Company::Gree, DeviceType::AC, Task::Off, greeYx1fsfOff},

        {"PANA48 ON",       Company::Panasonic, DeviceType::AC, Task::On,  panasonicOn},
        {"PANA48 OFF",      Company::Panasonic, DeviceType::AC, Task::Off, panasonicOff},
        {"PANA32 ON",       Company::Panasonic, DeviceType::AC, Task::On,  panasonic32On},
        {"PANA32 OFF",      Company::Panasonic, DeviceType::AC, Task::Off, panasonic32Off},

        {"SAMSUNG AC ON",  Company::Samsung,    DeviceType::AC, Task::On,  samsungAcOn},
        {"SAMSUNG AC OFF", Company::Samsung,    DeviceType::AC, Task::Off, samsungAcOff},

        {"LG GE6711 ON",       Company::LG, DeviceType::AC, Task::On,  lgGe6711On},
        {"LG GE6711 OFF",      Company::LG, DeviceType::AC, Task::Off, lgGe6711Off},
        {"LG 6711A ON",        Company::LG, DeviceType::AC, Task::On,  lg6711aOn},
        {"LG 6711A OFF",       Company::LG, DeviceType::AC, Task::Off, lg6711aOff},
        {"LG AKB75215403 ON",  Company::LG, DeviceType::AC, Task::On,  lgAkb75215403On},
        {"LG AKB75215403 OFF", Company::LG, DeviceType::AC, Task::Off, lgAkb75215403Off},
        {"LG AKB74955603 ON",  Company::LG, DeviceType::AC, Task::On,  lgAkb74955603On},
        {"LG AKB74955603 OFF", Company::LG, DeviceType::AC, Task::Off, lgAkb74955603Off},
        {"LG AKB73757604 ON",  Company::LG, DeviceType::AC, Task::On,  lgAkb73757604On},
        {"LG AKB73757604 OFF", Company::LG, DeviceType::AC, Task::Off, lgAkb73757604Off},

        {"TCL AC ON",      Company::TCL,        DeviceType::AC, Task::On,  tclOn},
        {"TCL AC OFF",     Company::TCL,        DeviceType::AC, Task::Off, tclOff},

        {"SAMSUNG TV PWR", Company::Samsung,    DeviceType::TV, Task::On,  samsungTvToggle},
        {"SAMSUNG TV PWR", Company::Samsung,    DeviceType::TV, Task::Off, samsungTvToggle},
        {"SONY TV PWR",    Company::Sony,       DeviceType::TV, Task::On,  sonyTvToggle},
        {"SONY TV PWR",    Company::Sony,       DeviceType::TV, Task::Off, sonyTvToggle},
        {"NOBEL TV PWR",   Company::Nobel,      DeviceType::TV, Task::On,  nobelTvToggle},
        {"NOBEL TV PWR",   Company::Nobel,      DeviceType::TV, Task::Off, nobelTvToggle},

        {"LG TV PWR",         Company::LG,         DeviceType::TV, Task::On,  lgTvToggle},
        {"LG TV PWR",         Company::LG,         DeviceType::TV, Task::Off, lgTvToggle},
        {"PANASONIC TV PWR",  Company::Panasonic,  DeviceType::TV, Task::On,  panasonicTvToggle},
        {"PANASONIC TV PWR",  Company::Panasonic,  DeviceType::TV, Task::Off, panasonicTvToggle},
        {"TOSHIBA TV PWR",    Company::Toshiba,    DeviceType::TV, Task::On,  toshibaTvToggle},
        {"TOSHIBA TV PWR",    Company::Toshiba,    DeviceType::TV, Task::Off, toshibaTvToggle},
        {"PHILIPS TV PWR",    Company::Philips,    DeviceType::TV, Task::On,  philipsTvToggle},
        {"PHILIPS TV PWR",    Company::Philips,    DeviceType::TV, Task::Off, philipsTvToggle},
        {"SHARP TV PWR",      Company::Sharp,      DeviceType::TV, Task::On,  sharpTvToggle},
        {"SHARP TV PWR",      Company::Sharp,      DeviceType::TV, Task::Off, sharpTvToggle},
        {"JVC TV PWR",        Company::JVC,        DeviceType::TV, Task::On,  jvcTvToggle},
        {"JVC TV PWR",        Company::JVC,        DeviceType::TV, Task::Off, jvcTvToggle},
        {"HITACHI TV PWR",    Company::Hitachi,    DeviceType::TV, Task::On,  hitachiTvToggle},
        {"HITACHI TV PWR",    Company::Hitachi,    DeviceType::TV, Task::Off, hitachiTvToggle},
        {"SANYO TV PWR",      Company::Sanyo,      DeviceType::TV, Task::On,  sanyoTvToggle},
        {"SANYO TV PWR",      Company::Sanyo,      DeviceType::TV, Task::Off, sanyoTvToggle},
        {"HISENSE TV PWR",    Company::Hisense,    DeviceType::TV, Task::On,  hisenseTvToggle},
        {"HISENSE TV PWR",    Company::Hisense,    DeviceType::TV, Task::Off, hisenseTvToggle},
        {"HISENSE TV2 PWR",   Company::Hisense,    DeviceType::TV, Task::On,  hisenseTvToggle2},
        {"HISENSE TV2 PWR",   Company::Hisense,    DeviceType::TV, Task::Off, hisenseTvToggle2},
        {"VIZIO TV PWR",      Company::Vizio,      DeviceType::TV, Task::On,  vizioTvToggle},
        {"VIZIO TV PWR",      Company::Vizio,      DeviceType::TV, Task::Off, vizioTvToggle},
        {"VIZIO TV2 PWR",     Company::Vizio,      DeviceType::TV, Task::On,  vizioTvToggle2},
        {"VIZIO TV2 PWR",     Company::Vizio,      DeviceType::TV, Task::Off, vizioTvToggle2},
        {"HAIER TV PWR",      Company::Haier,      DeviceType::TV, Task::On,  haierTvToggle},
        {"HAIER TV PWR",      Company::Haier,      DeviceType::TV, Task::Off, haierTvToggle},
        {"TCL TV PWR",        Company::TCL,        DeviceType::TV, Task::On,  tclTvToggle},
        {"TCL TV PWR",        Company::TCL,        DeviceType::TV, Task::Off, tclTvToggle},
        {"MIDEA TV PWR",      Company::Midea,      DeviceType::TV, Task::On,  mideaTvToggle},
        {"MIDEA TV PWR",      Company::Midea,      DeviceType::TV, Task::Off, mideaTvToggle},
        {"MITSU TV PWR",      Company::Mitsubishi, DeviceType::TV, Task::On,  mitsubishiTvToggle},
        {"MITSU TV PWR",      Company::Mitsubishi, DeviceType::TV, Task::Off, mitsubishiTvToggle},
        {"DAIKIN TV PWR",     Company::Daikin,     DeviceType::TV, Task::On,  daikinTvToggle},
        {"DAIKIN TV PWR",     Company::Daikin,     DeviceType::TV, Task::Off, daikinTvToggle},
        {"GREE TV PWR",       Company::Gree,       DeviceType::TV, Task::On,  greeTvToggle},
        {"GREE TV PWR",       Company::Gree,       DeviceType::TV, Task::Off, greeTvToggle},
        {"MITSU NORMAL ON",    Company::Mitsubishi, DeviceType::AC, Task::On,  sendAcStdOn},
        {"MITSU NORMAL OFF",   Company::Mitsubishi, DeviceType::AC, Task::Off, sendAcStdOff},

        {"HAIER NORMAL ON",    Company::Haier, DeviceType::AC, Task::On,  haierOn},
        {"HAIER NORMAL OFF",   Company::Haier, DeviceType::AC, Task::Off, haierOff},

        {"GREE NORMAL ON",     Company::Gree, DeviceType::AC, Task::On,  greeYaw1fOn},
        {"GREE NORMAL OFF",    Company::Gree, DeviceType::AC, Task::Off, greeYaw1fOff},

        {"PANA NORMAL ON",     Company::Panasonic, DeviceType::AC, Task::On,  panasonicOn},
        {"PANA NORMAL OFF",    Company::Panasonic, DeviceType::AC, Task::Off, panasonicOff},

        {"SAMSUNG NORMAL ON",  Company::Samsung, DeviceType::AC, Task::On,  samsungAcOn},
        {"SAMSUNG NORMAL OFF", Company::Samsung, DeviceType::AC, Task::Off, samsungAcOff},

        {"LG NORMAL ON",       Company::LG, DeviceType::AC, Task::On,  lgGe6711On},
        {"LG NORMAL OFF",      Company::LG, DeviceType::AC, Task::Off, lgGe6711Off},

        {"TCL NORMAL ON",      Company::TCL, DeviceType::AC, Task::On,  tclOn},
        {"TCL NORMAL OFF",     Company::TCL, DeviceType::AC, Task::Off, tclOff},
        {"HAIER9 SET16C",        Company::Haier, DeviceType::AC, Task::Set16C,  haierSet16C},
        {"HAIER YRW02 SET16C",   Company::Haier, DeviceType::AC, Task::Set16C,  haierYrw02Set16C},
        {"HAIER176 SET16C",      Company::Haier, DeviceType::AC, Task::Set16C,  haier176Set16C},
        {"HAIER160 SET16C",      Company::Haier, DeviceType::AC, Task::Set16C,  haier160Set16C},
        {"HAIER9 SWING ON",      Company::Haier, DeviceType::AC, Task::SwingOn,  haierSwingOn},
        {"HAIER9 SWING OFF",     Company::Haier, DeviceType::AC, Task::SwingOff, haierSwingOff},
        {"HAIER YRW02 SWING ON", Company::Haier, DeviceType::AC, Task::SwingOn,  haierYrw02SwingOn},
        {"HAIER YRW02 SWING OFF",Company::Haier, DeviceType::AC, Task::SwingOff, haierYrw02SwingOff},
        {"HAIER176 SWING ON",    Company::Haier, DeviceType::AC, Task::SwingOn,  haier176SwingOn},
        {"HAIER176 SWING OFF",   Company::Haier, DeviceType::AC, Task::SwingOff, haier176SwingOff},
        {"HAIER160 SWING ON",    Company::Haier, DeviceType::AC, Task::SwingOn,  haier160SwingOn},
        {"HAIER160 SWING OFF",   Company::Haier, DeviceType::AC, Task::SwingOff, haier160SwingOff},

        {"MITSU144 SET16C", Company::Mitsubishi, DeviceType::AC, Task::Set16C, sendAcStdSet16C},
        {"MITSU136 SET16C", Company::Mitsubishi, DeviceType::AC, Task::Set16C, send136Set16C},
        {"MITSU112 SET16C", Company::Mitsubishi, DeviceType::AC, Task::Set16C, send112Set16C},
        {"MITSU144 SWING ON",  Company::Mitsubishi, DeviceType::AC, Task::SwingOn,  sendAcStdSwingOn},
        {"MITSU144 SWING OFF", Company::Mitsubishi, DeviceType::AC, Task::SwingOff, sendAcStdSwingOff},
        {"MITSU136 SWING ON",  Company::Mitsubishi, DeviceType::AC, Task::SwingOn,  send136SwingOn},
        {"MITSU136 SWING OFF", Company::Mitsubishi, DeviceType::AC, Task::SwingOff, send136SwingOff},
        {"MITSU112 SWING ON",  Company::Mitsubishi, DeviceType::AC, Task::SwingOn,  send112SwingOn},
        {"MITSU112 SWING OFF", Company::Mitsubishi, DeviceType::AC, Task::SwingOff, send112SwingOff},
        {"GREE YAW1F SET16C",  Company::Gree, DeviceType::AC, Task::Set16C, greeYaw1fSet16C},
        {"GREE YBOFB SET16C",  Company::Gree, DeviceType::AC, Task::Set16C, greeYbofbSet16C},
        {"GREE YX1FSF SET16C", Company::Gree, DeviceType::AC, Task::Set16C, greeYx1fsfSet16C},
        {"GREE YAW1F SWING ON",   Company::Gree, DeviceType::AC, Task::SwingOn,  greeYaw1fSwingOn},
        {"GREE YAW1F SWING OFF",  Company::Gree, DeviceType::AC, Task::SwingOff, greeYaw1fSwingOff},
        {"GREE YBOFB SWING ON",   Company::Gree, DeviceType::AC, Task::SwingOn,  greeYbofbSwingOn},
        {"GREE YBOFB SWING OFF",  Company::Gree, DeviceType::AC, Task::SwingOff, greeYbofbSwingOff},
        {"GREE YX1FSF SWING ON",  Company::Gree, DeviceType::AC, Task::SwingOn,  greeYx1fsfSwingOn},
        {"GREE YX1FSF SWING OFF", Company::Gree, DeviceType::AC, Task::SwingOff, greeYx1fsfSwingOff},

        {"PANA48 SET16C", Company::Panasonic, DeviceType::AC, Task::Set16C, panasonicSet16C},
        {"PANA48 SWING ON",  Company::Panasonic, DeviceType::AC, Task::SwingOn,  panasonicSwingOn},
        {"PANA48 SWING OFF", Company::Panasonic, DeviceType::AC, Task::SwingOff, panasonicSwingOff},
        {"PANA32 SET16C", Company::Panasonic, DeviceType::AC, Task::Set16C, panasonic32Set16C},
        {"PANA32 SWING ON",  Company::Panasonic, DeviceType::AC, Task::SwingOn,  panasonic32SwingOn},
        {"PANA32 SWING OFF", Company::Panasonic, DeviceType::AC, Task::SwingOff, panasonic32SwingOff},

        {"SAMSUNG AC SET16C", Company::Samsung, DeviceType::AC, Task::Set16C, samsungAcSet16C},
        {"SAMSUNG AC SWING ON",  Company::Samsung, DeviceType::AC, Task::SwingOn,  samsungAcSwingOn},
        {"SAMSUNG AC SWING OFF", Company::Samsung, DeviceType::AC, Task::SwingOff, samsungAcSwingOff},

        {"LG GE6711 SET16C",       Company::LG, DeviceType::AC, Task::Set16C, lgGe6711Set16C},
        {"LG 6711A SET16C",        Company::LG, DeviceType::AC, Task::Set16C, lg6711aSet16C},
        {"LG AKB75215403 SET16C",  Company::LG, DeviceType::AC, Task::Set16C, lgAkb75215403Set16C},
        {"LG AKB74955603 SET16C",  Company::LG, DeviceType::AC, Task::Set16C, lgAkb74955603Set16C},
        {"LG AKB73757604 SET16C",  Company::LG, DeviceType::AC, Task::Set16C, lgAkb73757604Set16C},
        {"LG GE6711 SWING ON",       Company::LG, DeviceType::AC, Task::SwingOn,  lgGe6711SwingOn},
        {"LG GE6711 SWING OFF",      Company::LG, DeviceType::AC, Task::SwingOff, lgGe6711SwingOff},
        {"LG 6711A SWING ON",        Company::LG, DeviceType::AC, Task::SwingOn,  lg6711aSwingOn},
        {"LG 6711A SWING OFF",       Company::LG, DeviceType::AC, Task::SwingOff, lg6711aSwingOff},
        {"LG AKB75215403 SWING ON",  Company::LG, DeviceType::AC, Task::SwingOn,  lgAkb75215403SwingOn},
        {"LG AKB75215403 SWING OFF", Company::LG, DeviceType::AC, Task::SwingOff, lgAkb75215403SwingOff},
        {"LG AKB74955603 SWING ON",  Company::LG, DeviceType::AC, Task::SwingOn,  lgAkb74955603SwingOn},
        {"LG AKB74955603 SWING OFF", Company::LG, DeviceType::AC, Task::SwingOff, lgAkb74955603SwingOff},
        {"LG AKB73757604 SWING ON",  Company::LG, DeviceType::AC, Task::SwingOn,  lgAkb73757604SwingOn},
        {"LG AKB73757604 SWING OFF", Company::LG, DeviceType::AC, Task::SwingOff, lgAkb73757604SwingOff},

        {"TCL AC SET16C", Company::TCL, DeviceType::AC, Task::Set16C, tclSet16C},
        {"TCL AC SWING ON",  Company::TCL, DeviceType::AC, Task::SwingOn,  tclSwingOn},
        {"TCL AC SWING OFF", Company::TCL, DeviceType::AC, Task::SwingOff, tclSwingOff},
        {"MIDEA AC ON",       Company::Midea, DeviceType::AC, Task::On,      mideaOn},
        {"MIDEA AC OFF",      Company::Midea, DeviceType::AC, Task::Off,     mideaOff},
        {"MIDEA AC SET16C",   Company::Midea, DeviceType::AC, Task::Set16C,  mideaSet16C},
        {"MIDEA AC SWING ON", Company::Midea, DeviceType::AC, Task::SwingOn, mideaSwingOn},
        {"MIDEA AC SWING OFF",Company::Midea, DeviceType::AC, Task::SwingOff,mideaSwingOff},

        {"DAIKIN280 ON",        Company::Daikin, DeviceType::AC, Task::On,       daikinOn},
        {"DAIKIN280 OFF",       Company::Daikin, DeviceType::AC, Task::Off,      daikinOff},
        {"DAIKIN280 SET16C",    Company::Daikin, DeviceType::AC, Task::Set16C,   daikinSet16C},
        {"DAIKIN280 SWING ON",  Company::Daikin, DeviceType::AC, Task::SwingOn,  daikinSwingOn},
        {"DAIKIN280 SWING OFF", Company::Daikin, DeviceType::AC, Task::SwingOff, daikinSwingOff},

        {"DAIKIN312 ON",        Company::Daikin, DeviceType::AC, Task::On,       daikin2On},
        {"DAIKIN312 OFF",       Company::Daikin, DeviceType::AC, Task::Off,      daikin2Off},
        {"DAIKIN312 SET16C",    Company::Daikin, DeviceType::AC, Task::Set16C,   daikin2Set16C},
        {"DAIKIN312 SWING ON",  Company::Daikin, DeviceType::AC, Task::SwingOn,  daikin2SwingOn},
        {"DAIKIN312 SWING OFF", Company::Daikin, DeviceType::AC, Task::SwingOff, daikin2SwingOff},

        {"DAIKIN216 ON",        Company::Daikin, DeviceType::AC, Task::On,       daikin216On},
        {"DAIKIN216 OFF",       Company::Daikin, DeviceType::AC, Task::Off,      daikin216Off},
        {"DAIKIN216 SET16C",    Company::Daikin, DeviceType::AC, Task::Set16C,   daikin216Set16C},
        {"DAIKIN216 SWING ON",  Company::Daikin, DeviceType::AC, Task::SwingOn,  daikin216SwingOn},
        {"DAIKIN216 SWING OFF", Company::Daikin, DeviceType::AC, Task::SwingOff, daikin216SwingOff},

        {"DAIKIN160 ON",        Company::Daikin, DeviceType::AC, Task::On,       daikin160On},
        {"DAIKIN160 OFF",       Company::Daikin, DeviceType::AC, Task::Off,      daikin160Off},
        {"DAIKIN160 SET16C",    Company::Daikin, DeviceType::AC, Task::Set16C,   daikin160Set16C},
        {"DAIKIN160 SWING ON",  Company::Daikin, DeviceType::AC, Task::SwingOn,  daikin160SwingOn},
        {"DAIKIN160 SWING OFF", Company::Daikin, DeviceType::AC, Task::SwingOff, daikin160SwingOff},

        {"DAIKIN176 ON",        Company::Daikin, DeviceType::AC, Task::On,       daikin176On},
        {"DAIKIN176 OFF",       Company::Daikin, DeviceType::AC, Task::Off,      daikin176Off},
        {"DAIKIN176 SET16C",    Company::Daikin, DeviceType::AC, Task::Set16C,   daikin176Set16C},
        {"DAIKIN176 SWING ON",  Company::Daikin, DeviceType::AC, Task::SwingOn,  daikin176SwingOn},
        {"DAIKIN176 SWING OFF", Company::Daikin, DeviceType::AC, Task::SwingOff, daikin176SwingOff},

        {"DAIKIN128 ON",        Company::Daikin, DeviceType::AC, Task::On,       daikin128On},
        {"DAIKIN128 OFF",       Company::Daikin, DeviceType::AC, Task::Off,      daikin128Off},
        {"DAIKIN128 SET16C",    Company::Daikin, DeviceType::AC, Task::Set16C,   daikin128Set16C},
        {"DAIKIN128 SWING ON",  Company::Daikin, DeviceType::AC, Task::SwingOn,  daikin128SwingOn},
        {"DAIKIN128 SWING OFF", Company::Daikin, DeviceType::AC, Task::SwingOff, daikin128SwingOff},

        {"DAIKIN152 ON",        Company::Daikin, DeviceType::AC, Task::On,       daikin152On},
        {"DAIKIN152 OFF",       Company::Daikin, DeviceType::AC, Task::Off,      daikin152Off},
        {"DAIKIN152 SET16C",    Company::Daikin, DeviceType::AC, Task::Set16C,   daikin152Set16C},
        {"DAIKIN152 SWING ON",  Company::Daikin, DeviceType::AC, Task::SwingOn,  daikin152SwingOn},
        {"DAIKIN152 SWING OFF", Company::Daikin, DeviceType::AC, Task::SwingOff, daikin152SwingOff},

        {"DAIKIN64 ON",        Company::Daikin, DeviceType::AC, Task::On,       daikin64On},
        {"DAIKIN64 OFF",       Company::Daikin, DeviceType::AC, Task::Off,      daikin64Off},
        {"DAIKIN64 SET16C",    Company::Daikin, DeviceType::AC, Task::Set16C,   daikin64Set16C},
        {"DAIKIN64 SWING ON",  Company::Daikin, DeviceType::AC, Task::SwingOn,  daikin64SwingOn},
        {"DAIKIN64 SWING OFF", Company::Daikin, DeviceType::AC, Task::SwingOff, daikin64SwingOff},

        {"DAIKIN NORMAL ON",   Company::Daikin, DeviceType::AC, Task::On,       daikinOn},
        {"DAIKIN NORMAL OFF",  Company::Daikin, DeviceType::AC, Task::Off,      daikinOff},
    };
    static const uint8_t PRESET_COUNT = sizeof(presets) / sizeof(presets[0]);
    

    static const char* const companyNames[] = {
        "HAIER", "GREE", "PANASONIC", "MITSUBISHI", "SAMSUNG", "LG", "TCL", "SONY", "MIDEA", "DAIKIN", "NOBEL",
        "TOSHIBA", "PHILIPS", "SHARP", "JVC", "HITACHI", "SANYO", "HISENSE", "VIZIO"
    };
    static const char* const deviceTypeNames[] = { "AC", "TV" };
    static const char* const taskNames[] = {
        "ON", "OFF", "SET 16C", "SWING ON", "SWING OFF", "FAN OFF"
    };
    // ── Collect all OFF signals from all companies into a flat list ──
    // We'll use the existing getFilteredSignalCount/getFilteredSignalName/sendFilteredSignal
    // to iterate everything during the flooder loop.

    // Forward declarations needed by flooderTick()
    static Mode currentMode = Mode::Idle;
    static String sendResult = "";

    static void flooderTick() {
        if (!flooderRunning) return;

        // Build a flat list of all OFF signals across every company (AC only)
        // We iterate companies, device=AC, task=Off
        // We keep a global counter across all companies
        uint8_t totalSignals = 0;
        
        // First, count all OFF signals across all companies
        for (uint8_t c = 0; c < (uint8_t)Company::COUNT; c++) {
            Company comp = (Company)c;
            // Skip Sony (no AC OFF), skip any company with no AC signals
            totalSignals += getFilteredSignalCount(comp, DeviceType::AC, Task::Off);
        }
        
        if (totalSignals == 0) {
            flooderRunning = false;
            currentMode = Mode::Idle;
            sendResult = String("FLOODER: no OFF signals found");
            return;
        }
        
        // Wrap around if past the end
        if (flooderIndex >= totalSignals) {
            flooderIndex = 0;
        }
        
        // Find which company/company-internal-index corresponds to flooderIndex
        uint8_t globalOffset = 0;
        for (uint8_t c = 0; c < (uint8_t)Company::COUNT; c++) {
            Company comp = (Company)c;
            uint8_t count = getFilteredSignalCount(comp, DeviceType::AC, Task::Off);
            
            if (flooderIndex < globalOffset + count) {
                uint8_t localIndex = flooderIndex - globalOffset;
                
                // Send this signal
                sendFilteredSignal(comp, DeviceType::AC, Task::Off, localIndex);
                currentMode = Mode::Flooder;
                
                Serial.print("FLOODER: ");
                Serial.print(getCompanyName(c));
                Serial.print(" OFF #");
                Serial.println(localIndex);
                
                flooderIndex++;
                break;
            }
            globalOffset += count;
        }
        
        flooderLastSend = millis();
    }

    static const uint8_t MAX_SIGNALS = 30;
    static int cachedSignalCount = 0;
    static String cachedNames[MAX_SIGNALS];
    // currentMode and sendResult are declared above (before flooderTick)

    // ── UI navigation state (company/device/task/protocol browsing) ──
    struct NavFrame {
        Screen     screen;
        Company    company;
        DeviceType device;
        Task       task;
    };

    static const uint8_t MAX_NAV_DEPTH = 6;
    static NavFrame navStack[MAX_NAV_DEPTH];
    static int8_t   navTop = -1;

    static bool    currentIsCustom    = false;  // is the shown signal a raw saved one (Custom) or company/device/task one?
    static uint8_t currentSignalIndex = 0;      // index into either the filtered list or the raw saved-signal list
    static bool    signalSentFlag     = false;  // true = SignalReady page is showing "SENT" state

    // Tag chosen for the signal currently pending a save.
    static Company    pendingCompany = Company::Haier;
    static DeviceType pendingDevice  = DeviceType::AC;
    static Task       pendingTask    = Task::On;

    // ── Graph for analyser view ──
    static uint8_t graph[64] = {0};
    static uint8_t graphPos = 0;

    // ── Pending (just-captured) signal ──
    static uint16_t pendingPulses[IR_MAX_PULSES * 2];  // mark0,space0,mark1,space1,...
    static uint16_t pendingCount = 0;                  // total entries used in pendingPulses[]
    static String   pendingName;

    // ── Capture state ──
    static bool     lastPinState = HIGH;
    static uint32_t lastChangeTime = 0;
    static bool     captureActive = false;

    // ── File paths ──
    static const char* storagePath = "/ir_signals.json";
    static const char* tmpPath     = "/ir_signals.tmp";

    // ── Graph helpers ──
    static void rotateGraph(uint8_t value) {
        graph[graphPos++] = value;
        if (graphPos >= sizeof(graph)) graphPos = 0;
    }

    static uint8_t getWrappedGraphValue(uint8_t index) {
        if (index >= sizeof(graph)) return 0;
        int pos = graphPos + index;
        if (pos >= (int)sizeof(graph)) pos -= sizeof(graph);
        return graph[pos];
    }

    // ── Persistence & RAM Caching ──
    void refreshCachedNames() {
        cachedSignalCount = 0;
        if (!LittleFS.exists(storagePath)) return;

        File f = LittleFS.open(storagePath, "r");
        if (!f) return;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, f);
        f.close();

        if (error || !doc.is<JsonArray>()) return;
        JsonArray arr = doc.as<JsonArray>();
        cachedSignalCount = arr.size();
        for (int i = 0; i < cachedSignalCount && i < MAX_SIGNALS; i++) {
            cachedNames[i] = String(arr[i]["name"].as<const char*>());
        }
    }

    String nextSignalName() {
        int nextIndex = 1;
        while (true) {
            String candidate = String("ir ") + String(nextIndex);
            bool found = false;
            for (int i = 0; i < cachedSignalCount; i++) {
                if (cachedNames[i] == candidate) {
                    found = true;
                    break;
                }
            }
            if (!found) return candidate;
            nextIndex++;
        }
    }

    // ── Does a saved-signal JSON entry match this company/device/task tag? ──
    static bool signalMatches(JsonObject obj, Company company, DeviceType device, Task task) {
        uint8_t c = obj["c"].as<uint8_t>();
        uint8_t d = obj["d"].as<uint8_t>();
        uint8_t t = obj["t"].as<uint8_t>();
        return c == (uint8_t)company && d == (uint8_t)device && t == (uint8_t)task;
    }

    // ── Save to LittleFS ──
    static bool saveSignalToFile(const String& name, const uint16_t* pulses, uint16_t count,
                                  Company company, DeviceType device, Task task) {
        JsonDocument doc;
        JsonArray arr;

        if (LittleFS.exists(storagePath)) {
            File f = LittleFS.open(storagePath, "r");
            if (f) {
                DeserializationError error = deserializeJson(doc, f);
                f.close();
                if (!error && doc.is<JsonArray>()) {
                    arr = doc.as<JsonArray>();
                }
            }
        }

        if (arr.isNull()) {
            arr = doc.to<JsonArray>();
        }

        // Check if name exists already → update
        bool updated = false;
        for (JsonObject obj : arr) {
            if (String(obj["name"].as<const char*>()) == name) {
                obj["count"] = count;
                obj["c"] = (uint8_t)company;
                obj["d"] = (uint8_t)device;
                obj["t"] = (uint8_t)task;
                obj.remove("pulses");
                JsonArray pArr = obj["pulses"].to<JsonArray>();
                for (uint16_t i = 0; i < count; i++) {
                    pArr.add(pulses[i]);
                }
                updated = true;
                break;
            }
        }

        if (!updated) {
            JsonObject entry = arr.add<JsonObject>();
            entry["name"] = name;
            entry["count"] = count;
            entry["c"] = (uint8_t)company;
            entry["d"] = (uint8_t)device;
            entry["t"] = (uint8_t)task;
            JsonArray pArr = entry["pulses"].to<JsonArray>();
            for (uint16_t i = 0; i < count; i++) {
                pArr.add(pulses[i]);
            }
        }

        File fw = LittleFS.open(storagePath, "w");
        if (!fw) {
            Serial.println(F("IR save failed: unable to open storage file"));
            return false;
        }
        serializeJson(doc, fw);
        fw.close();

        refreshCachedNames();

        Serial.print(F("IR saved: "));
        Serial.print(name);
        Serial.print(F(" pulses="));
        Serial.println(count);
        return true;
    }

    // ── Load from LittleFS ──
    static bool loadSignalPulses(uint8_t index, String& outName, uint16_t* outPulses, uint16_t& outCount) {
        if (!LittleFS.exists(storagePath)) return false;
        File f = LittleFS.open(storagePath, "r");
        if (!f) return false;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, f);
        f.close();
        if (error || !doc.is<JsonArray>()) return false;
        JsonArray arr = doc.as<JsonArray>();
        if (index >= arr.size()) return false;

        JsonObject obj = arr[index];
        outName = String(obj["name"].as<const char*>());
        outCount = obj["count"].as<uint16_t>();
        if (outCount > IR_MAX_PULSES * 2) outCount = IR_MAX_PULSES * 2;

        JsonArray pArr = obj["pulses"];
        for (uint16_t i = 0; i < outCount && i < pArr.size(); i++) {
            outPulses[i] = pArr[i].as<uint16_t>();
        }
        return true;
    }

    static String loadSignalName(uint8_t index) {
        if (index < cachedSignalCount) return cachedNames[index];
        return String();
    }

    // ── Replay a raw mark/space pulse train using IRremoteESP8266 library ──
    // The library handles 38kHz carrier modulation with correct duty cycle,
    // proper timing, and no watchdog issues. Much more reliable than bit-banging.
    static void replayPulses(const uint16_t* pulses, uint16_t count) {
        for (uint8_t r = 0; r < IR_SEND_REPEATS; r++) {
            // sendRaw expects microsecond durations: [mark, space, mark, space, ...]
            // Our buffer is already in microseconds (IR_RESOLUTION = 1)
            rawSend.sendRaw(pulses, count, 38);  // 38 kHz carrier
            if (r + 1 < IR_SEND_REPEATS) {
                delay(40);  // 40ms gap between repeats
            }
        }
    }


    void begin() {
        if (!LittleFS.begin(true)) Serial.println("LittleFS Mount Failed");
        // SD card initialization is managed globally in setup() via sdcard_init()
        pinMode(IR_RX_PIN, INPUT);
        pinMode(IR_TX_PIN, OUTPUT);
        digitalWrite(IR_TX_PIN, LOW);
        currentMode = Mode::Idle;
        memset(graph, 0, sizeof(graph));

        // IRrecv handles the RX pin setup internally
        // Don't enable it yet — only when Analyse/Record mode starts

        acStd.begin();
        ac136.begin();
        ac112.begin();

        haierAc.begin();
        haierYrw02.begin();
        haier176.begin();
        haier160.begin();

        greeYaw1f.begin();
        greeYbofb.begin();
        greeYx1fsf.begin();

        panasonicAc.begin();
        panasonicAc32.begin();

        samsungAc.begin();

        lgGe6711.begin();
        lg6711a.begin();
        lgAkb75215403.begin();
        lgAkb74955603.begin();
        lgAkb73757604.begin();
        lgGe6711.setModel(lg_ac_remote_model_t::GE6711AR2853M);
        lg6711a.setModel(lg_ac_remote_model_t::LG6711A20083V);
        lgAkb75215403.setModel(lg_ac_remote_model_t::AKB75215403);
        lgAkb74955603.setModel(lg_ac_remote_model_t::AKB74955603);
        lgAkb73757604.setModel(lg_ac_remote_model_t::AKB73757604);

        tclAc.begin();
        mideaAc.begin();   
        rawSend.begin();

        refreshCachedNames();
        Serial.println(F("IR Cloner ready. Type 'help' for commands."));
    }

    void update() {
        // ── Flooder tick ──
        if (currentMode == Mode::Flooder && flooderRunning) {
            // Send immediately on first call, then every 500ms between signals
            if (flooderInitialSend || (millis() - flooderLastSend >= 500)) {
                flooderInitialSend = false;
                flooderTick();
            }
            return;  // Don't process Analyse/Record while flooding
        }
        if (currentMode != Mode::Analyse && currentMode != Mode::Record) return;

        bool currentPin = digitalRead(IR_RX_PIN);

        if (currentPin != lastPinState) {
            uint32_t now = micros();
            uint32_t duration = now - lastChangeTime;

            if (duration > IR_NOISE_FILTER) {
                if (captureActive && pendingCount < IR_MAX_PULSES * 2) {
                    pendingPulses[pendingCount++] = duration / IR_RESOLUTION;
                }

                // First transition → start capture
                if (!captureActive && lastPinState == HIGH && currentPin == LOW) {
                    captureActive = true;
                    pendingCount = 0;
                }

                lastChangeTime = now;
                lastPinState = currentPin;

                // Feed analyser graph
                if (currentMode == Mode::Analyse) {
                    uint8_t intensity = min((uint8_t)63, (uint8_t)(16 + (pendingCount / 4)));
                    rotateGraph(intensity);
                }
            }
        }

        // Check for timeout → signal capture complete
        if (captureActive && pendingCount > 0 && (micros() - lastChangeTime > IR_TIMEOUT)) {
            captureActive = false;

            if (currentMode == Mode::Analyse) {
                // Just keep updating graph, reset for next burst
                Serial.print("IR analyse: ");
                Serial.print(pendingCount);
                Serial.println(" transitions");
                pendingCount = 0;
            } else if (currentMode == Mode::Record) {
                Serial.print("IR captured: ");
                Serial.print(pendingCount);
                Serial.println(" transitions");
                currentMode = Mode::SaveConfirm;
            }
        }
    }

    void startAnalyse() {
        currentMode = Mode::Analyse;
        sendResult = String();
        memset(graph, 0, sizeof(graph));
        pendingCount = 0;
        captureActive = false;
        lastPinState = digitalRead(IR_RX_PIN);
        lastChangeTime = micros();
    }

    void startRecord() {
        currentMode = Mode::Record;
        pendingCount = 0;
        captureActive = false;
        lastPinState = digitalRead(IR_RX_PIN);
        lastChangeTime = micros();
        // Reset the tag to a sensible default; change with setPendingXxx()
        // before acceptSave() if the UI lets the user pick company/device/task.
        pendingCompany = Company::Haier;
        pendingDevice  = DeviceType::AC;
        pendingTask    = Task::On;
    }

    void stop() {
        if (currentMode != Mode::Idle) {
            currentMode = Mode::Idle;
            pendingCount = 0;
            captureActive = false;
            sendResult = String();
            memset(graph, 0, sizeof(graph));
        }
    }
    void startFlooder() {
        currentMode = Mode::Flooder;
        flooderIndex = 0;
        flooderRunning = true;
        flooderInitialSend = true;
        flooderLastSend = 0;
        sendResult = String("FLOODER STARTED");
        Serial.println("IR flooder started");
    }

    void stopFlooder() {
        flooderRunning = false;
        if (currentMode == Mode::Flooder) {
            currentMode = Mode::Idle;
        }
        sendResult = String("FLOODER STOPPED");
        Serial.println("IR flooder stopped");
    }
    Mode getMode() {
        return currentMode;
    }

    String getStatus() {
        switch (currentMode) {
            case Mode::Analyse:     return String("LISTENING");
            case Mode::Record:      return String("RECORD");
            case Mode::SaveConfirm: return String("CAPTURED");
            case Mode::SendResult:  return String("SENT");
            case Mode::Flooder:     return flooderRunning ? String("FLOODING") : String("IDLE");
            default:                return String("IDLE");
        }
    }

    String getDetailLine() {
        if (currentMode == Mode::Flooder) {
            return String("Signal #") + String(flooderIndex);
        }
        if (pendingCount == 0) return String("WAIT...");
        return String(pendingCount) + String(" pulses");
    }

    String getPendingSummary() {
        if (pendingCount == 0) return String("NO SIGNAL");
        String name = nextSignalName();
        return name + " " + String(pendingCount) + "p";
    }

    String getSendResult() {
        return sendResult;
    }

    uint8_t getGraphSize() {
        return sizeof(graph);
    }

    uint8_t getGraphValue(uint8_t index) {
        return getWrappedGraphValue(index);
    }

    bool hasSignals() {
        return cachedSignalCount > 0;
    }

    uint8_t getSignalCount() {
        return cachedSignalCount;
    }

    String getSignalName(uint8_t index) {
        if (index < cachedSignalCount) return cachedNames[index];
        return String();
    }

    uint8_t getPresetCount() {
        return PRESET_COUNT;
    }

    String getPresetName(uint8_t index) {
        if (index >= PRESET_COUNT) return String();
        return String(presets[index].name);
    }

    bool sendPreset(uint8_t index) {
        if (index >= PRESET_COUNT) return false;
        presets[index].sendFunc();
        sendResult = String(presets[index].name) + String(" sent");
        currentMode = Mode::SendResult;
        return true;
    }

    bool sendSignal(uint8_t index) {
        String name;
        uint16_t pulses[IR_MAX_PULSES * 2];
        uint16_t count = 0;

        if (!loadSignalPulses(index, name, pulses, count)) {
            sendResult = String("FAILED");
            currentMode = Mode::SendResult;
            return false;
        }

        Serial.print("IR sending: ");
        Serial.print(name);
        Serial.print(" (");
        Serial.print(count);
        Serial.println(" transitions)");

        replayPulses(pulses, count);

        sendResult = name + String(" sent");
        currentMode = Mode::SendResult;
        return true;
    }

    bool acceptSave(String name) {
        if (currentMode != Mode::SaveConfirm) return false;

        uint8_t count = getSignalCount();
        if (count >= MAX_SIGNALS) {
            Serial.print("IR save rejected: already have ");
            Serial.print(count);
            Serial.print(" signals (max ");
            Serial.print(MAX_SIGNALS);
            Serial.println(")");
            currentMode = Mode::Idle;
            return false;
        }

        if (name == "") name = nextSignalName();
        pendingName = name;
        if (!saveSignalToFile(name, pendingPulses, pendingCount,
                               pendingCompany, pendingDevice, pendingTask)) {
            Serial.println("IR save failed: unable to persist signal");
            return false;
        }
        Serial.println("IR save complete: " + name);
        currentMode = Mode::Idle;
        updateMenuCache();
        return true;
    }

    void rejectSave() {
        if (currentMode == Mode::SaveConfirm) {
            currentMode = Mode::Idle;
        }
    }

    bool deleteSignal(uint8_t index) {
        if (!LittleFS.exists(storagePath) || index >= cachedSignalCount) return false;

        File f = LittleFS.open(storagePath, "r");
        if (!f) return false;

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, f);
        f.close();
        if (error || !doc.is<JsonArray>()) return false;

        JsonArray arr = doc.as<JsonArray>();
        if (index >= arr.size()) return false;

        String deletedName = String(arr[index]["name"].as<const char*>());
        arr.remove(index);

        File fw = LittleFS.open(storagePath, "w");
        if (!fw) return false;
        serializeJson(doc, fw);
        fw.close();

        refreshCachedNames();

        Serial.print(F("IR deleted signal: "));
        Serial.println(deletedName);
        updateMenuCache();
        return true;
    }

    static void processCmd(String line) {
        line.trim();
        if (!line.length()) return;
        String lower = line; lower.toLowerCase();

        if (lower == "help" || lower == "?") {
            Serial.println(F("\n--- IR Cloner Commands ---"));
            Serial.println(F("list                 - show saved signals"));
            Serial.println(F("send <name>          - transmit a signal"));
            Serial.println(F("delete <name>        - delete a signal"));
            Serial.println(F("---------------------------\n"));
            return;
        }
        if (lower == "list") {
            Serial.print(F("Count: ")); Serial.println(cachedSignalCount);
            for (int i = 0; i < cachedSignalCount; i++) Serial.println(cachedNames[i]);
            return;
        }
        if (lower.startsWith("send ")) {
            String n = line.substring(5); n.trim();
            for (int i = 0; i < cachedSignalCount; i++) {
                if (cachedNames[i].equalsIgnoreCase(n)) {
                    sendSignal(i);
                    Serial.println(F("Sent."));
                    return;
                }
            }
            Serial.println(F("Not found."));
            return;
        }
        if (lower.startsWith("delete ")) {
            String n = line.substring(7); n.trim();
            for (int i = 0; i < cachedSignalCount; i++) {
                if (cachedNames[i].equalsIgnoreCase(n)) {
                    deleteSignal(i);
                    Serial.println(F("Deleted."));
                    return;
                }
            }
            Serial.println(F("Not found."));
            return;
        }
    }

    static String serialBuf = "";
    void handleSerial() {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\r') continue;
            if (c == '\n') {
                processCmd(serialBuf);
                serialBuf = "";
            } else {
                serialBuf += c;
                if (serialBuf.length() > 80) serialBuf = "";
            }
        }
    }


    uint8_t getCompanyCount() {
        return (uint8_t)Company::COUNT;
    }

    String getCompanyName(uint8_t companyIndex) {
        if (companyIndex >= (uint8_t)Company::COUNT) return String();
        return String(companyNames[companyIndex]);
    }

    uint8_t getDeviceTypeCount() {
        return (uint8_t)DeviceType::COUNT;
    }

    String getDeviceTypeName(uint8_t deviceIndex) {
        if (deviceIndex >= (uint8_t)DeviceType::COUNT) return String();
        return String(deviceTypeNames[deviceIndex]);
    }

    uint8_t getTaskCount() {
        return (uint8_t)Task::COUNT;
    }

    String getTaskName(uint8_t taskIndex) {
        if (taskIndex >= (uint8_t)Task::COUNT) return String();
        return String(taskNames[taskIndex]);
    }

    uint8_t getFilteredSignalCount(Company company, DeviceType device, Task task) {
        uint8_t count = 0;

        for (uint8_t i = 0; i < PRESET_COUNT; i++) {
            if (presets[i].company == company && presets[i].device == device &&
                presets[i].task == task) {
                count++;
            }
        }

        File f = LittleFS.open(storagePath, FILE_READ);
        if (f) {
            String content = f.readString();
            f.close();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, content);
            if (!error && doc.is<JsonArray>()) {
                JsonArray arr = doc.as<JsonArray>();
                for (JsonObject obj : arr) {
                    if (signalMatches(obj, company, device, task)) count++;
                }
            }
        }
        return count;
    }

    String getFilteredSignalName(Company company, DeviceType device, Task task, uint8_t index) {
        uint8_t seen = 0;
        for (uint8_t i = 0; i < PRESET_COUNT; i++) {
            if (presets[i].company == company && presets[i].device == device &&
                presets[i].task == task) {
                if (seen == index) return String(presets[i].name);
                seen++;
            }
        }

        uint8_t savedTarget = index - seen;
        File f = LittleFS.open(storagePath, FILE_READ);
        if (!f) return String();
        String content = f.readString();
        f.close();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, content);
        if (error || !doc.is<JsonArray>()) return String();
        JsonArray arr = doc.as<JsonArray>();

        uint8_t savedSeen = 0;
        for (JsonObject obj : arr) {
            if (signalMatches(obj, company, device, task)) {
                if (savedSeen == savedTarget) {
                    return String(obj["name"].as<const char*>());
                }
                savedSeen++;
            }
        }
        return String();
    }

    bool sendFilteredSignal(Company company, DeviceType device, Task task, uint8_t index) {
        uint8_t seen = 0;
        for (uint8_t i = 0; i < PRESET_COUNT; i++) {
            if (presets[i].company == company && presets[i].device == device &&
                presets[i].task == task) {
                if (seen == index) {
                    presets[i].sendFunc();
                    sendResult = String(presets[i].name) + String(" sent");
                    currentMode = Mode::SendResult;
                    return true;
                }
                seen++;
            }
        }

        uint8_t savedTarget = index - seen;

        File f = LittleFS.open(storagePath, FILE_READ);
        if (!f) {
            sendResult = String("FAILED");
            currentMode = Mode::SendResult;
            return false;
        }
        String content = f.readString();
        f.close();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, content);
        if (error || !doc.is<JsonArray>()) {
            sendResult = String("FAILED");
            currentMode = Mode::SendResult;
            return false;
        }
        JsonArray arr = doc.as<JsonArray>();

        uint8_t savedSeen = 0;
        for (uint16_t rawIndex = 0; rawIndex < arr.size(); rawIndex++) {
            JsonObject obj = arr[rawIndex];
            if (signalMatches(obj, company, device, task)) {
                if (savedSeen == savedTarget) {
                    String name;
                    uint16_t pulses[IR_MAX_PULSES * 2];
                    uint16_t count = 0;
                    if (!loadSignalPulses((uint8_t)rawIndex, name, pulses, count)) {
                        sendResult = String("FAILED");
                        currentMode = Mode::SendResult;
                        return false;
                    }
                    replayPulses(pulses, count);
                    sendResult = name + String(" sent");
                    currentMode = Mode::SendResult;
                    return true;
                }
                savedSeen++;
            }
        }

        sendResult = String("FAILED");
        currentMode = Mode::SendResult;
        return false;
    }


    void setPendingCompany(Company company) {
        if (company < Company::COUNT) pendingCompany = company;
    }

    void setPendingDevice(DeviceType device) {
        if (device < DeviceType::COUNT) pendingDevice = device;
    }

    void setPendingTask(Task task) {
        if (task < Task::COUNT) pendingTask = task;
    }

    Company getPendingCompany() { return pendingCompany; }
    DeviceType getPendingDevice() { return pendingDevice; }
    Task getPendingTask() { return pendingTask; }

    //
    //   Top list  →  companies + "CUSTOM"
    //     company →  Device list (AC / TV)   [only shown if the company makes both]
    //       device →  Task list (ON / OFF)   [only shown if both exist]
    //         task →  Protocol list          [only shown if more than one signal matches]
    //           protocol → Signal ready/send page
    //   "CUSTOM"  →  flat list of your recorded signals → Signal ready/send page
    //
    // Sending does NOT pop the navigation stack — it just flags the current
    // page as "sent". The first Back press after sending clears that flag and
    // reveals the same ready-to-send page again (so it can be re-run); only a
    // second Back press actually moves up to the previous list.

    static std::vector<String> cachedItemNames;
    static String cachedTitle;

    static void updateMenuCache() {
        cachedItemNames.clear();
        if (navTop < 0) {
            cachedTitle = String("IR");
            return;
        }
        const NavFrame& f = navStack[navTop];
        
        switch (f.screen) {
            case Screen::TopList:      cachedTitle = String("SELECT DEVICE"); break;
            case Screen::DeviceList:   cachedTitle = getCompanyName((uint8_t)f.company) + String(" - AC/TV"); break;
            case Screen::TaskList:     cachedTitle = getCompanyName((uint8_t)f.company) + String(" ") + getDeviceTypeName((uint8_t)f.device); break;
            case Screen::ProtocolList: cachedTitle = getCompanyName((uint8_t)f.company) + String(" ") + getDeviceTypeName((uint8_t)f.device) + String(" ") + getTaskName((uint8_t)f.task); break;
            case Screen::CustomList:   cachedTitle = String("CUSTOM SIGNALS"); break;
            case Screen::SignalReady:  cachedTitle = String("TRANSMIT"); break;
            default: cachedTitle = String(); break;
        }
        
        bool needsJson = (f.screen == Screen::TaskList || f.screen == Screen::ProtocolList || f.screen == Screen::CustomList || f.screen == Screen::SignalReady);
        
        JsonDocument doc;
        JsonArray arr;
        if (needsJson) {
            File file = LittleFS.open(storagePath, FILE_READ);
            if (file) {
                String fcontent = file.readString();
                file.close();
                DeserializationError error = deserializeJson(doc, fcontent);
                if (!error && doc.is<JsonArray>()) {
                    arr = doc.as<JsonArray>();
                }
            }
        }
        
        switch (f.screen) {
            case Screen::RootList:
                cachedItemNames.push_back(String("RECORD"));
                cachedItemNames.push_back(String("LIST"));
                break;
            case Screen::TopList: {
                uint8_t companyCount = getCompanyCount();
                for (uint8_t i = 0; i < companyCount; i++) cachedItemNames.push_back(getCompanyName(i));
                cachedItemNames.push_back(String("CUSTOM"));
                cachedItemNames.push_back(String("FLOOD OFF"));
                break;
            }
            case Screen::DeviceList:
                cachedItemNames.push_back(getDeviceTypeName(0));
                cachedItemNames.push_back(getDeviceTypeName(1));
                break;
            case Screen::TaskList: {
                for (uint8_t t = 0; t < (uint8_t)Task::COUNT; t++) {
                    bool has = false;
                    for (uint8_t i = 0; i < PRESET_COUNT; i++) {
                        if (presets[i].company == f.company && presets[i].device == f.device && presets[i].task == (Task)t) { has = true; break; }
                    }
                    if (!has && !arr.isNull()) {
                        for (JsonObject obj : arr) {
                            if (signalMatches(obj, f.company, f.device, (Task)t)) { has = true; break; }
                        }
                    }
                    if (has) cachedItemNames.push_back(getTaskName(t));
                }
                break;
            }
            case Screen::ProtocolList: {
                for (uint8_t i = 0; i < PRESET_COUNT; i++) {
                    if (presets[i].company == f.company && presets[i].device == f.device && presets[i].task == f.task) {
                        cachedItemNames.push_back(String(presets[i].name));
                    }
                }
                if (!arr.isNull()) {
                    for (JsonObject obj : arr) {
                        if (signalMatches(obj, f.company, f.device, f.task)) {
                            cachedItemNames.push_back(String(obj["name"].as<const char*>()));
                        }
                    }
                }
                break;
            }
            case Screen::CustomList: {
                for (int i = 0; i < cachedSignalCount; i++) {
                    cachedItemNames.push_back(cachedNames[i]);
                }
                break;
            }
            case Screen::SignalReady:
                if (currentIsCustom) {
                    if (currentSignalIndex < cachedSignalCount) {
                        cachedItemNames.push_back(cachedNames[currentSignalIndex]);
                    } else {
                        cachedItemNames.push_back(String("Custom Signal"));
                    }
                } else {
                    uint8_t seen = 0;
                    String foundName = String();
                    for (uint8_t i = 0; i < PRESET_COUNT; i++) {
                        if (presets[i].company == f.company && presets[i].device == f.device && presets[i].task == f.task) {
                            if (seen == currentSignalIndex) { foundName = String(presets[i].name); break; }
                            seen++;
                        }
                    }
                    if (foundName.length() == 0 && !arr.isNull()) {
                        for (JsonObject obj : arr) {
                            if (signalMatches(obj, f.company, f.device, f.task)) {
                                if (seen == currentSignalIndex) { foundName = String(obj["name"].as<const char*>()); break; }
                                seen++;
                            }
                        }
                    }
                    if (foundName.length() == 0) {
                        foundName = String(getTaskName((uint8_t)f.task));
                    }
                    cachedItemNames.push_back(foundName);
                }
                break;
        }
    }


    static void pushFrame(Screen screen, Company company = Company::Haier,
                           DeviceType device = DeviceType::AC, Task task = Task::On) {
        if (navTop + 1 < MAX_NAV_DEPTH) {
            navTop++;
            navStack[navTop] = { screen, company, device, task };
            updateMenuCache();
        }
    }
    static bool taskHasSignal(Company company, DeviceType device, Task task) {
        return getFilteredSignalCount(company, device, task) > 0;
    }

    static Task getNthAvailableTask(Company company, DeviceType device, uint8_t n) {
        uint8_t seen = 0;
        for (uint8_t t = 0; t < (uint8_t)Task::COUNT; t++) {
            if (taskHasSignal(company, device, (Task)t)) {
                if (seen == n) return (Task)t;
                seen++;
            }
        }
        return Task::On;
    }
    static bool companyHasDevice(Company company, DeviceType device) {
        for (uint8_t i = 0; i < PRESET_COUNT; i++) {
            if (presets[i].company == company && presets[i].device == device) return true;
        }
        File f = LittleFS.open(storagePath, FILE_READ);
        if (f) {
            String content = f.readString();
            f.close();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, content);
            if (!error && doc.is<JsonArray>()) {
                JsonArray arr = doc.as<JsonArray>();
                for (JsonObject obj : arr) {
                    if (obj["c"].as<uint8_t>() == (uint8_t)company && obj["d"].as<uint8_t>() == (uint8_t)device) return true;
                }
            }
        }
        return false;
    }

    static void resolveProtocol(Company company, DeviceType device, Task task) {
        uint8_t count = getFilteredSignalCount(company, device, task);
        if (count > 1) {
            pushFrame(Screen::ProtocolList, company, device, task);
        } else if (count == 1) {
            currentIsCustom    = false;
            currentSignalIndex = 0;
            signalSentFlag     = false;
            pushFrame(Screen::SignalReady, company, device, task);
        }
        // count == 0: nothing to show; caller should have checked availability first
    }

    static void resolveTask(Company company, DeviceType device) {
        uint8_t availableCount = 0;
        Task firstAvailable = Task::On;
        for (uint8_t t = 0; t < (uint8_t)Task::COUNT; t++) {
            if (taskHasSignal(company, device, (Task)t)) {
                if (availableCount == 0) firstAvailable = (Task)t;
                availableCount++;
            }
        }
        if (availableCount > 1) {
            pushFrame(Screen::TaskList, company, device);
        } else if (availableCount == 1) {
            resolveProtocol(company, device, firstAvailable);
        }
        // 0 available: nothing to do
    }

    void menuEnter() {
        navTop = -1;
        currentIsCustom    = false;
        currentSignalIndex = 0;
        signalSentFlag     = false;
        pushFrame(Screen::RootList);
    }

    Screen menuGetScreen() {
        return navTop >= 0 ? navStack[navTop].screen : Screen::TopList;
    }

    String menuGetCurrentSignalName() {
        if (cachedItemNames.size() > 0) return cachedItemNames[0];
        return String();
    }

    bool menuIsSent() {
        return signalSentFlag;
    }

    String menuGetSendResultText() {
        return sendResult;
    }

    String menuGetTitle() {
        return cachedTitle;
    }

    uint8_t menuGetItemCount() {
        return cachedItemNames.size();
    }

    String menuGetItemName(uint8_t index) {
        if (index < cachedItemNames.size()) return cachedItemNames[index];
        return String();
    }

    bool menuSend() {
        if (navTop < 0 || navStack[navTop].screen != Screen::SignalReady) return false;

        bool ok;
        if (currentIsCustom) {
            ok = sendSignal(currentSignalIndex);
        } else {
            const NavFrame& f = navStack[navTop];
            ok = sendFilteredSignal(f.company, f.device, f.task, currentSignalIndex);
        }
        currentMode = Mode::Idle;
        signalSentFlag = true;
        updateMenuCache();
        return ok;
    }

    void menuSelect(uint8_t index) {
        if (navTop < 0) return;
        NavFrame current = navStack[navTop];

        switch (current.screen) {
            case Screen::RootList:
                if (index == 0) {
                    startRecord();
                } else if (index == 1) {
                    pushFrame(Screen::TopList);
                }
                break;
            case Screen::TopList: {
                if (index < getCompanyCount()) {
                    Company company = (Company)index;
                    bool hasAC = companyHasDevice(company, DeviceType::AC);
                    bool hasTV = companyHasDevice(company, DeviceType::TV);
                    if (hasAC && hasTV) {
                        pushFrame(Screen::DeviceList, company);
                    } else if (hasAC || hasTV) {
                        resolveTask(company, hasAC ? DeviceType::AC : DeviceType::TV);
                    }
                    // if neither device type has a signal, do nothing (empty company)
                } else if (index == getCompanyCount()) {
                    pushFrame(Screen::CustomList);
                } else if (index == getCompanyCount() + 1) {
                    // FLOOD OFF selected — start flooding immediately
                    startFlooder();
                }
                break;
            }
            case Screen::DeviceList:
                resolveTask(current.company, (DeviceType)index);
                break;
            case Screen::TaskList:
                resolveProtocol(current.company, current.device,
                                getNthAvailableTask(current.company, current.device, index));
                break;
            case Screen::ProtocolList:
                currentIsCustom    = false;
                currentSignalIndex = index;
                signalSentFlag     = false;
                pushFrame(Screen::SignalReady, current.company, current.device, current.task);
                break;
            case Screen::CustomList:
                currentIsCustom    = true;
                currentSignalIndex = index;
                signalSentFlag     = false;
                pushFrame(Screen::SignalReady);
                break;
            case Screen::SignalReady:
                menuSend();
                break;
        }
    }

    bool menuCanGoBack() {
        return navTop > 0;
    }

    void menuBack() {
        signalSentFlag = false;
        if (navTop > 0) {
            navTop--;
            updateMenuCache();
        }
    }
}

