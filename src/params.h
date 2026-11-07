// Parameter-Definitionen der Groovebox (Elektron-artige Seiten mit je 8 Reglern)
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <math.h>

// Speicher-Seiten je Spur (je 8 Parameter): 0 TRG, 1 SRC, 2 FLT, 3 AMP, 4 LFO1, 5 INS, 6 SRC2, 7 LFO2, 8 LFO3, 9 SEQ, 10 FLT2, 11 ARP
// Sprache der Oberflaeche: 0 Deutsch, 1 English (definiert in main.cpp). TR waehlt den passenden Text.
extern int g_lang;
#define TR(de, en) (g_lang ? (en) : (de))

enum { NTRACKS = 8, NSTEPS = 128, NPATTERNS = 16, NPAGES = 21, NPARAMS = NPAGES * 8, NLFO = 3, NBUS = 3 };
enum { OLD_NPARAMS = 96, V13_NPARAMS = 128 };   // bis Projektformat v12: 12 Seiten, v13: 16 Seiten
enum { NENV = 3, NMODT = 3 };   // Huellkurven je Spur, Ziele je Huellkurve/LFO
// Oberflaechen-Seiten (Reihenfolge = Navigation). Spurseiten zuerst.
// neue Seiten nur hinten anfuegen (Nummern dienen auch als Screenshot-Seiten)
enum UiPage { PG_TRG, PG_SEQ, PG_ARP, PG_SRC, PG_SRC2, PG_FLT, PG_FLT2, PG_AMP, PG_LFO1, PG_LFO2, PG_LFO3, PG_INS, PG_MIX, PG_FX, PG_CMP, PG_PRJ, PG_SONG, PG_MST, PG_SYS, PG_LIVE,
              PG_SRC3, PG_SRC4, PG_SRC5, PG_SEND, PG_FXB, PG_FXC, PG_ENV1, PG_ENV2, PG_ENV3, NUI_PAGES };
enum { PG_LFO = PG_LFO1 };
enum { PAGE_INS = PG_INS, PAGE_FX = PG_FX, PAGE_CMP = PG_CMP, PAGE_MST = PG_MST, PAGE_SYS = PG_SYS, PAGE_MIX = PG_MIX, PAGE_PRJ = PG_PRJ,
       FIRST_GLOBAL_PAGE = PG_MIX };
static const int PG_STORE[NUI_PAGES] = {0, 9, 11, 1, 6, 2, 10, 3, 4, 7, 8, 5, -1, -1, -1, -1, -1, -1, -1, -1, 12, 13, 14, 15, -1, -1, 16, 17, 18};   // Spurseite -> (erste) Speicherseite; Regler: uiParam()
static const int PG_GROUP[NUI_PAGES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0};
inline int busOfPage(int p) { return p == PG_FX ? 0 : (p == PG_FXB ? 1 : (p == PG_FXC ? 2 : -1)); }
static const char* const GROUP_NAMES_DE[3] = {"SPUR", "LIVE, MIX & FX", "PROJEKT"};
static const char* const GROUP_NAMES_EN[3] = {"TRACK", "LIVE, MIX & FX", "PROJECT"};
#define GROUP_NAMES (g_lang ? GROUP_NAMES_EN : GROUP_NAMES_DE)
enum { NO_LOCK = 255 };

// Reihenfolge = gespeicherter Wert (neue Engines nur hinten anfuegen!)
enum Machine { M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_FM, M_SAMPLE, M_TOM, M_RIM, M_SHKR, M_WTBL, M_CHOP, M_GRAN, M_COUNT };
static const char* const MACHINE_NAMES[M_COUNT] = {"KICK", "SNARE", "HAT", "CLAP", "SYNTH", "FM", "SMPL", "TOM", "RIM", "SHKR", "WTBL", "CHOP", "GRAN"};
static const char* const MACHINE_LONG_DE[M_COUNT] = {"Bassdrum", "Snare", "Hi-Hat", "Clap", "Analog-Synth", "FM-Synth (4 OP)", "Sample-Player",
                                                     "Tom", "Rimshot", "Shaker", "Wavetable-Synth", "Sample-Chopper", "Granulator"};
static const char* const MACHINE_LONG_EN[M_COUNT] = {"Bass drum", "Snare", "Hi-hat", "Clap", "Analog synth", "FM synth (4 OP)", "Sample player",
                                                     "Tom", "Rimshot", "Shaker", "Wavetable synth", "Sample chopper", "Granulator"};
#define MACHINE_LONG (g_lang ? MACHINE_LONG_EN : MACHINE_LONG_DE)
// Anzeige-Reihenfolge im Engine-Waehler: Drums, Synths, Samples
static const uint8_t MACHINE_ORDER[M_COUNT] = {M_KICK, M_SNARE, M_HAT, M_CLAP, M_TOM, M_RIM, M_SHKR, M_SYNTH, M_FM, M_WTBL, M_SAMPLE, M_CHOP, M_GRAN};
inline bool machineIsDrum(int m) { return m <= M_CLAP || m == M_TOM || m == M_RIM || m == M_SHKR; }
inline bool machineUsesSample(int m) { return m == M_SAMPLE || m == M_CHOP || m == M_GRAN; }
static const char* const PAGE_NAMES[NUI_PAGES] = {"TRG", "SEQ", "ARP", "SRC", "SR2", "FLT", "FL2", "AMP", "LF1", "LF2", "LF3", "INS", "MIX", "FX A", "CMP", "PRJ", "SNG", "MST", "SET", "LIVE",
                                                  "SR3", "SR4", "SR5", "SND", "FX B", "FX C", "EN1", "EN2", "EN3"};
static const char* const PAGE_LONG_DE[NUI_PAGES] = {"TRIG", "SEQUENCER", "ARPEGGIATOR", "KLANG", "KLANG 2", "FILTER", "FILTER 2 (BASE-WIDTH)", "VERSTÄRKER", "LFO 1", "LFO 2", "LFO 3", "INSERT-FX",
                                                    "MIXER", "EFFEKT-BUS A", "KOMPRESSOR", "PROJEKT", "SONG", "MASTER", "EINSTELLUNGEN", "LIVE",
                                                    "KLANG 3", "KLANG 4", "KLANG 5", "SENDS", "EFFEKT-BUS B", "EFFEKT-BUS C",
                                                    "HÜLLKURVE 1", "HÜLLKURVE 2", "HÜLLKURVE 3"};
static const char* const PAGE_LONG_EN[NUI_PAGES] = {"TRIG", "SEQUENCER", "ARPEGGIATOR", "SOUND", "SOUND 2", "FILTER", "FILTER 2 (BASE-WIDTH)", "AMPLIFIER", "LFO 1", "LFO 2", "LFO 3", "INSERT FX",
                                                    "MIXER", "EFFECT BUS A", "COMPRESSOR", "PROJECT", "SONG", "MASTER", "SETTINGS", "LIVE",
                                                    "SOUND 3", "SOUND 4", "SOUND 5", "SENDS", "EFFECT BUS B", "EFFECT BUS C",
                                                    "ENVELOPE 1", "ENVELOPE 2", "ENVELOPE 3"};
#define PAGE_LONG (g_lang ? PAGE_LONG_EN : PAGE_LONG_DE)

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
    K_ARPM, K_ACHD, K_ARAT, K_OCT, K_SUBO, K_FXT, K_WDRAW, K_EDEST
};

struct ParamDef { const char* name; Kind kind; uint8_t def; uint8_t max; };

// Parameterindizes (Seite*8 + Regler)
enum {
    P_NOTE = 0, P_VEL, P_LEN, P_PROB, P_RTRG, P_MTIM, P_T6, P_COND, P_T7 = P_COND, P_RFAD = P_MTIM,
    P_S0 = 8, P_S1, P_S2, P_S3, P_S4, P_S5, P_S6, P_S7,
    P_FREQ = 16, P_RESO, P_FTYP, P_FENV, P_FATK, P_FDEC, P_FSUS, P_FREL,
    P_AATK = 24, P_ADEC, P_ASUS, P_AREL, P_VOL, P_PAN, P_A6, P_A7,   // P_A6/P_A7 = bis v12 DLY/REV (jetzt auf der SEND-Seite)
    P_L1 = 32,                                                   // LFO1 (Seite 4)
    P_ITYP = 40, P_IP1, P_IP2, P_IP3, P_IMIX, P_CRSH, P_SRR, P_DRIV,
    P_X0 = 48, P_X1, P_X2, P_X3, P_X4, P_X5, P_X6, P_X7,      // SRC2 (maschinenabhaengig)
    P_L2 = 56, P_L3 = 64,                                        // LFO2, LFO3
    P_DIR = 72, P_QLEN, P_QSPD, P_PULS, P_EROT, P_DENS, P_CHNG, P_RANGE,  // SEQ: Richtung, Laenge, Tempo, Euklid, Generator
    P_BASE = 80, P_WDTH, P_KEYT, P_VELF, P_FCRV, P_ACRV,                                   // FLT2: Base-Width-Filter, Key-/Velocity-Tracking
    P_AMOD = 88, P_ACHD, P_AOCT, P_ARAT, P_AGAT, P_AACC,                   // ARP: Modus, Akkord, Oktaven, Tempo, Gate, Akzent
    P_Y0 = 96, P_Z0 = 104, P_W0 = 112,                                     // ab v13: Klangseiten 3-5 (FM: Operator 2-4)
    P_DLY = 120, P_REV, P_SNDC,                                            // ab v13: Sends an Effekt-Bus A/B/C
    // ab v14: Huellkurven 1-3 mit je 3 Zielen (ENV1 = frueher AMP-, ENV2 = Filter-Huellkurve: ADSR/Kurve bleiben an ihrem Platz)
    P_E1D1 = 128, P_E1A1, P_E1D2, P_E1A2, P_E1D3, P_E1A3, P_E1VEL,
    P_E2D1 = 136, P_E2D2, P_E2A2, P_E2D3, P_E2A3, P_E2VEL,               // ENV2 Tiefe 1 = P_FENV (frueher Filter-ENV)
    P_E3ATK = 144, P_E3DEC, P_E3SUS, P_E3REL, P_E3CRV, P_E3VEL, P_E3D1, P_E3A1,
    P_E3D2 = 152, P_E3A2, P_E3D3, P_E3A3,
    P_L1D2 = 156, P_L1A2, P_L1D3, P_L1A3,                                 // ab v14: LFO-Ziele 2 und 3
    P_L2D2 = 160, P_L2A2, P_L2D3, P_L2A3,
    P_L3D2 = 164, P_L3A2, P_L3D3, P_L3A3,
    OLD_P_DLY = 30, OLD_P_REV = 31                                         // bis v12
};
// FM: Operator k liegt auf einer eigenen Seite (OP1 = SRC2, OP2-4 = SRC3-5): RATIO FINE DETN LEVEL ATK DEC SUS REL
static const int FM_OPB[4] = {P_X0, P_Y0, P_Z0, P_W0};
enum { OP_RAT = 0, OP_FINE, OP_DETN, OP_LVL, OP_ATK, OP_DEC, OP_SUS, OP_REL };
inline int fmOpOf(int idx) { for (int k = 0; k < 4; k++) if (idx >= FM_OPB[k] && idx < FM_OPB[k] + 8) return k; return -1; }
// Master-Seite
enum { MS_BPM = 0, MS_SWNG, MS_VOL, MS_DRIV, MS_SCALE, MS_ROOT };
// LFO: Versatz innerhalb einer LFO-Seite
enum { LF_RATE = 0, LF_SYNC, LF_DEST, LF_DEPTH, LF_WAVE, LF_MODE, LF_PHASE, LF_FADE };
static const int LFO_BASE[NLFO] = {32, 56, 64};
// Modulationsziele: Wert 0 = aus, 1..NPARAMS = Parameter + 1, MOD_VCA = Lautstaerke der Stimme (nur Huellkurven)
enum { MOD_VCA = NPARAMS + 1 };
static const int LFO_DST[NLFO][NMODT] = {{32 + LF_DEST, P_L1D2, P_L1D3}, {56 + LF_DEST, P_L2D2, P_L2D3}, {64 + LF_DEST, P_L3D2, P_L3D3}};
static const int LFO_DEP[NLFO][NMODT] = {{32 + LF_DEPTH, P_L1A2, P_L1A3}, {56 + LF_DEPTH, P_L2A2, P_L2A3}, {64 + LF_DEPTH, P_L3A2, P_L3A3}};
static const int ENV_ATK[NENV] = {P_AATK, P_FATK, P_E3ATK}, ENV_DEC[NENV] = {P_ADEC, P_FDEC, P_E3DEC},
                 ENV_SUS[NENV] = {P_ASUS, P_FSUS, P_E3SUS}, ENV_REL[NENV] = {P_AREL, P_FREL, P_E3REL},
                 ENV_CRV[NENV] = {P_ACRV, P_FCRV, P_E3CRV}, ENV_VEL[NENV] = {P_E1VEL, P_E2VEL, P_E3VEL};
static const int ENV_DST[NENV][NMODT] = {{P_E1D1, P_E1D2, P_E1D3}, {P_E2D1, P_E2D2, P_E2D3}, {P_E3D1, P_E3D2, P_E3D3}};
static const int ENV_DEP[NENV][NMODT] = {{P_E1A1, P_E1A2, P_E1A3}, {P_FENV, P_E2A2, P_E2A3}, {P_E3A1, P_E3A2, P_E3A3}};
inline int envOfPage(int p) { return p == PG_ENV1 ? 0 : (p == PG_ENV2 ? 1 : (p == PG_ENV3 ? 2 : -1)); }
inline int lfoOfPage(int p) { return p == PG_LFO1 ? 0 : (p == PG_LFO2 ? 1 : (p == PG_LFO3 ? 2 : -1)); }
// Regler einer Spurseite: Anzahl (8, Huellkurven/LFO 12) und Parameterindex je Regler (-1 = leer)
inline int uiSlots(int p) { return envOfPage(p) >= 0 || lfoOfPage(p) >= 0 ? 12 : 8; }
inline int uiParam(int p, int k) {
    if (k < 0 || k >= uiSlots(p) || p < 0 || p >= NUI_PAGES || PG_STORE[p] < 0) return -1;
    int e = envOfPage(p), l = lfoOfPage(p);
    if (e >= 0) {   // ATK DEC SUS REL | DEST1 DEPTH1 DEST2 DEPTH2 | DEST3 DEPTH3 CURVE VELO
        static const int* const A[4] = {ENV_ATK, ENV_DEC, ENV_SUS, ENV_REL};
        if (k < 4) return A[k][e];
        if (k < 10) return ((k - 4) & 1) ? ENV_DEP[e][(k - 4) / 2] : ENV_DST[e][(k - 4) / 2];
        return k == 10 ? ENV_CRV[e] : ENV_VEL[e];
    }
    if (l >= 0 && k >= 8) return ((k - 8) & 1) ? LFO_DEP[l][1 + (k - 8) / 2] : LFO_DST[l][1 + (k - 8) / 2];
    if (p == PG_FLT) { static const int F[8] = {P_FREQ, P_RESO, P_FTYP, P_KEYT, P_VELF, -1, -1, -1}; return F[k]; }   // Huellkurve -> ENV2
    if (p == PG_FLT2) return k < 2 ? P_BASE + k : -1;
    if (p == PG_AMP) return k == 0 ? P_VOL : (k == 1 ? P_PAN : -1);                                            // Huellkurve -> ENV1
    return PG_STORE[p] * 8 + k;
}
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
    {"FREQ", K_NUM, 127, 127}, {"RESO", K_NUM, 0, 127}, {"TYPE", K_FTYPE, 0, FT_COUNT - 1}, {"DEPTH1", K_BIP, 64, 127},   // ab v14: Tiefe 1 von ENV2
    {"ATK", K_NUM, 0, 127}, {"DEC", K_NUM, 60, 127}, {"SUS", K_NUM, 0, 127}, {"REL", K_NUM, 60, 127},      // ENV2 (Filter-Huellkurve)
    // AMP
    {"ATK", K_NUM, 0, 127}, {"DEC", K_NUM, 80, 127}, {"SUS", K_NUM, 127, 127}, {"REL", K_NUM, 50, 127},
    {"VOL", K_NUM, 100, 127}, {"PAN", K_BIP, 64, 127}, PDEF_NONE, PDEF_NONE,
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
    {"CURVE", K_BIP, 64, 127}, {"CURVE", K_BIP, 64, 127}, PDEF_NONE, PDEF_NONE,   // Huellkurvenform ENV2/ENV1 (ab v11)
    // ARP: spielt waehrend der Trig-Laenge die Akkordtoene ueber OCT Oktaven im Tempo RATE
    {"MODE", K_ARPM, 0, 5}, {"CHRD", K_ACHD, 2, 9}, {"OCT", K_CNT, 0, 3}, {"RATE", K_ARAT, 2, 5},
    {"GATE", K_PCT, 60, 100}, {"ACCT", K_PCT, 0, 100}, PDEF_NONE, PDEF_NONE,
    // SRC3-5 (maschinenabhaengig: FM-Operatoren 2-4)
    PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE,
    PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE,
    PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE,
    // SEND: Anteil an Effekt-Bus A/B/C (bis v12: DLY/REV auf der AMP-Seite)
    {"SND.A", K_NUM, 0, 127}, {"SND.B", K_NUM, 0, 127}, {"SND.C", K_NUM, 0, 127}, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE,
    // ab v14: ENV1-Ziele (Ziel 1 = Lautstaerke wie frueher die AMP-Huellkurve)
    {"DEST1", K_EDEST, MOD_VCA, MOD_VCA}, {"DEPTH1", K_BIP, 127, 127}, {"DEST2", K_EDEST, 0, MOD_VCA}, {"DEPTH2", K_BIP, 64, 127},
    {"DEST3", K_EDEST, 0, MOD_VCA}, {"DEPTH3", K_BIP, 64, 127}, {"VELO", K_PCT, 0, 100}, PDEF_NONE,
    // ENV2-Ziele (Ziel 1 = Filterfrequenz, Tiefe 1 = P_FENV)
    {"DEST1", K_EDEST, P_FREQ + 1, MOD_VCA}, {"DEST2", K_EDEST, 0, MOD_VCA}, {"DEPTH2", K_BIP, 64, 127}, {"DEST3", K_EDEST, 0, MOD_VCA},
    {"DEPTH3", K_BIP, 64, 127}, {"VELO", K_PCT, 0, 100}, PDEF_NONE, PDEF_NONE,
    // ENV3 (frei)
    {"ATK", K_NUM, 0, 127}, {"DEC", K_NUM, 60, 127}, {"SUS", K_NUM, 0, 127}, {"REL", K_NUM, 60, 127},
    {"CURVE", K_BIP, 64, 127}, {"VELO", K_PCT, 0, 100}, {"DEST1", K_EDEST, 0, MOD_VCA}, {"DEPTH1", K_BIP, 64, 127},
    {"DEST2", K_EDEST, 0, MOD_VCA}, {"DEPTH2", K_BIP, 64, 127}, {"DEST3", K_EDEST, 0, MOD_VCA}, {"DEPTH3", K_BIP, 64, 127},
    // LFO1-3: Ziele 2 und 3
    {"DEST2", K_LDEST, 0, NPARAMS}, {"DEPTH2", K_BIP, 64, 127}, {"DEST3", K_LDEST, 0, NPARAMS}, {"DEPTH3", K_BIP, 64, 127},
    {"DEST2", K_LDEST, 0, NPARAMS}, {"DEPTH2", K_BIP, 64, 127}, {"DEST3", K_LDEST, 0, NPARAMS}, {"DEPTH3", K_BIP, 64, 127},
    {"DEST2", K_LDEST, 0, NPARAMS}, {"DEPTH2", K_BIP, 64, 127}, {"DEST3", K_LDEST, 0, NPARAMS}, {"DEPTH3", K_BIP, 64, 127},
};
// Arpeggiator: Akkord-Intervalle (Halbtoene), Tempo in Steps
static const char* const ARP_MODES_DE[6] = {"AUS", "UP", "DOWN", "UPDN", "RAND", "AKKD"};   // AKKD = Akkordtoene gleichzeitig
static const char* const ARP_MODES_EN[6] = {"OFF", "UP", "DOWN", "UPDN", "RAND", "CHRD"};
#define ARP_MODES (g_lang ? ARP_MODES_EN : ARP_MODES_DE)
enum { ARP_CHORD = 5, NCHORDV = 3 };   // Zusatzstimmen je Spur fuer Akkorde
static const char* const ARP_CHORDS_DE[10] = {"OKT", "5TH", "MAJ", "MIN", "SUS4", "MAJ7", "MIN7", "DOM7", "DIM", "SKALA"};
static const char* const ARP_CHORDS_EN[10] = {"OCT", "5TH", "MAJ", "MIN", "SUS4", "MAJ7", "MIN7", "DOM7", "DIM", "SCALE"};
#define ARP_CHORDS (g_lang ? ARP_CHORDS_EN : ARP_CHORDS_DE)
static const int8_t ARP_IV[10][5] = {{0, -1}, {0, 7, -1}, {0, 4, 7, -1}, {0, 3, 7, -1}, {0, 5, 7, -1},
                                     {0, 4, 7, 11, -1}, {0, 3, 7, 10, -1}, {0, 4, 7, 10, -1}, {0, 3, 6, 9, -1}, {-1}};
static const char* const ARP_RATES[6] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4"};
static const float ARP_STEPS[6] = {0.5f, 2.f / 3.f, 1.f, 4.f / 3.f, 2.f, 4.f};
// Tonskalen pro Spur: Wert 0 = chromatisch, sonst 1 + Skala*12 + Grundton
enum { NSCALES = 10 };
static const char* const SCALE_NAMES[NSCALES] = {"MAJ", "MIN", "DOR", "PHR", "LYD", "MIX", "HMI", "PMA", "PMI", "BLU"};
static const char* const SCALE_LONG_DE[NSCALES] = {"DUR", "MOLL", "DORISCH", "PHRYGISCH", "LYDISCH", "MIXOLYDISCH",
                                                   "HARM. MOLL", "PENTA DUR", "PENTA MOLL", "BLUES"};
static const char* const SCALE_LONG_EN[NSCALES] = {"MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN",
                                                   "HARM. MINOR", "PENTA MAJOR", "PENTA MINOR", "BLUES"};
#define SCALE_LONG (g_lang ? SCALE_LONG_EN : SCALE_LONG_DE)
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
    {{"ALGO", K_ALGO, 0, 7}, {"FBK", K_NUM, 0, 127}, {"VELO", K_NUM, 64, 127}, {"PITCH", K_BIP, 64, 127},
     {"P.DEC", K_NUM, 40, 127}, {"NOIS", K_NUM, 0, 127}, PDEF_NONE, PDEF_NONE},
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
    {{"TABL", K_WTAB, 0, 4}, {"POS", K_NUM, 0, 127}, {"WARP", K_BIP, 64, 127}, {"DETN", K_NUM, 0, 127},
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
// zweite Klangseite: SYNTH (Oktaven, Pegel je Oszillator, Sync, Kreuzmodulation), WTBL (Oktave, Pegel, eigene Wavetable), FM: Operator 1 (FM_OP_DEFS)
static const ParamDef SRC2_DEFS[M_COUNT][8] = {
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    // SYNTH
    {{"OCT1", K_OCT, 3, 6}, {"OCT2", K_OCT, 3, 6}, {"LVL1", K_NUM, 127, 127}, {"LVL2", K_NUM, 127, 127},
     {"SUB.O", K_SUBO, 0, 1}, {"SYNC", K_OFFON, 0, 1}, {"XMOD", K_NUM, 0, 127}, {"RING", K_NUM, 0, 127}},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},   // FM: siehe FM_OP_DEFS
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    // WTBL (TABL = USER: Wavetable aus dem Sample UWAV, je 2048 bzw. 256 Werte ein Frame)
    {{"OCT", K_OCT, 3, 6}, {"LVL", K_NUM, 127, 127}, {"SUB.O", K_SUBO, 0, 1}, {"UWAV", K_SMPL, 0, 249},
     {"DRAW", K_WDRAW, 0, 0}, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
    {PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE},
};
// FM-Operatoren 1-4: Ratio (Tabelle FM_RATIOS), FINE = +-0,5 Ratio, DETN = +-50 Cent, Pegel, eigene Huellkurve
static const ParamDef FM_OP_DEFS[4][8] = {
    {{"RAT", K_RATIO, 3, 27}, {"FINE", K_BIP, 64, 127}, {"DETN", K_BIP, 64, 127}, {"O.LVL", K_NUM, 127, 127},
     {"O.ATK", K_NUM, 0, 127}, {"O.DEC", K_NUM, 70, 127}, {"O.SUS", K_NUM, 110, 127}, {"O.REL", K_NUM, 55, 127}},
    {{"RAT", K_RATIO, 7, 27}, {"FINE", K_BIP, 64, 127}, {"DETN", K_BIP, 64, 127}, {"O.LVL", K_NUM, 55, 127},
     {"O.ATK", K_NUM, 0, 127}, {"O.DEC", K_NUM, 60, 127}, {"O.SUS", K_NUM, 40, 127}, {"O.REL", K_NUM, 50, 127}},
    {{"RAT", K_RATIO, 3, 27}, {"FINE", K_BIP, 64, 127}, {"DETN", K_BIP, 64, 127}, {"O.LVL", K_NUM, 0, 127},
     {"O.ATK", K_NUM, 0, 127}, {"O.DEC", K_NUM, 55, 127}, {"O.SUS", K_NUM, 30, 127}, {"O.REL", K_NUM, 50, 127}},
    {{"RAT", K_RATIO, 3, 27}, {"FINE", K_BIP, 64, 127}, {"DETN", K_BIP, 64, 127}, {"O.LVL", K_NUM, 0, 127},
     {"O.ATK", K_NUM, 0, 127}, {"O.DEC", K_NUM, 50, 127}, {"O.SUS", K_NUM, 20, 127}, {"O.REL", K_NUM, 50, 127}}};
inline bool machineHasSrc2(int m) { return m == M_FM || m == M_SYNTH || m == M_WTBL; }
inline bool machineHasSrc35(int m) { return m == M_FM; }   // Klangseiten 3-5 (FM: Operator 2-4)
// Parameter, der ein Sample aus dem Pool waehlt (SMPL/CHOP/GRAN: SMPL, WTBL: UWAV = eigene Wavetable), sonst -1
inline int machineSampleParam(int m) { return (m == M_SAMPLE || m == M_CHOP || m == M_GRAN) ? P_S0 : (m == M_WTBL ? P_X3 : -1); }

// Effekt-Busse A/B/C: TYPE, 5 Regler je nach Typ, FWD (Anteil an den naechsten Bus), LVL (Rueckweg)
enum BusType { BF_OFF, BF_DELAY, BF_TAPE, BF_REVERB, BF_SPRING, BF_CHORUS, BF_COUNT };
static const char* const BUS_TYPE_NAMES[BF_COUNT] = {"OFF", "DELAY", "TAPE", "REVERB", "SPRING", "CHORUS"};
enum { BX_TYPE = 0, BX_P1, BX_P2, BX_P3, BX_P4, BX_P5, BX_FWD, BX_LVL };
static const ParamDef BUS_DEFS[BF_COUNT][8] = {
    {{"TYPE", K_FXT, 0, BF_COUNT - 1}, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, {"FWD", K_NUM, 0, 127}, {"LVL", K_NUM, 100, 127}},
    {{"TYPE", K_FXT, 0, BF_COUNT - 1}, {"TIME", K_DTIM, 9, 14}, {"FBK", K_FBK, 50, 127}, {"TONE", K_NUM, 80, 127}, {"PING", K_NUM, 127, 127},
     {"LOCUT", K_NUM, 10, 127}, {"FWD", K_NUM, 0, 127}, {"LVL", K_NUM, 100, 127}},
    {{"TYPE", K_FXT, 0, BF_COUNT - 1}, {"TIME", K_DTIM, 7, 14}, {"FBK", K_FBK, 55, 127}, {"WOW", K_NUM, 40, 127}, {"SAT", K_NUM, 50, 127},
     {"AGE", K_NUM, 50, 127}, {"FWD", K_NUM, 0, 127}, {"LVL", K_NUM, 100, 127}},
    {{"TYPE", K_FXT, 0, BF_COUNT - 1}, {"SIZE", K_NUM, 80, 127}, {"DAMP", K_NUM, 60, 127}, {"PRE", K_MS, 0, 127}, {"WIDE", K_NUM, 100, 127},
     {"MOD", K_NUM, 40, 127}, {"FWD", K_NUM, 0, 127}, {"LVL", K_NUM, 100, 127}},
    {{"TYPE", K_FXT, 0, BF_COUNT - 1}, {"DECAY", K_NUM, 70, 127}, {"TONE", K_NUM, 70, 127}, {"DRIP", K_NUM, 70, 127}, {"WIDE", K_NUM, 90, 127},
     {"LOCUT", K_NUM, 30, 127}, {"FWD", K_NUM, 0, 127}, {"LVL", K_NUM, 100, 127}},
    {{"TYPE", K_FXT, 0, BF_COUNT - 1}, {"RATE", K_NUM, 40, 127}, {"DEPTH", K_NUM, 60, 127}, {"VOICE", K_CNT, 2, 2}, {"WIDE", K_NUM, 110, 127},
     {"FBK", K_NUM, 0, 127}, {"FWD", K_NUM, 0, 127}, {"LVL", K_NUM, 110, 127}}};
inline const ParamDef& busDef(int bus, int type, int k) {
    if (k == BX_FWD && bus >= NBUS - 1) return PDEF_NONE;   // letzter Bus: kein Weiterleiten
    return BUS_DEFS[(type % BF_COUNT + BF_COUNT) % BF_COUNT][k & 7];
}

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

static const float FM_RATIOS_OLD[16] = {0.25f, 0.5f, 0.75f, 1, 1.5f, 2, 2.5f, 3, 3.5f, 4, 5, 6, 7, 8, 9, 11};   // bis v12
enum { FM_NRAT = 28 };
static const float FM_RATIOS[FM_NRAT] = {0.25f, 0.5f, 0.75f, 1, 1.25f, 1.5f, 1.75f, 2, 2.5f, 3, 3.5f, 4, 4.5f, 5, 5.5f, 6, 6.5f, 7, 7.5f, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const float DLY_STEPS[15] = {0.25f, 0.5f, 2.f / 3, 0.75f, 1, 4.f / 3, 1.5f, 2, 8.f / 3, 3, 4, 16.f / 3, 6, 8, 16};
static const int RTRG_DIV[7] = {0, 2, 3, 4, 6, 8, 16};

inline const ParamDef& paramDef(int machine, int idx) {
    if (idx < 0 || idx >= NPARAMS) return PDEF_NONE;
    if (idx >= P_S0 && idx <= P_S7) return SRC_DEFS[machine][idx - P_S0];
    if (machine == M_FM) { int op = fmOpOf(idx); if (op >= 0) return FM_OP_DEFS[op][idx - FM_OPB[op]]; }
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
    case K_RANGE: if (v) snprintf(buf, n, "+%d", v); else snprintf(buf, n, "%s", TR("AUS", "OFF")); break;
    case K_MTIM: if (v == 23) snprintf(buf, n, "0"); else snprintf(buf, n, "%+d/24", v - 23); break;
    case K_WTAB: { const char* wt[5] = {"BASIC", "PWM", TR("VOKAL", "VOWEL"), TR("OBERT", "HARM"), "USER"}; snprintf(buf, n, "%s", wt[v % 5]); } break;
    case K_SLICE: if (v) snprintf(buf, n, "%d", v); else snprintf(buf, n, "AUTO"); break;
    case K_FBK: snprintf(buf, n, "%d%%", v * 120 / 127); break;
    case K_COND:
        if (v <= 0) snprintf(buf, n, "---");
        else if (v < CND_AB) { static const char* cn[9] = {"", "FILL", "!FILL", "PRE", "!PRE", "NEI", "!NEI", "1ST", "!1ST"}; snprintf(buf, n, "%s", cn[v]); }
        else { int a, b; bool ng; condDecode(v, a, b, ng); snprintf(buf, n, "%s%d:%d", ng ? "!" : "", a, b); }
        break;
    case K_TRG: snprintf(buf, n, "%s", v ? "NOTE" : "FREE"); break;
    case K_RTRG: if (v) snprintf(buf, n, "x%d", RTRG_DIV[v]); else snprintf(buf, n, "OFF"); break;
    case K_RATIO: snprintf(buf, n, "%.2f", FM_RATIOS[v < FM_NRAT ? v : FM_NRAT - 1]); break;
    case K_OCT: snprintf(buf, n, "%+d", v - 3); break;
    case K_SUBO: snprintf(buf, n, "%s", v ? "-2" : "-1"); break;
    case K_FXT: snprintf(buf, n, "%s", BUS_TYPE_NAMES[v % BF_COUNT]); break;
    case K_WDRAW: snprintf(buf, n, ">>"); break;
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
