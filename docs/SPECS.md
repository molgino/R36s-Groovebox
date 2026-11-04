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
| Audio | 44,1 kHz, 32 Bit Gleitkomma intern, Ausgabe 16 Bit |

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
| | R36S (RK3326, 1,3 GHz), Version 305f93e | nach Optimierung (Instruktionen, gemessen) |
|---|---|---|
| Audio-Last Leerlauf | 5,1 % | −83 % |
| Audio-Last 8× Kick | 23 % | −34 % |
| Audio-Last 8× FM | 31 % | −22 % |
| Audio-Last Worst Case (16 Stimmen, alle LFOs/FX, 3 Akkord-Spuren) | 86 % | −21 % (dazu Compiler-Tuning für Cortex-A35) |
| Bildaufbau inkl. Ausgabe | 10–13 ms, Menü 17,5 ms, Hilfe 20,6 ms | Zeichnen −35 % (Menü −57 %), Ausgabe ohne Abfragen pro Pixel, Ruhe mit 30 Bildern/s |
| Speicher | 26 MB | |

Neue Gerätewerte: SET → Leistungstest (Bericht im Browser unter Logs → bench.txt).

## Gerät (R36S, typische Herstellerangaben – Klone können abweichen)
Rockchip RK3326 (4 × Cortex-A35, bis 1,5 GHz), 1 GB RAM, 3,5″-IPS 640 × 480, Akku ca. 3200 mAh, Kopfhöreranschluss, Lautsprecher, 2 Analogsticks, Steuerkreuz, 4 Aktionstasten, 4 Schultertasten.
