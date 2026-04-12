// ESPclock - Unified display driver header
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

// Include this header to get the full display abstraction: state globals,
// myTimer(), and all displayXxx() functions.  The chip-specific hardware
// objects are exposed here so that espclock.cpp can reach them if needed.
//
// Select the active driver with a build flag:
//   TM1637  -D DISPLAY_TM1637 -D DISPLAY_CLK=<pin> -D DISPLAY_DIO=<pin>
//   TM1652  -D DISPLAY_TM1652 -D DISPLAY_DATA_PIN=<pin> -D DISPLAY_DIGITS=<n>

#pragma once

#include "display_api.h"

#if defined(DISPLAY_TM1637)
#include <TM1637Display.h>
extern TM1637Display mydisplay;

#elif defined(DISPLAY_TM1652)
#include <TM1652.h>
#include <TM16xxDisplay.h>
extern TM1652        module;
extern TM16xxDisplay display;

#else
#error "No display driver selected. Define DISPLAY_TM1637 or DISPLAY_TM1652 in build_flags."
#endif
