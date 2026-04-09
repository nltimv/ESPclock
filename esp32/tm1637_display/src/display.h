// ESPclock - TM1637 display management (ESP32, GPIO9/GPIO10)
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#pragma once

// Include the shared abstract display API (globals + function prototypes)
#include "display_api.h"
#include "TM1637Display.h"

// TM1637 pin assignments (ESP32)
#define CLK 9
#define DIO 10

// TM1637 hardware object
extern TM1637Display mydisplay;
