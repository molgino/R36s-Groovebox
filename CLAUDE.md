# R36S Groovebox – Projektkontext

Elektron-artige Groovebox (C++/SDL2, ALSA) für den Handheld **R36S** (RK3326, ArkOS 2.0, Kernel 4.4.189, glibc 2.30).
Sprache mit dem Nutzer: **Deutsch**. Anleitung für Nutzer: `README.md`.

## Aufbau
- `src/engine.*` Sequencer + 13 Engines (KICK SNARE HAT CLAP SYNTH FM SMPL TOM RIM SHKR WTBL CHOP GRAN – Enum-Werte nur hinten anfügen, Anzeige-Reihenfolge `MACHINE_ORDER`), Wavetables `buildWavetables`, Insert-FX, Delay/Reverb, Kompressor mit Sidechain; Mikrotiming `P_MTIM` (`pend`/`preDone`, negative Werte werden einen Step früher geplant)
- `src/params.h` alle Parameter. Speicherseiten je Spur (TRG SRC FLT AMP LFO1 INS SRC2 LFO2 LFO3 SEQ FLT2, je 8 = 88 Parameter; `LFO_BASE`, `LF_*`) und UI-Seiten `PG_*` (TRG SEQ SRC SR2 FLT FL2 AMP LF1 LF2 LF3 INS | MIX FX CMP | PRJ MST SET); SET (=PG_SYS) ist eine Listen-Seite (`SetItem`, `drawSettings`), Mapping `PG_STORE`; FM = 4 OP/8 Algorithmen (`FM_MODS/FM_CARRIERS/FM_LAYOUT`); globale Skala `mst[MS_SCALE]`
- `src/main.cpp` UI (640x480): Kacheln (`tile`, Wert-Symbole `paramSymbol` nach Parametername), Engine-Symbole (`drawIcon`), Engine-Wähler, Grafiken je Engine (`graphKind`), Bedienung, Einstellungen, PC-Modus-Steuerung
- `src/web.cpp` + `src/webpage.cpp` Mini-Webserver (Port 80, sonst 8080): Upload-Seite für Samples/Projekte, Umwandlung im Browser; Neuladen über `samplesPrepare/samplesSwap` (zwei Bänke) + `reloadSamples` (Nummern per Name nachführen)
- `src/midi.cpp` ALSA-Seq + rawmidi + USB-Seriell + RTP-MIDI (Port 5004/5005); `src/dhcpd.cpp` Mini-DHCP (`groovebox --dhcpd usb0`)
- `arkos/` Ports-Skripte, `arkos/groovebox/pc-modus.sh` (USB-Gadget: RNDIS + DHCP + SSH), `dtb-otg/` gepatchte DTBs
- Projektformat v8 (`Project` in engine.h: 88 Parameter, 128 Steps, `speed` pro Spur; Index 5 = MTIM, bis v6 RFAD; Seite 10 FLT2 = Base-Width-Filter `P_BASE/P_WDTH` + `P_KEYT/P_VELF`) mit Migration v1–v7 in `Engine::load` (`remapLegacy`, v5-Zweig) – bei Strukturänderungen Version erhöhen + migrieren
- Sequencer: 12 Ticks pro 16tel (`doTick`/`stepTrack`), Tempo-Faktor `SPEED_TPS`, Trig-Bedingungen `P_COND` (`condOk`, Durchläufe `loopCount` über `stepCnt`), FILL `eng.fill`; SEQ-Seite: Richtung `P_DIR`, Euklid `P_PULS/P_EROT`, Turing-Generator `P_DENS/P_CHNG/P_RANGE` (`tmReg`), LEN/SPD zeigen auf `Pattern.len/speed`
- Projekte `projects/songNN.gbx` (32 Plätze, aktiver Platz in settings.cfg `song`), Presets `presets/<ENGINE>/NN.gbp`

## Groovebox-OS (`os/`)
- Eigenes Image statt ArkOS: Linux `LINUX_VERSION` (gregkh/linux-Tag) + ROCKNIX-Patches/DTS (gepinnt `ROCKNIX_COMMIT`, `os/versions.env`), Panel aus ArkOS-DTB als Overlay `os/dts/panel-arkos.dtso` (generic-dsi), eigener Tree `rk3326-groovebox-r36s.dts` (OTG-Port an, usb-role-switch)
- `os/kernel/build.sh` (arm64-Runner nativ), `os/uboot/build.sh` (x86: rkbin-Tools), `os/rootfs/build.sh` (mmdebstrap Debian trixie, Overlay in `os/rootfs/overlay`), `os/image/build.sh` (MBR: U-Boot @64, GBBOOT FAT, GBROOT ext4; GROOVEBOX FAT wird beim 1. Start angelegt)
- Kein Initramfs: Root per `root=PARTUUID=47425653-02` (feste MBR-ID in image/build.sh), `LABEL=` geht nur im Userspace; ROCKNIX-Config hat `PANIC_TIMEOUT=1` (Panic = sofortiger Neustart) → Testphase mit `panic=60 console=tty0`
- Bild auf dem OS: `src/drmout.cpp` (make DRM=1, `GB_DISPLAY=drm`): SDL-Software-Renderer auf Surface → zwei DRM-Dumb-Buffer + Page-Flip, SDL läuft mit Videotreiber offscreen; GPU-Weg (SDL kmsdrm/Panfrost) blieb schwarz. Umschalten per `/data/config/groovebox.env`; `present()` statt `SDL_RenderPresent`
- App-Update ohne Image: `tools/os-update.sh` → `dist/groovebox-os-update.tgz` (groovebox DRM=1, pc-modus.sh, sbin/, VERSION; Build-Job `os-app` in build.yml, Asset im Release nightly); Wege: PUT `/api/update` (web.cpp, nur wenn /etc/groovebox-os), `/data/update/*.tgz`, deploy.ps1/.sh (erkennt `"os":true` in /api/list); `groovebox-run` installiert vor dem Start, Rollback nach 3 Fehlstarts <15 s
- Workflow `.github/workflows/os.yml` → Release **os-nightly**; Kernel/fremder Code nicht im Container bauen, nur in CI
- Gerät: `/opt/groovebox` (Symlinks nach `/data` = Partition GROOVEBOX), Dienst `groovebox.service`, Logs `/data/logs/{boot.txt,groovebox.log}`, `pc-modus.sh` (aus|an|an audio|laufwerk|laufwerk-aus, configfs-Gadget RNDIS+MIDI+UAC2 bzw. Mass-Storage), SET „USB-Laufwerk“ (`startDrive/stopDrive`, nur wenn `/etc/groovebox-os` existiert)

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
- Song-Modus, Sampling vom USB-Eingang, Arpeggiator, Pattern-Mutation
