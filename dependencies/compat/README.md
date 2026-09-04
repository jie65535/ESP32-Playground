# Tracked compatibility components

This directory contains the small project-specific files that cannot be
restored as an unmodified upstream component.

- `arduino/idf_component.yml` replaces Arduino-ESP32's broad registry
  manifest with the minimal dependency declaration used by PGOS.
- `bluepad32_arduino/` is based on the `bluepad32_arduino` component in
  `ricardoquesada/esp-idf-arduino-bluepad32-template` commit
  `d07a9385f46f7215f51fc3eb5e40c5a484cfe102`. PGOS additionally makes its
  console integration conditional so the system USB console can own CDC.

The bootstrap script copies these files into the ignored `components/` build
tree after restoring the large upstream sources.
