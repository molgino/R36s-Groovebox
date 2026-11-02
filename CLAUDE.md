# R36S Groovebox – Projektkontext

Elektron-artige Groovebox (C++/SDL2, ALSA) für den Handheld **R36S** (RK3326, ArkOS 2.0, Kernel 4.4.189, glibc 2.30).
Sprache mit dem Nutzer: **Deutsch**. Anleitung für Nutzer: `README.md`.

## Aufbau
- `src/engine.*` Sequencer + 7 Engines (KICK SNARE HAT CLAP SYNTH FM SMPL), Insert-FX, Delay/Reverb, Kompressor mit Sidechain
- `src/params.h` alle Parameter. Speicherseiten je Spur (TRG SRC FLT AMP LFO INS SRC2, je 8) und UI-Seiten `PG_*` (TRG SRC SRC2 FLT AMP LFO INS | MIX FX CMP | PRJ MST SYS), Mapping `PG_STORE`; FM = 4 OP/8 Algorithmen (`FM_MODS/FM_CARRIERS/FM_LAYOUT`); globale Skala `mst[MS_SCALE]`
- `src/main.cpp` UI (640x480, Grafiken/Animationen), Bedienung, Einstellungen, PC-Modus-Steuerung
- `src/midi.cpp` ALSA-Seq + rawmidi + USB-Seriell + RTP-MIDI (Port 5004/5005); `src/dhcpd.cpp` Mini-DHCP (`groovebox --dhcpd usb0`)
- `arkos/` Ports-Skripte, `arkos/groovebox/pc-modus.sh` (USB-Gadget: RNDIS + DHCP + SSH), `dtb-otg/` gepatchte DTBs
- Projektformat v4 (`Project` in engine.h, 56 Parameter/Spur) mit Migration v1–v3 in `Engine::load` – bei Strukturänderungen Version erhöhen + migrieren
- Projekte `projects/songNN.gbx` (32 Plätze, aktiver Platz in settings.cfg `song`), Presets `presets/<ENGINE>/NN.gbp`

## Bauen
- Desktop: `make` (libsdl2-dev, libasound2-dev). Tests ohne Display: `./groovebox --render out.wav 4`, `./groovebox --shot bild.bmp <PG-Nummer | 20 Hilfe, 21 Lock, 22 Overlay, 23 Engine, 24 Skala, 25 Menü, 26 Abfrage>`
- R36S: `./tools/cross-build.sh && ./tools/package.sh` (Ubuntu-18.04-arm64-Sysroot, kein libstdc++ – nur C-Header, keine Exceptions/RTTI)
- GitHub Actions baut bei jedem Push auf `main` das Release **nightly**

## Gerät (R36S) – per USB-Kabel
- Auf dem R36S Groovebox → Seite SYS → **USB = PC** (OTG-Port freigeschaltet via „Groovebox - OTG-Port freischalten“, geladene DTB: `rk3326-r35s-linux.dtb`)
- R36S: `192.168.77.1`, PC bekommt `192.168.77.10`. SSH: `ark` / `ark`
- Aufspielen: `tools/deploy.ps1` (Windows) bzw. `tools/deploy.sh`; Log: `deploy.ps1 -Log`; Pfad auf dem Gerät `/roms/ports/groovebox/`
- Kernel hat nur Netzwerk-Gadgets (ecm/rndis/ncm) – MIDI zum PC über RTP-MIDI (rtpMIDI unter Windows)
- Bekannt: dwc2 hängt beim Rollenwechsel ohne Neuladen des Moduls (pc-modus.sh lädt dwc2 neu)

## Offene Ideen
- Kernel-Module `usb_f_midi`/`usb_f_uac2` für ArkOS 4.4.189 bauen (echtes USB-MIDI/Audio)
- Song-Modus, Sampling vom USB-Eingang, Trig-Bedingungen
