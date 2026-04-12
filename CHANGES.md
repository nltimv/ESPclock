# Changes from upstream

This project is a fork of [telepath9/ESPclock](https://github.com/telepath9/ESPclock),
which is licensed under the [GNU General Public License v3.0 (GPL-3.0)](LICENSE).

The following changes have been made in this fork by
[nltimv](https://github.com/nltimv):

---

## 2026-04-12

### Central device finder app and cross-origin discovery support (`device-finder/`, `lib/espclock_common/src/web_server.cpp`)
- Added a new centrally-hostable static web app (`device-finder/site/index.html`)
  that:
  - accepts `DEVICE_ID`,
  - tries mDNS deep links (`http://espclock-<DEVICE_ID>.local`),
  - scans a user-selected `/24` subnet by calling `/uicheck` to find the
    matching `ESPclock-<DEVICE_ID>` AP identity,
  - generates a QR code for the discovered deep link,
  - supports setup-mode deep-link flow using `http://192.168.4.1/` when the
    client is connected to the device AP.
- Added `device-finder/Dockerfile` and `device-finder/nginx/default.conf` to
  host the finder app with NGINX.
- Added default CORS response headers in the firmware web server so the central
  finder app can query `/uicheck` from a different origin.

### `lib/espclock_common/src/tz_lookup.h` + `tz_lookup.cpp`: New IANA→POSIX lookup — reads from `tz.json`
- New header `tz_lookup.h` declares `const char* tzLookup(const char* ianaName)`.
- `tz_lookup.cpp` opens `/tz.json` from LittleFS at call time and uses ArduinoJson's
  `DeserializationOption::Filter` to materialise only the single requested key, keeping
  heap usage minimal.  Result is copied into a static 80-byte buffer and returned.
- `tz.json` is therefore the single source of truth: served to the browser for populating
  the region/city dropdowns, and read by the firmware for POSIX string resolution.

### `lib/espclock_common/src/ntp.h`, `src/espclock.cpp`, `web_server.cpp`, `wifi_manager.cpp`: IANA name stored + sent from device
- Added `tz_iana` global (`const char*`, default `"UTC"`) alongside `tz_posix`.
- `/uicheck` endpoint now returns the IANA name in the `"tz"` field instead of the POSIX string.
- `/updatetime` and `/setup_timezone` receive the IANA name from the browser, call
  `tzLookup()` to derive `tz_posix`, and persist the IANA name in `config.json`.
- `checkConfig()` in `wifi_manager.cpp` reads the IANA name from `config.json`,
  calls `tzLookup()` to populate `tz_posix`, and calls `configTzTime()`.

### `data/index.html`: POSIX lookup removed from JS; simplified TZ restore
- Both submit handlers now post the selected IANA name directly (e.g. `Europe/Amsterdam`);
  no `TZ_DATA` JS lookup is performed.
- `setTzFromIana()` replaces `setTzFromPosix()`: it receives the IANA name returned by
  `/uicheck` and directly selects the matching region and city dropdowns — no scanning
  or `localStorage` needed.
- Removed `saveTzChoice()` and all `localStorage` usage (disambiguation is now handled
  by the device storing the exact IANA name).
- `buildTzData` comment updated to note that POSIX values are used by the C++ table only.

### `data/index.html`: City dropdown values changed to IANA names
- Removed city-merging logic from `buildTzData`; it now stores the raw JSON directly.
- `populateTzCity` sets each option's value to the full IANA name (e.g. `Europe/Amsterdam`)
  and derives the display label from it.
- `populateTzRegion` extracts unique region prefixes from flat JSON keys.
- Removed the dead block-comment containing the old merged TZ data.

### `data/index.html`: Verified and corrected TZ_DATA against IANA tzdata (nayarsystems/posix_tz_db)
Cross-referenced every POSIX string against the authoritative nayarsystems/posix_tz_db
(which mirrors IANA tzdata). The following errors were found and corrected:
- **Africa/Cairo**: now uses correct DST rules `EET-2EEST,M4.5.5/0,M10.5.4/24`
  (Egypt reinstated DST in 2023); split from Tripoli (`EET-2`, no DST).
- **Africa/Juba, Khartoum**: moved from EAT-3 group to CAT-2 (UTC+2) — both are
  Central Africa Time, not East Africa Time.
- **America/Adak**: corrected abbreviation `HAST10HADT` → `HST10HDT`.
- **America/Asunción**: fixed STD/DST order; Paraguay standard is UTC-4
  (`<-04>4<-03>,M9.1.6/0,M4.1.6/0`), not UTC-3.
- **America/Godthab (Nuuk)**: now includes DST rules
  `<-02>2<-01>,M3.5.0/-1,M10.5.0/0`; split from Noronha (`<-02>2`, no DST).
- **America/Havana**: corrected DST end month from October (M10) to November (M11)
  and fixed transition format: `CST5CDT,M3.2.0/0,M11.1.0/1`.
- **America/Santiago**: fixed STD/DST order; Chilean standard is UTC-4
  (`<-04>4<-03>,M9.1.6/24,M4.1.6/24`), not UTC-3.
- **America/St. John's**: simplified transition string to `NST3:30NDT,M3.2.0,M11.1.0`.
- **America/Bogotá, Guayaquil, Lima, Rio Branco**: separated from `EST5` group
  into `<-05>5` (different POSIX abbreviation).
- **Antarctica/Macquarie**: added (observes Australian DST).
- **Antarctica/Vostok**: corrected UTC+6 → UTC+5 (`<+05>-5`).
- **Asia/Almaty, Qyzylorda**: moved from UTC+6 to UTC+5 group (Kazakhstan reform 2024).
- **Asia/Novosibirsk**: moved from UTC+6 group to UTC+7 group (correct offset).
- **Asia/Bishkek**: added to UTC+6 group (was missing).
- **Asia/Amman**: `EET-2EEST,…` → `<+03>-3` (Jordan abolished DST in 2022).
- **Asia/Damascus**: `EET-2EEST,…` → `<+03>-3` (Syria moved to permanent UTC+3).
- **Asia/Gaza, Hebron**: corrected DST transition `M3.4.4/48,M10.4.5` →
  `M3.4.4/50,M10.4.4/50`.
- **Asia/Kabul, Kathmandu, Rangoon, Tehran**: corrected abbreviation format from
  `<+04:30>` style to `<+0430>` style (standard POSIX form).
- **Atlantic/Azores**: removed redundant DST name `<+00>0` → `<+00>`.
- **Atlantic/Stanley (Falkland Islands)**: corrected UTC-2 → UTC-3 (`<-03>3`);
  split from South Georgia (`<-02>2`).
- **Australia/Eucla**: corrected `<+08:45>` → `<+0845>`.
- **Australia/Lord Howe**: corrected `<+10:30>` → `<+1030>`.
- **Europe/Dublin**: separated from London group; Dublin uses the inverted
  `IST-1GMT0,M10.5.0,M3.5.0/1` string (IST = Irish Standard Time in summer).
- **Europe/Lisbon**: separated from London/GMT0BST group; Lisbon uses
  `WET0WEST,M3.5.0/1,M10.5.0` (Western European Time).
- **Europe/Chisinau**: separated from Athens/Helsinki group; uses slightly different
  spring transition `EET-2EEST,M3.5.0,M10.5.0/3` (midnight, not 3 am).
- **Europe/Kirov**: added to Moscow group.
- **Pacific/Apia**: removed DST rules; Samoa no longer observes DST → `<+13>-13`.
- **Pacific/Fiji**: removed DST rules; Fiji suspended DST in 2022 → `<+12>-12`.
- **Pacific/Chatham**: corrected abbreviation format to `<+1245>` / `<+1345>`.
- **Pacific/Chuuk**: corrected to `<+10>-10`; separated from Guam/Saipan (`ChST-10`).
- **Pacific/Marquesas**: corrected `<-09:30>` → `<-0930>`.
- **Pacific/Norfolk**: corrected `<+12>-12` DST name → implicit `<+12>`.
- **Pacific/Port Moresby**: corrected `AEST-10` → `<+10>-10` (AEST is an Australian
  abbreviation; PNG uses a different local abbreviation).

### `data/tz.json` (new file): IANA timezone database with POSIX strings
- New file served by the ESP as `/tz.json`.
- Contains all inhabited canonical IANA timezone names as keys
  (e.g. `"Europe/Amsterdam"`) with their POSIX timezone strings as values,
  sourced from the authoritative nayarsystems/posix_tz_db (which mirrors IANA tzdata).
- The JS `buildTzData(raw)` function groups the flat JSON by region (the
  `Region/City` prefix), merges cities that share the same POSIX string within
  a region into a single dropdown option, and replaces underscores with spaces in
  city names.
- `window.onload` now fetches `/tz.json` and `/uicheck` in parallel via
  `Promise.all`, so dropdown population and value restoration always happen in
  the correct order with a single round-trip.
- The old large inline `TZ_DATA` JS constant in `index.html` has been removed;
  the new code is entirely data-driven from `tz.json`.

### `data/index.html`: Two-dropdown timezone picker with full coverage
- Replaced both flat `<select>` timezone lists with a linked region → city two-dropdown
  UX (no external library; works fully offline in AP mode).
- Added a self-contained `TZ_DATA` JS object covering all inhabited IANA timezone
  regions (Africa, America, Antarctica, Arctic, Asia, Atlantic, Australia, Europe,
  Indian, Pacific, UTC) with correct POSIX strings and DST rules.
- Cities sharing an identical POSIX string are merged into a single option labelled
  `"City1, City2, ..."` within the same region.
- Helper functions `populateTzRegion`, `populateTzCity`, and `setTzFromPosix`
  replace the old static `<option>` lists and handle dropdown linking and saved-value
  restoration.
- Refactored NTP settings layout from a horizontal flex row to a vertical column so
  the two timezone selects fit naturally below the NTP server input.

### `data/index.html`: Merge duplicate timezone options
- Merged `Europe/Rome` and `Europe/Paris` (both `CET-1CEST,M3.5.0,M10.5.0/3`)
  into a single `"Europe/Rome, Europe/Paris"` option in both timezone dropdowns,
  ensuring every option is individually selectable.

### Fix: spurious default AP started in normal mode (`src/espclock.cpp`)
- `WiFi.mode(WIFI_AP_STA)` was called unconditionally in `setup()`, even when
  `checkConfig()` had already restored a saved connection (`connected = true`).
  On ESP8266, switching to `WIFI_AP_STA` immediately activates the AP interface
  with the chip's default SSID (e.g. `ESP-EEA8FE`).  Because the subsequent
  `WiFi.softAP(esp_ssid, …)` call is guarded by `if (!connected)`, the AP was
  never configured with the intended credentials, leaving the rogue default AP
  running.
- Fixed by switching the mode conditionally:
  `WiFi.mode(connected ? WIFI_STA : WIFI_AP_STA)`.  In normal (non-setup) mode
  the AP interface is never activated.

### Move `platformio.ini` to project root
- Moved `firmware/platformio.ini` to the project root so PlatformIO can be
  invoked from the repository root without entering the `firmware/` subdirectory.
- Updated `lib_extra_dirs` from `../lib` to `lib` (now relative to root).
- Added explicit `src_dir = firmware/src` and `data_dir = firmware/data` to
  preserve the existing source layout.

### Unified display driver and single PlatformIO project (`firmware/`, `lib/espclock_common/`)
- Removed the three separate per-variant PlatformIO projects
  (`esp8266/tm1637_display/`, `esp32/tm1637_display/`, `esp32/tm1652_display/`)
  and replaced them with a single unified project at `firmware/`.
- `firmware/platformio.ini` — one config, three build environments:
  - `d1_mini_tm1637` — ESP8266 Wemos D1 Mini + TM1637 display
  - `esp32dev_tm1637` — ESP32 DevKit + TM1637 display
  - `esp32dev_tm1652` — ESP32 DevKit + TM1652 display
  - Common settings (`monitor_speed`, `framework`, `board_build.filesystem`,
    `lib_extra_dirs`) live in the shared `[env]` base section and are
    inherited by all environments without repetition.
  - Display type and pin assignments are selected via `build_flags`
    (`-D DISPLAY_TM1637 -D DISPLAY_CLK=<pin> -D DISPLAY_DIO=<pin>` or
     `-D DISPLAY_TM1652 -D DISPLAY_DATA_PIN=<pin> -D DISPLAY_DIGITS=<n>`).
- `firmware/src/espclock.cpp` — single main file covering all three variants;
  the only platform-specific code is `MDNS.update()` behind `#ifdef ESP8266`.
- `firmware/data/index.html` — single shared web UI (previously duplicated
  in each variant's own `data/` directory).
- `lib/espclock_common/src/display.h` / `display.cpp` — unified display
  driver; `myTimer()` and display-state globals defined once; TM1637 and
  TM1652 chip-specific code in `#if defined(DISPLAY_TM1637)` /
  `#elif defined(DISPLAY_TM1652)` blocks.  A build-time `#error` fires if
  neither flag is set, preventing silent misconfiguration.

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
