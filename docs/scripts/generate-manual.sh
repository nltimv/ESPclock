#!/usr/bin/env bash
# ESPclock - Script to generate a booklet PDF from the user manual Markdown
# This file is part of the ESPclock project fork by nltimv.
# Originally written by telepath9 (https://github.com/telepath9/ESPclock)
# Licensed under the GNU General Public License v3.0 (GPL-3.0)
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file has been modified by nltimv (https://github.com/nltimv).
#
# Usage: ./generate-manual.sh
#
# Prerequisites:
#   - Docker (or Podman — the script auto-detects)
#
# The script builds a small container image with pandoc + texlive on first run.
# Subsequent runs reuse the cached image.
#
# Output:
#   docs/user-manual-booklet.pdf  — A4 landscape, 2-up booklet layout
#   docs/user-manual.pdf          — intermediate A5 pages (kept for reference)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DOCS_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE_NAME="espclock-manual-builder"

INPUT="user-manual.md"
INTERMEDIATE="user-manual.pdf"
OUTPUT="user-manual-booklet.pdf"

# ── Detect container runtime ────────────────────────────────────────────────
if command -v podman &>/dev/null; then
    CONTAINER_RT=podman
elif command -v docker &>/dev/null; then
    CONTAINER_RT=docker
else
    echo "Error: Neither docker nor podman is installed." >&2
    exit 1
fi
echo "Using container runtime: $CONTAINER_RT"

# ── Build image if it doesn't exist ─────────────────────────────────────────
if ! $CONTAINER_RT image exists "$IMAGE_NAME" 2>/dev/null && \
   ! $CONTAINER_RT inspect "$IMAGE_NAME" &>/dev/null; then
    echo "── Building container image '$IMAGE_NAME' (first run only) ──"
    $CONTAINER_RT build -t "$IMAGE_NAME" "$SCRIPT_DIR"
fi

if [ ! -f "$DOCS_DIR/$INPUT" ]; then
    echo "Error: Input file not found: $DOCS_DIR/$INPUT" >&2
    exit 1
fi

# ── Run PDF generation inside container ──────────────────────────────────────
echo "── Generating booklet PDF ──"
$CONTAINER_RT run --rm \
    -v "$DOCS_DIR:/data:Z" \
    -w /data \
    "$IMAGE_NAME" \
    /data/scripts/build-inside-container.sh

echo ""
echo "Created: $DOCS_DIR/$OUTPUT"
echo ""
echo "Print this file double-sided (flip on short edge),"
echo "then fold in half to create your booklet."
