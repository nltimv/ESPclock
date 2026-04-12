// ESPclock - Unified display driver
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

// This single file implements the display abstraction API (declared in
// display_api.h) for both the TM1637 and TM1652 display chips.
// The active driver is chosen at build time via a preprocessor flag:
//   -D DISPLAY_TM1637   (also requires -D DISPLAY_CLK=<pin> -D DISPLAY_DIO=<pin>)
//   -D DISPLAY_TM1652   (also requires -D DISPLAY_DATA_PIN=<pin> -D DISPLAY_DIGITS=<n>)

#include "display.h"

// ── Display-state globals (declared extern in display_api.h) ──────────────
bool    colon      = true;
bool    blink      = true;
bool    br_auto    = false;
bool    twelve     = false;
uint8_t brightness = 7;

// ── Non-blocking timer ─────────────────────────────────────────────────────
unsigned long myTimer(unsigned long everywhen) {
    static unsigned long t1;
    unsigned long diff_time = millis() - t1;
    int ret = 0;
    if (diff_time >= everywhen) {
        t1 = millis();
        ret = 1;
    }
    return ret;
}

// ══════════════════════════════════════════════════════════════════════════
//  TM1637 driver (used by ESP8266 and ESP32 TM1637 builds)
// ══════════════════════════════════════════════════════════════════════════
#if defined(DISPLAY_TM1637)

TM1637Display mydisplay(DISPLAY_CLK, DISPLAY_DIO);

// ── Fixed segment patterns ─────────────────────────────────────────────────
static const uint8_t SEG_try[] = {
    SEG_D | SEG_E | SEG_F | SEG_G,               // t
    SEG_E | SEG_G,                                // r
    SEG_B | SEG_C | SEG_D | SEG_F | SEG_G        // Y
};

static const uint8_t SEG_Err[] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,       // E
    SEG_E | SEG_G,                                // r
    SEG_E | SEG_G                                 // r
};

// ── Animation state ────────────────────────────────────────────────────────
static uint8_t       px         = 4;
static const uint8_t SEG_WAIT[] = { SEG_G };
static bool          forw       = true;   // true = sweeping right→left

// ── Display abstraction implementation ────────────────────────────────────

void displayInit() {
    mydisplay.setBrightness(7);
    mydisplay.clear();
}

void displayClear() {
    mydisplay.clear();
}

void displayShowError(uint8_t code) {
    mydisplay.setSegments(SEG_Err, 3, 0);
    mydisplay.showNumberDec(code, false, 1, 3);
}

void displayShowTrying() {
    mydisplay.setSegments(SEG_try, 3, 0);
}

void displayShowAttempt(uint8_t n) {
    mydisplay.showNumberDec(n, true, 1, 3);
}

void displaySetBrightness(uint8_t br) {
    mydisplay.setBrightness(br);
}

void displayShowTime(int hour, int minute, bool colonOn, bool twelveHr) {
    // 12-hr conversion: 0→12, 1-12→1-12, 13-23→1-11
    int dispHour = twelveHr ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    uint8_t colonMask = colonOn ? 0b01000000 : 0;
    mydisplay.showNumberDecEx(dispHour, colonMask, false, 2, 0);
    mydisplay.showNumberDecEx(minute,   colonMask, true,  2, 2);
}

void displayAnim() {
    if (myTimer(500)) {
        if (forw) {                         // sweep 4 → 0
            mydisplay.clear();
            mydisplay.setSegments(SEG_WAIT, 1, px);
            --px;
            if (px == 0) forw = false;
        } else {                            // sweep 0 → 3
            mydisplay.clear();
            mydisplay.setSegments(SEG_WAIT, 1, px);
            ++px;
            if (px == 3) forw = true;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════
//  TM1652 driver (used by ESP32 TM1652 builds)
// ══════════════════════════════════════════════════════════════════════════
#elif defined(DISPLAY_TM1652)

// module(data_pin, n_digits): low-level TM1652 driver
TM1652       module(DISPLAY_DATA_PIN, DISPLAY_DIGITS);
// display(&module, n_digits): higher-level helper for number/string output
TM16xxDisplay display(&module, DISPLAY_DIGITS);

// ── Animation state ────────────────────────────────────────────────────────
static uint8_t px   = 4;
static bool    forw = true;   // true = sweeping right→left

// ── Display abstraction implementation ────────────────────────────────────

void displayInit() {
    // begin(enabled, brightness 0-7, frequency selector 0-7)
    module.begin(true, 4, 6);
    display.clear();
}

void displayClear() {
    display.clear();
}

void displayShowError(uint8_t code) {
    display.setDisplayToString("Err", 0, 0);
    module.setDisplayDigit(code, 3, false);
}

void displayShowTrying() {
    display.setDisplayToString("trY", 0, 0);
}

void displayShowAttempt(uint8_t n) {
    module.setDisplayDigit(n, 3, false);
}

void displaySetBrightness(uint8_t br) {
    module.setupDisplay(true, br, 6);
}

void displayShowTime(int hour, int minute, bool colonOn, bool twelveHr) {
    // 12-hr conversion: 0→12, 1-12→1-12, 13-23→1-11
    int dispHour = twelveHr ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    int timeVal  = dispHour * 100 + minute;
    // TM16xx: bit 2 (0x04) of the dot-mask controls the colon
    uint8_t dotMask = colonOn ? 0x04 : 0x00;
    display.setDisplayToDecNumber(timeVal, dotMask, true);
}

void displayAnim() {
    if (myTimer(500)) {
        if (forw) {                         // sweep 4 → 0
            display.clear();
            module.setSegments(0x40, px);   // 0x40 = SEG_G (middle bar)
            --px;
            if (px == 0) forw = false;
        } else {                            // sweep 0 → 3
            display.clear();
            module.setSegments(0x40, px);
            ++px;
            if (px == 3) forw = true;
        }
    }
}

#endif  // DISPLAY_TM1652
