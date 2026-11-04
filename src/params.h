// Parameter-Definitionen der Groovebox (Elektron-artige Seiten mit je 8 Reglern)
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <math.h>

// Speicher-Seiten je Spur (je 8 Parameter): 0 TRG, 1 SRC, 2 FLT, 3 AMP, 4 LFO1, 5 INS, 6 SRC2, 7 LFO2, 8 LFO3, 9 SEQ, 10 FLT2, 11 ARP
// Sprache der Oberflaeche: 0 Deutsch, 1 English (definiert in main.cpp). TR waehlt den passenden Text.
extern int g_lang;
#define TR(de, en) (g_lang ? (en) : (de))

enum { NTRACKS = 8, NSTEPS = 128, NPATTERNS = 16, NPAGES = 12, NPARAMS = NPAGES * 8, NLFO = 3 };
// Oberflaechen-Seiten (Reihenfolge = Navigation). Spurseiten zuerst.
enum UiPage { PG_TRG, PG_SEQ, PG_ARP, PG_SRC, PG_SRC2, PG_FLT, PG_FLT2, PG_AMP, PG_LFO1, PG_LFO2, PG_LFO3, PG_INS, PG_MIX, PG_FX, PG_CMP, PG_PRJ, PG_SONG, PG_MST, PG_SYS, PG_LIVE, NUI_PAGES };
enum { PG_LFO = PG_LFO1 };
enum { PAGE_INS = PG_INS, PAGE_FX = PG_FX, PAGE_CMP = PG_CMP, PAGE_MST = PG_MST, PAGE_SYS = PG_SYS, PAGE_MIX = PG_MIX, PAGE_PRJ = PG_PRJ,
       FIRST_GLOBAL_PAGE = PG_MIX };
static const int PG_STORE[NUI_PAGES] = {0, 9, 11, 1, 6, 2, 10, 3, 4, 7, 8, 5, -1, -1, -1, -1, -1, -1, -1, -1};   // Spurseite -> Speicherseite
static const int PG_GROUP[NUI_PAGES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 1};
static const char* const GROUP_NAMES[3] = {"SPUR", "LIVE, MIX & FX", "PROJEKT"};
enum { NO_LOCK = 255 };

// Reihenfolge = gespeicherter Wert (neue Engines nur hinten anfuegen!)
enum Machine { M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_FM, M_SAMPLE, M_TOM, M_RIM, M_SHKR, M_WTBL, M_CHOP, M_GRAN, M_COUNT };
static const char* const MACHINE_NAMES[M_COUNT] = {"KICK", "SNARE", "HAT", "CLAP", "SYNTH", "FM", "SMPL", "TOM", "RIM", "SHKR", "WTBL", "CHOP", "GRAN"};
static const char* const MACHINE_LONG[M_COUNT] = {"Bassdrum", "Snare", "Hi-Hat", "Clap", "Analog-Synth", "FM-Synth (4 OP)", "Sample-Player",
                                                  "Tom", "Rimshot", "Shaker", "Wavetable-Synth", "Sample-Chopper", "Granulator"};
// Anzeige-Reihenfolge im Engine-Waehler: Drums, Synths, Samples
static const uint8_t MACHINE_ORDER[M_COUNT] = {M_KICK, M_SNARE, M_HAT, M_CLAP, M_TOM, M_RIM, M_SHKR, M_SYNTH, M_FM, M_WTBL, M_SAMPLE, M_CHOP, M_GRAN};
inline bool machineIsDrum(int m) { return m <= M_CLAP || m == M_TOM || m == M_RIM || m == M_SHKR; }
inline bool machineUsesSample(int m) { return m == M_SAMPLE || m == M_CHOP || m == M_GRAN; }
static const char* const PAGE_NAMES[NUI_PAGES] = {"TRG", "SEQ", "ARP", "SRC", "SR2", "FLT", "FL2", "AMP", "LF1", "LF2", "LF3", "INS", "MIX", "FX", "CMP", "PRJ", "SNG", "MST", "SET", "LIVE"};
static const char* const PAGE_LONG[NUI_PAGES] = {"TRIG", "SEQUENCER", "ARPEGGIATOR", "KLANG", "KLANG 2", "FILTER", "FILTER 2 + KURVEN", "VERSTÄRKER", "LFO 1", "LFO 2", "LFO 3", "INSERT-FX",
                                                 "MIXER", "EFFEKTE", "KOMPRESSOR", "PROJEKT", "SONG", "MASTER", "EINSTELLUNGEN", "LIVE"};

// Filter-1-Typen (Werte nur hinten anfuegen: 0..2 = alte LP/BP/HP)
enum { FT_LP12, FT_BP, FT_HP12, FT_LP24, FT_LADR, FT_HP24, FT_NTCH, FT_PEAK, FT_CMBP, FT_CMBN, FT_VOWL, FT_COUNT };
// Formanten (Hz) A E I O U fuer FT_VOWL
static const float VOWEL_F[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};

enum Kind {
    K_NONE, K_NUM, K_BIP, K_NOTE, K_PCT, K_LEN, K_WAVE, K_FTYPE, K_LDST, K_LWAV,
    K_TRG, K_RTRG, K_RATIO, K_DTIM, K_BPM, K_SWNG, K_CNT,
    K_SMPL, K_SEMI, K_OFFON, K_DEV, K_BUFS, K_CLK, K_CHM, K_INFO, K_USB,
    K_ALGO, K_DECAY, K_ACT, K_SLOT, K_COND, K_LSYNC, K_LDEST, K_LMODE, K_FBK, K_DIR, K_TLEN, K_TSPD, K_RANGE, K_MTIM, K_WTAB, K_SLICE,
    K_SCALE, K_ROOT, K_INS, K_MS, K_MACH, K_THR, K_CRAT, K_CATK, K_CREL, K_CGAIN, K_SCSRC, K_GR,
    K_ARPM, K_ACHD, K_ARAT
};

struct ParamDef { const char* name; Kind kind; uint8_t def; uint8_t max; };

// Parameterindizes (Seite*8 + Regler)
enum {
    P_NOTE = 0, P_VEL, P_LEN, P_PROB, P_RTRG, P_MTIM, P_T6, P_COND, P_T7 = P_COND, P_RFAD = P_MTIM,
    P_S0 = 8, P_S1, P_S2, P_S3, P_S4, P_S5, P_S6, P_S7,
    P_FREQ = 16, P_RESO, P_FTYP, P_FENV, P_FATK, P_FDEC, P_FSUS, P_FREL,
    P_AATK = 24, P_ADEC, P_ASUS, P_AREL, P_VOL, P_PAN, P_DLY, P_REV,
    P_L1 = 32,                                                   // LFO1 (Seite 4)
    P_ITYP = 40, P_IP1, P_IP2, P_IP3, P_IMIX, P_CRSH, P_SRR, P_DRIV,
    P_X0 = 48, P_X1, P_X2, P_X3, P_X4, P_X5, P_X6, P_X7,      // SRC2 (maschinenabhaengig)
    P_L2 = 56, P_L3 = 64,                                        // LFO2, LFO3
    P_DIR = 72, P_QLEN, P_QSPD, P_PULS, P_EROT, P_DENS, P_CHNG, P_RANGE,  // SEQ: Richtung, Laenge, Tempo, Euklid, Generator
    P_BASE = 80, P_WDTH, P_KEYT, P_VELF, P_FCRV, P_ACRV,                                   // FLT2: Base-Width-Filter, Key-/Velocity-Tracking
    P_AMOD = 88, P_ACHD, P_AOCT, P_ARAT, P_AGAT, P_AACC                    // ARP: Modus, Akkord, Oktaven, Tempo, Gate, Akzent
};
// Master-Seite
enum { MS_BPM = 0, MS_SWNG, MS_VOL, MS_DRIV, MS_SCALE, MS_ROOT };
// LFO: Versatz innerhalb einer LFO-Seite
enum { LF_RATE = 0, LF_SYNC, LF_DEST, LF_DEPTH, LF_WAVE, LF_MODE, LF_PHASE, LF_FADE };
static const int LFO_BASE[NLFO] = {32, 56, 64};
enum { LW_SIN, LW_TRI, LW_SAWUP, LW_SAWDN, LW_SQR, LW_EXPUP, LW_EXPDN, LW_SH, LW_RND, LW_COUNT };
enum { LM_FREE, LM_TRIG, LM_HOLD, LM_ONE, LM_HALF };
static const float LFO_SYNC_STEPS[12] = {0, 64, 32, 16, 8, 4, 2, 1, 0.5f, 8.f / 3, 4.f / 3, 2.f / 3};
// Geschwindigkeit pro Spur: Ticks (12 pro 16tel) pro Step
static const int SPEED_TPS[7] = {96, 48, 24, 16, 12, 8, 6};
static const char* const SPEED_NAMES[7] = {"1/8x", "1/4x", "1/2x", "3/4x", "1x", "3/2x", "2x"};
enum { SPEED_1X = 4 };
// Trig-Bedingungen (Digitone-II-Stil): 0 keine, 1-8 FILL/PRE/NEI/1ST (+ negiert), dann A:B und !A:B (B = 2..8)
enum { CND_NONE, CND_FILL, CND_NFILL, CND_PRE, CND_NPRE, CND_NEI, CND_NNEI, CND_1ST, CND_N1ST, CND_AB = 9, CND_COUNT = 9 + 35 * 2 };
inline void condDecode(int v, int& a, int& b, bool& neg) {   // nur fuer v >= CND_AB
    int i = v - CND_AB; neg = i >= 35; if (neg) i -= 35;
    b = 2; while (i >= b) { i -= b; b++; }
    a = i + 1;
}
// FX-Seite (Send-Effekte)
enum { FX_DTIM = 0, FX_DFBK, FX_DTON, FX_DWID, FX_RSIZ, FX_RDMP, FX_RPRE, FX_RWID };

static const ParamDef PDEF_NONE = {"", K_NONE, 0, 0};

static const ParamDef COMMON_DEFS[NPARAMS] = {
    // TRIG
    {"NOTE", K_NOTE, 60, 127}, {"VEL", K_NUM, 100, 127}, {"LEN", K_LEN, 48, 127}, {"PROB", K_PCT, 100, 100},
    {"RTRG", K_RTRG, 0, 6}, {"MTIM", K_MTIM, 23, 46}, {"ENGINE", K_MACH, 0, M_COUNT - 1}, {"COND", K_COND, 0, CND_COUNT - 1},
    // SRC (maschinenabhaengig, siehe SRC_DEFS)
    PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE,
    // FILT
    {"FREQ", K_NUM, 127, 127}, {"RESO", K_NUM, 0, 127}, {"TYPE", K_FTYPE, 0, FT_COUNT - 1}, {"ENV", K_BIP, 64, 127},
    {"F.ATK", K_NUM, 0, 127}, {"F.DEC", K_NUM, 60, 127}, {"F.SUS", K_NUM, 0, 127}, {"F.REL", K_NUM, 60, 127},
    // AMP
    {"ATK", K_NUM, 0, 127}, {"DEC", K_NUM, 80, 127}, {"SUS", K_NUM, 127, 127}, {"REL", K_NUM, 50, 127},
    {"VOL", K_NUM, 100, 127}, {"PAN", K_BIP, 64, 127}, {"DLY", K_NUM, 0, 127}, {"REV", K_NUM, 0, 127},
    // LFO / FX
    // LFO1
    {"RATE", K_NUM, 64, 127}, {"SYNC", K_LSYNC, 0, 11}, {"DEST", K_LDEST, 0, NPARAMS}, {"DEPTH", K_BIP, 64, 127},
    {"WAVE", K_LWAV, 0, LW_COUNT - 1}, {"MODE", K_LMODE, 0, 4}, {"PHASE", K_NUM, 0, 127}, {"FADE", K_BIP, 64, 127},
    // INS (Insert-Effekt; Namen von P1-P3 haengen vom Typ ab, siehe INS_PNAMES)
    {"TYPE", K_INS, 0, 6}, {"P1", K_NUM, 64, 127}, {"P2", K_NUM, 64, 127}, {"P3", K_NUM, 64, 127},
    {"MIX", K_NUM, 64, 127}, {"CRSH", K_NUM, 0, 127}, {"SRR", K_NUM, 0, 127}, {"DRIV", K_NUM, 0, 127},
    // SRC2 (maschinenabhaengig, siehe SRC2_DEFS)
    PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE,
    // LFO2, LFO3
    {"RATE", K_NUM, 64, 127}, {"SYNC", K_LSYNC, 0, 11}, {"DEST", K_LDEST, 0, NPARAMS}, {"DEPTH", K_BIP, 64, 127},
    {"WAVE", K_LWAV, 0, LW_COUNT - 1}, {"MODE", K_LMODE, 0, 4}, {"PHASE", K_NUM, 0, 127}, {"FADE", K_BIP, 64, 127},
    {"RATE", K_NUM, 64, 127}, {"SYNC", K_LSYNC, 0, 11}, {"DEST", K_LDEST, 0, NPARAMS}, {"DEPTH", K_BIP, 64, 127},
    {"WAVE", K_LWAV, 0, LW_COUNT - 1}, {"MODE", K_LMODE, 0, 4}, {"PHASE", K_NUM, 0, 127}, {"FADE", K_BIP, 64, 127},
    // SEQ (LEN/SPD zeigen auf Pattern.len/speed)
    {"DIR", K_DIR, 0, 5}, {"LEN", K_TLEN, 16, 128}, {"SPD", K_TSPD, 4, 6}, {"PULS", K_NUM, 0, 64},
    {"ROT", K_NUM, 0, 127}, {"DENS", K_PCT, 0, 100}, {"CHNG", K_PCT, 0, 100}, {"RANGE", K_RANGE, 0, 48},
    // FLT2: Base-Width-Filter (Hochpass BASE, Tiefpass BASE+WDTH), Key-Tracking und Velocity fuer Filter 1
    {"BASE", K_NUM, 0, 127}, {"WDTH", K_NUM, 127, 127}, {"KEYT", K_PCT, 0, 100}, {"VELF", K_BIP, 64, 127},
    {"F.KRV", K_BIP, 64, 127}, {"A.KRV", K_BIP, 64, 127}, PDEF_NONE, PDEF_NONE,   // Huellkurvenform (ab v11)
    // ARP: spielt waehrend der Trig-Laenge die Akkordtoene ueber OCT Oktaven im Tempo RATE
    {"MODE", K_ARPM, 0, 5}, {"CHRD", K_ACHD, 2, 9}, {"OCT", K_CNT, 0, 3}, {"RATE", K_ARAT, 2, 5},
    {"GATE", K_PCT, 60, 100}, {"ACCT", K_PCT, 0, 100}, PDEF_NONE, PDEF_NONE,
};
// Arpeggiator: Akkord-Intervalle (Halbtoene), Tempo in Steps
static const char* const ARP_MODES[6] = {"AUS", "UP", "DOWN", "UPDN", "RAND", "AKKD"};   // AKKD = Akkordtoene gleichzeitig
enum { ARP_CHORD = 5, NCHORDV = 3 };   // Zusatzstimmen je Spur fuer Akkorde
static const char* const ARP_CHORDS[10] = {"OKT", "5TH", "MAJ", "MIN", "SUS4", "MAJ7", "MIN7", "DOM7", "DIM", "SKALA"};
static const int8_t ARP_IV[10][5] = {{0, -1}, {0, 7, -1}, {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 5, 7, -1},
                                     {0, 4, 7, 11, -1}, {0, 3, 7, 10, -1}, {0, 4, 7, 10, -1}, {0, 3, 6, 9, -1}, {-1}};
static const char* const ARP_RATES[6] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4"};
static const float ARP_STEPS[6] = {0.5f, 2.f / 3.f, 1.f, 4.f / 3.f, 2.f, 4.f};
// Tonskalen pro Spur: Wert 0 = chromatisch, sonst 1 + Skala*12 + Grundton
enum { NSCALES = 10 };
static const char* const SCALE_NAMES[NSCALES] = {"MAJ", "MIN", "DOR", "PHR", "LYD", "MIX", "HMI", "PMA", "PMI", "BLU"};
static const char* const SCALE_LONG[NSCALES] = {"DUR", "MOLL", "DORISCH", "PHRYGISCH", "LYDISCH", "MIXOLYDISCH",
                                                "HARM. MOLL", "PENTA DUR", "PENTA MOLL", "BLUES"};
static const uint16_t SCALE_MASK[NSCALES] = {   // Bit n = Halbton n ueber dem Grundton
    0xAB5, 0x5AD, 0x6AD, 0x5AB, 0xAD5, 0x6B5, 0x9AD, 0x295, 0x4A9, 0x4E9};
static const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
inline bool inScale(int note, int sv) {
    if (sv <= 0) return true;
    int root = (sv - 1) % 12, sc = ((sv - 1) / 12) % NSCALES;
    return (SCALE_MASK[sc] >> (((note - root) % 12 + 12) % 12)) & 1;
}
inline int quantizeNote(int note, int sv) {   // naechste Note in der Skala
    if (sv <= 0) return note;
    for (int d = 0; d < 7; d++) {
        if (note + d <= 127 && inScale(note + d, sv)) return note + d;
        if (note - d >= 0 && inScale(note - d, sv)) return note - d;
    }
    return note;
}
inline int stepScale(int note, int dir, int sv) {   // naechste Skalennote in Richtung dir
    int n = note;
    do { n += dir; } while (n >= 0 && n <= 127 && !inScale(n, sv));
    return n < 0 ? note : (n > 127 ? note : n);
}
// Skala (mst[MS_SCALE]: 0 = aus, sonst 1 + Grundton + 12*Skala) und Grundton getrennt einstellen
inline int scaleType(int sv) { return sv > 0 ? (sv - 1) / 12 % NSCALES + 1 : 0; }   // 0 = aus
inline int scaleCombine(int type, int root) { return type > 0 ? 1 + (root % 12) + 12 * (type - 1) : 0; }

enum InsType { INS_OFF, INS_CHORUS, INS_PHASER, INS_FLANGER, INS_DRIVE, INS_RING, INS_APAN, INS_COUNT };
static const char* const INS_NAMES[INS_COUNT] = {"OFF", "CHORS", "PHASE", "FLNGR", "DRIVE", "RING", "APAN"};
static const char* const INS_PNAMES[INS_COUNT][3] = {
    {"-", "-", "-"}, {"RATE", "DEPTH", "WIDTH"}, {"RATE", "DEPTH", "FBK"}, {"RATE", "DEPTH", "FBK"},
    {"GAIN", "TONE", "LEVEL"}, {"FREQ", "FINE", "-"}, {"RATE", "DEPTH", "-"}};

static const ParamDef SRC_DEFS[M_COUNT][8] = {
    // KICK
    {{"TUNE", K_NUM, 40, 127}, {"DCAY", K_NUM, 60, 127}, {"SWEP", K_NUM, 70, 127}, {"S.TIM", K_NUM, 40, 127},
     {"CLCK", K_NUM, 50, 127}, {"BODY", K_NUM, 0, 127}, PDEF_NONE, PDEF_NONE},
    // SNARE
    {{"TUNE", K_NUM, 50, 127}, {"DCAY", K_NUM, 40, 127}, {"SNAP", K_NUM, 70, 127}, {"N.DEC", K_NUM, 50, 127},
     {"TONE", K_NUM, 80, 127}, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    // HAT
    {{"TUNE", K_NUM, 64, 127}, {"DCAY", K_NUM, 30, 127}, {"TONE", K_NUM, 90, 127}, {"METL", K_NUM, 64, 127},
     PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    // CLAP
    {{"TUNE", K_NUM, 64, 127}, {"DCAY", K_NUM, 50, 127}, {"SPRD", K_NUM, 50, 127}, {"RPTS", K_CNT, 3, 5},
     PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    // SYNTH
    {{"WAV1", K_WAVE, 0, 3}, {"WAV2", K_WAVE, 0, 3}, {"DETN", K_BIP, 72, 127}, {"MIX", K_NUM, 64, 127},
     {"SUB", K_NUM, 0, 127}, {"NOIS", K_NUM, 0, 127}, {"PW", K_NUM, 64, 127}, {"GLID", K_NUM, 0, 127}},
    // FM (4 Operatoren): Algorithmus, Feedback (OP4), Frequenzverhaeltnisse
    {{"ALGO", K_ALGO, 0, 7}, {"FBK", K_NUM, 0, 127}, {"RAT1", K_RATIO, 3, 15}, {"RAT2", K_RATIO, 5, 15},
     {"RAT3", K_RATIO, 3, 15}, {"RAT4", K_RATIO, 3, 15}, {"FINE", K_BIP, 64, 127}, {"NOIS", K_NUM, 0, 127}},
    // SAMPLE (END < STRT = rueckwaerts)
    {{"SMPL", K_SMPL, 0, 249}, {"TUNE", K_SEMI, 24, 48}, {"FINE", K_BIP, 64, 127}, {"STRT", K_NUM, 0, 127},
     {"END", K_NUM, 127, 127}, {"LOOP", K_OFFON, 0, 1}, {"GAIN", K_NUM, 64, 127}, PDEF_NONE},
    // TOM
    {{"TUNE", K_NUM, 50, 127}, {"DCAY", K_NUM, 70, 127}, {"SWEP", K_NUM, 40, 127}, {"NOIS", K_NUM, 30, 127},
     {"BODY", K_NUM, 20, 127}, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    // RIM
    {{"TUNE", K_NUM, 64, 127}, {"DCAY", K_NUM, 30, 127}, {"TONE", K_NUM, 70, 127}, {"CLCK", K_NUM, 80, 127},
     PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    // SHAKER
    {{"TONE", K_NUM, 90, 127}, {"DCAY", K_NUM, 40, 127}, {"ATK", K_NUM, 30, 127}, {"GRIT", K_NUM, 40, 127},
     PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    // WAVETABLE
    {{"TABL", K_WTAB, 0, 3}, {"POS", K_NUM, 0, 127}, {"WARP", K_BIP, 64, 127}, {"DETN", K_NUM, 0, 127},
     {"SUB", K_NUM, 0, 127}, {"NOIS", K_NUM, 0, 127}, {"P.ENV", K_BIP, 64, 127}, {"GLID", K_NUM, 0, 127}},
    // CHOP (Sample in gleiche Scheiben, SLIC 0 = AUTO: Step-Nummer waehlt die Scheibe)
    {{"SMPL", K_SMPL, 0, 249}, {"SLCS", K_NUM, 15, 63}, {"SLIC", K_SLICE, 0, 64}, {"TUNE", K_SEMI, 24, 48},
     {"FINE", K_BIP, 64, 127}, {"LEN", K_PCT, 100, 100}, {"REV", K_OFFON, 0, 1}, {"GAIN", K_NUM, 64, 127}},
    // GRAN
    {{"SMPL", K_SMPL, 0, 249}, {"POS", K_NUM, 30, 127}, {"SPRY", K_NUM, 10, 127}, {"SIZE", K_NUM, 50, 127},
     {"DENS", K_NUM, 60, 127}, {"PTCH", K_SEMI, 24, 48}, {"P.SPR", K_NUM, 0, 127}, {"GAIN", K_NUM, 64, 127}},
};

static const ParamDef MST_DEFS[8] = {
    {"BPM", K_BPM, 0, 0}, {"SWNG", K_SWNG, 0, 30}, {"VOL", K_NUM, 100, 127}, {"DRIVE", K_NUM, 0, 127},
    {"SCALE", K_SCALE, 0, 120}, {"ROOT", K_ROOT, 0, 11}, PDEF_NONE, PDEF_NONE,
};
// zweite Klangseite: nur FM nutzt sie bisher (Pegel = Mod-Index bzw. Ausgangspegel, Decay je Operator)
static const ParamDef SRC2_DEFS[M_COUNT][8] = {
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {{"LVL2", K_NUM, 50, 127}, {"LVL3", K_NUM, 0, 127}, {"LVL4", K_NUM, 0, 127}, {"DEC1", K_DECAY, 127, 127},
     {"DEC2", K_DECAY, 50, 127}, {"DEC3", K_DECAY, 50, 127}, {"DEC4", K_DECAY, 50, 127}, {"VELO", K_NUM, 64, 127}},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
};
inline bool machineHasSrc2(int m) { return m == M_FM; }

// FM-Algorithmen (DX-artig). Bitmaske der Modulatoren je Operator (Bit k = OPk+1) und der Traeger.
static const uint8_t FM_MODS[8][4] = {
    {0x2, 0x4, 0x8, 0}, {0x2, 0xC, 0, 0}, {0x6, 0, 0x8, 0}, {0x2, 0, 0x8, 0},
    {0x8, 0x8, 0x8, 0}, {0, 0x4, 0x8, 0}, {0x2, 0, 0, 0}, {0, 0, 0, 0}};
static const uint8_t FM_CARRIERS[8] = {0x1, 0x1, 0x1, 0x5, 0x7, 0x3, 0xD, 0xF};
// Darstellung: Spalte/Zeile je Operator
static const uint8_t FM_LAYOUT[8][4][2] = {
    {{3, 1}, {2, 1}, {1, 1}, {0, 1}}, {{2, 1}, {1, 1}, {0, 0}, {0, 2}}, {{2, 1}, {1, 0}, {1, 2}, {0, 2}},
    {{1, 0}, {0, 0}, {1, 2}, {0, 2}}, {{1, 0}, {1, 1}, {1, 2}, {0, 1}}, {{2, 0}, {2, 2}, {1, 2}, {0, 2}},
    {{1, 0}, {0, 0}, {1, 1}, {1, 2}}, {{0, 1}, {1, 1}, {2, 1}, {3, 1}}};
static const ParamDef FX_DEFS[8] = {
    {"D.TIME", K_DTIM, 9, 14}, {"D.FBK", K_FBK, 50, 127}, {"D.TONE", K_NUM, 80, 127}, {"D.PING", K_NUM, 127, 127},
    {"R.SIZE", K_NUM, 80, 127}, {"R.DAMP", K_NUM, 60, 127}, {"R.PRE", K_MS, 0, 127}, {"R.WIDE", K_NUM, 100, 127},
};
inline float fxPreMs(int v) { return v * 250.f / 127.f; }

// Kompressor-Seite (Master, mit Sidechain)
enum { CM_THR = 0, CM_RAT, CM_ATK, CM_REL, CM_GAIN, CM_SRC, CM_MIX, CM_GR };
static const ParamDef CMP_DEFS[8] = {
    {"THR", K_THR, 127, 127}, {"RATIO", K_CRAT, 3, 8}, {"ATK", K_CATK, 50, 127}, {"REL", K_CREL, 60, 127},
    {"GAIN", K_CGAIN, 0, 127}, {"SC", K_SCSRC, 0, 8}, {"MIX", K_NUM, 127, 127}, {"GR", K_GR, 0, 0},
};
static const float CMP_RATIOS[9] = {1.5f, 2, 3, 4, 6, 8, 12, 20, 1000};
inline float cmpThrDb(int v) { return -48.f + v * 48.f / 127.f; }
inline float cmpAtkMs(int v) { return 0.1f * exp2f(v * (10.f / 127.f)); }
inline float cmpRelMs(int v) { return 10.f * exp2f(v * (7.6f / 127.f)); }
inline float cmpGainDb(int v) { return v * 24.f / 127.f; }

// System-Seite (nicht im Projekt, sondern in settings.cfg)
enum { SY_OUT = 0, SY_BUF, SY_MIN, SY_NOUT, SY_CLK, SY_MCH, SY_USB, SY_PORT };
static const ParamDef SYS_DEFS[8] = {
    {"OUT", K_DEV, 0, 0}, {"BUF", K_BUFS, 1, 3}, {"M.IN", K_OFFON, 1, 1}, {"N.OUT", K_OFFON, 0, 1},
    {"CLK", K_CLK, 0, 2}, {"M.CH", K_CHM, 0, 1}, {"USB", K_USB, 0, 2}, {"MIDI", K_INFO, 0, 0},
};
static const int BUF_SIZES[4] = {128, 256, 512, 1024};

static const float FM_RATIOS[16] = {0.25f, 0.5f, 0.75f, 1, 1.5f, 2, 2.5f, 3, 3.5f, 4, 5, 6, 7, 8, 9, 11};
static const float DLY_STEPS[15] = {0.25f, 0.5f, 2.f / 3, 0.75f, 1, 4.f / 3, 1.5f, 2, 8.f / 3, 3, 4, 16.f / 3, 6, 8, 16};
static const int RTRG_DIV[7] = {0, 2, 3, 4, 6, 8, 16};

inline const ParamDef& paramDef(int machine, int idx) {
    if (idx >= P_S0 && idx <= P_S7) return SRC_DEFS[machine][idx - P_S0];
    if (idx >= P_X0 && idx <= P_X7) return SRC2_DEFS[machine][idx - P_X0];
    return COMMON_DEFS[idx];
}

// Laenge in Steps (Wert 127 = unendlich -> negativ)
inline float lenSteps(int v) { return v >= 127 ? -1.f : 0.125f * powf(2.f, v / 16.f); }

inline void fmtParam(const ParamDef& d, int v, char* buf, int n) {
    static const char* notes[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    static const char* waves[4] = {"SAW", "SQR", "TRI", "SIN"};
    static const char* ftypes[FT_COUNT] = {"LP12", "BP", "HP12", "LP24", "LADR", "HP24", "NTCH", "PEAK", "CMB+", "CMB-", "VOWL"};
    static const char* ldst[6] = {"PIT", "FRQ", "VOL", "PAN", "RES", "CRS"};
    static const char* lwav[LW_COUNT] = {"SIN", "TRI", "SAW+", "SAW-", "SQR", "EXP+", "EXP-", "S&H", "RND"};
    static const char* dtim[15] = {"1/64", "1/32", "1/16T", "1/32.", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/1"};
    static const char* lsync[12] = {"FREE", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T"};
    static const char* lmode[5] = {"FREE", "TRIG", "HOLD", "ONE", "HALF"};
    switch (d.kind) {
    case K_NONE: snprintf(buf, n, "-"); break;
    case K_NUM: snprintf(buf, n, "%d", v); break;
    case K_BIP: snprintf(buf, n, "%+d", v - 64); break;
    case K_NOTE: snprintf(buf, n, "%s%d", notes[v % 12], v / 12 - 1); break;
    case K_PCT: snprintf(buf, n, "%d%%", v); break;
    case K_LEN: {
        float s = lenSteps(v);
        if (s < 0) snprintf(buf, n, "INF");
        else if (s < 1) snprintf(buf, n, "%.2f", s);
        else snprintf(buf, n, "%.1f", s);
    } break;
    case K_WAVE: snprintf(buf, n, "%s", waves[v & 3]); break;
    case K_FTYPE: snprintf(buf, n, "%s", ftypes[v % FT_COUNT]); break;
    case K_LDST: snprintf(buf, n, "%s", ldst[v % 6]); break;
    case K_LWAV: snprintf(buf, n, "%s", lwav[v % LW_COUNT]); break;
    case K_LSYNC: snprintf(buf, n, "%s", lsync[v % 12]); break;
    case K_LMODE: snprintf(buf, n, "%s", lmode[v % 5]); break;
    case K_LDEST: if (v) snprintf(buf, n, "P%d", v - 1); else snprintf(buf, n, "---"); break;
    case K_DIR: { static const char* dn[6] = {"FWD", "REV", "PING", "PEND", "RND", "DRNK"}; snprintf(buf, n, "%s", dn[v % 6]); } break;
    case K_TLEN: snprintf(buf, n, "%d", v); break;
    case K_TSPD: snprintf(buf, n, "%s", SPEED_NAMES[v % 7]); break;
    case K_ARPM: snprintf(buf, n, "%s", ARP_MODES[v % 6]); break;
    case K_ACHD: snprintf(buf, n, "%s", ARP_CHORDS[v % 10]); break;
    case K_ARAT: snprintf(buf, n, "%s", ARP_RATES[v % 6]); break;
    case K_RANGE: if (v) snprintf(buf, n, "+%d", v); else snprintf(buf, n, "AUS"); break;
    case K_MTIM: if (v == 23) snprintf(buf, n, "0"); else snprintf(buf, n, "%+d/24", v - 23); break;
    case K_WTAB: { static const char* wt[4] = {"BASIC", "PWM", "VOKAL", "OBERT"}; snprintf(buf, n, "%s", wt[v & 3]); } break;
    case K_SLICE: if (v) snprintf(buf, n, "%d", v); else snprintf(buf, n, "AUTO"); break;
    case K_FBK: snprintf(buf, n, "%d%%", v * 120 / 127); break;
    case K_COND:
        if (v <= 0) snprintf(buf, n, "---");
        else if (v < CND_AB) { static const char* cn[9] = {"", "FILL", "!FILL", "PRE", "!PRE", "NEI", "!NEI", "1ST", "!1ST"}; snprintf(buf, n, "%s", cn[v]); }
        else { int a, b; bool ng; condDecode(v, a, b, ng); snprintf(buf, n, "%s%d:%d", ng ? "!" : "", a, b); }
        break;
    case K_TRG: snprintf(buf, n, "%s", v ? "NOTE" : "FREE"); break;
    case K_RTRG: if (v) snprintf(buf, n, "x%d", RTRG_DIV[v]); else snprintf(buf, n, "OFF"); break;
    case K_RATIO: snprintf(buf, n, "%.2f", FM_RATIOS[v & 15]); break;
    case K_DTIM: snprintf(buf, n, "%s", dtim[v % 15]); break;
    case K_SWNG: snprintf(buf, n, "%d%%", 50 + v); break;
    case K_CNT: snprintf(buf, n, "%d", v + 1); break;
    case K_BPM: snprintf(buf, n, "%d", v); break;
    case K_SEMI: snprintf(buf, n, "%+d", v - 24); break;
    case K_OFFON: snprintf(buf, n, "%s", v ? "ON" : "OFF"); break;
    case K_BUFS: snprintf(buf, n, "%d", BUF_SIZES[v & 3]); break;
    case K_CLK: snprintf(buf, n, "%s", v == 1 ? "SEND" : (v == 2 ? "RECV" : "OFF")); break;
    case K_CHM: snprintf(buf, n, "%s", v ? "1-8" : "AUTO"); break;
    case K_SCALE:
        if (v <= 0) snprintf(buf, n, "CHROM");
        else snprintf(buf, n, "%s", SCALE_NAMES[((v - 1) / 12) % NSCALES]);
        break;
    case K_ROOT: snprintf(buf, n, "%s", NOTE_NAMES[v % 12]); break;
    case K_ALGO: snprintf(buf, n, "A%d", v + 1); break;
    case K_DECAY: if (v >= 127) snprintf(buf, n, "INF"); else snprintf(buf, n, "%d", v); break;
    case K_ACT: snprintf(buf, n, ">>"); break;
    case K_SLOT: snprintf(buf, n, "%02d", v + 1); break;
    case K_INS: snprintf(buf, n, "%s", v < INS_COUNT ? INS_NAMES[v] : "?"); break;
    case K_MS: snprintf(buf, n, "%.0fms", v * 250.f / 127.f); break;
    case K_MACH: snprintf(buf, n, "%s", v < M_COUNT ? MACHINE_NAMES[v] : "?"); break;
    case K_THR: snprintf(buf, n, "%.0fdB", cmpThrDb(v)); break;
    case K_CRAT: if (v >= 8) snprintf(buf, n, "INF"); else snprintf(buf, n, "%g:1", CMP_RATIOS[v]); break;
    case K_CATK: { float ms = cmpAtkMs(v); if (ms < 10) snprintf(buf, n, "%.1fms", ms); else snprintf(buf, n, "%.0fms", ms); } break;
    case K_CREL: { float ms = cmpRelMs(v); if (ms < 1000) snprintf(buf, n, "%.0fms", ms); else snprintf(buf, n, "%.1fs", ms / 1000); } break;
    case K_CGAIN: snprintf(buf, n, "+%.1f", cmpGainDb(v)); break;
    case K_SCSRC: if (v) snprintf(buf, n, "T%d", v); else snprintf(buf, n, "INT"); break;
    case K_USB: snprintf(buf, n, "%s", v == 1 ? "PC" : (v == 2 ? "PC+A" : "HOST")); break;
    default: snprintf(buf, n, "%d", v); break;
    }
}
