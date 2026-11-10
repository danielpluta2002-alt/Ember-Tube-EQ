#!/bin/sh
# Builds a macOS .pkg installer from bin/EmberEQ.vst3 and bin/EmberEQ.clap
set -e
VERSION=1.0.0
OUT=dist
mkdir -p "$OUT" pkgroot/vst3 pkgroot/clap
rm -rf pkgroot/vst3/* pkgroot/clap/*
cp -R bin/EmberEQ.vst3 pkgroot/vst3/
cp -R bin/EmberEQ.clap pkgroot/clap/
# ad-hoc sign so Apple Silicon will load the binaries
codesign --force --deep -s - pkgroot/vst3/EmberEQ.vst3
codesign --force --deep -s - pkgroot/clap/EmberEQ.clap

pkgbuild --root pkgroot/vst3 --identifier audio.ember.tubeeq.vst3 --version $VERSION \
         --install-location /Library/Audio/Plug-Ins/VST3 vst3.pkg
pkgbuild --root pkgroot/clap --identifier audio.ember.tubeeq.clap --version $VERSION \
         --install-location /Library/Audio/Plug-Ins/CLAP clap.pkg

cat > distribution.xml <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Ember Tube EQ $VERSION</title>
  <options customize="allow" require-scripts="false" hostArchitectures="x86_64,arm64"/>
  <choices-outline>
    <line choice="vst3"/>
    <line choice="clap"/>
  </choices-outline>
  <choice id="vst3" title="VST3 Plugin"><pkg-ref id="audio.ember.tubeeq.vst3"/></choice>
  <choice id="clap" title="CLAP Plugin"><pkg-ref id="audio.ember.tubeeq.clap"/></choice>
  <pkg-ref id="audio.ember.tubeeq.vst3">vst3.pkg</pkg-ref>
  <pkg-ref id="audio.ember.tubeeq.clap">clap.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution distribution.xml --package-path . "$OUT/EmberTubeEQ-$VERSION-macOS-Setup.pkg"
rm -f vst3.pkg clap.pkg distribution.xml
rm -rf pkgroot
echo "Built $OUT/EmberTubeEQ-$VERSION-macOS-Setup.pkg"
