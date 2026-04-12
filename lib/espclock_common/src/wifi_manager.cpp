// ESPclock - WiFi management (scan, mDNS, configuration)
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#include "wifi_manager.h"
#include "display_api.h"
#include "ntp.h"
#include "json_config.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#else  // ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#endif

#include <FS.h>
#include <LittleFS.h>

// ── Device identity globals ────────────────────────────────────────────────
const char *esp_ssid     = "ESPclock-" DEVICE_ID;
const char *mdns_name    = "espclock-" DEVICE_ID;
const char *esp_password = "waltwhite64";   // AP password (≥8 chars required)

// ── WiFi credential globals ────────────────────────────────────────────────
const char *ssid;
const char *password;
bool creds_available = false;
bool connected       = false;

// ── Setup / reconnection globals ──────────────────────────────────────────
bool          newScan             = false;
uint8_t       attempts            = 0;
bool          setup_mode          = true;
unsigned long ap_shutdown_start   = 0;
bool          ap_shutdown_pending = false;

// ── Function implementations ───────────────────────────────────────────────

void wifiScan() {
    WiFi.disconnect();

    byte n = WiFi.scanNetworks();
    Serial.print(n);
    Serial.println(" network(s) found.");

    // Sort by RSSI (strongest first) and remove duplicate SSIDs before storing
    JsonDocument net_list;
    JsonArray network = net_list["network"].to<JsonArray>();

    bool *used = new bool[n]();

    for (byte picked = 0; picked < n; picked++) {
        int     bestIndex = -1;
        int32_t bestRssi  = -1000;

        for (byte i = 0; i < n; i++) {
            if (used[i]) continue;
            int32_t currentRssi = WiFi.RSSI(i);
            if (bestIndex == -1 || currentRssi > bestRssi) {
                bestIndex = i;
                bestRssi  = currentRssi;
            }
        }

        if (bestIndex < 0) break;
        used[bestIndex] = true;

        String ssidName = WiFi.SSID(bestIndex);
        if (ssidName.length() == 0) continue;

        // Skip duplicates
        bool duplicate = false;
        for (JsonVariant entry : network) {
            const char *existingSsid = entry["credentials"][0] | "";
            if (ssidName.equals(existingSsid)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        JsonArray creds = network[network.size()]["credentials"].to<JsonArray>();
        creds.add(ssidName);
        creds.add("");
    }

    delete[] used;

    net_list["found"] = network.size();

    File fx = LittleFS.open("/network_list.json", "w");
    serializeJsonPretty(net_list, fx);
    fx.close();

    WiFi.scanDelete();
}

void initMDNS() {
    MDNS.end();
    if (MDNS.begin(mdns_name)) {
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println("mDNS fail");
    }
}

void checkConfig() {
    if (!LittleFS.exists("/config.json")) return;

    Serial.println(F("config exists, trying to restore it"));
    creds_available = true;

    File fld = LittleFS.open("/config.json", "r");
    JsonDocument load_cf;
    DeserializationError error = deserializeJson(load_cf, fld);

    if (error) {
        fld.close();
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    ssid     = load_cf[F("ssid")];
    password = load_cf[F("pw")];

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        displayShowTrying();

        if (myTimer(3000)) {
            ++attempts;
            displayShowAttempt(attempts);
        } else if (attempts == 4) {
            attempts        = 0;
            creds_available = false;
            break;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        attempts   = 0;
        connected  = true;
        setup_mode = false;
        Serial.println("WIFI RESTORED");

        if (load_cf["ntp_ad"].is<const char*>()) {
            ntp_addr = strdup(load_cf["ntp_ad"]);
        }

        if (load_cf["tz"].is<const char*>()) {
            tz_posix = strdup(load_cf["tz"]);
            configTzTime(tz_posix, ntp_addr);
            start_NtpClient = true;
        } else {
            start_NtpClient = false;
            Serial.println(F("Missing timezone in config.json. Re-save time settings from WebUI."));
        }

        brightness = (uint8_t)load_cf["br"];
        displaySetBrightness(brightness);
        blink   = load_cf[F("blink")];
        br_auto = load_cf[F("br_auto")];
        twelve  = load_cf[F("twelve")];
    }

    fld.close();
}
