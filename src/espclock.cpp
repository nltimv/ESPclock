// ESPclock - Unified firmware main entry point
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

// This single source file covers all three build environments defined in
// platformio.ini (ESP8266 + TM1637, ESP32 + TM1637, ESP32 + TM1652).
// Platform-specific behaviour is isolated to two #ifdef ESP8266 guards.

#include <Arduino.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#else  // ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#endif

#include <LittleFS.h>

#include "display.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "ntp.h"
#include "json_config.h"

// ── NTP / time globals ─────────────────────────────────────────────────────
const char *ntp_addr      = "pool.ntp.org";
const char *tz_iana       = "UTC";
const char *tz_posix      = "UTC0";
bool        start_NtpClient = false;
struct tm   timeinfo;

// ── setup() ───────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    displayInit();

    // Mount the filesystem (Err0 = mount failure)
    if (!LittleFS.begin()) {
        displayShowError(0);
        delay(10000);
        return;
    }

    // Verify the web UI is present (Err1 = missing index.html)
    if (!LittleFS.exists("/index.html")) {
        displayShowError(1);
        delay(10000);
        return;
    }

    // Attempt to restore a previously saved configuration
    checkConfig();

    // Only enable the AP interface when entering setup mode; in normal mode
    // WIFI_STA is sufficient and prevents ESP from raising a default AP.
    WiFi.mode(connected ? WIFI_STA : WIFI_AP_STA);
    WiFi.setAutoReconnect(true);

    initMDNS();
    delay(100);

    // Scan for nearby networks when not already connected
    if (WiFi.status() != WL_CONNECTED) {
        wifiScan();
    }

    // Only start the AP in setup mode (no saved config)
    if (!connected) {
        WiFi.softAP(esp_ssid, esp_password, false, 2);
    }

    setupRoutes();
}

// ── loop() ────────────────────────────────────────────────────────────────
void loop() {
#ifdef ESP8266
    MDNS.update();   // ESP8266 requires periodic mDNS polling; ESP32 does not
#endif

    // Shut down AP after the setup-mode grace period (15 s)
    if (ap_shutdown_pending && (millis() - ap_shutdown_start) >= 15000UL) {
        WiFi.mode(WIFI_STA);
        ap_shutdown_pending = false;
    }

    // Perform a deferred WiFi rescan when requested by /scan or /refresh
    if (newScan) {
        wifiScan();
        newScan = false;
    }

    // ── Clock display ──────────────────────────────────────────────────────
    if (start_NtpClient) {
        getLocalTime(&timeinfo);

        if (myTimer(1000)) {
            // Auto-brightness: adjust at transition hours
            if (br_auto) {
                switch (timeinfo.tm_hour) {
                    case 0:  brightness = 0; displaySetBrightness(0); break;
                    case 9:  brightness = 6; displaySetBrightness(6); break;
                    case 17: brightness = 3; displaySetBrightness(3); break;
                    case 20: brightness = 2; displaySetBrightness(2); break;
                }
            }

            // Render the current time
            if (blink) {
                displayShowTime(timeinfo.tm_hour, timeinfo.tm_min, colon, twelve);
                colon = !colon;
            } else {
                displayShowTime(timeinfo.tm_hour, timeinfo.tm_min, true, twelve);
            }
        }
    } else {
        displayAnim();
    }

    // ── WiFi connection ────────────────────────────────────────────────────
    if (!connected && creds_available) {
        displayAnim();
        WiFi.begin(ssid, password);

        while (true) {
            displayAnim();

            if (WiFi.status() != WL_CONNECTED && creds_available) {
                delay(200);
            } else if (WiFi.status() == WL_CONNECTED) {
                connected = true;
                initMDNS();

                // First-time setup: auto-save credentials and defaults;
                // NTP and AP shutdown are deferred to /setup_timezone.
                if (setup_mode) {
                    JsonDocument config;
                    config[F("ssid")]    = ssid;
                    config[F("pw")]      = password;
                    config[F("ntp_ad")]  = ntp_addr;
                    config[F("tz")]      = tz_iana;
                    config[F("br_auto")] = br_auto;
                    config[F("br")]      = brightness;
                    config[F("blink")]   = blink;
                    config[F("twelve")]  = twelve;
                    config.shrinkToFit();
                    File fc = LittleFS.open("/config.json", "w+");
                    serializeJsonPretty(config, fc);
                    fc.close();
                    setup_mode = false;
                }
                break;
            } else if (attempts == 4) {
                attempts        = 0;
                creds_available = false;
                Serial.println("RESET Attempts from LOOP");
                Serial.println(password);
                break;
            }
        }
    }
}
