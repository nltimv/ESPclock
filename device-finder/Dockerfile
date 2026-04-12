# ESPclock - NGINX container for the central device-finder static web app
# This file is part of the ESPclock project fork by nltimv.
# Originally written by telepath9 (https://github.com/telepath9/ESPclock)
# Licensed under the GNU General Public License v3.0 (GPL-3.0)
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file has been modified by nltimv (https://github.com/nltimv).

FROM nginx:1.27-alpine

COPY nginx/default.conf /etc/nginx/conf.d/default.conf
COPY site/ /usr/share/nginx/html/

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
  CMD wget --quiet --spider http://127.0.0.1/ || exit 1
