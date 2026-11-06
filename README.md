# SYNTH CITY – Groovebox für den R36S

Groovebox für die Hosentasche: Step-Sequencer mit Parameter-Locks, 13 Klangerzeuger und USB-Audio-Interface auf dem Handheld R36S (eigenes SYNTH CITY OS oder ArkOS 2.0; läuft auch auf dem Linux-Desktop).
Neo-Noir-Optik in Neon-Magenta und Cyan; beim Start erscheint ein Splashscreen mit Synthwave-Skyline (beliebige Taste überspringt).
8 Spuren, 16 Patterns mit bis zu 128 Steps pro Spur, Parameter-Locks, Akkorde, Sample-Spuren, LIVE-Seite mit WAV-Aufnahme, USB-Audio, USB-MIDI, Ping-Pong-Delay, Reverb und ein DJ-Filter. Technische Daten: [docs/SPECS.md](docs/SPECS.md).
Sie startet als App aus dem Ports-Menü oder auf Wunsch direkt beim Booten statt EmulationStation.

## Schnellstart

* Beim **ersten Start** ist der Demo-Song **„Neon Nights“** geladen: **START** spielt ihn ab (Song-Modus, 4 Patterns). Jederzeit wieder: SET → „Demo-Song laden“.
* **Bedienen geht komplett mit Steuerkreuz und Tasten** – die Sticks sind nur Abkürzungen. Überall gilt: **L2/R2 = Seite**, **SELECT antippen = Seitenkarte**, **START = Play/Stop** (beim Spielen halten = **FILL**). Auf Spurseiten: **L1/R1 = Regler**, **X/Y = Wert**.
* **SELECT antippen** öffnet die **Seitenkarte** mit allen Seiten (Signalweg der Spur, LIVE/Mix/FX, Song/Projekt/Einstellungen). **SELECT halten** zeigt die Tastenhilfe.
* Unten im Bedienfeld steht zu jedem Regler eine kurze Erklärung.
* Fertige Klänge: Seite PRJ → **PRESET** über Platz 32 hinaus drehen → **Werksklänge** (z. B. „NIGHT DRIVE BASS“, „PUNCH KICK“, „HAZE PAD“) → **P.LADEN**. Ein Werksklang setzt auch die Engine.
* **Rave & Acid (Werksklänge):** „303 ACID“, „303 SQUARE ACID“, „ACID SCREAMER“, „ACID DEEP“ (Acid-Bässe im 303-Stil), „REESE BASS“, „REESE GROWL“ (Jungle/DnB), „HOOVER“, „HOOVER LEAD“, „RAVE STAB“ (Moll-Akkord auf einer Taste), „DUB CHORD“, „SUPERSAW“, „TRANCE PLUCK“, „WOBBLE BASS“, „JUNGLE SUB“, „HOUSE ORGAN“ (FM).
  * **Acid spielen wie auf der 303:** Die Acid-Presets haben eine kurze Notenlänge (LEN 0.71). **Slide** = einen Trig mit LEN über 1 Step locken – die Note läuft dann zur nächsten hin (GLID bestimmt die Gleitzeit). **Akzent** = höhere Anschlagstärke (VEL-Lock): VELF öffnet dabei das Filter weiter. Am meisten passiert mit FREQ, RESO, ENV und F.DEC – am besten per LFO oder P-Lock bewegen.
* Vertan? **SELECT + B** macht die letzte Änderung rückgängig (12 Schritte), SET → „Wiederholen“ stellt sie wieder her.

## Oberfläche: Signalweg

**Animationen:** Leere Flächen zeigen das echte Audiosignal: Oszilloskop, Spektrum und Takt-Ringe in freien Kacheln, eine Piano-Roll auf TRIG, die Oszillatoren auf der OSC-Seite, fließende Partikel zu den Effekt-Bussen auf SENDS, und je Effekt-Bus eine eigene Animation (Delay-Echos, Tonband-Spulen, Hall-Ringe, schwingende Feder, Chorus-Stimmen). Auch Einstellungen, Song, LIVE und Projekt haben passende Bilder. Jede Regler-Kachel zeigt oben Name und Wert und darunter ein bewegtes Bild des Werts (Hüllkurve, Welle, Oktav-Treppe, Akkord, Arp-Folge, Lautsprecher, Hall-Ringe …).

Auf den Spurseiten zeigt die Oberfläche den Weg des Klangs: links alle 8 Spuren mit Mini-Steps und Pegel, oben die Blöcke **QUELLE → FILTER → AMP → INSERT** (zusammengehörige Seiten untereinander: QUELLE/QUELLE 2, FILTER 1/FILTER 2), darunter die Reihe **SEQ**: TRIG · SEQ · ARP · LFO1–3. **L2/R2** gehen alle Spurseiten der Reihe nach durch (TRIG → SEQ → ARP → QUELLE → QUELLE 2 → FILTER 1 → FILTER 2 → AMP → INSERT → LFO 1–3, im Kreis), direkt springen über die Seitenkarte (SELECT). **L1/R1** wählen den Regler. Der gewählte Block zeigt Grafik, 8 Regler und eine Erklärung des gewählten Reglers, unten liegen die 16 Steps der Spur.

* **SELECT + L1/R1** springt einen Schlag (4 Steps) – vier Kicks auf 1/5/9/13 sind so in wenigen Tastendrücken gesetzt.
* **Rhythmus-Vorlagen** für die gewählte Spur: SET → „Rhythmus-Vorlage“ (Four on the Floor, Backbeat, Offbeat, Achtel, Sechzehntel, Breakbeat, Clave, Tresillo, Dembow …), mit ←/→ wählen, A setzt sie.
* **Hüllkurvenform:** Seite FILTER 2 + KURVEN, Regler **F.KRV** (Filter) und **A.KRV** (Lautstärke): Minus = weich/rund, 0 = Standard, Plus = knackig/schnell abfallend. Die Grafiken zeigen die Form.

## LIVE-Seite (Menü → LIVE)

Die 8 Spuren liegen auf dem Bildschirm genau so wie die Tasten: **Steuerkreuz ↑ → ↓ ←** = Spur 1–4, **X A B Y** = Spur 5–8 – jeder Druck schaltet die Spur stumm oder wieder an.

| Taste | Funktion |
|---|---|
| L1 halten | FILL (Trig-Bedingungen FILL) – auf allen anderen Seiten: START beim Spielen halten |
| R1 | **WAV-Aufnahme** der Summe starten/stoppen (max. 5 Min.), gespeichert als `samples/rec/rec-NNN.wav` (Stereo, 24 Bit) – im Browser unter Samples herunterladen |
| L2 / R2 | Seite (wie überall); Pattern zurück/vor: SELECT + ←/→ |
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
| SYNTH | WAV1, WAV2 (SAW/SQR/TRI/SIN), DETN, MIX, SUB, NOIS, PW, GLID. Seite **OSC**: OCT1, OCT2 (±3 Oktaven), LVL1, LVL2 (Pegel je Oszillator), SUB.O (Sub −1/−2 Oktaven), SYNC (Hard-Sync OSC2 → OSC1), XMOD (OSC2 moduliert OSC1), RING (Ringmodulation) |
| FM    | **4 Operatoren, 8 Algorithmen.** Seite FM: ALGO, FBK (Feedback OP4), VELO (Anschlag → Modulation), PITCH/P.DEC (Tonhöhen-Hüllkurve ±24 Halbtöne), NOIS. Je Operator eine eigene Seite **OPERATOR 1–4**: RAT (0,25–16), FINE (±0,5 für unharmonische Glocken), DETN (±50 Cent), O.LVL (Träger: Lautstärke, Modulator: Helligkeit), O.ATK/O.DEC/O.SUS/O.REL (eigene Hüllkurve) |
| SMPL  | SMPL (Sample-Auswahl), TUNE (±24 Halbtöne), FINE, STRT, END (END < STRT = rückwärts), LOOP, GAIN |
| TOM   | TUNE, DCAY, SWEP (Pitch-Abfall), NOIS (Anschlag), BODY |
| RIM   | TUNE, DCAY, TONE, CLCK |
| SHKR  | TONE, DCAY, ATK (Ausholen), GRIT (Körnigkeit) |
| WTBL  | **Wavetable**: TABL (BASIC Sinus→Dreieck→Säge→Rechteck, PWM, VOKAL A→U, OBERTÖNE, **USER** = eigene), POS (Position in der Tabelle, auch als LFO-Ziel), WARP (Phasenverbiegung), DETN (Unisono), SUB, NOIS, P.ENV (Filterhüllkurve → Position), GLID. Seite **OSC**: OCT, LVL, SUB.O, **UWAV** (eigene Wavetable aus einer WAV-Datei, X = Explorer), **DRAW** (X = Wavetable selbst zeichnen) |
| CHOP  | **Sample-Chopper**: SMPL, SLCS (2–64 Scheiben), SLIC (AUTO = Scheibe nach Step-Nummer, oder feste Scheibe, pro Step lockbar), TUNE, FINE, LEN (Anteil der Scheibe), REV, GAIN |
| GRAN  | **Granulator**: SMPL, POS, SPRY (Streuung), SIZE (10–500 ms), DENS (2–128 Körner/s), PTCH, P.SPR (Tonhöhen-Streuung), GAIN |

**Klangcharakter:** KICK, SNARE, HAT, CLAP und RIM sind an die klassischen analogen Drumcomputer angelehnt (Kick: Sinus mit schnellem Pitch-Sweep und Sättigung über BODY, Klick über CLCK; Snare: zwei verstimmte Töne mit eigenem Pitch-Abfall plus gefiltertes Rauschen, TONE = Helligkeit; Hihat: sechs Rechteck-Oszillatoren durch Bandpass + Hochpass; Clap: sägezahnförmige Einzelschläge vor der Fahne).
SYNTH und WTBL starten mit dem **24-dB-Tiefpass** (LP24) mit sanfter Sättigung; die Oszillatoren driften leicht wie bei analogen Geräten. Weitere Filtertypen (Ladder, Kamm, Vokal …) siehe FLT.

Die Engine wählst du pro Spur und Pattern: auf der TRG-Seite mit dem Regler ENGINE oder mit SELECT + X/Y. **X** auf dem Regler ENGINE öffnet eine **Liste aller Engines** (mit Symbol und Kurzbeschreibung): ↑/↓ wählt, A übernimmt, SELECT bricht ab. Y und der rechte Stick blättern weiter direkt.
Beim Drehen erscheint der **Engine-Wähler** mit Symbolen für alle 13 Engines.

**Eigene Wavetables:** Eine WAV-Datei mit Frames zu **2048 Werten** (das übliche Format vieler Wavetable-Synths) oder 256 Werten wird als Tabelle gelesen; POS fährt durch die Frames. Andere Längen gelten als ein einzelner Zyklus. Am besten in `samples/wavetables/` ablegen – der Explorer startet dort. TABL springt beim Wählen automatisch auf USER.

**Wavetable zeichnen** (WTBL → Seite OSC → Regler DRAW → **X**): vier Wellen W1–W4 mit je 64 Punkten; POS blendet von W1 (0) über W2 und W3 nach W4 (127). Die Änderungen sind sofort hörbar.

| Taste | Wirkung |
|---|---|
| ←/→ | Punkt wählen (linker Stick: schnell fahren) |
| ↑/↓ | Wert des Punktes; **rechter Stick setzt den Wert direkt** – mit beiden Sticks wird gezeichnet |
| L1/R1 | Welle W1–W4 |
| A | Werkzeuge: Sinus, Dreieck, Säge, Rechteck, Puls, Rauschen, Glätten, Normalisieren, Umkehren, Leeren, von voriger Welle, auf alle Wellen |
| X | vorhören |
| START | speichern als `samples/wavetables/draw-NN.wav` (16 Frames × 2048) und der Spur zuweisen |
| B | abbrechen (vorige Tabelle bleibt) |

Jede Spur spielt eine Stimme; SYNTH, FM, WTBL und die Sample-Engines spielen im ARP-Modus **AKKD** bis zu 4 Akkordtöne gleichzeitig.
Die Engine gilt pro Spur und Pattern. Wechselst du sie, werden nur die SRC-Regler auf die Grundwerte der neuen Engine gesetzt.

Jede Spur hat diese Seiten:

* **TRG**: NOTE, VEL, LEN, PROB (Wahrscheinlichkeit), RTRG (Retrigs pro Step), **MTIM** (Mikrotiming), ENGINE, **COND** (Trig-Bedingung, siehe unten)
* **SEQ**: Laufrichtung, Länge, Tempo-Faktor, Euklid und Generator (siehe unten)
* **SRC** (dazu **OSC** bei SYNTH/WTBL, **OPERATOR 1–4** bei FM): Regler der Engine (siehe Tabelle). Grafisch dargestellt werden:
  * bei SYNTH die tatsächlich klingende, gemischte Wellenform
  * bei FM das Algorithmus-Diagramm, in dem die Operatoren beim Spielen aufleuchten; auf den Operator-Seiten dessen Hüllkurve mit Ratio, Verstimmung und Rolle (Träger hörbar / moduliert OP n)
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
* **AMP**: ADSR (als Hüllkurve mit Live-Punkt dargestellt, Zeiten in ms/s), Lautstärke, Panorama
* **SND** (Sends): **SND.A, SND.B, SND.C** – Anteil der Spur an den Effekt-Bussen A, B, C (bisher DLY/REV auf der AMP-Seite)
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

* **FX A / FX B / FX C**: drei **Effekt-Busse** mit frei wählbarem Effekt (Regler TYPE; ein Wechsel setzt die Regler auf die Grundwerte des Effekts). Jeder Bus hat **FWD** (schickt seinen Ausgang zusätzlich in den nächsten Bus, z. B. Delay in den Hall) und **LVL** (Pegel in der Summe). Oben zeigt die Seite den Weg Spuren → Bus → Summe, den Pegel und eine Erklärung des gewählten Reglers. Grundeinstellung: A = Delay, B = Reverb, C = Chorus.

  | TYPE | Regler |
  |---|---|
  | DELAY | TIME (1/64 bis 1/1 im Songtempo, triolisch/punktiert; Wechsel gleiten), **FBK bis 120 %** (kippt in eine selbsterhaltende Schleife, Sättigung hält sie im Rahmen), TONE, PING (0 = mono, 127 = Ping-Pong), LOCUT |
  | TAPE | Bandecho: TIME, FBK, **WOW** (Gleichlaufschwankungen), **SAT** (Bandsättigung), **AGE** (dunkler, dünner, leichtes Rauschen); zwei Tonköpfe für Stereo |
  | REVERB | Hall mit 8 Leitungen (FDN) und Diffusion: SIZE (0,25 bis 11 s), DAMP, PRE (Predelay bis 250 ms), WIDE, MOD (Bewegung im Schweif) |
  | SPRING | Federhall: DECAY, TONE, **DRIP** (typisches Zirpen), WIDE (zwei Federn), LOCUT |
  | CHORUS | RATE, DEPTH, VOICE (1–3 Stimmen, Ensemble), WIDE, FBK |

* **CMP**: Master-Kompressor mit **Sidechain** (siehe unten)
* **MIX**: Mixer mit Mute/Solo (siehe unten)
* **PRJ**: Projekte und Presets (siehe unten)
* **MST**: BPM, Swing, Gesamtlautstärke, DRIVE (Sättigung der Summe) die globale Tonskala SCALE und ihr Grundton ROOT
* **SET**: Einstellungen als Liste (siehe unten, wird in `settings.cfg` gespeichert)

## Steps, Seiten und Geschwindigkeit

* Jede Spur hat **1 bis 128 Steps**. Die Länge stellst du auf der Seite SEQ (LEN) oder in SET → SPUR ein.
* Angezeigt werden immer 16 Steps. Die Leiste über den Step-Tasten zeigt die bis zu 8 Seiten: Die Cursor-Seite ist hell, die laufende Seite in Spurfarbe.
* Das Steuerkreuz läuft durch alle Steps. **SELECT + L2/R2 springt eine Step-Seite** (16 Steps), SELECT + L1/R1 einen Schlag (4 Steps).
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
| **Trig** (mit allen Locks, auch trigless) | Step mit **A halten** (oder im Lock-Modus) **+ SELECT + L1** | Ziel-Step **A halten + SELECT + R1** – geht auch auf anderen Spuren; im Lock-Modus mit ←/→ weiterspringen und mehrfach einfügen |
| **Seite** (die 8 Werte einer Spurseite) | Seitenkarte (SELECT antippen), Seite markieren, **X** | Zur Ziel-Spur wechseln, Seitenkarte, gleiche Seite markieren, **Y** |
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

## Song-Modus (SNG-Seite, Gruppe PROJEKT): Clip-Matrix

Der Song ist eine **Matrix wie in einer Clip-Ansicht**: jede **Zeile** ist eine Szene (bis 64), jede **Spalte** eine Spur. In jeder Zelle steht, **welches Pattern diese Spur** in der Zeile spielt – Spur 1 kann also die Drums aus Pattern 1 spielen, während der Bass aus Pattern 4 kommt. Klang und Steps einer Spur kommen jeweils aus ihrem Pattern.

* **Pattern-Mutes:** Stummschalten (Spurtasten auf LIVE, MIX, SELECT + A) gehört zum **Pattern** und wird mit ihm gespeichert. Wechselst du das Pattern, gelten dessen Mutes.
* **Song-Mutes:** Jede Zeile hat eigene Mutes. Eine neue Zeile und jede Zelle, deren Pattern du änderst, **übernimmt die Mutes des Patterns**; danach kannst du sie in der Zeile frei ändern (das Pattern bleibt unverändert). Läuft der Song, wirkt Stummschalten auf die laufende Zeile.
* Spalten: **ALLE** (alle Spuren gemeinsam), **1–8** (je Spur), **WDH** (Wiederholungen).

| Taste | Wirkung |
|---|---|
| ↑/↓, ←/→ | Zelle wählen |
| X / Y | Pattern der Zelle +/−; unter 01 wird die Zelle **leer** („--“, Spur schweigt). In ALLE: alle Spuren auf ein Pattern (bei „MIX“ vereinheitlicht der erste Druck). In WDH: Wiederholungen 1–16 |
| A | Zelle stumm/an (rotes M). In ALLE: Mutes wieder von den Patterns übernehmen |
| L1 | neue Zeile unter der gewählten (Kopie; im leeren Song: aktuelles Pattern mit seinen Mutes) |
| B | Zeile löschen |
| R1 | Song-Modus an/aus |
| START | Song ab der gewählten Zeile spielen / stoppen |

* Die Länge einer Zeile richtet sich nach der längsten belegten Spur. Spuren, deren Pattern in der nächsten Zeile gleich bleibt, laufen ohne Neustart weiter.
* Im Song-Modus zeigen die Spurseiten das Pattern, das die gewählte Spur gerade spielt (Kopf „P04“). Bearbeiten wirkt auf dieses Pattern.
* Ein Pattern-Wechsel (SELECT + ←/→, SET) beendet den Song-Modus.
* Im Kopf steht „SNG2/5“; START auf anderen Seiten setzt den Song fort. Seite wechseln wie überall mit L2/R2.

**Song exportieren (WAV):** Unter der Matrix liegt die Leiste **EXPORT WAV** (in der letzten Zeile ↓ drücken). Mit ←/→ das Feld wählen, mit X/Y **VON** (Anfangszeile) und **BIS** (Schlusszeile) einstellen – Standard ist der ganze Song („ENDE“). Die gewählten Zeilen sind links mit einem cyanfarbenen Strich markiert, die Länge steht daneben. **A** startet den Export: Die Groovebox rechnet die Zeilen samt Wiederholungen offline (schneller als Echtzeit, die Wiedergabe stoppt dafür), lässt Hall und Delay ausklingen und speichert `samples/rec/<Projekt>-song-NN.wav` (Stereo, 44,1 kHz, 24 Bit). B bricht ab. Herunterladen im Browser unter Samples → rec.
* Projekte ab Version 12 speichern Matrix und Mutes; ältere Songs (Pattern-Kette) werden beim Laden in Zeilen mit demselben Pattern auf allen Spuren umgestellt.

**Projektformat v13** (16 Seiten je Spur): Ältere Projekte und Sounds werden beim Laden umgestellt – Delay und Reverb landen auf Bus A und B mit den bisherigen Werten, die Sends DLY/REV werden zu SND.A/SND.B, FM-Sounds bekommen ihre Operatoren mit gleichem Klang (Ratio, Pegel, Abklingzeit; FINE wird zu DETN), Locks und LFO-Ziele ziehen mit. Gespeichert wird danach im neuen Format; ältere Versionen der Groovebox können es nicht mehr lesen.

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
| `FILL` / `!FILL` | nur bei eingeschaltetem bzw. ausgeschaltetem FILL-Modus (**START beim Spielen halten** auf jeder Seite, LIVE: L1 halten, dauerhaft: SET → FILL; im Header blinkt FILL) |
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

Über die Seitenkarte (SELECT → Zeile PROJEKT → EINSTELLUNGEN) oder L2/R2 auf den Projektseiten. Links stehen die **Bereiche**, rechts deren Einträge, unten eine kurze Erklärung zum gewählten Eintrag:

* **PATTERN:** Pattern wählen, Tempo, Swing, Tonskala, Grundton, Länge aller Spuren, FILL dauerhaft, Pattern kopieren/einfügen/leeren/mutieren, Rückgängig/Wiederholen, Demo-Song
* **SPUR n:** Länge und Tempo-Faktor der gewählten Spur, Rhythmus-Vorlagen, Spur leeren
* **AUDIO:** Ausgang (interner Lautsprecher, USB-Interface …), Puffer mit Latenz in ms, Lautstärke, Rechenkerne
* **MIDI:** Eingang, Noten ausgeben, Clock (senden/empfangen), Eingangskanäle
* **USB & PC:** HOST oder PC-Modus, Audio an den PC, PC-Ton, Testton, Adresse
* **SAMPLES:** Bibliothek/Projekt/fehlende Samples, Browser-Adresse, USB-Laufwerk, alte Sample-Nummern
* **SYSTEM:** **Sprache / Language**, Bild-Versatz, Tasten neu lernen, Leistungstest, Info, Beenden

Zwei Ebenen wie ein Menü: Zuerst ist links ein **Bereich** gewählt (↑/↓ wechselt den Bereich). **A** (oder →) öffnet ihn, dann wählt ↑/↓ den Eintrag, ←/→ (oder X/Y) ändert den Wert und **A** führt Aktionen aus. **B** geht zurück zu den Bereichen, ein weiteres **B** verlässt SET zur vorherigen Seite. L1/R1 springen jederzeit direkt zum vorigen/nächsten Bereich.

## Mixer (MIX-Seite)

Hier siehst du alle 8 Spuren als Kanalzüge mit Fader, Pegelanzeige, Panorama, Delay- und Reverb-Send.

* Steuerkreuz ←/→ (oder linker Stick) wählt den Kanal.
* Steuerkreuz ↑/↓ oder L1/R1 wählt die Zeile (VOL, PAN, S.A, S.B, S.C = Sends an die Effekt-Busse).
* Rechter Stick ↑/↓ oder X/Y ändert den Wert.
* **A = Mute, B = Solo.** Sind mehrere Spuren auf Solo, klingen alle Solo-Spuren.

## Projekte und Sounds (PRJ-Seite)

Projekte und Sounds haben **freie Namen**. Die PRJ-Seite hat acht Schaltflächen (mit **A** auslösen):

| Schaltfläche | Funktion |
|---|---|
| **LADEN…** | öffnet den Projekt-Explorer: alle Projekte alphabetisch, das aktive ist markiert. A lädt (bei ungespeicherten Änderungen mit Rückfrage) |
| **SPEICHERN** | speichert unter dem aktuellen Namen (steht unter der Schaltfläche). Das automatische Speichern alle 2 Minuten nutzt denselben Namen |
| **SPEICH. ALS…** | Bildschirm-Tastatur: neuen Namen eingeben, START speichert (vorhandene Namen mit Rückfrage) |
| **NEU…** | Name eingeben, dann leeres Projekt anlegen |
| **SOUND LADEN** | Sound-Explorer: Ordner **Werksklänge** und je Engine ein Ordner mit deinen Sounds |
| **SOUND SPEICH.** | Klang der Spur (Engine und alle Seiten außer den Steps) unter einem Namen speichern |
| **INIT SOUND** | Klang der Spur auf die Grundwerte der Engine zurücksetzen |
| **DEMO-SONG** | „Neon Nights“ als neues Projekt anlegen |

**Bildschirm-Tastatur:** Steuerkreuz wählt die Taste, **A** tippt, **B** löscht, **X** Leerzeichen, **Y** Groß/klein, **START** = OK, **SELECT** = Abbruch. Erlaubt sind Buchstaben, Ziffern, Leerzeichen und `- _ . ( ) +`, bis 32 Zeichen.

Gespeichert wird in `projects/<Name>.gbx` und `presets/<ENGINE>/<Name>.gbp`. Ältere Projekte (`song01` …) und Presets (`01` …) erscheinen unter ihren bisherigen Namen.

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

WAV-Dateien kommen in den Ordner `samples/`, gern mit **Unterordnern bis 8 Ebenen tief**, z. B. `samples/drums/808/kick.wav`.
Erlaubt sind 8/16/24/32-Bit-PCM und 32-Bit-Float in jeder Samplerate. Stereo wird zu Mono gemischt. **Nur WAV:** MP3, AIFF, FLAC, OGG usw. werden nicht gelesen (SET → „Samples“ zeigt, wie viele solche Dateien im Ordner liegen). Der Ordner `samples/rec/` (WAV-Aufnahmen) erscheint nicht im Explorer.

**Bibliothek und Projekt-Pool:** Die Groovebox kennt alle WAV-Dateien im Ordner (die *Bibliothek*, bis 8192 Dateien), lädt aber nur die Samples in den Speicher, die das aktuelle Projekt benutzt (bis 253 pro Projekt, je höchstens 60 s). Das Projekt speichert die Samples **per Name** (z. B. `drums/808/kick`). Neue Dateien, Unterordner oder Umsortieren verändern ein Projekt deshalb nicht mehr. Fehlt beim Laden eine Datei, meldet die Groovebox „N SAMPLES FEHLEN“; die Namen stehen im Log. Auch gespeicherte Sounds (Presets) merken sich ihr Sample per Name.
Ältere Projekte und Sounds (gespeichert mit Sample-Nummern) werden beim Laden über die frühere Reihenfolge zugeordnet und beim nächsten Speichern auf Namen umgestellt.

SET → **Samples** zeigt: Dateien in der Bibliothek, Samples im Projekt, fehlende Samples und Dateien in anderen Formaten.

**Auswahllisten:** Wie beim Sample gibt es auch für das **LFO-Ziel** (Regler DEST auf LF1–LF3) eine Liste: **X** öffnet sie, die Wurzel zeigt „--- (kein Ziel)“ und einen Ordner je Seite (SRC, FLT, AMP …), darin die Parameter mit kurzer Erklärung. Sie startet beim aktuellen Ziel. Im Lock-Modus wird das Ziel als Lock gesetzt.

**Sample-Explorer:** Auf dem Regler **SMPL** (Seite QUELLE von SMPL, CHOP, GRAN) öffnet **X** den Explorer mit allen Ordnern und Unterordnern (bis 8 Ebenen). Er startet im Ordner des aktuellen Samples. Steuerkreuz ↑/↓ wählt, **A** oder → öffnet einen Ordner bzw. übernimmt das Sample, **X** hört vorab hinein, **B** oder ← geht einen Ordner zurück, L1/R1 blättert seitenweise, SELECT bricht ab (das vorher eingestellte Sample bleibt). Im Lock-Modus (A lang halten) wird das gewählte Sample als **Lock auf den Step** gesetzt. Der rechte Stick blättert direkt durch die Samples des Projekts; „(leer)“ heißt, auf diesem Platz liegt kein Sample.

### Samples per Browser hochladen (ohne SD-Karte)

1. R36S per USB-Kabel mit dem PC verbinden und **PC-Modus** einschalten (SET → USB = PC).
2. Im Browser **http://192.168.77.1** öffnen. Die Adresse steht auch in SET unter „Samples per Browser“.
3. Der **Sample-Explorer** zeigt einen Ordner nach dem anderen (Pfadleiste oben, Klick auf einen Ordner öffnet ihn, „..“ geht zurück). Dateien **oder ganze Ordner** auf die Liste ziehen lädt sie in den angezeigten Ordner hoch, Unterordner bleiben erhalten (auch über „Ordner hochladen“). **WAV, MP3, FLAC, AIFF, OGG …** wandelt der Browser automatisch in Mono-WAV mit 44,1 kHz um (abschaltbar, optional normalisieren).
4. Etwa 1,5 Sekunden nach dem letzten Upload liest die Groovebox die Bibliothek neu, ohne Aussetzer. Spuren behalten ihr Sample; geänderte Dateien im Projekt werden neu geladen.

Im Explorer:
* **Suche** über alle Ordner (Teil des Namens oder Ordners, bis 400 Treffer)
* je Sample: ▶ anhören, **i** Infos (Format, Kanäle, Rate, Bits, Länge), ✎ umbenennen/verschieben (Pfad mit „/“), ⤓ herunterladen, ✕ löschen
* **Mehrfachauswahl** (Häkchen): gemeinsam verschieben oder löschen
* **+ Ordner** legt einen Unterordner an; Ordner lassen sich umbenennen/verschieben und samt Inhalt löschen (mit Rückfrage)
* Ordnernamen vom USB-Stick mit Umlauten oder Sonderzeichen (&, Komma …) gehen; nur Zeichen, die FAT nicht kennt (`\ : * ? " < > |`), werden beim Hochladen ersetzt
* Achtung: Projekte merken sich Samples per Name – umbenannte oder verschobene Samples dort neu wählen

**Projekte** im Browser: hochladen mit beliebigem Namen (Name = Dateiname, bei gleichem Namen mit Rückfrage), herunterladen (Sicherung), umbenennen, löschen.

Ein Start-Paket mit 13 selbst erzeugten Samples liegt bei: Drums, Keys, eine Vokal-Fläche und FX.
Es wird mit `tools/gensamples.py` erzeugt.

* Mit SELECT + X/Y stellst du eine Spur auf **SMPL** um.
* Den Sample-Regler auf der SRC-Seite kannst du pro Step locken. So spielt eine einzige Spur verschiedene Samples.
* NOTE C-4 ist die Originaltonhöhe.
* Ohne LOOP läuft ein Sample als One-Shot bis zum Ende. Mit LOOP gilt die Trig-Länge (LEN) bzw. die Haltedauer der MIDI-Taste.
* Das Projekt merkt sich die Samples per **Name**. Dateien darfst du beliebig hinzufügen und Ordner anlegen. Benennst du eine benutzte Datei um oder verschiebst sie, findet das Projekt sie nicht mehr – dann im Explorer neu wählen.

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

**Grundidee:** Alles geht mit Steuerkreuz und Tasten. Die Sticks sind Abkürzungen (links Regler wählen, rechts Wert stufenlos). Auf jeder Seite gilt: **L2/R2 = Seite**, **SELECT antippen = Seitenkarte**, **START = Play/Stop**.

| Taste | Funktion |
|---|---|
| Steuerkreuz ↑/↓ | Spur wählen (Mixer: Zeile, Effekt-/Kompressor-/Master-Seite: Regler) |
| Steuerkreuz ←/→ | Step-Cursor (Mixer: Kanal, Effekt-/Kompressor-/Master-Seite: Regler) |
| A | Trig setzen oder löschen |
| **A lang halten** (ca. ½ s) | **Lock-Modus** rastet ein: A loslassen, mit X/Y (oder rechtem Stick) Locks setzen, **L1/R1 Regler und L2/R2 Seite wechseln weiterhin** (z. B. Filter und Sample auf demselben Step locken), ←/→ springt zum nächsten Step, B löscht die Locks des Steps, A antippen = fertig |
| A halten + X/Y | Parameter-Lock auf diesen Step (gelb) |
| A halten + B | alle Locks des Steps löschen |
| B | Spur vorhören |
| **L1 / R1** | **Regler wählen** (1–8, leere Felder werden übersprungen) |
| **X / Y** | **Wert + / −** (halten = Wiederholung, bei großen Bereichen schneller) |
| **L2 / R2** | **Seite zurück / vor** – Spurseiten im Kreis TRIG → SEQ → ARP → QUELLE → QUELLE 2 → FILTER 1 → FILTER 2 → AMP → INSERT → LFO 1–3; LIVE/Mix/FX und Projekt-Seiten jeweils unter sich |
| **SELECT kurz antippen** | **Seitenkarte**: alle Seiten auf einen Blick – Sequenz, Klang-Signalweg mit Pfeilen, Modulation, LIVE/Mix/FX, Projekt – mit Kurzwerten und einer Erklärung. Steuerkreuz wählt, A öffnet, X/Y kopiert/fügt die 8 Werte einer Spurseite ein |
| SELECT halten | Hilfe-Einblendung |
| START | Play / Stop. **Beim Spielen halten = FILL**, solange gedrückt |
| Linker Stick | Regler wählen (je Antippen ein Feld) |
| Rechter Stick ↑/↓ | Wert stufenlos ändern |
| SELECT + rechter Stick | ←/→ Seite, ↑/↓ DJ-Filter (oben Hochpass, unten Tiefpass) |
| SELECT + ←/→ | Pattern wechseln (beim Abspielen am Pattern-Ende; beendet den Song-Modus) |
| SELECT + ↑ | Aufnahme an/aus (für MIDI-Eingabe) |
| SELECT + ↓ | Tap-Tempo (mehrmals im Takt tippen) |
| SELECT + A | Spur stummschalten (im Lock-Modus: Trig ↔ trigless) |
| SELECT + B | **Rückgängig** (Spur leeren: SET → SPUR → „Spur leeren“) |
| SELECT + X/Y | Engine der Spur wechseln (alternativ: X auf dem Regler ENGINE öffnet die Liste) |
| SELECT + L1 / R1 | Step-Cursor einen Schlag (4 Steps) zurück/vor; **im Lock-Modus bzw. mit gehaltenem A: Trig kopieren / einfügen** |
| SELECT + L2 / R2 | Step-Cursor eine Step-Seite (16 Steps) zurück/vor |
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
| **PC (MIDI + Audio)** | Audiogerät „Synth City“ (Ein-/Ausgang, 44,1 oder 48 kHz – der PC wählt) + MIDI-Gerät „Synth City“ + Netzwerk (Browser-Upload, SSH) |
| **PC (MIDI)** | MIDI-Gerät + Netzwerk |
| SET → **USB-Laufwerk** | Die Partition GROOVEBOX als normales USB-Laufwerk: Samples reinkopieren, am PC auswerfen, am R36S A drücken |

* **Alles class compliant, ohne Treiber:** Audio als USB Audio Class 2.0, MIDI als USB-MIDI 1.0, Netzwerk als CDC-NCM. Läuft an Windows 10 (ab 2004) / 11, macOS, iPad/iPhone (USB-Adapter), Linux und Android.
* **iPhone/iPad mit USB-C:** Ein direktes USB-C-auf-USB-C-Kabel am OTG-Port geht nicht – der OTG-Port des R36S hat keine USB-C-Erkennung (CC-Widerstände), das iPhone schaltet dann weder Strom noch Daten ein. So geht es: iPhone → **USB-C-auf-USB-A-Adapter** (OTG, z. B. Apple „USB-C auf USB Adapter“) → normales **USB-A-auf-C-Kabel** → OTG-Port des R36S. Der R36S meldet sich als Gerät mit eigener Stromversorgung (Akku), damit iOS nicht „Zubehör benötigt zu viel Strom“ meldet.
* **Ältere Geräte:** SET → „USB-Netzwerk“ = **RNDIS** für ältere Windows-10-Versionen; SET → „USB-Audio-Klasse“ = **UAC1** für Geräte ohne UAC2 (Windows 7/8, Konsolen, ältere Recorder; braucht ein OS-Image ab 11/2026). Die Umstellung baut die USB-Verbindung kurz neu auf.
* **MIDI prüfen:** SET → MIDI → „MIDI-Geräte“ zeigt die Verbindung (z. B. „PC (USB-MIDI)“) und zählt empfangene/gesendete Nachrichten. Kommen Noten an, während der MIDI-Eingang AUS ist, meldet die Groovebox das. Noten und Clock zum PC: „Noten ausgeben“ bzw. „Clock = SENDEN“.
* **Windows zeigt „Synth City“ nicht als MIDI-Gerät** (Windows 11 mit dem neuen MIDI-Dienst), obwohl Audio geht: R36S ab- und wieder anstecken; hilft das nicht, den Dienst „Windows MIDI Service“ (midisrv) neu starten oder Windows neu starten.
* Der Klang geht im Modus **PC (MIDI + Audio)** automatisch **zusätzlich** zum PC (SET → „Audio an PC (USB)“, Standard AN). Lautsprecher/Kopfhörer laufen weiter und geben den Takt vor – liest der PC nicht mit, läuft der Sequencer trotzdem weiter. In SET steht, ob der PC gerade mitliest.
* **Lautstärken getrennt:** Die Lautstärketasten regeln nur Lautsprecher/Kopfhörer des R36S, die USB-Lautstärke steht in SET („USB-Lautstärke“). Mit „Intern bei USB-Audio: STUMM“ (Standard) sind Lautsprecher und Kopfhörer stumm, solange USB-Audio zum PC läuft.
* **Klangqualität USB:** 24 Bit in beide Richtungen, 44,1 oder 48 kHz (der PC wählt). Gerechnet wird intern in 32-Bit-Gleitkomma; erst am Ausgang wird mit Dither auf 24 Bit gewandelt. Macht ein älterer PC/Kernel Probleme, stellt `GB_UAC_SSIZE=2` in `config/groovebox.env` auf 16 Bit zurück.
* Ein Taktausgleich regelt die USB-Spur unhörbar nach (Uhren von R36S und PC laufen minimal unterschiedlich) – keine Knackser bei langen Aufnahmen. Im Log steht die Abweichung als „Takt … ppm“.
* **PC-Ton über den R36S:** Wählt man am PC „Synth City“ als Wiedergabegerät, klingt der PC-Ton aus Lautsprecher/Kopfhörer des R36S (SET → „PC-Ton über R36S“).
* **Sampling vom PC:** SET → „PC-Ton als Sample aufnehmen“ → A. Die Aufnahme startet mit dem ersten Ton, den der PC abspielt (max. 60 s), A beendet sie. Das Sample landet als `samples/pc/pc-NN.wav` (24 Bit) und steht sofort zur Verfügung.
* SSH: `ssh ark@192.168.77.1`, Passwort `ark`.

### App aktualisieren ohne neues Image

Neue Groovebox-Versionen kommen als **`groovebox-os-update.tgz`** im Release **nightly**. Das Image muss dafür nicht neu geflasht werden. Drei Wege:

1. **Browser:** PC-Modus an, http://192.168.77.1 öffnen, unter „Groovebox aktualisieren“ die Datei wählen.
2. **Laufwerk:** Die Datei in den Ordner `update/` auf GROOVEBOX kopieren (SET → USB-Laufwerk oder Karte im PC). Beim nächsten Start wird sie installiert.
3. **Skript vom PC:** `tools\deploy.ps1` (Windows) bzw. `tools/deploy.sh` – erkennt das Groovebox-OS selbst und spielt das neueste Update ein.

Die Groovebox startet dafür kurz neu. Startet die neue Version dreimal nicht, wird automatisch die vorige wiederhergestellt. Protokoll: `logs/update.log`.

**Lautstärke:** Die Lautstärketasten am R36S regeln im Groovebox-OS Lautsprecher und Kopfhörer (Anzeige auf dem Display, wird gespeichert).

**Power-Taste:** kurz drücken schaltet das Display aus und wieder an (die Musik läuft weiter, spart Akku), **lang drücken (1,5 s)** speichert das Projekt und fährt den R36S sauber herunter.

**Bildschirm & Fernbedienung im Browser:** Unter http://192.168.77.1/screen erscheint das Live-Bild der Groovebox (bis ca. 30 Bilder/s über eine dauerhafte WebSocket-Verbindung, nur geänderte Bildteile; Tasten kommen ohne Umweg an). Oben stehen Bilder/s, Datenrate und Verzögerung (ms).
* **Touch direkt aufs Bild:** Spur, Seite im Signalweg, Reiter oben, Kachel, Step oder Feld der Seitenkarte antippen wählt es aus. **Auf einer Kachel ziehen** (hoch oder rechts = mehr) ändert den Wert, auch im Mixer die Fader. **Step antippen** = Trig an/aus; **Step halten und mit einem zweiten Finger eine Kachel ziehen** = P-Lock (wie A halten + Regler). Eine gewählte Kachel nochmal antippen öffnet die Auswahlliste (Sample, LFO-Ziel, Engine, Wavetable zeichnen). Mixer: M/S antippen, LIVE: Spur antippen = Mute, FILL/REC/START antippen. Einstellungen: Eintrag antippen, nach links/rechts ziehen = Wert. „Tasten“ blendet die Touch-Tasten aus (größeres Bild).
* **Touch-Tasten (Handy/Tablet):** Im Querformat liegt das Bild in der Mitte, Steuerkreuz + L1/L2 links, A/B/X/Y + R1/R2 rechts, SELECT/START darunter – wie am R36S. Mehrere Finger gleichzeitig gehen (z. B. A halten + Steuerkreuz), Wischen über das Steuerkreuz wechselt die Richtung, Diagonalen drücken zwei Richtungen. ⛶ = Vollbild (dreht auf Querformat, Bildschirm bleibt an). „Sticks“ blendet die Analog-Sticks ein.
* **Tastatur:** Pfeile = Steuerkreuz, Z/Leertaste = A, X/Rücktaste/Esc = B, S = X, A = Y, Q/W = L1/R1, 1/2 = L2/R2, Shift/Tab = SELECT, Enter = START (halten = FILL), T/F/G/H = linker Stick, I/J/K/L = rechter Stick. „?“ zeigt die Belegung.
* **Gamepad:** Ein am PC oder Handy angeschlossenes Gamepad (Xbox, PlayStation, Bluetooth …) wird erkannt, Tasten in der Lage wie am R36S.
* **Ton im Browser:** Oben auf „🔈 Ton“ tippen – der Klang der Groovebox kommt dann live im Browser (16 Bit Stereo, ca. 175 kB/s, Verzögerung ca. 0,1–0,2 s; der Regler daneben ist die Lautstärke). Lautsprecher/Kopfhörer am R36S laufen weiter (leiser drehen per Lautstärketasten). War der Ton beim letzten Mal an, startet er beim ersten Antippen der Seite wieder (Browser verlangen eine Berührung, bevor sie Ton abspielen). Am iPhone den Lautlos-Schalter beachten.
* **Als App auf dem Homescreen:** iPhone/iPad: in Safari `http://192.168.77.1/screen` öffnen → Teilen → „Zum Home-Bildschirm“ – startet danach ohne Browserleiste im Vollbild mit dem SYNTH-CITY-Symbol. Android: Chrome-Menü → „Zum Startbildschirm hinzufügen“. Hinweis: Weil der R36S unverschlüsselt (http) erreichbar ist, bietet Chrome kein „App installieren“ an; die Verknüpfung öffnet die Seite dann im Browser.
* Bis zu 3 Geräte können gleichzeitig zuschauen, bedienen und zuhören. Reißt die Verbindung ab, verbindet sich die Seite selbst neu und lässt alle Tasten los.

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

## Mehrkern-Audio

Bei spürbarer Last (ab etwa 4 klingenden Stimmen) rechnet ein **zweiter Prozessorkern** mit: Beide Kerne holen sich abwechselnd Stimmen ab, der Audio-Kern wartet nie auf einen schlafenden Helfer. Auf dem SYNTH CITY OS laufen beide mit Echtzeit-Priorität. Ein- und ausschalten: SET → **Audio-Rechenkerne** (1/2). Bei Knacksern auf 1 stellen und im Log melden. Der Leistungstest zeigt Demo-Song und Worst Case mit 1 und 2 Kernen.

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
