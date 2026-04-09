// ESPclock - TM1652 display management (ESP32, GPIO6/GPIO4)
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#pragma once

// Include the shared abstract display API (globals + function prototypes)
#include "display_api.h"
#include <TM1652.h>
#include <TM16xxDisplay.h>

// TM1652 hardware objects
extern TM1652       module;   // low-level chip driver
extern TM16xxDisplay display; // higher-level display helper
