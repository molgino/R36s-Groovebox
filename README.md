# R36S Groovebox

Elektron-artige Groovebox für den R36S unter ArkOS 2.0 (läuft auch auf dem Linux-Desktop).
8 Spuren, 16 Patterns à 16 Steps, Parameter-Locks, Ping-Pong-Delay, Reverb und ein DJ-Filter.
Sie startet als App aus dem Ports-Menü oder auf Wunsch direkt beim Booten statt EmulationStation.

## Klangerzeugung

| Maschine | Regler auf der SRC-Seite |
|---|---|
| KICK  | TUNE, DCAY, SWEP (Pitch-Sweep), S.TIM, CLCK, BODY (Sättigung) |
| SNARE | TUNE, DCAY, SNAP (Rauschanteil), N.DEC, TONE |
| HAT   | TUNE, DCAY, TONE (Hochpass), METL (Metall/Rauschen) |
| CLAP  | TUNE, DCAY, SPRD (Abstand), RPTS (Anzahl Schläge) |
| SYNTH | WAV1, WAV2 (SAW/SQR/TRI/SIN), DETN, MIX, SUB, NOIS, PW, GLID |
| FM    | RATI, INDX, FBK, M.DEC, FINE, NOIS |

Jede Spur ist monophon (wie bei Digitakt und Co.) und hat diese Seiten:

* **TRIG**: NOTE, VEL, LEN, PROB (Wahrscheinlichkeit), RTRG (Retrigs pro Step), RFAD
* **SRC**: Regler der Maschine (siehe Tabelle)
* **FILT**: Multimode-Filter LP/BP/HP mit eigener ADSR-Hüllkurve
* **AMP**: ADSR, Lautstärke, Panorama, Delay- und Reverb-Send
* **LFO**: Tempo, Tiefe, Ziel (Pitch/Freq/Vol/Pan/Reso/Crush), Wellenform, Retrigger, Bitcrusher, Samplerate-Reduktion, Drive

Dazu kommt die globale Seite **MST** mit BPM, Swing, Delay (Zeit/Feedback/Ton), Reverb (Größe/Dämpfung) und der Gesamtlautstärke.

## Bedienung

| Taste | Funktion |
|---|---|
| Steuerkreuz ←/→ | Step-Cursor |
| Steuerkreuz ↑/↓ | Spur wählen |
| A | Trig setzen oder löschen |
| **A halten** + X/Y oder linker Stick | **Parameter-Lock** auf diesen Step (gelb) |
| A halten + B | alle Locks des Steps löschen |
| B | Spur vorhören |
| X / Y | Wert + / − (halten = Wiederholung, nach 1 s schneller) |
| Linker Stick ↑/↓ | Wert stufenlos ändern |
| Rechter Stick ↑/↓ | DJ-Filter: oben Hochpass, unten Tiefpass |
| L1 / R1 | Regler wählen |
| L2 / R2 | Seite wählen (TRIG SRC FILT AMP LFO MST) |
| START | Play / Stop |
| **SELECT halten** | Hilfe-Einblendung |
| SELECT + ←/→ | Pattern wechseln (beim Abspielen am Pattern-Ende) |
| SELECT + ↑/↓ | Tempo |
| SELECT + A | Spur stummschalten |
| SELECT + B | Spur löschen |
| SELECT + X/Y | Maschine der Spur wechseln |
| SELECT + L1/R1 | Pattern kopieren / einfügen |
| SELECT + L2/R2 | Spurlänge (Polymetrie, 1 bis 16) |
| SELECT + START | Speichern (2 s halten: Speichern und Beenden) |

Das Projekt liegt in `projects/project1.gbx`. Gespeichert wird automatisch beim Beenden und alle 2 Minuten, wenn sich etwas geändert hat.

Am Desktop gilt diese Tastaturbelegung: Pfeiltasten, Z=A, X=B, S=X, A=Y, Q/W=L1/R1, 1/2=L2/R2, Enter=START, Shift/Backspace=SELECT, I/K=linker Stick, O/L=rechter Stick.

## Installation auf dem R36S (ArkOS 2.0)

1. Die Datei `dist/r36s-groovebox-arkos.zip` entpacken. Ihren Inhalt in den Ordner `ports` der SD-Karte kopieren. Das ist die Partition `EASYROMS`, auf dem Gerät `/roms/ports/`. Danach sieht es so aus:
   ```
   ports/Groovebox.sh
   ports/Groovebox - Tasten lernen.sh
   ports/Groovebox - Autostart AN.sh
   ports/Groovebox - Autostart AUS.sh
   ports/groovebox/groovebox            (Programm, aarch64)
   ports/groovebox/groovebox.service
   ```
2. Am R36S unter **Ports** den Eintrag **Groovebox** starten. Falls er nicht auftaucht, EmulationStation neu starten (Start → Quit → Restart EmulationStation).
3. Beim ersten Start fragt die App alle Tasten einmal ab, weil die R36S-Klone unterschiedlich belegt sind. Das Ergebnis landet in `groovebox/input.cfg`. Später kannst du die Tasten über **Groovebox - Tasten lernen** neu belegen.

### Als Startoberfläche (OS-Ebene)

**Groovebox - Autostart AN** richtet einen systemd-Dienst ein, deaktiviert EmulationStation und startet das Gerät neu.
Ab dann bootet der R36S direkt in die Groovebox.

* Wenn du die Groovebox beendest (SELECT+START 2 s halten) oder sie abstürzt, startet automatisch EmulationStation.
* Dort stellst du mit **Groovebox - Autostart AUS** den normalen Start wieder her.
* Beim Ausschalten wird gespeichert, denn das SIGTERM von systemd wird abgefangen.

### Fehlersuche

* Die Log-Datei liegt in `ports/groovebox/groovebox.log`. Dort stehen Audio-Samplerate, Puffergröße und das erkannte Gamepad.
* Bei Knacksern im Ton in `Groovebox.sh` `--buffer 256` durch `--buffer 512` ersetzen.
* Der Header zeigt die CPU-Last der Audio-Engine an. Mit dem Demo-Beat ist sie sehr gering. Gemessen habe ich nur auf dem PC (unter 1 %). Auf dem R36S ist sie höher, sollte aber reichlich Luft lassen.

## Selbst bauen

```bash
# Desktop (zum Testen)
sudo apt install libsdl2-dev && make && ./groovebox --windowed

# Cross-Build für den R36S (auf x86-Linux)
sudo apt install g++-aarch64-linux-gnu curl && ./tools/cross-build.sh
```

Der Cross-Build lädt einen kleinen Ubuntu-18.04-arm64-Sysroot (glibc 2.27, SDL 2.0.8 Header) nach `.sysroot`.
Dadurch läuft das Programm auf ArkOS und nutzt dort dessen eigene SDL2 mit KMS/DRM.
Der Code benutzt bewusst keine libstdc++ und hängt nur von libSDL2, libm und libc ab.

Weitere Kommandozeilen-Optionen:

* `--render out.wav 8`: Das aktuelle Projekt offline als WAV rendern.
* `--shot bild.bmp [seite]`: einen Screenshot erzeugen.

## Aufbau

* `src/engine.*`: Sequencer (sampelgenau in 16-Sample-Blöcken, Swing, Retrigs, Polymetrie), Stimmen, Delay, Freeverb-Reverb, DJ-Filter, Limiter
* `src/params.h`: alle Parameterdefinitionen und Wertanzeigen
* `src/input.*`: Gamepad mit lernbarer Belegung, SDL-GameController und Tastatur
* `src/main.cpp`: Oberfläche (640×480) und Bedienlogik
* `src/font.h`: Bitmap-Schrift, erzeugt mit `tools/genfont.py` aus DejaVu Sans Mono
