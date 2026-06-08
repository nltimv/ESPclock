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
static unsigned long g_timer_anchor_ms = 0;
static bool          g_timer_anchor_initialized = false;

unsigned long myTimer(unsigned long everywhen) {
    if (!g_timer_anchor_initialized) {
        g_timer_anchor_ms = millis();
        g_timer_anchor_initialized = true;
        return 0;
    }

    unsigned long diff_time = millis() - g_timer_anchor_ms;
    int ret = 0;
    if (diff_time >= everywhen) {
        g_timer_anchor_ms = millis();
        ret = 1;
    }
    return ret;
}

void myTimerReset() {
    g_timer_anchor_ms = millis();
    g_timer_anchor_initialized = true;
}

// ══════════════════════════════════════════════════════════════════════════
//  TM1637 driver (used by ESP8266 and ESP32 TM1637 builds)
// ══════════════════════════════════════════════════════════════════════════
#if defined(DISPLAY_TM1637)

TM1637Display mydisplay(DISPLAY_CLK, DISPLAY_DIO);

// ── Fixed segment patterns ─────────────────────────────────────────────────
static const uint8_t SEG_Err[] = {
    SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,       // E
    SEG_E | SEG_G,                                // r
    SEG_E | SEG_G                                 // r
};
static const uint8_t SEG_h = SEG_C | SEG_E | SEG_F | SEG_G;

// ── Animation state ────────────────────────────────────────────────────────
static uint8_t       px         = 4;
static const uint8_t SEG_WAIT[] = { SEG_G };
static bool          forw       = true;   // true = sweeping right→left

// Returns whether SEG_DP should be lit on TM1637 digit #2 (center separator).
// The center separator follows colonOn so the time colon blinks.
static inline bool shouldShowDotSegment(bool colonOn) {
    return colonOn;
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
    displayShowAttempt(1);
}

void displayShowAttempt(uint8_t n) {
    uint8_t dash_count = (n == 0) ? 1 : (n > 4 ? 4 : n);
    uint8_t digits[4] = {0x00, 0x00, 0x00, 0x00};
    for (uint8_t i = 0; i < dash_count; ++i) {
        digits[i] = SEG_G;
    }
    mydisplay.setSegments(digits, 4, 0);
}

void displaySetBrightness(uint8_t br) {
    mydisplay.setBrightness(br);
}

void displayShowTime(int hour, int minute, bool colonOn, bool twelveHr) {
    // 12-hr conversion: 0→12, 1-12→1-12, 13-23→1-11
    int dispHour = twelveHr ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    bool dotSegmentOn = shouldShowDotSegment(colonOn);
    uint8_t digits[4] = {
        (uint8_t)(dispHour >= 10 ? mydisplay.encodeDigit(dispHour / 10) : 0x00),
        (uint8_t)(mydisplay.encodeDigit(dispHour % 10) | (dotSegmentOn ? SEG_DP : 0x00)),
        mydisplay.encodeDigit(minute / 10),
        mydisplay.encodeDigit(minute % 10)
    };
    mydisplay.setSegments(digits, 4, 0);
}

void displayShowTimePartial(int hour, int minute, bool colonOn, bool twelveHr,
                            bool showHour, bool showMinute) {
    int dispHour = twelveHr ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    bool dotSegmentOn = shouldShowDotSegment(colonOn);
    uint8_t hourUnits = showHour ? mydisplay.encodeDigit(dispHour % 10) : 0x00;
    uint8_t digits[4] = {
        (uint8_t)(showHour && dispHour >= 10 ? mydisplay.encodeDigit(dispHour / 10) : 0x00),
        (uint8_t)(hourUnits | (dotSegmentOn ? SEG_DP : 0x00)),
        (uint8_t)(showMinute ? mydisplay.encodeDigit(minute / 10) : 0x00),
        (uint8_t)(showMinute ? mydisplay.encodeDigit(minute % 10) : 0x00)
    };
    mydisplay.setSegments(digits, 4, 0);
}

void displayShowHourMode(bool twelveHr) {
    uint8_t secondDigit = mydisplay.encodeDigit(twelveHr ? 2 : 4);
    uint8_t digits[4] = {
        mydisplay.encodeDigit(twelveHr ? 1 : 2),
        secondDigit,
        SEG_h,
        0x00
    };
    mydisplay.setSegments(digits, 4, 0);
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
    displayShowAttempt(1);
}

void displayShowAttempt(uint8_t n) {
    uint8_t dash_count = (n == 0) ? 1 : (n > 4 ? 4 : n);
    display.clear();
    for (uint8_t i = 0; i < dash_count; ++i) {
        module.setSegments(0x40, 3 - i);  // 0x40 = SEG_G (middle bar)
    }
}

void displaySetBrightness(uint8_t br) {
    module.setupDisplay(true, br, 6);
}

void displayShowTime(int hour, int minute, bool colonOn, bool twelveHr) {
    // 12-hr conversion: 0→12, 1-12→1-12, 13-23→1-11
    int dispHour = twelveHr ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    int timeVal  = dispHour * 100 + minute;
    // TM16xx: bit 2 (0x04) controls the center colon.
    uint8_t dotMask = (colonOn ? 0x04 : 0x00);
    display.setDisplayToDecNumber(timeVal, dotMask, true);
}

void displayShowTimePartial(int hour, int minute, bool colonOn, bool twelveHr,
                            bool showHour, bool showMinute) {
    if (showHour && showMinute) {
        displayShowTime(hour, minute, colonOn, twelveHr);
        return;
    }
    int dispHour = twelveHr ? (hour % 12 == 0 ? 12 : hour % 12) : hour;
    if (showHour) {
        if (dispHour >= 10) {
            module.setDisplayDigit(dispHour / 10, 0, false);
        } else {
            module.setSegments(0x00, 0);
        }
        module.setDisplayDigit(dispHour % 10, 1, colonOn);
    } else {
        module.setSegments(0x00, 0);
        module.setSegments(colonOn ? 0x80 : 0x00, 1);
    }
    if (showMinute) {
        module.setDisplayDigit(minute / 10, 2, false);
        module.setDisplayDigit(minute % 10, 3, false);
    } else {
        module.setSegments(0x00, 2);
        module.setSegments(0x00, 3);
    }
}

void displayShowHourMode(bool twelveHr) {
    module.setDisplayDigit(twelveHr ? 1 : 2, 0, false);
    module.setDisplayDigit(twelveHr ? 2 : 4, 1, false);
    module.setSegments(0x74, 2); // lowercase "h"
    module.setSegments(0x00, 3);
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
