#!/usr/bin/env bash
# Fetch the public benchmark traces Memogent uses.
# Right now we pull LSApp (599k events / 87 apps / 292 users).
set -euo pipefail

DEST="${1:-bench/traces}"
mkdir -p "${DEST}"

if [ ! -f "${DEST}/lsapp.tsv" ]; then
    echo "fetching LSApp ..."
    # The canonical mirror at github.com/aliannejadi/LSApp requires a tarball.
    curl -fsSL https://github.com/aliannejadi/LSApp/raw/main/lsapp.tsv.zip -o "${DEST}/lsapp.tsv.zip"
    unzip -o "${DEST}/lsapp.tsv.zip" -d "${DEST}"
    rm -f "${DEST}/lsapp.tsv.zip"
fi

echo "datasets available at ${DEST}"
ls -lh "${DEST}"
