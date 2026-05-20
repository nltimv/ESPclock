#!/usr/bin/env bash
# ESPclock - Inner build script, runs inside the container
# This file is part of the ESPclock project fork by nltimv.
# Originally written by telepath9 (https://github.com/telepath9/ESPclock)
# Licensed under the GNU General Public License v3.0 (GPL-3.0)
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This file has been modified by nltimv (https://github.com/nltimv).

set -euo pipefail

INPUT="user-manual.md"
INTERMEDIATE="user-manual.pdf"
OUTPUT="user-manual-booklet.pdf"

echo "  Step 1/2: Converting Markdown → A5 PDF"
pandoc "$INPUT" \
    -o "$INTERMEDIATE" \
    --pdf-engine=pdflatex \
    -V geometry:"a5paper,margin=1.5cm" \
    -V fontsize:11pt \
    -V documentclass:article \
    -V 'header-includes:\usepackage{tikz}' \
    -V 'header-includes:\usepackage{etoc}' \
    -V 'header-includes:\usepackage{booktabs}' \
    -V 'header-includes:\usepackage{enumitem}' \
    -V 'header-includes:\setlist{nosep,leftmargin=1.2em}' \
    -V 'header-includes:\usepackage{titlesec}' \
    -V 'header-includes:\titleformat{\part}[block]{}{}{0pt}{}' \
    -V 'header-includes:\titlespacing*{\part}{0pt}{0pt}{0pt}' \
    -V 'header-includes:\titleformat{\section}{\large\bfseries}{}{0em}{}' \
    -V 'header-includes:\titleformat{\subsection}{\normalsize\bfseries}{}{0em}{}' \
    -V 'header-includes:\titleformat{\subsubsection}{\normalsize\itshape}{}{0em}{}' \
    -V 'header-includes:\titlespacing*{\section}{0pt}{1.5ex plus .5ex minus .2ex}{0.8ex plus .2ex}' \
    -V 'header-includes:\titlespacing*{\subsection}{0pt}{1.2ex plus .3ex minus .1ex}{0.5ex plus .1ex}' \
    -V 'header-includes:\pagestyle{plain}' \
    --columns=50

echo "  Step 2/2: Imposing A5 pages → A4 landscape booklet"

# Get total page count
PAGES=$(pdfinfo "$INTERMEDIATE" | grep '^Pages:' | awk '{print $2}')
echo "    Source has $PAGES pages"

# Pad to a multiple of 4 for proper booklet signatures
REM=$((PAGES % 4))
if [ "$REM" -ne 0 ]; then
    PAD=$((4 - REM))
    echo "    Padding with $PAD blank page(s)"

    # Create a blank A5 page
    cat > /tmp/blank.tex << 'LATEX'
\documentclass{article}
\usepackage[a5paper]{geometry}
\begin{document}
\null
\thispagestyle{empty}
\end{document}
LATEX
    pdflatex -interaction=batchmode -output-directory=/tmp /tmp/blank.tex >/dev/null 2>&1

    BLANK_ARGS=""
    for _ in $(seq 1 "$PAD"); do BLANK_ARGS="$BLANK_ARGS /tmp/blank.pdf"; done
    # shellcheck disable=SC2086
    pdfjam --fitpaper true --outfile "/tmp/padded.pdf" "$INTERMEDIATE" $BLANK_ARGS 2>/dev/null
    mv /tmp/padded.pdf "$INTERMEDIATE"
    PAGES=$((PAGES + PAD))
    echo "    Padded to $PAGES pages"
fi

# Build booklet page order
# For a booklet, sheets are printed 2-up. When folded, the page order must be:
# Sheet 1 front: last, first  |  Sheet 1 back: second, second-to-last
# e.g. for 8 pages: 8,1 | 2,7 | 6,3 | 4,5
ORDER=""
LO=1
HI=$PAGES
while [ "$LO" -lt "$HI" ]; do
    if [ -n "$ORDER" ]; then ORDER="$ORDER,"; fi
    ORDER="${ORDER}${HI},${LO},$((LO + 1)),$((HI - 1))"
    LO=$((LO + 2))
    HI=$((HI - 2))
done

echo "    Booklet page order: $ORDER"

# Impose 2-up on A4 landscape in booklet order
pdfjam --nup 2x1 --landscape --paper a4paper \
    --outfile "$OUTPUT" \
    "$INTERMEDIATE" "$ORDER" 2>/dev/null

rm -f "$INTERMEDIATE"
rm -f .crop-tmp.pdf

echo "  Done!"
