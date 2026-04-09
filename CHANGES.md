# Changes from upstream

This project is a fork of [telepath9/ESPclock](https://github.com/telepath9/ESPclock),
which is licensed under the [GNU General Public License v3.0 (GPL-3.0)](LICENSE).

The following changes have been made in this fork by
[nltimv](https://github.com/nltimv):

---

## 2026-04-09

### Shared firmware library and ESP32 code merge (`lib/espclock_common/`, `esp32/`)
- Created a shared PlatformIO library `lib/espclock_common/` containing all code
  that is identical across ESP8266 and ESP32 variants:
  - `display_api.h` — abstract display interface (function prototypes and state
    globals) that every platform-specific `display.cpp` must implement.
  - `wifi_manager.h` / `wifi_manager.cpp` — WiFi scan (RSSI-sorted,
    de-duplicated), mDNS, `checkConfig()`, and WiFi/setup-mode globals.
    Uses `#ifdef ESP8266` / `#else` guards for platform-specific includes.
  - `web_server.h` / `web_server.cpp` — `AsyncWebServer` object and all HTTP
    route handlers; calls `displaySetBrightness()` via the display abstraction.
  - `ntp.h` — shared `extern` declarations for NTP/time-state variables.
  - `json_config.h` — ArduinoJson compile-time optimisation macros.
- Introduced a display abstraction layer (`displayInit`, `displayClear`,
  `displayShowError`, `displayShowTrying`, `displayShowAttempt`,
  `displaySetBrightness`, `displayShowTime`, `displayAnim`), implemented
  independently in each variant's `display.cpp`.
- Upgraded ESP32 firmware to match ESP8266 feature set:
  - Switched from GMT-offset-based NTP to POSIX timezone strings (`configTzTime`).
  - Added `DEVICE_ID`-based AP SSID and mDNS hostname.
  - Upgraded WiFi scan to the RSSI-sorted, duplicate-filtered algorithm.
  - Added `/setup_timezone` endpoint (defers AP shutdown until timezone is set).
  - Updated `/updatetime` to accept POSIX `tz` string instead of `offset` int.
  - Updated `/uicheck` to return `ntp` and `tz` fields.
  - Updated `/config` to preserve existing WiFi credentials in normal mode.
  - Updated config.json schema: stores `tz` (POSIX string) instead of `offset` (int).
  - Removed unused `/alarm` placeholder and `/uptime` endpoints.
- Gave both ESP32 variants their own PlatformIO project structure with
  `platformio.ini`, `src/`, and `data/` directories.
- Shared the feature-complete web UI (ESP8266 HTML) with both ESP32 variants.

### ESP8266 TM1637 firmware — code split (`esp8266/tm1637_display/src/`)
- Split the single monolithic `espclock8266.cpp` into five logical modules:
  - `display.h` / `display.cpp` — TM1637 hardware object, segment constants,
    `myTimer()`, `displayAnim()`, and all display-state globals.
  - `wifi_manager.h` / `wifi_manager.cpp` — WiFi scan, mDNS initialisation,
    `checkConfig()`, device-identity constants, and WiFi/setup-mode globals.
  - `web_server.h` / `web_server.cpp` — `AsyncWebServer` object and all HTTP
    route handlers, factored into a single `setupRoutes()` function.
  - `ntp.h` — shared `extern` declarations for the NTP/time-state variables.
  - `json_config.h` — ArduinoJson compile-time optimisation macros and single
    include point for `ArduinoJson.h`.
  - `espclock8266.cpp` (slimmed down) — NTP globals, `setup()`, and `loop()`.
- Simplified the time-display logic in `loop()` (eliminated duplicated
  branch arms for the 12-hr / blink combination).
- Made animation-state variables (`px`, `forw`, `SEG_WAIT`) `static` and
  local to `display.cpp`.

---

## 2026-04-08

### All firmware variants (`esp32/tm1637_display`, `esp32/tm1652_display`, `esp8266/tm1637_display`)
- After the timezone-setup step completes, the device's IP address and mDNS
  hostname are now displayed as clickable hyperlinks in the setup UI so the
  user can navigate directly to the clock's web interface.

---

## 2026-04-06

### ESP8266 TM1637 firmware (`esp8266/tm1637_display/src/espclock8266.cpp`)
- Added `DEVICE_ID` compile-time macro so multiple devices can coexist on the
  same network without conflicting AP SSIDs or mDNS names
  (`ESPclock-<DEVICE_ID>` / `espclock-<DEVICE_ID>`).
- Added `setup_mode` flag: the device only starts its access point when no
  configuration has been saved yet, so the AP does not reappear after every
  reboot once the device is configured.
- Added automatic AP shutdown with a 15-second grace period after the
  first-time setup completes, giving the user time to read the device's IP
  address before the AP disappears.
- Replaced GMT-offset-based NTP configuration (`configTime`) with POSIX
  timezone string support (`configTzTime`), allowing correct handling of
  daylight-saving-time rules.
- Added default NTP server (`pool.ntp.org`) and default timezone (`UTC0`).
- Improved WiFi network scan: networks are now sorted by signal strength
  (RSSI, strongest first) and duplicate SSIDs are removed before the list is
  returned to the UI.
- The `/uicheck` status endpoint now also returns the current NTP server,
  timezone string, setup mode flag, and AP SSID.
- Added `/setup_timezone` POST endpoint to allow the timezone to be set as a
  separate step after WiFi configuration.
- Added IP address and mDNS hostname to the JSON response of the timezone
  setup endpoint.
- Various bug fixes and code cleanup.

### ESP32 TM1637 firmware (`esp32/tm1637_display/src/espclock32.cpp`)
- Added `setup_mode` flag and automatic AP shutdown (same behaviour as
  ESP8266 variant described above).
- The device only starts its access point when in setup mode.
- Auto-saves WiFi credentials to `config.json` upon the first successful
  connection, so the device reconnects automatically on subsequent reboots.
- Fixed NTP initialisation: the NTP client is no longer started when the NTP
  address is missing from the saved configuration, preventing a crash.
- Removed the restriction that prevented re-saving an existing configuration
  file; configuration can now be updated at any time.
- Added IP address and mDNS hostname to the timezone-setup JSON response.
- Added `setup_mode` and AP SSID to the `/uicheck` status JSON.

### ESP32 TM1652 firmware (`esp32/tm1652_display/src/espclock32.cpp`)
- Same changes as the ESP32 TM1637 variant listed above.

### Web UI — ESP32 (`esp32/data/index.html`)
- Updated to expose the `setup_mode` flag and AP SSID returned by the
  firmware.
- Added POSIX timezone string input field to the time-settings section.
- Added a dedicated timezone-setup step that calls the `/setup_timezone`
  endpoint.
- IP/mDNS links shown after setup complete (see 2026-04-08 entry).

### Web UI — ESP8266 (`esp8266/tm1637_display/data/index.html`) *(new file)*
- The upstream repository did not include a web UI for the ESP8266 variant.
  This fork adds `esp8266/tm1637_display/data/index.html` with the same
  feature set as the updated ESP32 web UI described above.

### Project infrastructure (`esp8266/tm1637_display/`)
- Added `platformio.ini`, `.gitignore`, and `.vscode/extensions.json` for
  the ESP8266 project, which were absent from the upstream repository.
