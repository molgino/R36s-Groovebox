// Kurze Erklaerung je Reglername (fuer Einsteiger, erscheint in der Kopfzeile des Bedienfelds)
#pragma once
#include <string.h>
struct ParamHelp { const char* name; const char* text; const char* en; };
static const ParamHelp PARAM_HELP[] = {
    {"NOTE", "Tonhöhe des Steps", "Pitch of the step"}, {"VEL", "Anschlagstärke (Lautstärke des Trigs)", "Velocity (loudness of the trig)"}, {"LEN", "Notenlänge", "Note length"},
    {"PROB", "Wahrscheinlichkeit, dass der Trig spielt", "Probability that the trig plays"}, {"RTRG", "Wiederholungen innerhalb des Steps (Ratchet)", "Repeats within the step (ratchet)"},
    {"MTIM", "Mikrotiming: Trig etwas früher oder später", "Microtiming: trig slightly earlier or later"}, {"ENGINE", "Klangerzeuger der Spur", "Sound engine of the track"},
    {"COND", "Bedingung: z. B. nur jeder 2. Durchlauf", "Condition, e.g. only every 2nd pass"},
    {"FREQ", "Grenzfrequenz: wie hell der Klang ist", "Cutoff: how bright the sound is"}, {"RESO", "Resonanz: betont die Grenzfrequenz", "Resonance: emphasises the cutoff"},
    {"TYPE", "Art des Filters bzw. Effekts", "Type of filter or effect"}, {"ENV", "wie stark die Hüllkurve das Filter öffnet", "How much the envelope opens the filter"},
    {"F.ATK", "Filter-Hüllkurve: Einschwingzeit", "Filter envelope: attack time"}, {"F.DEC", "Filter-Hüllkurve: Abklingzeit", "Filter envelope: decay time"},
    {"F.SUS", "Filter-Hüllkurve: Haltepegel", "Filter envelope: sustain level"}, {"F.REL", "Filter-Hüllkurve: Ausklingzeit nach dem Loslassen", "Filter envelope: release time"},
    {"ATK", "Einschwingzeit der Lautstärke", "Attack time of the volume"}, {"DEC", "Abklingzeit der Lautstärke", "Decay time of the volume"}, {"SUS", "Haltepegel der Lautstärke", "Sustain level of the volume"},
    {"REL", "Ausklingzeit nach dem Loslassen", "Release time after note off"}, {"VOL", "Lautstärke der Spur", "Track volume"}, {"PAN", "Position links/rechts", "Position left/right"},
    {"DLY", "Anteil ins Echo (Delay)", "Amount sent to the delay"}, {"REV", "Anteil in den Hall (Reverb)", "Amount sent to the reverb"},
    {"RATE", "Tempo", "Speed"}, {"SYNC", "an das Songtempo gekoppelt", "Locked to the song tempo"}, {"DEST", "Ziel: welchen Regler der LFO bewegt", "Target: which control the LFO moves"},
    {"DEPTH", "Stärke der Modulation", "Modulation amount"}, {"WAVE", "Wellenform", "Waveform"}, {"MODE", "Verhalten bzw. Spielweise", "Behaviour / play mode"},
    {"PHASE", "Startpunkt der Welle", "Start point of the wave"}, {"FADE", "LFO blendet ein oder aus", "LFO fades in or out"},
    {"P1", "Effekt-Regler 1", "Effect control 1"}, {"P2", "Effekt-Regler 2", "Effect control 2"}, {"P3", "Effekt-Regler 3", "Effect control 3"}, {"MIX", "Mischung trocken/Effekt", "Dry/effect mix"},
    {"CRSH", "Bitcrusher: grober, digitaler Klang", "Bitcrusher: coarse digital sound"}, {"SRR", "niedrigere Abtastrate: Lo-Fi", "Lower sample rate: lo-fi"}, {"DRIV", "Übersteuerung", "Overdrive"},
    {"DRIVE", "Übersteuerung", "Overdrive"},
    {"DIR", "Laufrichtung der Steps", "Step direction"}, {"SPD", "Tempo der Spur (z. B. doppelt)", "Track speed (e.g. double)"}, {"PULS", "Euklid: Anzahl gleichmäßig verteilter Trigs", "Euclid: number of evenly spread trigs"},
    {"ROT", "Euklid: Muster verschieben", "Euclid: rotate the pattern"}, {"DENS", "Zufallsmuster: Dichte", "Random pattern: density"}, {"CHNG", "Zufallsmuster: wie oft es sich ändert", "Random pattern: how often it changes"},
    {"RANGE", "Zufallsmuster: Tonumfang", "Random pattern: note range"},
    {"BASE", "Hochpass: entfernt Bässe unterhalb", "High-pass: removes bass below"}, {"WDTH", "Breite des Frequenzbands", "Width of the frequency band"},
    {"KEYT", "Filter folgt der Tonhöhe", "Filter follows the pitch"}, {"VELF", "Anschlag öffnet das Filter", "Velocity opens the filter"},
    {"F.KRV", "Form der Filter-Hüllkurve: weich bis knackig", "Shape of the filter envelope: soft to snappy"}, {"A.KRV", "Form der Lautstärke-Hüllkurve: weich bis knackig", "Shape of the volume envelope: soft to snappy"},
    {"CHRD", "Akkord für Arp/Akkord-Modus", "Chord for arp / chord mode"}, {"OCT", "Oktavumfang", "Octave range"}, {"GATE", "Notenlänge im Arp", "Note length in the arp"}, {"ACCT", "Akzent: betonte Noten", "Accent: emphasised notes"},
    {"TUNE", "Stimmung", "Tuning"}, {"DCAY", "Abklingzeit", "Decay time"}, {"SWEP", "Tonhöhen-Abfall (Kick-Punch)", "Pitch drop (kick punch)"}, {"S.TIM", "Dauer des Tonhöhen-Abfalls", "Duration of the pitch drop"},
    {"CLCK", "Anschlag-Klick", "Attack click"}, {"BODY", "Sättigung, mehr Druck", "Saturation, more punch"}, {"SNAP", "Rauschanteil (Snare-Teppich)", "Noise amount (snare wires)"},
    {"N.DEC", "Abklingzeit des Rauschens", "Decay of the noise"}, {"TONE", "Klangfarbe hell/dunkel", "Tone bright/dark"}, {"METL", "metallisch oder rauschig", "Metallic or noisy"},
    {"SPRD", "Abstand der Klatscher", "Spacing of the claps"}, {"RPTS", "Anzahl der Klatscher", "Number of claps"}, {"NOIS", "Rauschanteil", "Noise amount"}, {"GRIT", "Körnigkeit", "Graininess"},
    {"WAV1", "Wellenform Oszillator 1", "Waveform oscillator 1"}, {"WAV2", "Wellenform Oszillator 2", "Waveform oscillator 2"}, {"DETN", "Verstimmung: breiter, schwebender Klang", "Detune: wider, floating sound"},
    {"SUB", "Sub-Oszillator: Oktave tiefer", "Sub oscillator: one octave lower"}, {"PW", "Pulsbreite", "Pulse width"}, {"GLID", "Gleiten zwischen Tönen", "Glide between notes"},
    {"ALGO", "FM-Algorithmus: wie die Operatoren verbunden sind", "FM algorithm: how the operators connect"}, {"FBK", "Rückkopplung (rauer Klang)", "Feedback (rough sound)"},
    {"RAT1", "Frequenzverhältnis Operator 1", "Frequency ratio operator 1"}, {"RAT2", "Frequenzverhältnis Operator 2", "Frequency ratio operator 2"}, {"RAT3", "Frequenzverhältnis Operator 3", "Frequency ratio operator 3"},
    {"RAT4", "Frequenzverhältnis Operator 4", "Frequency ratio operator 4"}, {"FINE", "Feinstimmung", "Fine tuning"}, {"LVL2", "Stärke Operator 2", "Level operator 2"}, {"LVL3", "Stärke Operator 3", "Level operator 3"},
    {"LVL4", "Stärke Operator 4", "Level operator 4"}, {"DEC1", "Abklingzeit Operator 1", "Decay operator 1"}, {"DEC2", "Abklingzeit Operator 2", "Decay operator 2"}, {"DEC3", "Abklingzeit Operator 3", "Decay operator 3"},
    {"DEC4", "Abklingzeit Operator 4", "Decay operator 4"}, {"VELO", "Anschlag beeinflusst die Klangfarbe", "Velocity changes the tone"},
    {"SMPL", "Sample auswählen (X = Explorer)", "Choose sample (X = browser)"}, {"STRT", "Startpunkt im Sample", "Start point in the sample"}, {"END", "Endpunkt (vor dem Start = rückwärts)", "End point (before start = reverse)"},
    {"LOOP", "Sample in Schleife", "Loop the sample"}, {"GAIN", "Pegel", "Level"}, {"TABL", "Wavetable auswählen", "Choose wavetable"}, {"POS", "Position in der Tabelle / im Sample", "Position in table / sample"},
    {"WARP", "Welle verbiegen", "Bend the wave"}, {"P.ENV", "Hüllkurve bewegt die Position", "Envelope moves the position"}, {"SLCS", "Anzahl der Scheiben", "Number of slices"},
    {"SLIC", "welche Scheibe spielt", "Which slice plays"}, {"SPRY", "Streuung der Körner", "Grain spread"}, {"SIZE", "Größe", "Size"}, {"PTCH", "Tonhöhe", "Pitch"},
    {"P.SPR", "Tonhöhen-Streuung der Körner", "Pitch spread of the grains"},
    {"D.TIME", "Echo-Zeit im Takt", "Delay time in beats"}, {"D.FBK", "Echo-Wiederholungen", "Delay repeats"}, {"D.TONE", "Echo-Klangfarbe", "Delay tone"}, {"D.PING", "Echo springt links/rechts", "Delay bounces left/right"},
    {"R.SIZE", "Raumgröße des Halls", "Reverb room size"}, {"R.DAMP", "Hall-Dämpfung (dunkler)", "Reverb damping (darker)"}, {"R.PRE", "Verzögerung vor dem Hall", "Delay before the reverb"}, {"R.WIDE", "Stereobreite des Halls", "Reverb stereo width"},
    {"THR", "Schwelle, ab der komprimiert wird", "Threshold where compression starts"}, {"RATIO", "Stärke der Kompression", "Compression strength"}, {"SC", "Sidechain: welche Spur pumpt", "Sidechain: which track pumps"},
    {"BPM", "Tempo in Schlägen pro Minute", "Tempo in beats per minute"}, {"SWNG", "Swing: jeder 2. Step später", "Swing: every 2nd step later"}, {"SCALE", "Tonleiter für alle Spuren", "Scale for all tracks"},
    {"ROOT", "Grundton der Tonleiter", "Root note of the scale"},
    // FM ab v13
    {"RAT", "Frequenzverhältnis (1 = Grundton, 2 = Oktave)", "Frequency ratio (1 = root, 2 = octave)"},
    {"O.LVL", "Pegel: als Träger Lautstärke, als Modulator Helligkeit", "Level: loudness as carrier, brightness as modulator"},
    {"O.ATK", "Einschwingzeit dieses Operators", "Attack time of this operator"}, {"O.DEC", "Abklingzeit dieses Operators", "Decay time of this operator"},
    {"O.SUS", "Haltepegel dieses Operators", "Sustain level of this operator"}, {"O.REL", "Ausklingzeit dieses Operators", "Release time of this operator"},
    {"PITCH", "Tonhöhen-Hüllkurve: Anfangsversatz in Halbtönen", "Pitch envelope: start offset in semitones"}, {"P.DEC", "Dauer der Tonhöhen-Hüllkurve", "Duration of the pitch envelope"},
    // SYNTH / WTBL zweite Seite
    {"OCT1", "Oktave Oszillator 1", "Octave oscillator 1"}, {"OCT2", "Oktave Oszillator 2", "Octave oscillator 2"}, {"LVL1", "Pegel Oszillator 1", "Level oscillator 1"},
    {"LVL2", "Pegel Oszillator 2", "Level oscillator 2"}, {"SUB.O", "Sub-Oszillator eine oder zwei Oktaven tiefer", "Sub oscillator one or two octaves down"},
    {"SYNC", "Hard-Sync: OSC2 startet mit OSC1 neu (OCT2/DETN ändern!)", "Hard sync: OSC2 restarts with OSC1 (change OCT2/DETN!)"},
    {"XMOD", "OSC2 moduliert die Frequenz von OSC1 (metallisch)", "OSC2 modulates the frequency of OSC1 (metallic)"},
    {"RING", "Ringmodulation OSC1 x OSC2 (Glocke)", "Ring modulation OSC1 x OSC2 (bell)"}, {"OCT", "Oktave", "Octave"},
    {"UWAV", "eigene Wavetable (TABL = USER), X = Explorer", "User wavetable (TABL = USER), X = browser"}, {"DRAW", "Wavetable selbst zeichnen (X öffnet den Editor)", "Draw your own wavetable (X opens the editor)"},
    {"SND.A", "Anteil an Effekt-Bus A", "Send to effect bus A"}, {"SND.B", "Anteil an Effekt-Bus B", "Send to effect bus B"}, {"SND.C", "Anteil an Effekt-Bus C", "Send to effect bus C"},
};
// Effekt-Busse: Erklaerung je Typ und Regler
struct BusHelp { int type; const char* name; const char* text; const char* en; };
static const BusHelp BUS_HELP[] = {
    {-1, "TYPE", "Effekt dieses Busses (wechseln setzt die Regler zurück)", "Effect of this bus (changing resets the controls)"},
    {-1, "FWD", "schickt den Ausgang zusätzlich in den nächsten Bus (z. B. Delay in den Hall)", "Also sends the output into the next bus (e.g. delay into reverb)"},
    {-1, "LVL", "Pegel des Effekts in der Summe", "Level of the effect in the mix"},
    {1, "TIME", "Echo-Zeit im Takt", "Delay time in beats"}, {1, "FBK", "Echo-Wiederholungen", "Delay repeats"}, {1, "TONE", "Echo-Klangfarbe (dunkler)", "Delay tone (darker)"},
    {1, "PING", "Echo springt links/rechts", "Delay bounces left/right"}, {1, "LOCUT", "Bass aus den Echos nehmen", "Remove bass from the echoes"},
    {2, "TIME", "Bandecho-Zeit im Takt (Wechsel gleitet)", "Tape delay time in beats (changes glide)"}, {2, "FBK", "Wiederholungen", "Repeats"},
    {2, "WOW", "Gleichlaufschwankungen des Bandes", "Wow and flutter of the tape"}, {2, "SAT", "Band-Sättigung", "Tape saturation"}, {2, "AGE", "Alter: dunkler, dünner, rauschiger", "Age: darker, thinner, noisier"},
    {3, "SIZE", "Raumgröße und Hallzeit", "Room size and decay time"}, {3, "DAMP", "Höhendämpfung (dunkler)", "High damping (darker)"}, {3, "PRE", "Verzögerung vor dem Hall", "Delay before the reverb"},
    {3, "WIDE", "Stereobreite", "Stereo width"}, {3, "MOD", "Bewegung im Hallschweif (chorusartig)", "Movement in the tail (chorus-like)"},
    {4, "DECAY", "Ausklingen der Feder", "Decay of the spring"}, {4, "TONE", "Klangfarbe der Feder", "Tone of the spring"}, {4, "DRIP", "typisches Feder-Zirpen (Boing)", "Typical spring chirp (boing)"},
    {4, "WIDE", "Stereo: zwei Federn", "Stereo: two springs"}, {4, "LOCUT", "Bass vor der Feder entfernen", "Remove bass before the spring"},
    {5, "RATE", "Geschwindigkeit der Modulation", "Modulation speed"}, {5, "DEPTH", "Tiefe der Modulation", "Modulation depth"}, {5, "VOICE", "Anzahl Stimmen (1-3, Ensemble)", "Number of voices (1-3, ensemble)"},
    {5, "WIDE", "Stereobreite", "Stereo width"}, {5, "FBK", "Rückkopplung (flangerartig)", "Feedback (flanger-like)"},
};
static inline const char* busHelp(int type, const char* name) {
    for (unsigned i = 0; i < sizeof BUS_HELP / sizeof *BUS_HELP; i++)
        if ((BUS_HELP[i].type == type || BUS_HELP[i].type < 0) && !strcmp(BUS_HELP[i].name, name)) return g_lang ? BUS_HELP[i].en : BUS_HELP[i].text;
    return "";
}
static inline const char* paramHelp(const char* name) {
    for (unsigned i = 0; i < sizeof PARAM_HELP / sizeof *PARAM_HELP; i++) if (!strcmp(PARAM_HELP[i].name, name)) return g_lang ? PARAM_HELP[i].en : PARAM_HELP[i].text;
    return "";
}
