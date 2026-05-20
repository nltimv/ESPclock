// ESPclock - WiFi management (scan, mDNS, configuration)
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#pragma once

#include <Arduino.h>

// ── Device identity ────────────────────────────────────────────────────────
// By default, a deterministic ID uses the last 6 hex chars of the MAC address.
// Optional override at build time: build_flags = -D DEVICE_ID='"abcd"'
extern const char *device_id;     // Device ID (default: MAC-derived)
extern const char *esp_ssid;      // AP SSID  ("ESPclock-<DEVICE_ID>")
extern const char *mdns_name;     // mDNS name ("espclock-<DEVICE_ID>")
extern const char *esp_password;  // AP password

// ── WiFi credential state ──────────────────────────────────────────────────
extern const char *ssid;
extern const char *password;
extern bool creds_available;
extern bool connected;

// ── Offline/online state ───────────────────────────────────────────────────
extern bool          newScan;             // request a fresh network scan in loop()
extern uint8_t       attempts;           // consecutive connection-attempt counter
extern bool          setup_mode;         // true = offline mode, false = online mode
extern unsigned long ap_shutdown_start;  // millis() snapshot for AP window timer
extern bool          ap_shutdown_pending; // true = AP shutdown timer is active

// ── Functions ──────────────────────────────────────────────────────────────

// Scan for nearby networks, sort by RSSI, remove duplicate SSIDs,
// and write the result to /network_list.json on LittleFS.
void wifiScan();

// (Re-)start the mDNS responder for mdns_name.local.
void initMDNS();

// Build deterministic identity strings from hardware MAC (or DEVICE_ID override).
void initDeviceIdentity();

// Load /config.json from LittleFS and attempt to restore the saved WiFi
// connection and display/NTP settings.
void checkConfig();

// Switch the device to offline mode. Optionally clears /config.json first.
void switchToOfflineMode(bool clearConfig);
