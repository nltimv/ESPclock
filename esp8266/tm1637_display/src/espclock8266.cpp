// ESPclock - ESP8266 TM1637 display firmware (main entry point)
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>

#include "display.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "ntp.h"
#include "json_config.h"

// ── NTP / time globals ─────────────────────────────────────────────────────
const char *ntp_addr      = "pool.ntp.org";
const char *tz_posix      = "UTC0";
bool        start_NtpClient = false;
struct tm   timeinfo;

// ── setup() ───────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    mydisplay.setBrightness(7);
    mydisplay.clear();

    // Mount the filesystem (Err0 = mount failure)
    if (!LittleFS.begin()) {
        mydisplay.setSegments(SEG_Err, 3, 0);
        mydisplay.showNumberDec(0, false, 1, 3);
        delay(10000);
        return;
    }

    // Verify the web UI is present (Err1 = missing index.html)
    if (!LittleFS.exists("/index.html")) {
        mydisplay.setSegments(SEG_Err, 3, 0);
        mydisplay.showNumberDec(1, false, 1, 3);
        delay(10000);
        return;
    }

    // Attempt to restore a previously saved configuration
    checkConfig();

    WiFi.mode(WIFI_AP_STA);
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
    MDNS.update();

    // Shut down AP after the setup-mode grace period (15 s)
    if (ap_shutdown_pending && (millis() - ap_shutdown_start) >= 15000UL) {
        WiFi.mode(WIFI_STA);
        ap_shutdown_pending = false;
    }

    // Perform a deferred WiFi rescan when requested by the /scan or /refresh handler
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
                    case 0:
                        brightness = 0;
                        mydisplay.setBrightness(0);
                        break;
                    case 9:
                        brightness = 6;
                        mydisplay.setBrightness(6);
                        break;
                    case 17:
                        brightness = 3;
                        mydisplay.setBrightness(3);
                        break;
                    case 20:
                        brightness = 2;
                        mydisplay.setBrightness(2);
                        break;
                }
            }

            // Render the current time
            if (blink) {
                // Compute the displayed hour with correct 12-hr conversion:
                //   0  → 12 (midnight), 1-12 → 1-12, 13-23 → 1-11
                int dispHour = twelve ? (timeinfo.tm_hour % 12 == 0 ? 12 : timeinfo.tm_hour % 12)
                                      : timeinfo.tm_hour;

                uint8_t colonMask = colon ? 0b01000000 : 0;
                mydisplay.showNumberDecEx(dispHour,         colonMask, false, 2, 0);
                mydisplay.showNumberDecEx(timeinfo.tm_min,  colonMask, true,  2, 2);
                colon = !colon;
            } else {
                // Static colon
                mydisplay.showNumberDecEx(timeinfo.tm_hour, 0b01000000, false, 2, 0);
                mydisplay.showNumberDecEx(timeinfo.tm_min,  0b01000000, true,  2, 2);
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
                    config[F("tz")]      = tz_posix;
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
