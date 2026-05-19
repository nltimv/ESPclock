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

#ifndef SETUP_BUTTON_PIN
#ifdef ESP8266
#define SETUP_BUTTON_PIN 14
#else
#define SETUP_BUTTON_PIN 4
#endif
#endif

#ifndef ACTION_BUTTON_PIN
#ifdef ESP8266
#define ACTION_BUTTON_PIN 12
#else
#define ACTION_BUTTON_PIN 5
#endif
#endif

#ifndef BUTTON_ACTIVE_STATE
#define BUTTON_ACTIVE_STATE LOW
#endif

#ifndef BUTTON_PIN_MODE
#define BUTTON_PIN_MODE INPUT_PULLUP
#endif

static const unsigned long AP_OFFLINE_WINDOW_MS  = 15UL * 60UL * 1000UL;
static const unsigned long AP_SETUP_CONFIRM_WINDOW_MS = 5UL * 1000UL;
static const unsigned long BUTTON_DEBOUNCE_MS    = 35UL;
static const unsigned long BUTTON_SHORT_PRESS_MS = 80UL;
static const unsigned long BUTTON_LONG_PRESS_MS  = 1200UL;
static const unsigned long BUTTON_REPEAT_MS      = 300UL;
static const unsigned long EDIT_FLASH_MS         = 450UL;
static const unsigned long RESET_HINT_MS         = 5000UL;
static const unsigned long RESET_HOLD_MS         = 10000UL;

enum class EditField : uint8_t {
    NONE = 0,
    TWELVE_24,
    HOUR,
    MINUTE
};

static bool          in_time_setup           = false;
static EditField     edit_field              = EditField::NONE;
static tm            edit_time               = {};
static bool          show_edit_value         = true;
static unsigned long edit_flash_toggle_at    = 0;

struct ButtonState {
    bool          raw_state = false;
    bool          debounced_state = false;
    unsigned long raw_changed_ms = 0;
    unsigned long pressed_at_ms = 0;
    bool          short_released = false;
    bool          long_released = false;
    bool          long_pressed = false;
    bool          long_press_fired = false;
    unsigned long long_press_next_ms = 0;
};

static ButtonState   setup_button;
static ButtonState   action_button;
static bool          both_held_active    = false;
static unsigned long both_held_since_ms  = 0;
static bool          reset_hint_active   = false;
static bool          reset_done          = false;
static bool          combo_in_progress   = false;

static void setClockFromTm(const tm &src) {
    tm copy = src;
    copy.tm_isdst = -1;
    time_t epoch = mktime(&copy);
    if (epoch < 0) return;
    timeval tv = {epoch, 0};
    settimeofday(&tv, nullptr);
    myTimerReset();
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

static void showResetConfirmFeedback() {
    for (uint8_t i = 0; i < 2; ++i) {
        displayClear();
        delay(140);
        displayShowTime(0, 0, true, false);
        delay(140);
    }
}

static void startTimeSetup() {
    loadCurrentTime(edit_time);
    in_time_setup        = true;
    edit_field           = EditField::TWELVE_24;
    show_edit_value      = true;
    edit_flash_toggle_at = millis();
}

static void advanceTimeSetupField() {
    switch (edit_field) {
        case EditField::TWELVE_24: edit_field = EditField::HOUR;      break;
        case EditField::HOUR:      edit_field = EditField::MINUTE;    break;
        case EditField::MINUTE:
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

    switch (edit_field) {
        case EditField::TWELVE_24:
            if (show_edit_value) {
                displayShowHourMode(twelve);
            } else {
                displayClear();
            }
            break;
        case EditField::HOUR:
            displayShowTimePartial(edit_time.tm_hour, edit_time.tm_min, true, twelve,
                                   show_edit_value, true);
            break;
        case EditField::MINUTE:
            displayShowTimePartial(edit_time.tm_hour, edit_time.tm_min, true, twelve,
                                   true, show_edit_value);
            break;
        default:
            displayShowTime(timeinfo.tm_hour, timeinfo.tm_min, true, twelve);
            break;
    }
}

static void updateButton(ButtonState &button, uint8_t pin) {
    bool pressed = (digitalRead(pin) == BUTTON_ACTIVE_STATE);
    unsigned long now = millis();

    button.short_released = false;
    button.long_released  = false;
    button.long_pressed   = false;

    if (pressed != button.raw_state) {
        button.raw_state = pressed;
        button.raw_changed_ms = now;
    }

    if ((now - button.raw_changed_ms) < BUTTON_DEBOUNCE_MS) {
        return;
    }

    if (button.debounced_state != button.raw_state) {
        button.debounced_state = button.raw_state;
        if (button.debounced_state) {
            button.pressed_at_ms    = now;
            button.long_press_fired = false;
        } else {
            unsigned long held = now - button.pressed_at_ms;
            if (held >= BUTTON_LONG_PRESS_MS) {
                button.long_released = true;
            } else if (held >= BUTTON_SHORT_PRESS_MS) {
                button.short_released = true;
            }
        }
    }

    // Fire long_pressed while the button is held past the long-press threshold,
    // then auto-repeat every BUTTON_REPEAT_MS.
    if (button.debounced_state) {
        unsigned long held = now - button.pressed_at_ms;
        if (held >= BUTTON_LONG_PRESS_MS) {
            if (!button.long_press_fired) {
                button.long_pressed          = true;
                button.long_press_fired      = true;
                button.long_press_next_ms    = now + BUTTON_REPEAT_MS;
            } else if (now >= button.long_press_next_ms) {
                button.long_pressed       = true;
                button.long_press_next_ms = now + BUTTON_REPEAT_MS;
            }
        }
    }
}

static void handleButtonInput() {
    updateButton(setup_button, SETUP_BUTTON_PIN);
    updateButton(action_button, ACTION_BUTTON_PIN);

    unsigned long now = millis();
    bool both_held = setup_button.debounced_state && action_button.debounced_state;

    if (both_held && !both_held_active) {
        both_held_active   = true;
        both_held_since_ms = now;
        reset_hint_active  = false;
        reset_done         = false;
        combo_in_progress  = true;
    }

    if (!both_held) {
        both_held_active = false;
        reset_hint_active = false;
        if (!setup_button.debounced_state && !action_button.debounced_state) {
            combo_in_progress = false;
        }
    }

    if (both_held_active) {
        unsigned long held = now - both_held_since_ms;
        reset_hint_active = (held >= RESET_HINT_MS && held < RESET_HOLD_MS && !reset_done);
        if (held >= RESET_HOLD_MS && !reset_done) {
            switchToOfflineMode(true);
            setDefaultClockTime();
            showResetConfirmFeedback();
            reset_done = true;
            reset_hint_active = false;
        }
    }

    if (!combo_in_progress) {
        // setup_mode=true means offline/local-setup mode; block manual time edits
        // once the device is in online mode and syncing time from the network.
        if (setup_mode && setup_button.long_pressed && !in_time_setup) {
            startTimeSetup();
        } else if (setup_button.short_released && in_time_setup) {
            advanceTimeSetupField();
        }

        if (in_time_setup && (action_button.short_released || action_button.long_pressed)) {
            cycleCurrentValue();
        }
    }
}

// ── setup() ───────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    pinMode(SETUP_BUTTON_PIN, BUTTON_PIN_MODE);
    pinMode(ACTION_BUTTON_PIN, BUTTON_PIN_MODE);

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

    initDeviceIdentity();
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
    // setup_mode flips to false in /setup_timezone before we start the final
    // AP shutdown timer, so false here means the short post-confirmation grace
    // period rather than the initial offline onboarding window.
    const unsigned long ap_shutdown_window_ms =
        setup_mode ? AP_OFFLINE_WINDOW_MS : AP_SETUP_CONFIRM_WINDOW_MS;
    if (ap_shutdown_pending && (millis() - ap_shutdown_start) >= ap_shutdown_window_ms) {
        WiFi.mode(WIFI_STA);
        ap_shutdown_pending = false;
    }

    // Perform a deferred WiFi rescan when requested by /scan or /refresh
    if (newScan) {
        wifiScan();
        newScan = false;
    }

    handleButtonInput();

    // ── Clock display ──────────────────────────────────────────────────────
    loadCurrentTime(timeinfo);

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

    if (reset_hint_active) {
        displayShowTime(88, 88, true, false);
    } else if (in_time_setup) {
        renderEditScreen();
    } else {
        displayShowTime(timeinfo.tm_hour, timeinfo.tm_min, colon, twelve);
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
