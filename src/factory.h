// Werksklaenge (benannte Presets fuer alle Spuren) und der Demo-Song "Neon Nights".
// Ein Werksklang = Engine-Grundwerte (Engine::initTrack) + die hier genannten Aenderungen.
#pragma once
#include "engine.h"
#include <string.h>

struct FSet { uint8_t p, v; };
struct FSound { uint8_t machine; const char* name; FSet set[16]; };

static const FSound FACTORY[] = {
    // Drums
    {M_KICK, "BOOM KICK", {{P_S0, 28}, {P_S1, 105}, {P_S2, 60}, {P_S3, 55}, {P_S4, 25}, {P_S5, 10}, {P_VOL, 110}, {255, 0}}},
    {M_KICK, "PUNCH KICK", {{P_S0, 42}, {P_S1, 55}, {P_S2, 95}, {P_S3, 30}, {P_S4, 85}, {P_S5, 45}, {P_VOL, 105}, {255, 0}}},
    {M_KICK, "TECHNO THUMP", {{P_S0, 34}, {P_S1, 70}, {P_S2, 80}, {P_S3, 40}, {P_S4, 60}, {P_S5, 90}, {P_VOL, 100}, {255, 0}}},
    {M_SNARE, "CRISP SNARE", {{P_S0, 55}, {P_S1, 45}, {P_S2, 80}, {P_S3, 55}, {P_S4, 95}, {P_REV, 25}, {255, 0}}},
    {M_SNARE, "DEEP SNARE", {{P_S0, 40}, {P_S1, 60}, {P_S2, 55}, {P_S3, 40}, {P_S4, 60}, {P_REV, 30}, {255, 0}}},
    {M_HAT, "CLOSED HAT", {{P_S0, 64}, {P_S1, 18}, {P_S2, 95}, {P_S3, 75}, {P_VOL, 80}, {255, 0}}},
    {M_HAT, "OPEN HAT", {{P_S0, 60}, {P_S1, 70}, {P_S2, 85}, {P_S3, 80}, {P_VOL, 110}, {255, 0}}},
    {M_CLAP, "CITY CLAP", {{P_S0, 60}, {P_S1, 60}, {P_S2, 55}, {P_S3, 3}, {P_REV, 40}, {P_VOL, 90}, {255, 0}}},
    {M_TOM, "SYN TOM", {{P_S0, 40}, {P_S1, 75}, {P_S2, 70}, {P_S3, 20}, {P_S4, 30}, {P_DLY, 30}, {255, 0}}},
    {M_RIM, "RIMSHOT", {{P_S0, 70}, {P_S1, 25}, {P_S2, 80}, {P_S3, 90}, {P_REV, 20}, {255, 0}}},
    {M_SHKR, "SHAKER", {{P_S0, 95}, {P_S1, 35}, {P_S2, 40}, {P_S3, 50}, {P_VOL, 50}, {255, 0}}},
    // Synths
    {M_SYNTH, "NIGHT DRIVE BASS", {{P_S0, 0}, {P_S1, 1}, {P_S2, 66}, {P_S3, 50}, {P_S4, 90}, {P_FTYP, FT_LP24}, {P_FREQ, 58}, {P_RESO, 45},
                                   {P_FENV, 100}, {P_FDEC, 45}, {P_ADEC, 50}, {P_ASUS, 90}, {P_VOL, 127}, {P_NOTE, 36}}},
    {M_SYNTH, "ACID SQUELCH", {{P_S0, 0}, {P_S1, 0}, {P_S2, 64}, {P_S3, 0}, {P_FTYP, FT_LADR}, {P_FREQ, 40}, {P_RESO, 112}, {P_FENV, 115},
                               {P_FDEC, 35}, {P_FSUS, 0}, {P_ASUS, 90}, {P_AREL, 20}, {P_DRIV, 40}, {P_NOTE, 36}}},
    {M_SYNTH, "NEON LEAD", {{P_S0, 1}, {P_S1, 0}, {P_S2, 74}, {P_S3, 70}, {P_FTYP, FT_LP24}, {P_FREQ, 85}, {P_RESO, 30}, {P_FENV, 85},
                            {P_AATK, 5}, {P_ASUS, 100}, {P_DLY, 55}, {P_REV, 35}, {P_S7, 30}, {P_NOTE, 72}}},
    {M_SYNTH, "HAZE PAD", {{P_S0, 0}, {P_S1, 0}, {P_S2, 80}, {P_S3, 64}, {P_FTYP, FT_LP24}, {P_FREQ, 70}, {P_AATK, 80}, {P_ADEC, 90},
                           {P_ASUS, 110}, {P_AREL, 90}, {P_REV, 85}, {P_AMOD, ARP_CHORD}, {P_ACHD, 6}, {P_LEN, 110}, {P_VOL, 70}}},
    {M_SYNTH, "COMB PLUCK", {{P_S0, 1}, {P_S5, 40}, {P_FTYP, FT_CMBP}, {P_FREQ, 75}, {P_RESO, 100}, {P_KEYT, 100}, {P_ADEC, 45},
                             {P_ASUS, 0}, {P_AREL, 40}, {P_DLY, 40}, {P_REV, 40}, {P_NOTE, 60}, {255, 0}}},
    {M_FM, "GLASS BELL", {{P_S0, 1}, {P_S2, 3}, {P_S3, 10}, {P_X0, 70}, {P_ADEC, 80}, {P_ASUS, 0}, {P_AREL, 80}, {P_REV, 60}, {P_DLY, 30},
                          {P_NOTE, 72}, {255, 0}}},
    {M_FM, "FM BASS", {{P_S0, 0}, {P_S1, 40}, {P_S2, 3}, {P_S3, 3}, {P_X0, 90}, {P_ADEC, 50}, {P_ASUS, 40}, {P_AREL, 20}, {P_NOTE, 36}, {255, 0}}},
    {M_WTBL, "VOWEL PAD", {{P_S0, 2}, {P_S1, 50}, {P_S3, 40}, {P_FTYP, FT_VOWL}, {P_FREQ, 50}, {P_RESO, 70}, {P_AATK, 60}, {P_ASUS, 110},
                           {P_AREL, 80}, {P_REV, 70}, {P_LEN, 110}, {255, 0}}},
    {M_WTBL, "DIGITAL PLUCK", {{P_S0, 3}, {P_S1, 30}, {P_S6, 100}, {P_FTYP, FT_LP24}, {P_FREQ, 80}, {P_FENV, 90}, {P_ADEC, 40}, {P_ASUS, 0},
                               {P_DLY, 45}, {P_NOTE, 60}, {P_VOL, 127}, {255, 0}}},
};
enum { NFACTORY = sizeof FACTORY / sizeof *FACTORY };

static inline void applyFactory(Pattern& P, int t, int idx) {
    const FSound& f = FACTORY[idx];
    Engine::initTrack(P, t, f.machine);
    for (int i = 0; i < 16 && f.set[i].p != 255 && (f.set[i].p || f.set[i].v); i++) P.p[t][f.set[i].p] = f.set[i].v;   // Ende: 255 oder leerer Eintrag
}
static inline int factoryByName(const char* nm) {
    for (int i = 0; i < NFACTORY; i++) if (!strcmp(FACTORY[i].name, nm)) return i;
    return 0;
}

static inline int condAB(int a, int b) {   // Trig-Bedingung a:b (a-ter von b Durchlaeufen)
    for (int v = CND_AB; v < CND_COUNT; v++) { int x, y; bool ng; condDecode(v, x, y, ng); if (x == a && y == b && !ng) return v; }
    return CND_NONE;
}

// Demo-Song "Neon Nights": 4 Patterns (Intro, Groove, Break, Groove+), Song-Kette, a-Moll, 112 BPM
static inline void buildDemo(Engine& E) {
    E.defaultProject(false);
    Project& J = E.proj;
    J.bpm = 112;
    J.mst[MS_SWNG] = 6;
    J.mst[MS_ROOT] = 9; J.mst[MS_SCALE] = (uint8_t)scaleCombine(2, 9);   // a-Moll
    static const char* kit[NTRACKS] = {"PUNCH KICK", "CRISP SNARE", "CLOSED HAT", "CITY CLAP", "NIGHT DRIVE BASS", "NEON LEAD", "HAZE PAD", "OPEN HAT"};
    for (int pi = 0; pi < 4; pi++) {
        Pattern& P = J.pat[pi];
        for (int t = 0; t < NTRACKS; t++) applyFactory(P, t, factoryByName(kit[t]));
        auto on = [&](int t, int s, int note = -1, int vel = -1) {
            Step& st = P.s[t][s]; st.on = 1;
            if (note >= 0) st.lock[P_NOTE] = (uint8_t)note;
            if (vel >= 0) st.lock[P_VEL] = (uint8_t)vel;
        };
        bool groove = pi == 1 || pi == 3, brk = pi == 2;
        if (!brk) for (int s = 0; s < 16; s += 4) on(0, s);
        if (pi == 3) on(0, 14, -1, 70);
        if (groove) { on(1, 4); on(1, 12); }
        if (pi == 3) { on(1, 15, -1, 50); P.s[1][15].lock[P_COND] = CND_FILL; }
        for (int s = 2; s < 16; s += 4) on(2, s);
        if (groove || brk) { on(2, 7, -1, 55); on(2, 15, -1, 55); }
        if (groove) { on(3, 12, -1, 80); }
        static const int bs[6] = {0, 3, 6, 8, 11, 14}, bn[6] = {33, 33, 45, 33, 36, 40};
        if (!brk) for (int k = 0; k < 6; k++) on(4, bs[k], bn[k]);
        if (groove) {
            static const int ls[5] = {0, 3, 6, 10, 12}, ln[5] = {69, 72, 76, 74, 72};
            for (int k = 0; k < 5; k++) on(5, ls[k], ln[k], k ? 90 : 110);
            P.s[5][10].lock[P_COND] = (uint8_t)condAB(2, 2);   // nur jeder 2. Durchlauf
        }
        if (pi >= 2 || pi == 1) { on(6, 0, 57); on(6, 8, pi == 2 ? 53 : 60); }
        if (pi == 3) for (int s = 2; s < 16; s += 4) on(7, s, -1, 70);
        // Bewegung: LFO1 auf den Bass-Filter (langsam, synchron), Sidechain-Gefuehl ueber den Kompressor
        P.p[4][P_L1 + LF_DEST] = P_FREQ + 1; P.p[4][P_L1 + LF_DEPTH] = 84; P.p[4][P_L1 + LF_SYNC] = 0; P.p[4][P_L1 + LF_RATE] = 22; P.p[4][P_L1 + LF_WAVE] = LW_TRI;
    }
    J.cmp[CM_SRC] = 1; J.cmp[CM_THR] = 70; J.cmp[CM_RAT] = 4;
    J.fx[FX_DTIM] = 6; J.fx[FX_DFBK] = 55; J.fx[FX_RSIZ] = 95;
    static const uint8_t chain[4][2] = {{0, 2}, {1, 4}, {2, 2}, {3, 4}};
    J.songLen = 4;
    for (int i = 0; i < 4; i++) { J.song[i].pat = chain[i][0]; J.song[i].rep = chain[i][1]; }
}
