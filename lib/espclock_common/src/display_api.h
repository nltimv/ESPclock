// ESPclock - Abstract display API
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

// Every hardware-specific display.cpp must implement each function below so
// that the shared wifi_manager and web_server modules can control the display
// without knowing which chip is fitted.

#pragma once

#include <Arduino.h>

// ── Display-state globals (defined in display.cpp) ─────────────────────────
extern bool    colon;      // current colon state (toggled each second when blink is on)
extern bool    blink;      // true = blink the colon
extern bool    br_auto;    // true = auto-adjust brightness by time-of-day
extern bool    twelve;     // true = 12-hour mode
extern uint8_t brightness;

// ── Non-blocking timer ─────────────────────────────────────────────────────
// Returns 1 (and resets the internal counter) every `everywhen` milliseconds.
// NOTE: uses a single shared static counter — do not call concurrently with
//       different intervals from ISR context.
unsigned long myTimer(unsigned long everywhen);

// ── Display control ────────────────────────────────────────────────────────
void displayInit();                    // initialise hardware, set max brightness
void displayClear();                   // blank all segments
void displayShowError(uint8_t code);   // show "Err <code>" on the display
void displayShowTrying();              // show "trY" while connecting to WiFi
void displayShowAttempt(uint8_t n);    // show attempt-counter digit n
void displaySetBrightness(uint8_t br); // apply a new brightness level
void displayShowTime(int hour, int minute, bool colonOn, bool twelveHr); // render clock face
// Like displayShowTime, but showHour/showMinute control which field is visible.
// Pass showHour=false to blank the hour digits, showMinute=false to blank minute digits.
void displayShowTimePartial(int hour, int minute, bool colonOn, bool twelveHr,
                            bool showHour, bool showMinute);
void displayShowHourMode(bool twelveHr); // render "12h" or "24h" for mode selection
void displayAnim();                    // bouncing-dot waiting animation
