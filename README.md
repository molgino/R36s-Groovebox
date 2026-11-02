# R36S Groovebox

Elektron-artige Groovebox für den R36S unter ArkOS 2.0 (läuft auch auf dem Linux-Desktop).
8 Spuren, 16 Patterns à 16 Steps, Parameter-Locks, Sample-Spuren, USB-Audio, USB-MIDI, Ping-Pong-Delay, Reverb und ein DJ-Filter.
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
| SMPL  | SMPL (Sample-Auswahl), TUNE (±24 Halbtöne), FINE, STRT, END (END < STRT = rückwärts), LOOP, GAIN |

Jede Spur ist monophon (wie bei Digitakt und Co.). Ihre **Engine** wählst du auf der TRG-Seite mit dem Regler **ENGINE**: KICK, SNARE, HAT, CLAP, SYNTH, FM oder SMPL. Beim Drehen erscheint eine Auswahlleiste.
Die Engine gilt pro Spur und Pattern. Wechselst du sie, werden nur die SRC-Regler auf die Grundwerte der neuen Engine gesetzt.

Jede Spur hat diese Seiten:

* **TRG**: NOTE, VEL, LEN, PROB (Wahrscheinlichkeit), RTRG (Retrigs pro Step), RFAD, ENGINE
* **SRC**: Regler der Maschine (siehe Tabelle)
* **FLT**: Multimode-Filter LP/BP/HP mit eigener ADSR-Hüllkurve
* **AMP**: ADSR, Lautstärke, Panorama, **Delay- und Reverb-Send** (DLY, REV)
* **LFO**: Tempo, Tiefe, Ziel (Pitch/Freq/Vol/Pan/Reso/Crush), Wellenform, Retrigger, Bitcrusher, Samplerate-Reduktion, Drive
* **INS**: Insert-Effekt der Spur. Zuerst unter TYPE den Effekt wählen, dann MIX aufdrehen:

  | TYPE | Regler |
  |---|---|
  | CHORS (Chorus, stereo) | RATE, DEPTH, WIDTH |
  | PHASE (6-stufiger Phaser, stereo) | RATE, DEPTH, FBK |
  | FLNGR (Flanger) | RATE, DEPTH, FBK (Mitte = 0, links negativ, rechts positiv) |
  | DRIVE (Overdrive) | GAIN, TONE, LEVEL |
  | RING (Ringmodulator) | FREQ, FINE |
  | APAN (Autopan) | RATE, DEPTH |

  Alle Regler, auch TYPE, lassen sich pro Step locken. Die Insert-Effekte sitzen hinter Filter und Hüllkurve, also vor Panorama und den Sends.

Dazu kommen die globalen Seiten (in der Leiste oben grauer dargestellt):

* **FX**: die Send-Effekte
  * Delay: D.TIME (1/32 bis 1/2, im Songtempo), D.FBK, D.TONE, D.PING (0 = mono, 127 = volles Ping-Pong)
  * Reverb: R.SIZE, R.DAMP, R.PRE (Predelay bis 250 ms), R.WIDE (Stereobreite)
* **CMP**: Master-Kompressor mit **Sidechain** (siehe unten)
* **MST**: BPM, Swing, Gesamtlautstärke und DRIVE (Sättigung der Summe)
* **SYS**: Audio-Ausgang, Puffergröße, MIDI und USB-Modus (wird in `settings.cfg` gespeichert)

## Kompressor und Sidechain (CMP-Seite)

| Regler | Funktion |
|---|---|
| THR | Schwelle −48 bis 0 dB (0 dB und GAIN 0 = aus) |
| RATIO | 1.5:1 bis 20:1 und INF (Limiter), mit weichem Knie |
| ATK / REL | Attack 0,1–100 ms, Release 10 ms–2 s |
| GAIN | Make-up-Gain bis +24 dB |
| SC | Sidechain-Quelle: INT = die Summe selbst, T1–T8 = diese Spur steuert den Kompressor |
| MIX | Parallel-Kompression (127 = voll komprimiert) |
| GR | Anzeige der aktuellen Gain-Reduktion. Zusätzlich zeigt der rote Balken im Header die Gain-Reduktion |

Bei SC = T1 bis T8 läuft die Quellspur **am Kompressor vorbei**, und alles andere wird von ihr „weggedrückt“. Die Effekt-Returns zählen dazu.

Klassisches Pumpen: SC = T1 (Kick), THR um −30 dB, RATIO 8:1 oder mehr, ATK kurz, REL 100–200 ms.

## Samples

WAV-Dateien kommen in den Ordner `groovebox/samples/`, gern mit einer Ebene Unterordnern, z. B. `samples/drums/kick.wav`.
Erlaubt sind 8/16/24/32-Bit-PCM und 32-Bit-Float in jeder Samplerate. Stereo wird zu Mono gemischt.
Gelesen wird beim Start, alphabetisch sortiert, höchstens 128 Dateien zu je höchstens 60 s.

Ein Start-Paket mit 13 selbst erzeugten Samples liegt bei: Drums, Keys, eine Vokal-Fläche und FX.
Es wird mit `tools/gensamples.py` erzeugt.

* Mit SELECT + X/Y stellst du eine Spur auf **SMPL** um.
* Den Sample-Regler auf der SRC-Seite kannst du pro Step locken. So spielt eine einzige Spur verschiedene Samples, wie beim Digitakt.
* NOTE C-4 ist die Originaltonhöhe.
* Ohne LOOP läuft ein Sample als One-Shot bis zum Ende. Mit LOOP gilt die Trig-Länge (LEN) bzw. die Haltedauer der MIDI-Taste.
* Das Projekt merkt sich die Sample-**Nummer**. Wenn du später Dateien hinzufügst, benenne sie so, dass sie hinten einsortiert werden. Sonst verschieben sich die Nummern.

## USB-Audio

Auf der SYS-Seite wählst du unter **OUT** den Ausgang. STD ist der eingebaute Lautsprecher bzw. die Kopfhörerbuchse, darüber hinaus wird jedes erkannte Gerät angeboten, zum Beispiel ein USB-DAC oder ein Audio-Interface.
Die Änderung greift nach knapp einer Sekunde. Wechselst du auf die SYS-Seite, wird die Geräteliste neu eingelesen.
**BUF** stellt die Puffergröße ein (128 bis 1024). Kleiner heißt weniger Latenz, größer heißt weniger Knackser.
Wenn ein gespeichertes Gerät beim Start fehlt, nimmt die Groovebox automatisch den Standardausgang.

## MIDI

Alle angeschlossenen MIDI-Geräte werden automatisch verbunden, auch wenn du sie erst während des Betriebs einsteckst. Das gilt für USB-Keyboards, Controller und Interfaces.
Unter **MIDI** auf der SYS-Seite steht, welche Geräte verbunden sind, und im Header blinkt „MIDI“ bei eingehenden Daten.

| SYS-Regler | Funktion |
|---|---|
| M.IN | MIDI-Eingang an/aus |
| N.OUT | Sequencer-Noten ausgeben: Spur 1–8 auf Kanal 1–8, mit Länge und Velocity, Retrigs inklusive |
| CLK | OFF, SEND (Clock + Start/Stop an andere Geräte) oder RECV (Tempo, Start und Stop von außen) |
| M.CH | AUTO: jeder Kanal spielt die gerade gewählte Spur. 1-8: Kanal 1–8 spielt Spur 1–8 |
| USB | HOST: Geräte am R36S (Keyboard, S-1, DAC). PC: der R36S ist ein MIDI-Gerät am PC. PC+A: zusätzlich ein Audio-Interface (siehe unten) |

* **Live spielen:** Die Noten spielen die gewählte Spur mit Anschlagstärke. Synth-, FM- und Loop-Spuren klingen, solange die Taste gehalten wird.
* **Live-Aufnahme** (REC an, Sequencer läuft): Gespielte Noten werden auf den nächsten Step quantisiert und mit Note und Velocity als Locks eingetragen.
* **Step-Aufnahme** (REC an, Sequencer gestoppt): Jede Note landet auf dem Cursor-Step, danach springt der Cursor einen Step weiter. So tippst du Melodien bequem ein.
* **CC 70–77** steuern die 8 Regler der aktuellen Seite (TRIG bis LFO). Bei laufender Aufnahme werden sie als Parameter-Locks auf Trigs gespeichert.
* **CC 1** (Modulationsrad) steuert den DJ-Filter.

Aufnahme an/aus: SELECT + ↑.

Technisch läuft MIDI über den ALSA-Sequencer. Fehlt der im Kernel, greift die Groovebox direkt auf die USB-MIDI-Geräte zu (rawmidi).


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
| L2 / R2 | Seite wählen (TRG SRC FLT AMP LFO INS, global: FX CMP MST SYS) |
| START | Play / Stop |
| **SELECT halten** | Hilfe-Einblendung |
| SELECT + ←/→ | Pattern wechseln (beim Abspielen am Pattern-Ende) |
| SELECT + ↑ | Aufnahme an/aus (für MIDI-Eingabe) |
| SELECT + ↓ | Tap-Tempo (mehrmals im Takt tippen) |
| SELECT + A | Spur stummschalten |
| SELECT + B | Spur löschen |
| SELECT + X/Y | Engine der Spur wechseln (alternativ: Regler ENGINE auf der TRG-Seite) |
| SELECT + L1/R1 | Pattern kopieren / einfügen |
| SELECT + L2/R2 | Spurlänge (Polymetrie, 1 bis 16) |
| SELECT + START | Speichern (2 s halten: Speichern und Beenden) |

Das Projekt liegt in `projects/project1.gbx`. Gespeichert wird automatisch beim Beenden und alle 2 Minuten, wenn sich etwas geändert hat.

Am Desktop gilt diese Tastaturbelegung: Pfeiltasten, Z=A, X=B, S=X, A=Y, Q/W=L1/R1, 1/2=L2/R2, Enter=START, Shift/Backspace=SELECT, I/K=linker Stick, O/L=rechter Stick.

## Am PC: Ableton, MIDI-OX & Co. (PC-Modus)

Normalerweise ist der OTG-Port ein **Host**: Geräte werden *an* den R36S gesteckt. Ein PC ist ebenfalls ein Host, deshalb sieht er den R36S zunächst nicht.
Im **PC-Modus** schaltet der R36S den Port in den Geräte-Modus (USB-Gadget) und meldet sich am PC als **„R36S Groovebox“** an:

* **PC**: ein USB-MIDI-Gerät mit 1 Ein- und 1 Ausgang, ohne Treiber, unter Windows, macOS und Linux.
* **PC+A**: zusätzlich ein USB-Audio-Interface mit 2 Kanälen bei 48 kHz. Auf der SYS-Seite unter OUT dann das Gadget-Gerät wählen (z. B. „UAC2Gadget“), damit der Ton der Groovebox in Ableton als Eingang ankommt.

Einschalten:

* in der App: SYS-Seite, Regler **USB** auf PC oder PC+A, oder
* im Ports-Menü: **Groovebox - PC-Modus MIDI**, **Groovebox - PC-Modus MIDI+Audio** bzw. **Groovebox - PC-Modus AUS**.

Danach den R36S mit dem PC verbinden. Am zuverlässigsten ist ein **USB-A-auf-USB-C-Kabel**. Ein C-auf-C-Kabel schaltet die Stromversorgung manchmal nicht frei.
Während der PC-Modus aktiv ist, funktionieren Geräte am R36S (S-1, Keyboard, DAC) nicht. Nach einem Neustart ist der R36S wieder Host.

**In Ableton:** *Einstellungen → Link, Tempo & MIDI*. Beim Eingang „R36S Groovebox“ **Spur** aktivieren, damit die Noten der Groovebox in Ableton ankommen, z. B. mit N.OUT an.
Zum Synchronisieren gibt es zwei Wege:

* Groovebox führt: CLK auf SEND, in Ableton beim Eingang **Sync** an und oben links EXT aktivieren.
* Ableton führt: beim Ausgang **Sync** an, CLK auf RECV.

### Kernel ohne USB-MIDI-Gadget: MIDI über USB-Seriell

Manche ArkOS-Kernel haben zwar den USB-Geräte-Modus, aber keinen MIDI-Baustein (`usb_f_midi`).
Dann meldet sich der R36S automatisch als **serielle Schnittstelle** am PC an (unter Windows als „USB Serial Device (COMx)“), und die Groovebox schickt und empfängt MIDI darüber.
Am PC übersetzt ein kostenloses Brückenprogramm das in einen normalen MIDI-Port:

**Windows**

1. Lade **loopMIDI** (Tobias Erichsen) herunter und lege zwei Ports an, z. B. „R36S in“ und „R36S out“.
2. Lade **Hairless MIDI<->Serial Bridge** herunter. Als Serial Port den COM-Port des R36S wählen, MIDI Out = „R36S in“, MIDI In = „R36S out“. Die Baudrate ist egal.
3. In Ableton „R36S in“ als Eingang (Spur/Sync) und „R36S out“ als Ausgang aktivieren.

**macOS:** Hairless MIDI<->Serial Bridge mit dem IAC-Treiber. **Linux:** `ttymidi`.

Welche Bausteine dein Kernel hat, steht nach dem ersten Versuch in `groovebox/pc-modus.log` in der Zeile „Verfuegbare Gadget-Funktionen“.
Ist dort `uac2` oder `uac1` dabei, funktioniert PC+A als Audio-Interface auch ohne den MIDI-Baustein.

Wenn es nicht klappt, steht der Grund in `groovebox/pc-modus.log`. Die App zeigt ihn zusätzlich direkt an.

## Installation auf dem R36S (ArkOS 2.0)

1. Die Datei `dist/r36s-groovebox-arkos.zip` entpacken. Ihren Inhalt in den Ordner `ports` der SD-Karte kopieren. Das ist die Partition `EASYROMS`, auf dem Gerät `/roms/ports/`. Danach sieht es so aus:
   ```
   ports/Groovebox.sh
   ports/Groovebox - Tasten lernen.sh
   ports/Groovebox - Autostart AN.sh
   ports/Groovebox - Autostart AUS.sh
   ports/groovebox/groovebox            (Programm, aarch64)
   ports/groovebox/groovebox.service
   ports/groovebox/pc-modus.sh          (PC-Modus, auch aus der App aufrufbar)
   ports/groovebox/samples/...          (Start-Sample-Paket)
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

* Die Log-Datei liegt in `ports/groovebox/groovebox.log`. Dort stehen die Audio-Ausgänge, Samplerate und Puffer, die geladenen Samples, die MIDI-Geräte und das erkannte Gamepad.
* Bei Knacksern im Ton auf der SYS-Seite BUF auf 512 stellen.
* **USB-Geräte am R36S:** Du brauchst ein USB-C-OTG-Adapterkabel am OTG-Port, nicht an der Ladebuchse. Geräte mit hohem Strombedarf brauchen einen aktiven Hub.
* **Wird ein USB-Gerät nicht erkannt?** Per SSH prüfen:
  * `aplay -l` zeigt die Audio-Ausgänge.
  * `aconnect -l` und `ls /dev/snd` zeigen die MIDI-Geräte.
  * Fehlt das USB-Gerät dort, enthält der ArkOS-Kernel keinen `snd-usb-audio`-Treiber.
* Der Header zeigt die CPU-Last der Audio-Engine an. Mit dem Demo-Beat ist sie sehr gering. Gemessen habe ich nur auf dem PC (unter 1 %). Auf dem R36S ist sie höher, sollte aber reichlich Luft lassen.

## Selbst bauen

```bash
# Desktop (zum Testen)
sudo apt install libsdl2-dev libasound2-dev && make && ./groovebox --windowed

# Cross-Build für den R36S (auf x86-Linux)
sudo apt install g++-aarch64-linux-gnu curl && ./tools/cross-build.sh
```

Der Cross-Build lädt einen kleinen Ubuntu-18.04-arm64-Sysroot (glibc 2.27, SDL 2.0.8 Header) nach `.sysroot`.
Dadurch läuft das Programm auf ArkOS und nutzt dort dessen eigene SDL2 mit KMS/DRM.
Der Code benutzt bewusst keine libstdc++ und hängt nur von libSDL2, libasound, libm und libc ab. All diese Bibliotheken bringt ArkOS mit.

Weitere Kommandozeilen-Optionen:

* `--render out.wav 8`: Das aktuelle Projekt offline als WAV rendern.
* `--shot bild.bmp [seite]`: einen Screenshot erzeugen.
* `--buffer N`: die Puffergröße der SYS-Seite übersteuern.

## Aufbau

* `src/engine.*`: Sequencer (sampelgenau in 16-Sample-Blöcken, Swing, Retrigs, Polymetrie), Stimmen, Delay, Freeverb-Reverb, DJ-Filter, Limiter
* `src/params.h`: alle Parameterdefinitionen und Wertanzeigen
* `src/samples.*`: WAV-Lader und Sample-Bibliothek
* `src/midi.*`: MIDI-Thread (ALSA-Sequencer mit automatischem Verbinden bzw. rawmidi) und lock-freie Queues zum Audio-Thread
* `src/input.*`: Gamepad mit lernbarer Belegung, SDL-GameController und Tastatur
* `src/main.cpp`: Oberfläche (640×480) und Bedienlogik
* `src/font.h`: Bitmap-Schrift, erzeugt mit `tools/genfont.py` aus DejaVu Sans Mono
