// ESPclock - IANA timezone name to POSIX string lookup
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#pragma once

// Given a canonical IANA timezone name (e.g. "Europe/Amsterdam"), return the
// corresponding POSIX TZ string (e.g. "CET-1CEST,M3.5.0,M10.5.0/3").
// Returns nullptr if the name is not found.
const char* tzLookup(const char* ianaName);
