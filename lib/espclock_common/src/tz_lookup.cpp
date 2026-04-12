// ESPclock - IANA timezone name to POSIX string lookup
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#include "tz_lookup.h"
#include "json_config.h"

#include <FS.h>
#include <LittleFS.h>
#include <string.h>

// Static buffer – large enough for the longest known POSIX string.
// Safe on the single-threaded ESP event loop; tzLookup() is only called
// during setup / config changes, never concurrently.
static char s_posixBuf[80];

const char* tzLookup(const char* ianaName) {
    if (!ianaName) return nullptr;

    File f = LittleFS.open("/tz.json", "r");
    if (!f) {
        Serial.println(F("tzLookup: failed to open /tz.json"));
        return nullptr;
    }

    // Use a filter so ArduinoJson only materialises the single key we need.
    JsonDocument filter;
    filter[ianaName] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f,
                                   DeserializationOption::Filter(filter));
    f.close();

    if (err) {
        Serial.print(F("tzLookup: deserializeJson failed: "));
        Serial.println(err.f_str());
        return nullptr;
    }

    if (!doc[ianaName].is<const char*>()) return nullptr;

    strlcpy(s_posixBuf, doc[ianaName].as<const char*>(), sizeof(s_posixBuf));
    return s_posixBuf;
}

