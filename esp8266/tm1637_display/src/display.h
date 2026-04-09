// ESPclock - TM1637 display management
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#pragma once

#include <Arduino.h>
#include "TM1637Display.h"

// TM1637 pin assignments
#define CLK 5   // D1 — GPIO5
#define DIO 4   // D2 — GPIO4

// Display object
extern TM1637Display mydisplay;

// Display-state globals (modified by the web-server route handlers)
extern bool  colon;     // current colon state (toggled each second when blink is on)
extern bool  blink;     // true = blink the colon
extern bool  br_auto;   // true = auto-adjust brightness by time-of-day
extern bool  twelve;    // true = 12-hour mode
extern uint8_t brightness;

// Fixed segment patterns
extern const uint8_t SEG_try[];
extern const uint8_t SEG_Err[];

// Non-blocking timer: returns 1 (and resets the internal counter) every
// `everywhen` milliseconds.  NOTE: uses a single shared static counter.
unsigned long myTimer(unsigned long everywhen);

// Bouncing-dot animation shown while waiting for WiFi / NTP
void displayAnim(void);
