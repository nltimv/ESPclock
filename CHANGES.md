# Changes from upstream

This project is a fork of [telepath9/ESPclock](https://github.com/telepath9/ESPclock),
which is licensed under the [GNU General Public License v3.0 (GPL-3.0)](LICENSE).

The following changes have been made in this fork by
[nltimv](https://github.com/nltimv):

---

## 2026-04-12

### Setup and discovery user experience (`device-finder/site/index.html`)
- Improved the setup and discovery user experience by unifying first-time setup and daily access into a single deep-link flow with simpler screens and clearer guidance.
- Added automatic local-network discovery (mDNS first, then subnet scan) and setup-mode detection so users can reach the device with fewer manual steps.

### Captive portal and setup reliability (`lib/espclock_common/src/wifi_manager.*`, `lib/espclock_common/src/web_server.cpp`, `src/espclock.cpp`)
- Removed the firmware captive-portal DNS redirect and shifted setup handoff to the finder flow to avoid interruptions on mobile platforms.
- Adjusted `/uicheck` CORS behavior to support hosted finder deployments while keeping the endpoint read-only.

### Timezone architecture and web UI (`lib/espclock_common/src/tz_lookup.*`, `lib/espclock_common/src/ntp.h`, `lib/espclock_common/src/web_server.cpp`, `lib/espclock_common/src/wifi_manager.cpp`, `data/index.html`, `data/tz.json`)
- Reworked timezone handling to use IANA names end-to-end, with device-side POSIX lookup from `tz.json`.
- Replaced large inline timezone data in the web UI with a data-driven region/city selector sourced from `/tz.json`.
- Updated and corrected timezone mappings against authoritative tzdata sources to improve timezone accuracy.

### Firmware structure and build layout (`platformio.ini`, `firmware/`, `lib/espclock_common/`, `src/espclock.cpp`)
- Consolidated firmware variants into a unified project structure and shared display abstraction to reduce duplication and simplify maintenance.
- Moved PlatformIO configuration to the repository root for consistent root-level builds.
- Fixed Wi-Fi mode initialization to prevent unintended default AP behavior in normal operation.

---

## 2026-04-09

### Shared codebase refactor (`lib/espclock_common/`, `esp32/`, `esp8266/tm1637_display/src/`)
- Introduced a shared firmware library and display abstraction, and split large source files into clearer modules.
- Aligned ESP32 behavior with the ESP8266 feature set for setup flow, timezone handling, configuration schema, and API responses.

---

## 2026-04-08

### Setup completion user experience (`esp32/tm1637_display`, `esp32/tm1652_display`, `esp8266/tm1637_display`)
- Added direct IP and mDNS links after timezone setup so users can open the device UI faster.

---

## 2026-04-06

### Firmware setup, networking, and time config (`esp8266/tm1637_display/src/espclock8266.cpp`, `esp32/tm1637_display/src/espclock32.cpp`, `esp32/tm1652_display/src/espclock32.cpp`)
- Added setup-mode improvements (conditional AP behavior, automatic AP shutdown, and safer config handling) to improve first-time setup and reconnection reliability.
- Improved network identity and discovery support with `DEVICE_ID`-based naming and richer setup/status API responses.
- Upgraded time configuration to POSIX timezone support and expanded timezone setup flow.

### Web UI and project scaffolding (`esp32/data/index.html`, `esp8266/tm1637_display/data/index.html`, `esp8266/tm1637_display/`)
- Expanded the web UI setup flow to match firmware changes and added an ESP8266 web UI where upstream had none.
- Added missing ESP8266 project scaffolding files needed for local development and builds.
