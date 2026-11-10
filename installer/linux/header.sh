#!/bin/sh
# Ember Tube EQ 1.0.0 - Linux Installer
# Usage: sh EmberTubeEQ-1.0.0-Linux-Setup.run            (install for current user)
#        sudo sh EmberTubeEQ-1.0.0-Linux-Setup.run       (install system-wide)
#        sh EmberTubeEQ-1.0.0-Linux-Setup.run --uninstall
set -e
NAME="Ember Tube EQ"
if [ "$(id -u)" = "0" ]; then
  VST3_DIR=/usr/lib/vst3; CLAP_DIR=/usr/lib/clap
else
  VST3_DIR="$HOME/.vst3"; CLAP_DIR="$HOME/.clap"
fi

if [ "$1" = "--uninstall" ]; then
  rm -rf "$VST3_DIR/EmberEQ.vst3" "$CLAP_DIR/EmberEQ.clap"
  echo "$NAME wurde entfernt."
  exit 0
fi

echo "== $NAME 1.0.0 Installer =="
echo "VST3 -> $VST3_DIR"
echo "CLAP -> $CLAP_DIR"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
LINE=$(awk '/^__ARCHIVE_BELOW__$/{print NR + 1; exit 0;}' "$0")
tail -n +"$LINE" "$0" | tar xz -C "$TMP"
mkdir -p "$VST3_DIR" "$CLAP_DIR"
rm -rf "$VST3_DIR/EmberEQ.vst3"
cp -r "$TMP/EmberEQ.vst3" "$VST3_DIR/"
cp "$TMP/EmberEQ.clap" "$CLAP_DIR/"
echo "Fertig! Starte deine DAW (z.B. Reaper, Bitwig, Ardour) und suche nach neuen Plugins."
echo "Deinstallieren: sh $0 --uninstall"
exit 0
__ARCHIVE_BELOW__
