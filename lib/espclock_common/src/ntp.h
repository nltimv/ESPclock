// ESPclock - NTP time-synchronisation state
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#pragma once

#include <time.h>

// NTP server address (e.g. "pool.ntp.org") – defined in the main .cpp
extern const char *ntp_addr;

// IANA timezone name (e.g. "Europe/Amsterdam") – defined in the main .cpp
extern const char *tz_iana;

// POSIX timezone string derived from tz_iana via tzLookup() – defined in the main .cpp
extern const char *tz_posix;

// Set to true once configTzTime() has been called and NTP is running
extern bool start_NtpClient;

// Populated each second via getLocalTime()
extern struct tm timeinfo;
