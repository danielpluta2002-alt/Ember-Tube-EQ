# Ember Tube EQ

Passiver Programm-Equalizer im Stil der klassischen Röhren-EQs der 1950er (Boost/Atten-Trick im Bass,
Bell-Boost mit Bandbreite in den Höhen, Höhen-Shelf, Röhren-Sättigung, VU-Meter).
Formate: **VST3** und **CLAP** für Windows, macOS (Intel + Apple Silicon) und Linux.

## Download
Fertige Installer gibt es unter **Releases** (rechts auf der Repository-Seite):
- Windows: `EmberTubeEQ-1.0.0-Windows-Setup.exe`
- macOS: `EmberTubeEQ-1.0.0-macOS-Setup.pkg` (beim ersten Öffnen: Rechtsklick → Öffnen)
- Linux: `EmberTubeEQ-1.0.0-Linux-Setup.run` (`sh EmberTubeEQ-1.0.0-Linux-Setup.run`)

## Bedienung
- Knöpfe: ziehen (hoch/runter), **Shift** = fein, **Doppelklick** = Standardwert, Mausrad.
- POWER-Schalter = Bypass.
- Pultec-Trick: LOW FREQUENCY 60, BOOST und ATTEN beide hoch → fetter Bass mit Senke darüber.

## Bauen
- Linux: `make` (benötigt X11/OpenGL-Dev-Pakete)
- macOS: `make macos-universal-10.15` und dann `sh installer/macos/build-pkg.sh`
- Automatisch für alle Systeme: Ordner in ein GitHub-Repository hochladen → Reiter "Actions" →
  Build läuft von selbst, Installer liegen danach unter "Artifacts".

Basierend auf DPF (DISTRHO Plugin Framework, ISC-Lizenz).
