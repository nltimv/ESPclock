// ESPclock - ArduinoJson compile-time configuration
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#pragma once

// Memory-saving ArduinoJson options for constrained microcontrollers
#define ARDUINOJSON_SLOT_ID_SIZE         1
#define ARDUINOJSON_STRING_LENGTH_SIZE   1
#define ARDUINOJSON_USE_DOUBLE           0
#define ARDUINOJSON_USE_LONG_LONG        0

#include "ArduinoJson.h"
