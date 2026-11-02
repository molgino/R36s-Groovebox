// Parameter-Definitionen der Groovebox (Elektron-artige Seiten mit je 8 Reglern)
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <math.h>

enum { NTRACKS = 8, NSTEPS = 16, NPATTERNS = 16, NPAGES = 6, NPARAMS = NPAGES * 8, PAGE_INS = 5, PAGE_FX = 6, PAGE_CMP = 7, PAGE_MST = 8, PAGE_SYS = 9, NUI_PAGES = 10 };
enum { NO_LOCK = 255 };

enum Machine { M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_FM, M_SAMPLE, M_COUNT };
static const char* const MACHINE_NAMES[M_COUNT] = {"KICK", "SNARE", "HAT", "CLAP", "SYNTH", "FM", "SMPL"};
static const char* const PAGE_NAMES[NUI_PAGES] = {"TRG", "SRC", "FLT", "AMP", "LFO", "INS", "FX", "CMP", "MST", "SYS"};

enum Kind {
    K_NONE, K_NUM, K_BIP, K_NOTE, K_PCT, K_LEN, K_WAVE, K_FTYPE, K_LDST, K_LWAV,
    K_TRG, K_RTRG, K_RATIO, K_DTIM, K_BPM, K_SWNG, K_CNT,
    K_SMPL, K_SEMI, K_OFFON, K_DEV, K_BUFS, K_CLK, K_CHM, K_INFO, K_USB,
    K_INS, K_MS, K_MACH, K_THR, K_CRAT, K_CATK, K_CREL, K_CGAIN, K_SCSRC, K_GR
};

struct ParamDef { const char* name; Kind kind; uint8_t def; uint8_t max; };

// Parameterindizes (Seite*8 + Regler)
enum {
    P_NOTE = 0, P_VEL, P_LEN, P_PROB, P_RTRG, P_RFAD, P_T6, P_T7,
    P_S0 = 8, P_S1, P_S2, P_S3, P_S4, P_S5, P_S6, P_S7,
    P_FREQ = 16, P_RESO, P_FTYP, P_FENV, P_FATK, P_FDEC, P_FSUS, P_FREL,
    P_AATK = 24, P_ADEC, P_ASUS, P_AREL, P_VOL, P_PAN, P_DLY, P_REV,
    P_LSPD = 32, P_LDEP, P_LDST, P_LWAV, P_LTRG, P_CRSH, P_SRR, P_DRIV,
    P_ITYP = 40, P_IP1, P_IP2, P_IP3, P_IMIX, P_I5, P_I6, P_I7
};
// Master-Seite
enum { MS_BPM = 0, MS_SWNG, MS_VOL, MS_DRIV };
// FX-Seite (Send-Effekte)
enum { FX_DTIM = 0, FX_DFBK, FX_DTON, FX_DWID, FX_RSIZ, FX_RDMP, FX_RPRE, FX_RWID };

static const ParamDef PDEF_NONE = {"", K_NONE, 0, 0};

static const ParamDef COMMON_DEFS[NPARAMS] = {
    // TRIG
    {"NOTE", K_NOTE, 60, 127}, {"VEL", K_NUM, 100, 127}, {"LEN", K_LEN, 48, 127}, {"PROB", K_PCT, 100, 100},
    {"RTRG", K_RTRG, 0, 6}, {"RFAD", K_BIP, 64, 127}, {"ENGINE", K_MACH, 0, M_COUNT - 1}, PDEF_NONE,
    // SRC (maschinenabhaengig, siehe SRC_DEFS)
    PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE,
    // FILT
    {"FREQ", K_NUM, 127, 127}, {"RESO", K_NUM, 0, 127}, {"TYPE", K_FTYPE, 0, 2}, {"ENV", K_BIP, 64, 127},
    {"F.ATK", K_NUM, 0, 127}, {"F.DEC", K_NUM, 60, 127}, {"F.SUS", K_NUM, 0, 127}, {"F.REL", K_NUM, 60, 127},
    // AMP
    {"ATK", K_NUM, 0, 127}, {"DEC", K_NUM, 80, 127}, {"SUS", K_NUM, 127, 127}, {"REL", K_NUM, 50, 127},
    {"VOL", K_NUM, 100, 127}, {"PAN", K_BIP, 64, 127}, {"DLY", K_NUM, 0, 127}, {"REV", K_NUM, 0, 127},
    // LFO / FX
    {"SPD", K_NUM, 64, 127}, {"DEP", K_BIP, 64, 127}, {"DEST", K_LDST, 1, 5}, {"WAVE", K_LWAV, 0, 4},
    {"TRIG", K_TRG, 0, 1}, {"CRSH", K_NUM, 0, 127}, {"SRR", K_NUM, 0, 127}, {"DRIV", K_NUM, 0, 127},
    // INS (Insert-Effekt; Namen von P1-P3 haengen vom Typ ab, siehe INS_PNAMES)
    {"TYPE", K_INS, 0, 6}, {"P1", K_NUM, 64, 127}, {"P2", K_NUM, 64, 127}, {"P3", K_NUM, 64, 127},
    {"MIX", K_NUM, 64, 127}, PDEF_NONE, PDEF_NONE, PDEF_NONE,
};
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
    // FM
    {{"RATI", K_RATIO, 5, 15}, {"INDX", K_NUM, 50, 127}, {"FBK", K_NUM, 0, 127}, {"M.DEC", K_NUM, 50, 127},
     {"FINE", K_BIP, 64, 127}, {"NOIS", K_NUM, 0, 127}, PDEF_NONE, PDEF_NONE},
    // SAMPLE (END < STRT = rueckwaerts)
    {{"SMPL", K_SMPL, 0, 127}, {"TUNE", K_SEMI, 24, 48}, {"FINE", K_BIP, 64, 127}, {"STRT", K_NUM, 0, 127},
     {"END", K_NUM, 127, 127}, {"LOOP", K_OFFON, 0, 1}, {"GAIN", K_NUM, 64, 127}, PDEF_NONE},
};

static const ParamDef MST_DEFS[8] = {
    {"BPM", K_BPM, 0, 0}, {"SWNG", K_SWNG, 0, 30}, {"VOL", K_NUM, 100, 127}, {"DRIVE", K_NUM, 0, 127},
    PDEF_NONE, PDEF_NONE, PDEF_NONE, PDEF_NONE,
};
static const ParamDef FX_DEFS[8] = {
    {"D.TIME", K_DTIM, 4, 7}, {"D.FBK", K_NUM, 60, 127}, {"D.TONE", K_NUM, 80, 127}, {"D.PING", K_NUM, 127, 127},
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
static const float DLY_STEPS[8] = {0.5f, 1, 1.5f, 2, 3, 4, 6, 8};
static const int RTRG_DIV[7] = {0, 2, 3, 4, 6, 8, 16};

inline const ParamDef& paramDef(int machine, int idx) {
    if (idx >= P_S0 && idx <= P_S7) return SRC_DEFS[machine][idx - P_S0];
    return COMMON_DEFS[idx];
}

// Laenge in Steps (Wert 127 = unendlich -> negativ)
inline float lenSteps(int v) { return v >= 127 ? -1.f : 0.125f * powf(2.f, v / 16.f); }

inline void fmtParam(const ParamDef& d, int v, char* buf, int n) {
    static const char* notes[12] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    static const char* waves[4] = {"SAW", "SQR", "TRI", "SIN"};
    static const char* ftypes[3] = {"LP", "BP", "HP"};
    static const char* ldst[6] = {"PIT", "FRQ", "VOL", "PAN", "RES", "CRS"};
    static const char* lwav[5] = {"SIN", "TRI", "SAW", "SQR", "S&H"};
    static const char* dtim[8] = {"1/32", "1/16", "3/32", "1/8", "3/16", "1/4", "3/8", "1/2"};
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
    case K_FTYPE: snprintf(buf, n, "%s", ftypes[v % 3]); break;
    case K_LDST: snprintf(buf, n, "%s", ldst[v % 6]); break;
    case K_LWAV: snprintf(buf, n, "%s", lwav[v % 5]); break;
    case K_TRG: snprintf(buf, n, "%s", v ? "NOTE" : "FREE"); break;
    case K_RTRG: if (v) snprintf(buf, n, "x%d", RTRG_DIV[v]); else snprintf(buf, n, "OFF"); break;
    case K_RATIO: snprintf(buf, n, "%.2f", FM_RATIOS[v & 15]); break;
    case K_DTIM: snprintf(buf, n, "%s", dtim[v & 7]); break;
    case K_SWNG: snprintf(buf, n, "%d%%", 50 + v); break;
    case K_CNT: snprintf(buf, n, "%d", v + 1); break;
    case K_BPM: snprintf(buf, n, "%d", v); break;
    case K_SEMI: snprintf(buf, n, "%+d", v - 24); break;
    case K_OFFON: snprintf(buf, n, "%s", v ? "ON" : "OFF"); break;
    case K_BUFS: snprintf(buf, n, "%d", BUF_SIZES[v & 3]); break;
    case K_CLK: snprintf(buf, n, "%s", v == 1 ? "SEND" : (v == 2 ? "RECV" : "OFF")); break;
    case K_CHM: snprintf(buf, n, "%s", v ? "1-8" : "AUTO"); break;
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
