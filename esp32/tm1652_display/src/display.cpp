// ESPclock - TM1652 display management (ESP32, GPIO6/GPIO4)
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#include "display.h"

// ── Hardware objects ───────────────────────────────────────────────────────
// module(GPIOpin, n_digits): low-level TM1652 driver
TM1652       module(6, 4);
// display(&module, n_digits): higher-level helper for number/string output
TM16xxDisplay display(&module, 4);

// ── Display-state globals ──────────────────────────────────────────────────
bool    colon      = true;
bool    blink      = true;
bool    br_auto    = false;
bool    twelve     = false;
uint8_t brightness = 7;

// ── Animation state (private to this translation unit) ────────────────────
static uint8_t px   = 4;
static bool    forw = true;   // true = moving right→left

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
    // Correct 12-hr conversion: 0→12, 1-12→1-12, 13-23→1-11
    int dispHour = twelveHr ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    int timeVal  = dispHour * 100 + minute;

    // TM16xx: bit 2 (0x04) of the dot-mask controls the colon between digits 1 and 2
    uint8_t dotMask = colonOn ? 0x04 : 0x00;
    display.setDisplayToDecNumber(timeVal, dotMask, true);
}

// ── Animation ─────────────────────────────────────────────────────────────
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
