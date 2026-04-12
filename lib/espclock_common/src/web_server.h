// ESPclock - HTTP web-server route definitions
// This file is part of the ESPclock project fork by nltimv.
// Originally written by telepath9 (https://github.com/telepath9/ESPclock)
// Licensed under the GNU General Public License v3.0 (GPL-3.0)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This file has been modified by nltimv (https://github.com/nltimv).

#pragma once

#include <ESPAsyncWebServer.h>

// The AsyncWebServer instance – defined in web_server.cpp
extern AsyncWebServer server;

// Register all HTTP routes and start the server.
// Call once from setup().
void setupRoutes();
