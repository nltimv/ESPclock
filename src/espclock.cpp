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
#include <time.h>
#include <sys/time.h>

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

#ifndef TOUCH_PIN
#ifdef ESP8266
#define TOUCH_PIN 14
#else
#define TOUCH_PIN 4
#endif
#endif

#ifndef TOUCH_ACTIVE_STATE
#define TOUCH_ACTIVE_STATE HIGH
#endif

static const unsigned long AP_OFFLINE_WINDOW_MS  = 15UL * 60UL * 1000UL;
static const unsigned long TOUCH_SHORT_PRESS_MS  = 80UL;
static const unsigned long TOUCH_LONG_PRESS_MS   = 1200UL;
static const unsigned long TOUCH_RESET_HINT_MS   = 5000UL;
static const unsigned long TOUCH_RESET_HOLD_MS   = 10000UL;
static const unsigned long DATE_VIEW_MS          = 3000UL;
static const unsigned long EDIT_FLASH_MS         = 450UL;

enum class EditField : uint8_t {
    NONE = 0,
    HOUR,
    MINUTE,
    TWELVE_24,
    YEAR,
    DAY,
    MONTH
};

static bool          in_time_setup           = false;
static EditField     edit_field              = EditField::NONE;
static tm            edit_time               = {};
static bool          show_edit_value         = true;
static unsigned long edit_flash_toggle_at    = 0;
static bool          show_date_until_timeout = false;
static unsigned long show_date_until_ms      = 0;
static bool          touch_prev              = false;
static unsigned long touch_press_started_ms  = 0;
static bool          reset_hint_shown        = false;
static bool          reset_done              = false;

static int daysInMonth(int year, int month) {
    static const int month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int d = month_days[month - 1];
    if (month == 2) {
        bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        if (leap) d = 29;
    }
    return d;
}

static void setClockFromTm(const tm &src) {
    tm copy = src;
    copy.tm_isdst = -1;
    time_t epoch = mktime(&copy);
    if (epoch < 0) return;
    timeval tv = {epoch, 0};
    settimeofday(&tv, nullptr);
}

static void setDefaultClockTime() {
    tm t = {};
    t.tm_year = 124;  // 2024
    t.tm_mon  = 0;    // Jan
    t.tm_mday = 1;
    t.tm_hour = 0;
    t.tm_min  = 0;
    t.tm_sec  = 0;
    setClockFromTm(t);
}

static void loadCurrentTime(tm &dst) {
    time_t now = time(nullptr);
    localtime_r(&now, &dst);
}

static bool isOnlineMode() {
    return !setup_mode;
}

static void showResetHintFeedback() {
    displayShowTime(88, 88, true, false);
    delay(220);
}

static void showResetConfirmFeedback() {
    for (uint8_t i = 0; i < 2; ++i) {
        displayClear();
        delay(140);
        displayShowTime(0, 0, true, false);
        delay(140);
    }
}

static void startDateView() {
    show_date_until_timeout = true;
    show_date_until_ms      = millis() + DATE_VIEW_MS;
}

static void startTimeSetup() {
    loadCurrentTime(edit_time);
    in_time_setup        = true;
    edit_field           = EditField::HOUR;
    show_edit_value      = true;
    edit_flash_toggle_at = millis();
    show_date_until_timeout = false;
}

static void advanceTimeSetupField() {
    switch (edit_field) {
        case EditField::HOUR:      edit_field = EditField::MINUTE;    break;
        case EditField::MINUTE:    edit_field = EditField::TWELVE_24; break;
        case EditField::TWELVE_24: edit_field = EditField::YEAR;      break;
        case EditField::YEAR:      edit_field = EditField::DAY;       break;
        case EditField::DAY:       edit_field = EditField::MONTH;     break;
        case EditField::MONTH:
            setClockFromTm(edit_time);
            in_time_setup = false;
            edit_field    = EditField::NONE;
            return;
        default:
            return;
    }
    show_edit_value      = true;
    edit_flash_toggle_at = millis();
}

static void cycleCurrentValue() {
    switch (edit_field) {
        case EditField::HOUR:
            edit_time.tm_hour = (edit_time.tm_hour + 1) % 24;
            break;
        case EditField::MINUTE:
            edit_time.tm_min = (edit_time.tm_min + 1) % 60;
            break;
        case EditField::TWELVE_24:
            twelve = !twelve;
            break;
        case EditField::YEAR: {
            int year = (edit_time.tm_year + 1900) + 1;
            if (year > 2099) year = 2000;
            edit_time.tm_year = year - 1900;
            int maxDay = daysInMonth(year, edit_time.tm_mon + 1);
            if (edit_time.tm_mday > maxDay) edit_time.tm_mday = maxDay;
            break;
        }
        case EditField::DAY: {
            int year  = edit_time.tm_year + 1900;
            int month = edit_time.tm_mon + 1;
            int maxDay = daysInMonth(year, month);
            edit_time.tm_mday++;
            if (edit_time.tm_mday > maxDay) edit_time.tm_mday = 1;
            break;
        }
        case EditField::MONTH: {
            edit_time.tm_mon = (edit_time.tm_mon + 1) % 12;
            int maxDay = daysInMonth(edit_time.tm_year + 1900, edit_time.tm_mon + 1);
            if (edit_time.tm_mday > maxDay) edit_time.tm_mday = maxDay;
            break;
        }
        default:
            break;
    }
    show_edit_value      = true;
    edit_flash_toggle_at = millis();
}

static void renderEditScreen() {
    if ((millis() - edit_flash_toggle_at) >= EDIT_FLASH_MS) {
        show_edit_value      = !show_edit_value;
        edit_flash_toggle_at = millis();
    }

    if (!show_edit_value) {
        displayClear();
        return;
    }

    switch (edit_field) {
        case EditField::HOUR:
        case EditField::MINUTE:
            displayShowTime(edit_time.tm_hour, edit_time.tm_min, true, twelve);
            break;
        case EditField::TWELVE_24:
            displayShowTime(twelve ? 12 : 24, 0, false, false);
            break;
        case EditField::YEAR: {
            int year = edit_time.tm_year + 1900;
            displayShowTime(year / 100, year % 100, false, false);
            break;
        }
        case EditField::DAY:
            displayShowTime(0, edit_time.tm_mday, false, false);
            break;
        case EditField::MONTH:
            displayShowTime(0, edit_time.tm_mon + 1, false, false);
            break;
        default:
            displayShowTime(timeinfo.tm_hour, timeinfo.tm_min, true, twelve);
            break;
    }
}

static void handleTouchInput() {
    bool pressed = (digitalRead(TOUCH_PIN) == TOUCH_ACTIVE_STATE);
    unsigned long now = millis();

    if (pressed && !touch_prev) {
        touch_press_started_ms = now;
        reset_hint_shown       = false;
        reset_done             = false;
    }

    if (pressed) {
        unsigned long held = now - touch_press_started_ms;
        if (!in_time_setup && isOnlineMode() && held >= TOUCH_RESET_HINT_MS && !reset_hint_shown) {
            showResetHintFeedback();
            reset_hint_shown = true;
        }

        if (!in_time_setup && isOnlineMode() && held >= TOUCH_RESET_HOLD_MS && !reset_done) {
            switchToOfflineMode(true);
            showResetConfirmFeedback();
            reset_done = true;
        }
    }

    if (!pressed && touch_prev) {
        unsigned long held = now - touch_press_started_ms;

        if (reset_done) {
            // Already handled while pressed.
        } else if (held >= TOUCH_LONG_PRESS_MS) {
            if (in_time_setup) {
                advanceTimeSetupField();
            } else {
                startTimeSetup();
            }
        } else if (held >= TOUCH_SHORT_PRESS_MS) {
            if (in_time_setup) {
                cycleCurrentValue();
            } else {
                startDateView();
            }
        }
    }

    touch_prev = pressed;
}

// ── setup() ───────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    pinMode(TOUCH_PIN, INPUT);

    displayInit();
    setDefaultClockTime();

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

    // Offline mode keeps AP available temporarily after boot.
    WiFi.mode(connected ? WIFI_STA : WIFI_AP_STA);
    WiFi.setAutoReconnect(true);

    initMDNS();
    delay(100);

    // Scan for nearby networks when not already connected
    if (WiFi.status() != WL_CONNECTED) {
        wifiScan();
    }

    // Start AP window only in offline mode.
    if (!connected) {
        WiFi.softAP(esp_ssid, esp_password, false, 2);
        ap_shutdown_start   = millis();
        ap_shutdown_pending = true;
    }

    setupRoutes();
}

// ── loop() ────────────────────────────────────────────────────────────────
void loop() {
#ifdef ESP8266
    MDNS.update();   // ESP8266 requires periodic mDNS polling; ESP32 does not
#endif

    // Shut down AP after the offline-mode boot window.
    if (ap_shutdown_pending && (millis() - ap_shutdown_start) >= AP_OFFLINE_WINDOW_MS) {
        WiFi.mode(WIFI_STA);
        ap_shutdown_pending = false;
    }

    // Perform a deferred WiFi rescan when requested by /scan or /refresh
    if (newScan) {
        wifiScan();
        newScan = false;
    }

    handleTouchInput();

    // ── Clock display ──────────────────────────────────────────────────────
    loadCurrentTime(timeinfo);

    if (show_date_until_timeout && millis() >= show_date_until_ms) {
        show_date_until_timeout = false;
    }

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

        if (!in_time_setup) {
            colon = !colon;
        }
    }

    if (in_time_setup) {
        renderEditScreen();
    } else if (show_date_until_timeout) {
        displayShowTime(timeinfo.tm_mon + 1, timeinfo.tm_mday, false, false);
    } else if (blink) {
        displayShowTime(timeinfo.tm_hour, timeinfo.tm_min, colon, twelve);
    } else {
        displayShowTime(timeinfo.tm_hour, timeinfo.tm_min, true, twelve);
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
                    ap_shutdown_pending = false;
                    WiFi.mode(WIFI_STA);
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
