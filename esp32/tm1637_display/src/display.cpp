// ESPclock - TM1637 display management (ESP32, GPIO9/GPIO10)
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#include "display.h"

// ── Hardware object ────────────────────────────────────────────────────────
TM1637Display mydisplay(CLK, DIO);

// ── Display-state globals ──────────────────────────────────────────────────
bool    colon      = true;
bool    blink      = true;
bool    br_auto    = false;
bool    twelve     = false;
uint8_t brightness = 7;

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

// ── Animation state (private to this translation unit) ────────────────────
static uint8_t       px         = 4;
static const uint8_t SEG_WAIT[] = { SEG_G };
static bool          forw       = true;   // true = moving right→left

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
    // Correct 12-hr conversion: 0→12, 1-12→1-12, 13-23→1-11
    int dispHour = twelveHr ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    uint8_t colonMask = colonOn ? 0b01000000 : 0;
    mydisplay.showNumberDecEx(dispHour, colonMask, false, 2, 0);
    mydisplay.showNumberDecEx(minute,   colonMask, true,  2, 2);
}

// ── Animation ─────────────────────────────────────────────────────────────
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
