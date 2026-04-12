// ESPclock - HTTP web-server route definitions
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#include "web_server.h"
#include "wifi_manager.h"
#include "display_api.h"
#include "ntp.h"
#include "json_config.h"
#include "tz_lookup.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#else  // ESP32
#include <WiFi.h>
#endif

#include <LittleFS.h>

// ── Server object ──────────────────────────────────────────────────────────
AsyncWebServer server(80);

// ── Helper ─────────────────────────────────────────────────────────────────
static void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "NOT FOUND");
}

static bool isTrustedFinderOrigin(const String &origin) {
    if (!(origin.startsWith("http://") || origin.startsWith("https://"))) return false;

    String hostPart = origin;
    int    schemeIx = hostPart.indexOf("://");
    if (schemeIx >= 0) hostPart = hostPart.substring(schemeIx + 3);
    int slashIx = hostPart.indexOf('/');
    if (slashIx >= 0) hostPart = hostPart.substring(0, slashIx);
    int colonIx = hostPart.indexOf(':');
    if (colonIx >= 0) hostPart = hostPart.substring(0, colonIx);
    bool localDomainOrigin = hostPart.endsWith(".local");

    if (origin.startsWith("http://localhost") || origin.startsWith("https://localhost") ||
        origin.startsWith("http://127.") || origin.startsWith("https://127.") ||
        origin.startsWith("http://192.168.") || origin.startsWith("https://192.168.") ||
        origin.startsWith("http://10.") || origin.startsWith("https://10.") ||
        localDomainOrigin) {
        return true;
    }

    // RFC1918 private range 172.16.0.0/12 spans 172.16.x.x through 172.31.x.x.
    for (uint8_t i = 16; i <= 31; i++) {
        String httpPrefix  = "http://172." + String(i) + ".";
        String httpsPrefix = "https://172." + String(i) + ".";
        if (origin.startsWith(httpPrefix) || origin.startsWith(httpsPrefix)) {
            return true;
        }
    }

    return false;
}

static void addFinderCorsHeaders(AsyncWebServerRequest *request, AsyncWebServerResponse *response) {
    if (!request->hasHeader("Origin")) return;
    String origin = request->header("Origin");
    if (!isTrustedFinderOrigin(origin)) return;

    response->addHeader("Access-Control-Allow-Origin", origin);
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Max-Age", "86400");
    response->addHeader("Vary", "Origin");
}

// ── Route registration ─────────────────────────────────────────────────────
void setupRoutes() {
    // Root – serve the single-page web UI
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
        Serial.println("Connection detected");
    });

    // Timezone data – serve the IANA→POSIX JSON map used by the browser dropdowns
    server.on("/tz.json", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/tz.json", "application/json");
    });

    // UI status – returns all current state so the front-end can sync its controls
    server.on("/uicheck", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument uicheck_json;
        uicheck_json["conn"]       = connected;
        uicheck_json["bright"]     = brightness;
        uicheck_json["br_auto"]    = br_auto;
        uicheck_json["blink"]      = blink;
        uicheck_json["twelve"]     = twelve;
        uicheck_json["config"]     = (LittleFS.exists("/config.json")) ? 1 : 0;
        uicheck_json["ntp"]        = ntp_addr;
        uicheck_json["tz"]         = tz_iana;
        uicheck_json["setup_mode"] = setup_mode;
        uicheck_json["ap_ssid"]    = esp_ssid;

        String uc_str;
        serializeJson(uicheck_json, uc_str);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", uc_str);
        addFinderCorsHeaders(request, response);
        request->send(response);
    });

    // Preflight support for finder app CORS requests
    server.on("/uicheck", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(204);
        addFinderCorsHeaders(request, response);
        request->send(response);
    });

    // Return the cached network list (and schedule a fresh scan)
    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        File f = LittleFS.open("/network_list.json", "r");
        if (!f) {
            request->send(500, "application/json", "{\"error\":\"Failed to open network_list.json\"}");
            f.close();
        } else {
            request->send(LittleFS, "/network_list.json", "application/json");
            newScan = true;
            f.close();
        }
    });

    // Receive WiFi credentials from the user
    server.on("/sendcreds", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument thebody;
            deserializeJson(thebody, data);
            ssid            = strdup(thebody["ssid"]);
            password        = strdup(thebody["pw"]);
            creds_available = true;
            request->send(200, "application/json", "{\"creds\":\"OK\"}");
        }
    );

    // Re-scan and return a refreshed network list
    server.on("/refresh", HTTP_GET, [](AsyncWebServerRequest *request) {
        File f = LittleFS.open("/network_list.json", "r");
        if (!f) {
            Serial.println(F("Error opening /network_list.json"));
            request->send(500, "application/json", "{\"error\":\"Failed to open network_list.json\"}");
            f.close();
            return;
        }
        request->send(LittleFS, "/network_list.json", "application/json");
        f.close();
        newScan = true;
    });

    // Poll the current WiFi connection status
    server.on("/wifi_status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (attempts == 4) {
            creds_available = false;
            Serial.println(password);
            Serial.println(F("handler says: 5 attempts->WRONG PASSWORD - RESET attempts to 0"));
            request->send(200, "application/json", "{\"stat\":\"fail\"}");
        } else {
            ++attempts;
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println(password);
                String ip   = WiFi.localIP().toString();
                String resp = "{\"stat\":\"ok\",\"ip\":\"" + ip + "\",\"mdns\":\"" + String(mdns_name) + "\"}";
                request->send(200, "application/json", resp);
            } else {
                Serial.println("PLEASE WAIT");
                request->send(200, "application/json", "{\"stat\":\"wait\"}");
            }
        }
    });

    // Update NTP server + timezone and start/restart the NTP client
    server.on("/updatetime", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument ntp_json;
            deserializeJson(ntp_json, data);

            if (ntp_json["ntp_addr"].is<const char*>()) {
                ntp_addr = strdup(ntp_json["ntp_addr"]);
            }
            if (ntp_json["tz"].is<const char*>()) {
                tz_iana  = strdup(ntp_json["tz"]);
                const char* posix = tzLookup(tz_iana);
                tz_posix = posix ? posix : "UTC0";
            }

            configTzTime(tz_posix, ntp_addr);
            start_NtpClient = true;

            request->send(200, "application/json", "{\"ntp\":\"OK\"}");
        }
    );

    // First-time timezone setup: persist the timezone and schedule AP shutdown
    server.on("/setup_timezone", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument tz_json;
            deserializeJson(tz_json, data);

            if (tz_json["tz"].is<const char*>()) {
                tz_iana  = strdup(tz_json["tz"]);
                const char* posix = tzLookup(tz_iana);
                tz_posix = posix ? posix : "UTC0";
            }

            // Persist the timezone into the saved config
            if (LittleFS.exists("/config.json")) {
                File fr = LittleFS.open("/config.json", "r");
                JsonDocument saved_cf;
                deserializeJson(saved_cf, fr);
                fr.close();
                saved_cf[F("tz")] = tz_iana;
                saved_cf.shrinkToFit();
                File fw = LittleFS.open("/config.json", "w+");
                serializeJsonPretty(saved_cf, fw);
                fw.close();
            }

            configTzTime(tz_posix, ntp_addr);
            start_NtpClient = true;

            // Schedule AP shutdown after a 15-second grace period
            ap_shutdown_start   = millis();
            ap_shutdown_pending = true;

            request->send(200, "application/json", "{\"status\":\"ok\"}");
        }
    );

    // Brightness slider
    server.on("/slider", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument bgt_json;
            deserializeJson(bgt_json, data);
            brightness = (uint8_t)atoi(bgt_json["bgt"]);
            displaySetBrightness(brightness);
            request->send(200, "application/json", "{\"status\":\"BGT OK\"}");
        }
    );

    // Auto-brightness toggle
    server.on("/br_auto", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument br_auto_json;
            deserializeJson(br_auto_json, data);
            br_auto = br_auto_json["br"];

            if (timeinfo.tm_hour < 9) {
                brightness = 0;
                displaySetBrightness(0);
                request->send(200, "application/json", "{\"status\":\"0\"}");
            } else if (timeinfo.tm_hour < 17) {
                brightness = 6;
                displaySetBrightness(6);
                request->send(200, "application/json", "{\"status\":\"6\"}");
            } else if (timeinfo.tm_hour < 20) {
                brightness = 3;
                displaySetBrightness(3);
                request->send(200, "application/json", "{\"status\":\"3\"}");
            } else {
                brightness = 2;
                displaySetBrightness(2);
                request->send(200, "application/json", "{\"status\":\"2\"}");
            }
        }
    );

    // Colon blink toggle
    server.on("/blink", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument blink_json;
            deserializeJson(blink_json, data);
            blink = (uint8_t)blink_json["bl"];
            request->send(200, "application/json", "{\"status\":\"updated\"}");
        }
    );

    // 12-hour mode toggle
    server.on("/twelve", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument twelve_json;
            deserializeJson(twelve_json, data);
            twelve = (uint8_t)twelve_json["tw"];
            request->send(200, "application/json", "{\"status\":\"updated\"}");
        }
    );

    // Save / delete the persistent configuration file
    server.on("/config", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument scf_json;
            deserializeJson(scf_json, data);
            bool saveconfig = scf_json["save"];

            if (saveconfig == 1) {
                JsonDocument config;

                if (!setup_mode && LittleFS.exists("/config.json")) {
                    // Normal mode: preserve existing WiFi credentials
                    File existing = LittleFS.open("/config.json", "r");
                    JsonDocument existing_cf;
                    deserializeJson(existing_cf, existing);
                    existing.close();
                    config[F("ssid")] = existing_cf[F("ssid")];
                    config[F("pw")]   = existing_cf[F("pw")];
                } else {
                    config[F("ssid")] = ssid;
                    config[F("pw")]   = password;
                }

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
                Serial.println(F("\nCONFIG SAVED"));

            } else if (LittleFS.exists("/config.json") && saveconfig == 0) {
                // Delete config and return to setup mode
                LittleFS.remove("/config.json");
                WiFi.disconnect();
                connected           = false;
                creds_available     = false;
                start_NtpClient     = false;
                attempts            = 0;
                setup_mode          = true;
                ap_shutdown_pending = false;
                WiFi.mode(WIFI_AP_STA);
                WiFi.softAP(esp_ssid, esp_password, false, 2);
                Serial.println(F("\n*Config.json DELETED*"));
            }

            request->send(200, "application/json", "{\"status\":\"updated\"}");
        }
    );

    server.onNotFound(notFound);

    server.begin();
}
