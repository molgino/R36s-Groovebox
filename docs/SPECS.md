# SYNTH CITY – Technische Daten

## Klangerzeugung
| | |
|---|---|
| Spuren | 8, je Spur eigene Engine pro Pattern |
| Engines | 13: KICK, SNARE, HAT, CLAP, TOM, RIM, SHAKER (analog modelliert), SYNTH (virtuell-analog, 2 Oszillatoren + Sub + Rauschen), FM (4 Operatoren, 8 Algorithmen), WAVETABLE, SAMPLE, CHOP (Slicer bis 64 Scheiben), GRANULAR |
| Stimmen | 1 je Spur, im Akkord-Modus bis 4 je Spur (32 gesamt) |
| Filter 1 | 11 Typen: LP12, BP, HP12, LP24, Ladder (4-Pol, selbstoszillierend), HP24, Notch, Peak, Kamm +/−, Vokal (Formant A-E-I-O-U) |
| Filter 2 | Base-Width (Hoch- + Tiefpass), Keytracking, Velocity → Cutoff |
| Hüllkurven | 2 ADSR je Spur (Filter, Lautstärke) mit einstellbarer Kurvenform |
| Modulation | 3 LFOs je Spur (9 Wellenformen, Sync, Fade, Trig-Modi), Ziel = jeder Spurparameter |
| Effekte | Insert je Spur (Chorus, Phaser, Flanger, Drive, Ringmod, Auto-Pan) + Bitcrusher, Sample-Rate-Reduktion, Drive; Send-Delay (Ping-Pong, Band-Echo-Gleiten), Reverb; Bus-Kompressor mit Sidechain; DJ-Filter |
| Audio | 44,1 kHz, 32 Bit Gleitkomma intern, DC-Sperre + Limiter in der Summe; WAV-Aufnahme/-Export und USB-Audio 24 Bit (mit Dither), Kopfhörer/Lautsprecher je nach Codec |

## Sequencer
| | |
|---|---|
| Patterns | 16 je Projekt, 32 Projekte |
| Steps | 1–128 je Spur (Polymetrie), Tempo-Faktor je Spur (1/8× … 2×) |
| Locks | Parameter-Locks auf jedem Step für alle 96 Spurparameter, Trigless Locks |
| Trig-Optionen | Wahrscheinlichkeit, Bedingungen (FILL, 1ST, PRE, NEI, a:b …), Ratchets, Mikrotiming |
| Generatoren | Euklid, Turing-Zufall, Pattern-Mutation, Rhythmus-Vorlagen, Arpeggiator (5 Modi) |
| Song | Pattern-Kette mit 64 Einträgen und Wiederholungen |
| Live | LIVE-Seite: 8 Mutes auf direkten Tasten, FILL, Pattern-Wechsel, DJ-Filter, WAV-Aufnahme (5 Min.) |
| Bearbeiten | Rückgängig/Wiederholen (12 Schritte), Kopieren von Trig, Seite, Pattern |

## Anschlüsse (SYNTH CITY OS)
| | |
|---|---|
| USB zum PC | ein Kabel: Netzwerk (Browser-Oberfläche), USB-MIDI, USB-Audio-Interface (Stereo-Ausgang an den PC, PC-Ton über das Gerät, Sampling vom PC), Massenspeicher-Modus für Samples |
| USB-Host | MIDI-Geräte und USB-Audio-Interfaces am OTG-Port |
| MIDI | Noten ein/aus, Clock senden/empfangen, Kanal je Spur, RTP-MIDI |
| Browser | Samples/Projekte hoch- und herunterladen, Bildschirm-Spiegelung mit Fernbedienung, Logs, App-Update ohne neues Image |

## Leistung (gemessen mit `--bench`)
Gemessen auf dem R36S (RK3326, Cortex-A35 1,3 GHz) mit SET → Leistungstest, Anteil eines Kerns (Budget 5,8 ms je 256-Frame-Block):

| | vor Optimierung (305f93e) | nach Optimierung (a098e23) |
|---|---|---|
| Audio Leerlauf | 5,1 % | **0,7 %** |
| Demo-Song (realistisch) | – | **9,9 %** (Spitze 24,5 %) |
| 8× Kick / 8× Synth / 8× FM | 23,3 / 24,8 / 31,1 % | **16,2 / 19,0 / 25,9 %** |
| Worst Case (16,5 Stimmen, alle LFOs/FX, 3 Akkord-Spuren) | 86,4 % (Spitze 94 %) | **70,5 %** (Spitze 79 %), mit 2 Kernen **40,7 %** (Spitze 49 %) |
| Bild je Frame inkl. Ausgabe (davon Ausgabe 4,3 ms) | 10,3–13,2 ms | **8,5–11,0 ms** |
| Seitenmenü / Hilfe | 17,5 / 20,6 ms | **12,5 / 14,0 ms** |
| Speicher | 26 MB | 24 MB |

Stand 11/2026 (Version e628798: FM mit 4 Operator-Hüllkurven, SYNTH-OSC-Seite, 3 Effekt-Busse, Animationen), R36S, Wiedergabe lief nebenher:

| | Last | Spitze |
|---|---|---|
| Audio Leerlauf | 2,3 % | 7,7 % |
| Demo-Song, 2 Kerne | 13,0 % | 36,5 % |
| 8× Kick / 8× Synth / 8× FM | 20,3 / 24,7 / 34,1 % | 26 / 28 / 39 % |
| Worst Case, 1 Kern | 77,7 % | 87,8 % |
| Worst Case, 2 Kerne | **45,5 %** | 58,3 % |
| Bild je Frame inkl. Ausgabe (4,3 ms) | 8,7–11,2 ms | Seitenmenü/Hilfe 14,2 ms |
| Speicher (inkl. gesperrter Thread-Stacks) | 122 MB | |

Neue Gerätewerte: SET → Leistungstest (Bericht im Browser unter Logs → bench.txt).

## Gerät (R36S, typische Herstellerangaben – Klone können abweichen)
Rockchip RK3326 (4 × Cortex-A35, bis 1,5 GHz), 1 GB RAM, 3,5″-IPS 640 × 480, Akku ca. 3200 mAh, Kopfhöreranschluss, Lautsprecher, 2 Analogsticks, Steuerkreuz, 4 Aktionstasten, 4 Schultertasten.
