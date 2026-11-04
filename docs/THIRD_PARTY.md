# Drittanbieter-Komponenten und Lizenzen

| Komponente | Verwendung | Lizenz |
|---|---|---|
| SDL2 | Grafik-/Eingabe-Bibliothek der App | zlib |
| ALSA-Bibliothek (libasound) | Audio/MIDI | LGPL-2.1 |
| libdrm | Display-Ausgabe (SYNTH CITY OS) | MIT |
| DejaVu Sans Mono Bold | Bildschirmschrift (`src/font.h`, erzeugt mit `tools/genfont.py`) | Bitstream-Vera-Lizenz / gemeinfrei |
| Linux-Kernel + ROCKNIX-Patches/Gerätebäume | SYNTH CITY OS | GPL-2.0 |
| U-Boot, Rockchip rkbin | Bootloader (SYNTH CITY OS) | GPL-2.0+ bzw. Rockchip-Binärlizenz |
| Debian-Pakete (systemd, dropbear, Mesa …) | Basissystem (SYNTH CITY OS) | jeweilige Paketlizenzen (`/usr/share/doc` ist im Image entfernt, siehe Debian) |

## Pflichten beim Weitergeben des OS-Images
Wer das Image weitergibt oder verkauft, muss für die GPL-Komponenten (Kernel, U-Boot, GPL-Pakete) den **vollständigen Quellcode** anbieten. Die Bauanleitungen liegen in `os/` (Kernel-Version und ROCKNIX-Stand in `os/versions.env`); am einfachsten wird zu jedem Release ein Quellcode-Archiv (Kernel-Quellen + Patches, U-Boot-Quellen) mitveröffentlicht oder ein schriftliches Angebot für 3 Jahre beigelegt. Die Debian-Quellen stehen unter snapshot.debian.org zum Datum des Builds bereit.

## Hinweis zu Marken
SYNTH CITY ist ein eigenständiges Projekt. Namen anderer Hersteller und Geräte werden in Anleitung und Werbung nicht verwendet; Klänge sind eigene Nachbildungen klassischer Klangfarben, keine Samples fremder Geräte. Die Lizenz des SYNTH-CITY-Quellcodes selbst legt der Rechteinhaber fest (noch keine LICENSE-Datei im Repository).
