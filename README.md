# SYNTH CITY – Groovebox für den R36S

Groovebox für die Hosentasche: Step-Sequencer mit Parameter-Locks, 13 Klangerzeuger und USB-Audio-Interface auf dem Handheld R36S (eigenes SYNTH CITY OS oder ArkOS 2.0; läuft auch auf dem Linux-Desktop).
Neo-Noir-Optik in Neon-Magenta und Cyan; beim Start erscheint ein Splashscreen mit Synthwave-Skyline (beliebige Taste überspringt).
8 Spuren, 16 Patterns mit bis zu 128 Steps pro Spur, Parameter-Locks, Akkorde, Sample-Spuren, LIVE-Seite mit WAV-Aufnahme, USB-Audio, USB-MIDI, Ping-Pong-Delay, Reverb und ein DJ-Filter. Technische Daten: [docs/SPECS.md](docs/SPECS.md).
Sie startet als App aus dem Ports-Menü oder auf Wunsch direkt beim Booten statt EmulationStation.

## Schnellstart

* Beim **ersten Start** ist der Demo-Song **„Neon Nights“** geladen: **START** spielt ihn ab (Song-Modus, 4 Patterns). Jederzeit wieder: SET → „Demo-Song laden“.
* **SELECT antippen** öffnet das Menü mit allen Seiten (LIVE, Mixer, FX, Song, Projekt, SET). **SELECT halten** zeigt die Tastenhilfe.
* Unten im Bedienfeld steht zu jedem Regler eine kurze Erklärung.
* Fertige Klänge: Seite PRJ → **PRESET** über Platz 32 hinaus drehen → **Werksklänge** (z. B. „NIGHT DRIVE BASS“, „PUNCH KICK“, „HAZE PAD“) → **P.LADEN**. Ein Werksklang setzt auch die Engine.
* Vertan? **SELECT + B** macht die letzte Änderung rückgängig (12 Schritte), SET → „Wiederholen“ stellt sie wieder her.

## Oberfläche: Signalweg

Auf den Spurseiten zeigt die Oberfläche den Weg des Klangs: links alle 8 Spuren mit Mini-Steps und Pegel, oben die Blöcke **QUELLE → FILTER → AMP → INSERT** (zusammengehörige Seiten untereinander: QUELLE/QUELLE 2, FILTER 1/FILTER 2), Auswahl mit **L1/R1**. Darunter die Reihe **SEQ**: TRIG · SEQ · ARP · LFO1–3 (**L2/R2**). Der gewählte Block zeigt Grafik, 8 Regler und eine Erklärung des gewählten Reglers, unten liegen die 16 Steps der Spur.

* **SELECT + L1/R1** springt einen Schlag (4 Steps) – vier Kicks auf 1/5/9/13 sind so in wenigen Tastendrücken gesetzt.
* **Rhythmus-Vorlagen** für die gewählte Spur: SET → „Rhythmus-Vorlage“ (Four on the Floor, Backbeat, Offbeat, Achtel, Sechzehntel, Breakbeat, Clave, Tresillo, Dembow …), mit ←/→ wählen, A setzt sie.
* **Hüllkurvenform:** Seite FILTER 2 + KURVEN, Regler **F.KRV** (Filter) und **A.KRV** (Lautstärke): Minus = weich/rund, 0 = Standard, Plus = knackig/schnell abfallend. Die Grafiken zeigen die Form.

## LIVE-Seite (Menü → LIVE)

Die 8 Spuren liegen auf dem Bildschirm genau so wie die Tasten: **Steuerkreuz ↑ → ↓ ←** = Spur 1–4, **X A B Y** = Spur 5–8 – jeder Druck schaltet die Spur stumm oder wieder an.

| Taste | Funktion |
|---|---|
| L1 halten | FILL (Trig-Bedingungen FILL) |
| R1 | **WAV-Aufnahme** der Summe starten/stoppen (max. 5 Min.), gespeichert als `samples/rec/rec-NNN.wav` – im Browser unter Samples herunterladen |
| L2 / R2 | Pattern zurück/vor (beim Abspielen am Pattern-Ende) |
| rechter Stick ↑/↓ | DJ-Filter (Hoch-/Tiefpass) |
| linker Stick ↑/↓ | Tempo ±1 BPM |
| START | Play / Stop |

## Akkorde (ARP-Seite, MODE = AKKD)

Im ARP-Modus **AKKD** spielt ein Trig alle Töne des gewählten Akkords (**CHRD**: Oktave, Quinte, Dur, Moll, sus4, Maj7, Min7, Dom7, Dim oder Skala = Terzschichtung der Projektskala) gleichzeitig – bis zu 4 Stimmen pro Spur, mit den Klangeinstellungen der Spur.

## Klangerzeugung

| Maschine | Regler auf der SRC-Seite |
|---|---|
| KICK  | TUNE, DCAY, SWEP (Pitch-Sweep), S.TIM, CLCK, BODY (Sättigung) |
| SNARE | TUNE, DCAY, SNAP (Rauschanteil), N.DEC, TONE |
| HAT   | TUNE, DCAY, TONE (Hochpass), METL (Metall/Rauschen) |
| CLAP  | TUNE, DCAY, SPRD (Abstand), RPTS (Anzahl Schläge) |
| SYNTH | WAV1, WAV2 (SAW/SQR/TRI/SIN), DETN, MIX, SUB, NOIS, PW, GLID |
| FM    | **4 Operatoren, 8 Algorithmen.** SRC: ALGO, FBK (Feedback OP4), RAT1–RAT4, FINE, NOIS. SRC2: LVL2–LVL4 (Mod-Index bzw. Pegel), DEC1–DEC4 (INF = halten), VELO (Anschlagdynamik) |
| SMPL  | SMPL (Sample-Auswahl), TUNE (±24 Halbtöne), FINE, STRT, END (END < STRT = rückwärts), LOOP, GAIN |
| TOM   | TUNE, DCAY, SWEP (Pitch-Abfall), NOIS (Anschlag), BODY |
| RIM   | TUNE, DCAY, TONE, CLCK |
| SHKR  | TONE, DCAY, ATK (Ausholen), GRIT (Körnigkeit) |
| WTBL  | **Wavetable**: TABL (BASIC Sinus→Dreieck→Säge→Rechteck, PWM, VOKAL A→U, OBERTÖNE), POS (Position in der Tabelle, auch als LFO-Ziel), WARP (Phasenverbiegung), DETN (Unisono), SUB, NOIS, P.ENV (Filterhüllkurve → Position), GLID |
| CHOP  | **Sample-Chopper**: SMPL, SLCS (2–64 Scheiben), SLIC (AUTO = Scheibe nach Step-Nummer, oder feste Scheibe, pro Step lockbar), TUNE, FINE, LEN (Anteil der Scheibe), REV, GAIN |
| GRAN  | **Granulator**: SMPL, POS, SPRY (Streuung), SIZE (10–500 ms), DENS (2–128 Körner/s), PTCH, P.SPR (Tonhöhen-Streuung), GAIN |

**Klangcharakter:** KICK, SNARE, HAT, CLAP und RIM sind an die klassischen analogen Drumcomputer angelehnt (Kick: Sinus mit schnellem Pitch-Sweep und Sättigung über BODY, Klick über CLCK; Snare: zwei verstimmte Töne mit eigenem Pitch-Abfall plus gefiltertes Rauschen, TONE = Helligkeit; Hihat: sechs Rechteck-Oszillatoren durch Bandpass + Hochpass; Clap: sägezahnförmige Einzelschläge vor der Fahne).
SYNTH und WTBL starten mit dem **24-dB-Tiefpass** (LP24) mit sanfter Sättigung; die Oszillatoren driften leicht wie bei analogen Geräten. Weitere Filtertypen (Ladder, Kamm, Vokal …) siehe FLT.

Die Engine wählst du pro Spur und Pattern: auf der TRG-Seite mit dem Regler ENGINE oder mit SELECT + X/Y.
Beim Drehen erscheint der **Engine-Wähler** mit Symbolen für alle 13 Engines.

Jede Spur spielt eine Stimme; SYNTH, FM, WTBL und die Sample-Engines spielen im ARP-Modus **AKKD** bis zu 4 Akkordtöne gleichzeitig.
Die Engine gilt pro Spur und Pattern. Wechselst du sie, werden nur die SRC-Regler auf die Grundwerte der neuen Engine gesetzt.

Jede Spur hat diese Seiten:

* **TRG**: NOTE, VEL, LEN, PROB (Wahrscheinlichkeit), RTRG (Retrigs pro Step), **MTIM** (Mikrotiming), ENGINE, **COND** (Trig-Bedingung, siehe unten)
* **SEQ**: Laufrichtung, Länge, Tempo-Faktor, Euklid und Generator (siehe unten)
* **SRC** (und **SRC2** bei FM): Regler der Engine (siehe Tabelle). Grafisch dargestellt werden:
  * bei SYNTH die tatsächlich klingende, gemischte Wellenform
  * bei FM das Algorithmus-Diagramm, in dem die Operatoren beim Spielen aufleuchten
  * bei SMPL die Sample-Wellenform mit Start, Ende und Abspielposition
* **FLT**: Filter 1 mit eigener ADSR-Hüllkurve. Oben siehst du die Filterkurve mit dem live modulierten Cutoff, unten die Hüllkurve. TYPE wählt den Filter (auch pro Step lockbar):

  | TYPE | Filter |
  |---|---|
  | LP12 / BP / HP12 | klassisches Zustandsvariables Filter, 12 dB/Okt. |
  | LP24 / HP24 | zwei Stufen, 24 dB/Okt., mit leichter Sättigung (Standard bei SYNTH und WTBL) |
  | LADR | Ladder-Filter: vier Transistor-Stufen mit Rückkopplung, gesättigt, schwingt bei voller RESO selbst |
  | NTCH / PEAK | Kerbfilter bzw. Anhebung um FREQ, RESO = Schärfe |
  | CMB+ / CMB- | Kammfilter: FREQ = Grundton der Kammzähne, RESO = Rückkopplung (metallisch, Flanger-/Saitenklänge; mit KEYT spielt er die Tonhöhe mit) |
  | VOWL | Formantfilter: FREQ fährt durch die Vokale A-E-I-O-U, RESO = Schärfe |

* **FL2**: Base-Width-Filter hinter dem ersten Filter: **BASE** ist die untere Kante (Hochpass), **WDTH** die Breite des durchgelassenen Bandes (Tiefpass darüber). BASE 0 / WDTH 127 = aus. Dazu **KEYT** (Keytracking: Cutoff von Filter 1 folgt der Tonhöhe) und **VELF** (Anschlagstärke → Cutoff). Die Grafik zeigt Filter 1 (grau) und die Gesamtkurve mit dem Band.
* **Symbole statt Balken**: Jede Kachel zeigt ein Symbol, das sich mit dem Wert verändert – Tastatur bei NOTE, Torte bei PROB, Hüllkurvenbogen bei Attack/Decay, Lautsprecher bei Lautstärke, Echos bei Delay, Ringe bei Reverb, Würfel bei CHNG usw.
* **AMP**: ADSR (als Hüllkurve mit Live-Punkt dargestellt, Zeiten in ms/s), Lautstärke, Panorama, **Delay- und Reverb-Send** (DLY, REV)
* **LF1 / LF2 / LF3**: drei LFOs pro Spur (siehe unten)
* **INS**: Insert-Effekt der Spur. Zuerst unter TYPE den Effekt wählen, dann MIX aufdrehen:

  | TYPE | Regler |
  |---|---|
  | CHORS (Chorus, stereo) | RATE, DEPTH, WIDTH |
  | PHASE (6-stufiger Phaser, stereo) | RATE, DEPTH, FBK |
  | FLNGR (Flanger) | RATE, DEPTH, FBK (Mitte = 0, links negativ, rechts positiv) |
  | DRIVE (Overdrive) | GAIN, TONE, LEVEL |
  | RING (Ringmodulator) | FREQ, FINE |
  | APAN (Autopan) | RATE, DEPTH |

  Dazu kommen auf derselben Seite **CRSH** (Bitcrusher), **SRR** (Samplerate-Reduktion) und **DRIV** (Drive).
  Alle Regler, auch TYPE, lassen sich pro Step locken. Die Insert-Effekte sitzen hinter Filter und Hüllkurve, also vor Panorama und den Sends.

Dazu kommen die globalen Seiten (in der Leiste oben grauer dargestellt):

* **FX**: die Send-Effekte
  * Delay:
    * D.TIME: 1/64 bis 1/1 im Songtempo, auch triolisch (T) und punktiert (.). Zeitwechsel gleiten wie beim Bandecho.
    * **D.FBK bis 120 %**: Ab etwa 100 % kippt das Delay in eine selbsterhaltende **Feedback-Schleife**. Sättigung und Bassfilter in der Schleife halten sie dabei im Rahmen.
    * D.TONE, D.PING (0 = mono, 127 = volles Ping-Pong)
  * Reverb: R.SIZE, R.DAMP, R.PRE (Predelay bis 250 ms), R.WIDE (Stereobreite)
* **CMP**: Master-Kompressor mit **Sidechain** (siehe unten)
* **MIX**: Mixer mit Mute/Solo (siehe unten)
* **PRJ**: Projekte und Presets (siehe unten)
* **MST**: BPM, Swing, Gesamtlautstärke, DRIVE (Sättigung der Summe) die globale Tonskala SCALE und ihr Grundton ROOT
* **SET**: Einstellungen als Liste (siehe unten, wird in `settings.cfg` gespeichert)

## Steps, Seiten und Geschwindigkeit

* Jede Spur hat **1 bis 128 Steps**. Die Länge stellst du über SELECT + L2/R2 oder in SET ein.
* Angezeigt werden immer 16 Steps. Die Leiste über den Step-Tasten zeigt die bis zu 8 Seiten: Die Cursor-Seite ist hell, die laufende Seite in Spurfarbe.
* Das Steuerkreuz läuft durch alle Steps. **L1/R1 springt eine Seite** (16 Steps) weiter.
* **Tempo-Faktor pro Spur** (SET → „Tempo-Faktor Spur“): 1/8×, 1/4×, 1/2×, 3/4×, 1×, 3/2× und 2×.
* Ein Pattern-Wechsel greift, wenn die längste Spur durchgelaufen ist.

## Sequencer-Werkzeuge (SEQ-Seite)

| Regler | Funktion |
|---|---|
| DIR | Laufrichtung: **FWD** (vorwärts), **REV** (rückwärts), **PING** (hin und her), **PEND** (hin und her, Enden doppelt), **RND** (zufälliger Step), **DRNK** (Zufallsschritt ±1, „betrunken“) |
| LEN | Spurlänge 1–128 |
| SPD | Tempo-Faktor 1/8× … 2× |
| PULS | **Euklid**: Anzahl Schläge, gleichmäßig über die Spurlänge verteilt (0 = aus, dann gelten die gesetzten Trigs) |
| ROT | Rotation des euklidischen Musters |
| DENS | **Generator** (Turing-Machine-Prinzip): Dichte der erzeugten Trigs (0 = aus) |
| CHNG | Mutation: 0 % = die erzeugte Schleife (Länge = Spurlänge, max. 32) wiederholt sich exakt, 100 % = jedes Mal neu |
| RANGE | Melodie-Generator: zufällige, aber wiederkehrende Tonhöhen über bis zu 48 Halbtöne, eingerastet in die globale Skala. Wirkt auf alle Trigs der Spur, auch auf euklidische und gesetzte. |

* Ist PULS oder DENS aktiv, kommen die Trigs aus dem Generator. Euklid und Generator werden dabei verodert.
* Locks auf den Steps, COND und PROB wirken weiter.
* Im Raster kennzeichnet ein gelbes **E** bzw. **G** Spuren mit Euklid bzw. Generator.
* Die Grafik zeigt den Euklid-Kreis mit wanderndem Playhead, die Richtung und das Schieberegister mit den erzeugten Tonhöhen.
* Alle SEQ-Regler außer LEN und SPD sind auch **LFO-Ziele**. Ein langsamer LFO auf PULS oder DENS lässt einen Beat „atmen“.

## Kopieren und Einfügen

| Was | Kopieren | Einfügen |
|---|---|---|
| **Trig** (mit allen Locks, auch trigless) | Step mit **A halten + L1** | Ziel-Step **A halten + R1** – geht auch auf anderen Spuren; im Lock-Modus mit ←/→ weiterspringen und mehrfach einfügen |
| **Seite** (die 8 Werte einer Spurseite) | Seitenmenü (SELECT antippen), Seite markieren, **X** | Zur Ziel-Spur wechseln, Seitenmenü, gleiche Seite markieren, **Y** |
| **Pattern** | SET → „Pattern kopieren“ | SET → „Pattern einfügen“ |

Die Seite SRC/SR2 lässt sich nur zwischen Spuren mit gleicher Engine übertragen (die Regler sind je Engine verschieden). Bei SEQ werden Länge und Tempo mitkopiert.

## Trigless Locks (nur Parameter, kein Ton)

Ein trigless Step trägt nur Parameter-Locks und schlägt keinen Ton an. Er ändert die Werte des gerade klingenden Tons, z. B. Filter oder Resonanz mitten in einer langen Fläche. Die Werte gelten bis zum nächsten Trig. Im Raster erscheint er als **Umriss mit Punkt**.

* **Trig mit Locks löschen** (A antippen): Der Ton fällt weg, die Locks bleiben als trigless Step stehen.
* **Trigless Step antippen:** Er wird wieder ein normaler Trig (Locks bleiben).
* **Trigless Step halten + Werte drehen:** Locks bearbeiten, der Step bleibt trigless.
* **Im Lock-Modus** (A lang halten): **SELECT + A** schaltet zwischen Trig und trigless um, **B** löscht die Locks. Ein trigless Step verschwindet damit ganz.

## Arpeggiator (ARP-Seite)

Für alle Spuren mit Tonhöhe (Synth, FM, Wavetable, Samples …). Ein Trig spielt dann statt eines Tons die Akkordtöne – so lange, wie der Trig dauert (LEN auf der TRG-Seite).

| Regler | Bedeutung |
|---|---|
| MODE | AUS, UP, DOWN, UPDN (hoch und runter), RAND (zufällig) |
| CHRD | OKT, 5TH, MAJ, MIN, SUS4, MAJ7, MIN7, DOM7, DIM, SKALA (Töne der globalen Skala) |
| OCT | über 1–4 Oktaven |
| RATE | 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4 |
| GATE | Tonlänge in % des Abstands |
| ACCT | Akzent: der erste Ton jeder Runde lauter |

Alle Werte lassen sich pro Step locken. Die Grafik zeigt die Tonfolge über die Trig-Länge.

## Song-Modus (SNG-Seite, Gruppe PROJEKT)

Eine Kette aus Patterns mit Wiederholungen (bis 64 Einträge):
* ↑/↓ Eintrag wählen, ←/→ Pattern ändern, X/Y Wiederholungen, A neuer Eintrag, B löschen
* Seite wechseln wie überall mit L2/R2, SELECT + rechtem Stick oder dem SELECT-Menü (rechter Stick links/rechts geht hier auch ohne SELECT)
* **START** spielt den Song ab dem gewählten Eintrag, **R1** schaltet den Song-Modus an/aus
* Im Song-Modus steht im Kopf „SNG 2/5“; START auf anderen Seiten setzt den Song fort

## Pattern-Mutation (SET → PATTERN)

„Pattern mutieren“ variiert das aktuelle Pattern: Trigs kommen dazu oder fallen weg, Velocity, Töne (in der Skala), gelegentliche Ratchets und Mikrotiming. Stärke mit ←/→ (LEICHT, MITTEL, STARK), A wendet sie an. „Mutation rückgängig“ holt das Pattern vor der letzten Mutation zurück.

## Mikrotiming (MTIM)

MTIM verschiebt einen Trig um bis zu ±23/24 eines Steps, vorgezogen oder verspätet. Pro Step lockst du es mit A halten und MTIM drehen.
Vorgezogene Trigs werden einen Step im Voraus geplant. Die Spur läuft also nicht insgesamt später.
Im Raster sitzen verschobene Trigs sichtbar versetzt, und beim Drehen zeigt ein Zeitlineal die Position.

## Trig-Bedingungen (COND)

COND auf der TRG-Seite gilt für die Spur. Meist lockst du COND aber pro Step: A halten und COND drehen. PROB gilt zusätzlich.

| COND | Trig spielt, wenn … |
|---|---|
| `A:B` | im A-ten von je B Durchläufen der Spur, z. B. `1:2` = jeder zweite Durchlauf, `4:4` = jeder vierte |
| `!A:B` | in allen Durchläufen außer dem A-ten von je B, z. B. `!1:4` |
| `FILL` / `!FILL` | nur bei eingeschaltetem bzw. ausgeschaltetem FILL-Modus (LIVE-Seite: **L1 halten**, oder SET → FILL; im Header blinkt FILL) |
| `PRE` / `!PRE` | die vorige Bedingung auf dieser Spur erfüllt war bzw. nicht |
| `NEI` / `!NEI` | die letzte Bedingung der Nachbarspur (Spur davor) erfüllt war bzw. nicht |
| `1ST` / `!1ST` | nur im ersten Durchlauf nach Start bzw. Pattern-Wechsel, bzw. in allen außer dem ersten |

## LFOs (LF1, LF2, LF3)

Jede Spur hat drei LFOs. Jeder kann **jeden Parameter der Spur** modulieren, auch die Regler eines anderen LFOs.

| Regler | Funktion |
|---|---|
| RATE | Frequenz (frei, 0,05–25 Hz) |
| SYNC | FREE oder taktsynchron: 4BAR, 2BAR, 1BAR, 1/2 … 1/32, 1/4T, 1/8T, 1/16T |
| ZIEL | Zielparameter (NOTE = Tonhöhe, ±12 Halbtöne bei voller Tiefe) |
| DEPTH | Tiefe, bipolar |
| WAVE | SIN, TRI, SAW+, SAW−, SQR, **EXP+**, **EXP−**, S&H, RND (weicher Zufall) |
| MODE | **FREE** (läuft durch), **TRIG** (Neustart bei jedem Trig), **HOLD** (Wert beim Trig festhalten), **ONE** (eine Periode, dann stehen bleiben), **HALF** (halbe Periode) |
| PHASE | Startphase bei TRIG/ONE/HALF |
| FADE | links einblenden, rechts ausblenden (startet bei jedem Trig) |

Die Grafik zeigt die Kurve, die laufende Phase und das Ziel.

## Optik

* Jede Engine hat ein **gezeichnetes Symbol** (Bassdrum, Snare, Becken, Hand, Tom, Stick, Shaker-Ei, Wellen …).
  Es erscheint im Raster, im Header und im Mixer und springt beim Trig.
* Statt Drehreglern gibt es **Kacheln** mit großem Wert und Füllbalken. Bipolare Werte füllen von der Mitte aus, bei Locks markiert ein Strich den Grundwert.
* **Grafiken der SRC-Seite:**
  * Drums: großes Symbol mit Ausklingkurve und Pitch-Sweep
  * Wavetable: gestapelte Frames mit der aktuellen Position
  * Chop: Scheiben mit der laufenden Scheibe
  * Granulator: Positionsfenster und aktive Körner

## Tonskala (global)

Auf der MST-Seite wählt **SCALE** die Skala und **ROOT** den Grundton (C bis B) für das ganze Projekt, beides unabhängig voneinander. Skalen: CHROM (aus), Dur, Moll, Dorisch, Phrygisch, Lydisch, Mixolydisch, Harmonisch Moll, Pentatonik Dur/Moll und Blues.
Beim Drehen zeigt eine Mini-Tastatur die Skalentöne. Die aktive Skala steht im Header neben dem Swing.

* NOTE (auch als Lock) springt nur zwischen Skalentönen.
* Sequencer, MIDI-Eingang und Aufnahme rasten in die Skala ein.

## Einstellungen (SET-Seite)

Über das SELECT-Menü (Gruppe PROJEKT) oder mit SELECT + rechtem Stick erreichst du die Einstellungsliste:

* **PATTERN:** Pattern wählen, Länge der Spur bzw. aller Spuren, Tempo, Swing, Tonskala, Grundton, Pattern kopieren/einfügen/leeren
* **AUDIO:** Ausgang (interner Lautsprecher, S-1, USB-Interface …), Puffer mit Latenz in ms, Lautstärke
* **MIDI:** Eingang, Noten ausgeben, Clock (senden/empfangen), Eingangskanäle
* **USB:** HOST (Geräte am R36S) oder PC (Netz-MIDI, SSH) und die IP-Adresse
* **SYSTEM:** Tasten neu lernen, Info, Beenden

Steuerkreuz ↑/↓ wählt, ←/→ (oder X/Y) ändert, **A** führt aus, **B** springt zurück zur vorherigen Seite.

## Mixer (MIX-Seite)

Hier siehst du alle 8 Spuren als Kanalzüge mit Fader, Pegelanzeige, Panorama, Delay- und Reverb-Send.

* Steuerkreuz ↑/↓ oder linker Stick ←/→ wählt den Kanal.
* Linker Stick ↑/↓ oder L1/R1 wählt die Zeile (VOL, PAN, DLY, REV).
* Rechter Stick ↑/↓ oder X/Y ändert den Wert.
* **A = Mute, B = Solo.** Sind mehrere Spuren auf Solo, klingen alle Solo-Spuren.

## Projekte und Presets (PRJ-Seite)

* **SONG** (32 Plätze) mit **LADEN**, **SPEICHERN** und **NEU** (legt ein leeres Projekt mit leeren Init-Patterns im nächsten freien Platz an; der Demo-Beat kommt nur beim allerersten Start).
  Gespeichert wird in `groovebox/projects/songNN.gbx`. Der aktive Platz wird gemerkt, und das automatische Speichern landet dort.
* **PRESET** (32 Plätze pro Engine) mit **P.LADEN** und **P.SPEICHERN**. Ein Preset enthält den kompletten Klang der Spur: Engine und alle Seiten außer den Steps.
  Gespeichert wird in `groovebox/presets/<ENGINE>/NN.gbp`.
* **INIT** setzt den Klang der gewählten Spur auf die Grundwerte der Engine zurück.
* Aktionen startest du mit **A** (oder X). Laden, Überschreiben, Neu und Init fragen vorher nach: **A = Ja, B = Nein**.

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

### Samples per Browser hochladen (ohne SD-Karte)

1. R36S per USB-Kabel mit dem PC verbinden und **PC-Modus** einschalten (SET → USB = PC).
2. Im Browser **http://192.168.77.1** öffnen. Die Adresse steht auch in SET unter „Samples per Browser“.
3. Dateien in das Feld ziehen. **WAV, MP3, FLAC, AIFF, OGG …** wandelt der Browser automatisch in Mono-WAV mit 44,1 kHz um (abschaltbar, optional normalisieren). Mit „neuer Ordner“ legst du einen Unterordner an.
4. Etwa 1,5 Sekunden nach dem letzten Upload lädt die Groovebox die Samples neu, ohne Aussetzer. Bestehende Spuren behalten ihr Sample, die Zuordnung läuft über den Namen.

Auf derselben Seite kannst du Samples anhören und löschen sowie Projekte (SONG 01–32) sichern und hochladen.

Ein Start-Paket mit 13 selbst erzeugten Samples liegt bei: Drums, Keys, eine Vokal-Fläche und FX.
Es wird mit `tools/gensamples.py` erzeugt.

* Mit SELECT + X/Y stellst du eine Spur auf **SMPL** um.
* Den Sample-Regler auf der SRC-Seite kannst du pro Step locken. So spielt eine einzige Spur verschiedene Samples.
* NOTE C-4 ist die Originaltonhöhe.
* Ohne LOOP läuft ein Sample als One-Shot bis zum Ende. Mit LOOP gilt die Trig-Länge (LEN) bzw. die Haltedauer der MIDI-Taste.
* Das Projekt merkt sich die Sample-**Nummer**. Beim Upload per Browser werden die Nummern automatisch nachgeführt. Kopierst du Dateien von Hand auf die Karte, benenne sie so, dass sie hinten einsortiert werden, sonst verschieben sich die Nummern.

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
| **A lang halten** (ca. ½ s) | **Lock-Modus** rastet ein: Du kannst A loslassen und mit rechtem Stick oder X/Y Locks setzen, ←/→ springt zum nächsten Step, B löscht die Locks des Steps, A antippen = fertig |
| A halten + X/Y | Parameter-Lock auf diesen Step (gelb), wie bisher |
| A halten + B | alle Locks des Steps löschen |
| B | Spur vorhören |
| **Linker Stick** | **Kachel wählen**: jedes Antippen springt genau ein Feld nach links/rechts/oben/unten, Halten wiederholt |
| **Rechter Stick ↑/↓** | **Wert stufenlos ändern** |
| **SELECT + rechter Stick ←/→** | **Seite wechseln** (der rechte Stick allein ändert nur Werte) |
| X / Y | Wert + / − in Einzelschritten (halten = Wiederholung) |
| **SELECT kurz antippen** | **Seitenmenü**: alle Seiten nach Gruppen (SPUR / MIX & FX / PROJEKT), Steuerkreuz wählt, A öffnet |
| SELECT halten | Hilfe-Einblendung |
| SELECT + rechter Stick ↑/↓ | DJ-Filter: oben Hochpass, unten Tiefpass. Die Richtung rastet beim Auslenken ein, schräg drücken wechselt nicht aus Versehen |
| L1/R1 | auf Spurseiten: **Block im Signalweg** (QUELLE → FILTER → BASE-W → AMP → INSERT), sonst Regler |
| L2/R2 | auf Spurseiten: **Modulation/Sequenz** (TRIG, SEQ, ARP, LFO1–3), sonst Seite |
| START | Play / Stop |
| SELECT + ←/→ | Pattern wechseln (beim Abspielen am Pattern-Ende) |
| SELECT + ↑ | Aufnahme an/aus (für MIDI-Eingabe) |
| SELECT + ↓ | Tap-Tempo (mehrmals im Takt tippen) |
| SELECT + A | Spur stummschalten |
| SELECT + B | **Rückgängig** (Spur leeren: SET → „Spur leeren“) |
| SELECT + X/Y | Engine der Spur wechseln (alternativ: Regler ENGINE auf der TRG-Seite) |
| SELECT + L1 / R1 | Step-Cursor einen Schlag (4 Steps) zurück/vor (FILL: LIVE-Seite oder SET) |
| SELECT + L2/R2 | Spurlänge (Polymetrie, 1 bis 128) |
| SELECT + START | Speichern (2 s halten: Speichern und Beenden) |

Im Header siehst du den **Akkustand**: grün beim Laden, gelb unter 30 %, rot und blinkend unter 10 %.

Das Projekt liegt in `projects/project1.gbx`. Gespeichert wird automatisch beim Beenden und alle 2 Minuten, wenn sich etwas geändert hat.

Am Desktop gilt diese Tastaturbelegung: Pfeiltasten, Z=A, X=B, S=X, A=Y, Q/W=L1/R1, 1/2=L2/R2, Enter=START, Shift/Backspace=SELECT, I/K=linker Stick, O/L=rechter Stick.

## Am PC: Ableton, MIDI-OX & Co. (PC-Modus)

Normalerweise ist der OTG-Port ein **Host**: Geräte werden *an* den R36S gesteckt. Ein PC ist ebenfalls ein Host, deshalb sieht er den R36S zunächst nicht.
Im **PC-Modus** schaltet der R36S den Port in den Geräte-Modus (USB-Gadget) und meldet sich am PC als **„Synth City“** an:

* **PC**: ein USB-MIDI-Gerät mit 1 Ein- und 1 Ausgang, ohne Treiber, unter Windows, macOS und Linux.
* **PC+A**: zusätzlich ein USB-Audio-Interface mit 2 Kanälen bei 48 kHz. Auf der SYS-Seite unter OUT dann das Gadget-Gerät wählen (z. B. „UAC2Gadget“), damit der Ton der Groovebox in Ableton als Eingang ankommt.

Einschalten:

* in der App: SYS-Seite, Regler **USB** auf PC oder PC+A, oder
* im Ports-Menü: **Groovebox - PC-Modus MIDI**, **Groovebox - PC-Modus MIDI+Audio** bzw. **Groovebox - PC-Modus AUS**.

Danach den R36S mit dem PC verbinden. Am zuverlässigsten ist ein **USB-A-auf-USB-C-Kabel**. Ein C-auf-C-Kabel schaltet die Stromversorgung manchmal nicht frei.
Während der PC-Modus aktiv ist, funktionieren Geräte am R36S (S-1, Keyboard, DAC) nicht. Nach einem Neustart ist der R36S wieder Host.

**In Ableton:** *Einstellungen → Link, Tempo & MIDI*. Beim Eingang „Synth City“ **Spur** aktivieren, damit die Noten der Groovebox in Ableton ankommen, z. B. mit N.OUT an.
Zum Synchronisieren gibt es zwei Wege:

* Groovebox führt: CLK auf SEND, in Ableton beim Eingang **Sync** an und oben links EXT aktivieren.
* Ableton führt: beim Ausgang **Sync** an, CLK auf RECV.

### OTG-Port freischalten (ArkOS auf dem R36S)

Im Device Tree von ArkOS ist der OTG-Anschluss fest auf Host gestellt (`otg-port { status = "disabled"; }`).
Der R36S kann dann nie als Gerät am PC erscheinen, die Diagnose zeigt „No support otg“ und „not attached“.

**Groovebox - OTG-Port freischalten** ersetzt die von `boot.ini` geladene `.dtb` durch eine Version, in der nur diese eine Zeile auf `okay` steht.
Das Original bleibt als `<name>.dtb.orig` erhalten. Geändert wird nur, wenn die Prüfsumme genau zur bekannten Originaldatei passt. Danach startet der R36S neu.

* Rückgängig: **Groovebox - OTG-Port Original**.
* Startet der R36S nicht mehr: SD-Karte in den PC, auf der Partition **BOOT** die Datei `rg351mp-kernel.dtb` löschen und `rg351mp-kernel.dtb.orig` in `rg351mp-kernel.dtb` umbenennen.
* Die Groovebox stellt den Port beim Start auf Host, damit S-1 & Co. weiter funktionieren. Im PC-Modus schaltet sie auf Gerät, und die 5 V am Port werden abgeschaltet.
* Am PC am besten ein USB-A-auf-USB-C-Kabel verwenden.

### Kernel nur mit Netzwerk-Gadget: Netzwerk-MIDI (RTP-MIDI)

Der Standard-ArkOS-Kernel des R36S hat nur Netzwerk-Bausteine (`rndis`, `ecm`, `ncm`), also weder USB-MIDI noch USB-Audio.
Der PC-Modus macht den R36S dann zu einer **USB-Netzwerkkarte**, und MIDI läuft darüber als **RTP-MIDI**.
Diesen Standard kann macOS von Haus aus, unter Windows übernimmt das der kostenlose Treiber **rtpMIDI**. Ableton sieht danach einen normalen MIDI-Port.
Audio zum PC ist mit diesem Kernel nicht möglich.

**Windows**

1. **rtpMIDI** (Tobias Erichsen) installieren.
2. Auf dem R36S auf der SYS-Seite **USB = PC** stellen. Die App zeigt die Adresse des R36S an: `IP 192.168.77.1`.
3. Den R36S per USB verbinden. Windows richtet „Remote NDIS“ automatisch ein und bekommt vom R36S die Adresse 192.168.77.10.
4. In rtpMIDI unter *My Sessions* mit **+** eine Session anlegen und aktivieren.
5. Unter *Directory* mit **+** einen Eintrag anlegen: Name „R36S“, Host = die angezeigte IP, Port **5004**. Den Eintrag wählen und **Connect** drücken.
6. Unter *Participants* erscheint „Synth City“. Auf der SYS-Seite steht bei MIDI „PC (NETZ-MIDI)“.
7. In Ableton heißt der Port wie die Session. Spur, Sync und Fernsteuerung wie gewohnt aktivieren.

**macOS:** Audio-MIDI-Setup → MIDI-Studio → Netzwerk. Dort eine Session anlegen und unter „Verzeichnis“ den R36S mit IP und Port 5004 hinzufügen.

Netzwerk-MIDI funktioniert übrigens auch über einen USB-WLAN-Stick, denn die Groovebox lauscht immer auf Port 5004/5005.

### Kernel ohne USB-MIDI-Gadget, aber mit Seriell: MIDI über USB-Seriell

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

## Groovebox-OS (eigenes System, Testphase)

Statt ArkOS kann der R36S direkt in die Groovebox starten. Das eigene System basiert auf einem aktuellen Linux-Kernel (mit den R36S-Anpassungen von ROCKNIX) und einem minimalen Debian.
Dadurch funktioniert der R36S am PC als **echtes USB-Audiointerface und USB-MIDI-Gerät** – ohne rtpMIDI, ohne Treiber.

**Wichtig: Eine ZWEITE microSD-Karte verwenden.** Die ArkOS-Karte bleibt unangetastet. Startet etwas nicht, einfach die alte Karte wieder einstecken.

1. Image `r36s-groovebox-os.img.xz` aus dem Release **os-nightly** laden.
2. Mit **balenaEtcher** oder dem **Raspberry Pi Imager** („Eigenes Image“) auf die zweite Karte schreiben (das .xz muss nicht entpackt werden).
3. Karte in den R36S (Steckplatz, aus dem ArkOS startet) und einschalten.
4. Beim ersten Start legt das System im freien Platz die Partition **GROOVEBOX** an (Samples, Projekte, Einstellungen, Logs) und fragt einmal die Tasten ab.

| Am PC (SET → USB) | Was der PC sieht |
|---|---|
| **PC (MIDI + Audio)** | Audiogerät „Synth City“ (Ein-/Ausgang, 44,1 kHz) + MIDI-Gerät „Synth City“ + Netzwerk (Browser-Upload, SSH) |
| **PC (MIDI)** | MIDI-Gerät + Netzwerk |
| SET → **USB-Laufwerk** | Die Partition GROOVEBOX als normales USB-Laufwerk: Samples reinkopieren, am PC auswerfen, am R36S A drücken |

* Der Klang geht im Modus **PC (MIDI + Audio)** automatisch **zusätzlich** zum PC (SET → „Audio an PC (USB)“, Standard AN). Lautsprecher/Kopfhörer laufen weiter und geben den Takt vor – liest der PC nicht mit, läuft der Sequencer trotzdem weiter. In SET steht, ob der PC gerade mitliest.
* **Lautstärken getrennt:** Die Lautstärketasten regeln nur Lautsprecher/Kopfhörer des R36S, die USB-Lautstärke steht in SET („USB-Lautstärke“). Mit „Intern bei USB-Audio: STUMM“ (Standard) sind Lautsprecher und Kopfhörer stumm, solange USB-Audio zum PC läuft.
* Ein Taktausgleich regelt die USB-Spur unhörbar nach (Uhren von R36S und PC laufen minimal unterschiedlich) – keine Knackser bei langen Aufnahmen. Im Log steht die Abweichung als „Takt … ppm“.
* **PC-Ton über den R36S:** Wählt man am PC „Synth City“ als Wiedergabegerät, klingt der PC-Ton aus Lautsprecher/Kopfhörer des R36S (SET → „PC-Ton über R36S“).
* **Sampling vom PC:** SET → „PC-Ton als Sample aufnehmen“ → A. Die Aufnahme startet mit dem ersten Ton, den der PC abspielt (max. 60 s), A beendet sie. Das Sample landet als `samples/pc/pc-NN.wav` und steht sofort zur Verfügung.
* SSH: `ssh ark@192.168.77.1`, Passwort `ark`.

### App aktualisieren ohne neues Image

Neue Groovebox-Versionen kommen als **`groovebox-os-update.tgz`** im Release **nightly**. Das Image muss dafür nicht neu geflasht werden. Drei Wege:

1. **Browser:** PC-Modus an, http://192.168.77.1 öffnen, unter „Groovebox aktualisieren“ die Datei wählen.
2. **Laufwerk:** Die Datei in den Ordner `update/` auf GROOVEBOX kopieren (SET → USB-Laufwerk oder Karte im PC). Beim nächsten Start wird sie installiert.
3. **Skript vom PC:** `tools\deploy.ps1` (Windows) bzw. `tools/deploy.sh` – erkennt das Groovebox-OS selbst und spielt das neueste Update ein.

Die Groovebox startet dafür kurz neu. Startet die neue Version dreimal nicht, wird automatisch die vorige wiederhergestellt. Protokoll: `logs/update.log`.

**Lautstärke:** Die Lautstärketasten am R36S regeln im Groovebox-OS Lautsprecher und Kopfhörer (Anzeige auf dem Display, wird gespeichert).

**Power-Taste:** kurz drücken schaltet das Display aus und wieder an (die Musik läuft weiter, spart Akku), **lang drücken (1,5 s)** speichert das Projekt und fährt den R36S sauber herunter.

**Bildschirm & Fernbedienung im Browser:** Unter http://192.168.77.1/screen erscheint das Live-Bild der Groovebox (bis ca. 30 Bilder/s, nur geänderte Bildteile werden übertragen). Bedienen per Maus/Touch über die eingeblendeten Tasten oder per Tastatur: Pfeile = Steuerkreuz, Z = A, X = B, S = X, A = Y, Q/W = L1/R1, 1/2 = L2/R2, Shift = SELECT, Enter = START, T/F/G/H = linker Stick, I/J/K/L = rechter Stick. Mit „Vollbild“ groß darstellen, z. B. für Aufnahmen oder Streams.

**Logs im Browser:** Auf der Upload-Seite (http://192.168.77.1) unter „Logs“ lassen sich groovebox.log, boot.txt, update.log und pc-modus.log direkt ansehen und herunterladen.

**Wenn etwas nicht klappt:** Die Karte in den PC stecken. Auf dem Laufwerk **GROOVEBOX** liegen im Ordner `logs/` die Dateien `boot.txt` (Display, Tasten, Sound, USB) und `groovebox.log`. Diese bitte schicken.
Bleibt das Display schwarz und die Partition GROOVEBOX fehlt, startet das System gar nicht – dann bitte melden, wie sich die LED verhält.
Auf der Partition **GBBOOT** lässt sich in `extlinux/extlinux.conf` testweise das ROCKNIX-Standard-Display wählen (siehe `LIESMICH.txt`).

Gebaut wird das Image automatisch von GitHub Actions (`.github/workflows/os.yml`, Skripte in `os/`).

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
3. Beim ersten Start fragt die App alle Tasten einmal ab, weil die R36S-Klone unterschiedlich belegt sind. Das Ergebnis landet in `groovebox/input.cfg`. Später kannst du die Tasten über **Groovebox - Tasten lernen** neu belegen. Ältere Belegungen kennen die Links/Rechts-Achsen der Sticks noch nicht, die App errät sie dann. Wenn Regler- oder Seitenwahl per Stick falsch herum oder gar nicht reagiert, einmal neu lernen.

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

## Aufspielen und Debuggen per Kabel (ohne SD-Karte)

Im PC-Modus ist der R36S per USB-Netzwerk unter **192.168.77.1** erreichbar. Ein eingebauter DHCP-Server gibt dem PC sofort `192.168.77.10`.
SSH startet die Groovebox selbst, „Enable Remote Services“ in ArkOS (das WLAN verlangt) brauchst du nicht. Zugang: `ark` / `ark`.

**Windows** (PowerShell, im Ordner `tools`):

```powershell
.\deploy.ps1 -SetupKey    # einmalig: Schluessel hinterlegen, danach ohne Passwort
.\deploy.ps1              # neueste Version von GitHub (nightly) aufspielen und Log anzeigen
.\deploy.ps1 -Zip .\r36s-groovebox-arkos.zip   # ein bestimmtes Paket aufspielen
.\deploy.ps1 -Log         # nur das Log live anzeigen
.\deploy.ps1 -Shell       # SSH-Konsole
```

**macOS/Linux:** `tools/deploy.sh` (gleiche Funktionen, `--setup-key`, `--log`, `--shell`).

Beim Aufspielen bleiben Projekte, Samples, Tastenbelegung und Einstellungen erhalten. Ersetzt werden nur Programm und Skripte.

## Tests und Leistungstest

* **Auf dem R36S:** SET → **Leistungstest** (ca. 30 s). Gemessen werden die Audio-Last je Engine (8 Stimmen, auch pro Stimme) und ein Worst Case mit allem an, die Zeichenzeit jeder Seite inkl. Ausgabe aufs Display und der Speicher. Den Bericht findest du im Browser unter **Logs → bench.txt** (Datei `/data/logs/bench.txt`). Die Wiedergabe läuft währenddessen weiter.
* Am PC: `./groovebox --bench [datei]` (gleicher Bericht), `make test` (Unit-Tests: Timing, Polymetrie, Bedingungen, Euklid, Trigless, Song, Arp, Stabilität aller Engines × Filtertypen, Speichern/Laden, Migration), `./groovebox --selftest [sek]` (UI-Szenarien mit simulierten Tasten, Bedienweg-Report, UI-Monkey mit 20 000 Zufallsbildern, Engine-Stresstest). Mit `SAN=1` laufen Tests und App mit AddressSanitizer/UBSan.
* GitHub Actions führt bei jedem Push alle Tests mit Sanitizern aus und hängt `bench-arm64.txt` (Messung auf einem ARM-Server) ans Release.

## Automatischer Build (GitHub Actions)

Jeder Push auf `main` baut das Programm für den R36S und legt das Paket als Release **nightly** ab. Genau das holt `deploy.ps1`.
Ein Tag wie `v1.0` erzeugt ein festes Versions-Release.

## Selbst bauen

```bash
# Desktop (zum Testen)
sudo apt install libsdl2-dev libasound2-dev && make && ./groovebox --windowed

# Cross-Build für den R36S (auf x86-Linux) und Paket
sudo apt install g++-aarch64-linux-gnu curl zip && ./tools/cross-build.sh && ./tools/package.sh
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
