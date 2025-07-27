#!/bin/bash

# Konfigurierbare Variablen
HOME_DIR="/home/mangar"
VERSION="0.4"

# Prüfen, ob Build-Parameter übergeben wurde
if [ -z "$1" ]; then
  echo "Usage: $0 <build>"
  exit 1
fi

BUILD="$1"

# Quell- und Zielpfade
SRC="${HOME_DIR}/dev/qapla/build/Release/Qapla"
DST_DIR="${HOME_DIR}/chess/release03/build"
DST_FILE="${DST_DIR}/qapla${VERSION}.${BUILD}"

# Kopieren mit Umbenennung
cp "$SRC" "$DST_FILE"
echo "Kopiert: $SRC → $DST_FILE"
