FROM docker.io/pandoc/extra:latest-ubuntu

# Install pdfjam (texlive-extra-utils) and its LaTeX dependencies
# (texlive-latex-extra provides pdfpages, eso-pic, etc.) plus ghostscript
# and poppler-utils (pdfinfo).
# TEXINPUTS bridges the apt-installed packages so that the image's own
# pdflatex at /opt/texlive/ can find them.
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ghostscript poppler-utils texlive-extra-utils texlive-latex-extra && \
    rm -rf /var/lib/apt/lists/*

ENV TEXINPUTS=.:/usr/share/texlive/texmf-dist/tex//:

ENTRYPOINT ["/bin/bash"]
