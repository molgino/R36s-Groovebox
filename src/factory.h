// Werksklaenge (benannte Presets fuer alle Spuren) und der Demo-Song "Neon Nights".
// Ein Werksklang = Engine-Grundwerte (Engine::initTrack) + die hier genannten Aenderungen.
#pragma once
#include "engine.h"
#include <string.h>

struct FSet { uint8_t p, v; };
struct FSound { uint8_t machine; const char* name; FSet set[28]; };

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
    // FM: Operator k liegt ab FM_OPB[k] (OP1 = P_X0, OP2 = P_Y0, OP3 = P_Z0, OP4 = P_W0) + OP_RAT/OP_FINE/OP_DETN/OP_LVL/OP_ATK/OP_DEC/OP_SUS/OP_REL
    {M_FM, "GLASS BELL", {{P_S0, 1}, {P_X0 + OP_DEC, 95}, {P_X0 + OP_SUS, 0}, {P_X0 + OP_REL, 85}, {P_Y0 + OP_RAT, 13}, {P_Y0 + OP_LVL, 70},
                          {P_Y0 + OP_DEC, 80}, {P_Y0 + OP_SUS, 0}, {P_Z0 + OP_RAT, 10}, {P_Z0 + OP_FINE, 72}, {P_Z0 + OP_LVL, 30},
                          {P_ASUS, 0}, {P_ADEC, 95}, {P_REV, 60}, {P_DLY, 30}, {P_NOTE, 72}}},
    {M_FM, "FM BASS", {{P_S0, 0}, {P_S1, 40}, {P_Y0 + OP_RAT, 3}, {P_Y0 + OP_LVL, 90}, {P_Y0 + OP_DEC, 50}, {P_Y0 + OP_SUS, 30},
                       {P_X0 + OP_SUS, 127}, {P_ADEC, 50}, {P_ASUS, 40}, {P_AREL, 20}, {P_NOTE, 36}, {255, 0}}},
    {M_FM, "DX E-PIANO", {{P_S0, 3}, {P_X0 + OP_DEC, 85}, {P_X0 + OP_SUS, 25}, {P_Y0 + OP_LVL, 55}, {P_Y0 + OP_DEC, 70}, {P_Y0 + OP_SUS, 10},
                          {P_Z0 + OP_LVL, 90}, {P_Z0 + OP_DEC, 80}, {P_Z0 + OP_SUS, 15}, {P_W0 + OP_RAT, 25}, {P_W0 + OP_LVL, 45},
                          {P_W0 + OP_DEC, 40}, {P_W0 + OP_SUS, 0}, {P_REV, 40}, {P_NOTE, 60}, {255, 0}}},
    {M_FM, "FM PLUCK", {{P_S0, 0}, {P_S1, 20}, {P_S3, 76}, {P_S4, 20}, {P_X0 + OP_DEC, 70}, {P_X0 + OP_SUS, 0}, {P_Y0 + OP_RAT, 9},
                        {P_Y0 + OP_LVL, 80}, {P_Y0 + OP_DEC, 50}, {P_Y0 + OP_SUS, 0}, {P_DLY, 45}, {P_REV, 30}, {P_NOTE, 60}, {255, 0}}},
    {M_WTBL, "VOWEL PAD", {{P_S0, 2}, {P_S1, 50}, {P_S3, 40}, {P_FTYP, FT_VOWL}, {P_FREQ, 50}, {P_RESO, 70}, {P_AATK, 60}, {P_ASUS, 110},
                           {P_AREL, 80}, {P_REV, 70}, {P_LEN, 110}, {255, 0}}},
    {M_WTBL, "DIGITAL PLUCK", {{P_S0, 3}, {P_S1, 30}, {P_S6, 100}, {P_FTYP, FT_LP24}, {P_FREQ, 80}, {P_FENV, 90}, {P_ADEC, 40}, {P_ASUS, 0},
                               {P_DLY, 45}, {P_NOTE, 60}, {P_VOL, 127}, {255, 0}}},
    // ---- Rave & Acid (90er): 303-Acid, Reese, Hoover, Stabs, Supersaw, House-Orgel
    // Acid: Ladder-Filter mit hoher Resonanz, kurzer Filter-Decay, GLID = Slide bei ueberlappenden Trigs (LEN > 1 Step), VELF = Akzent
    {M_SYNTH, "303 ACID", {{P_S0, 0}, {P_S3, 0}, {P_FTYP, FT_LADR}, {P_FREQ, 32}, {P_RESO, 108}, {P_FENV, 112}, {P_FDEC, 36},
                           {P_FSUS, 0}, {P_ADEC, 70}, {P_ASUS, 100}, {P_AREL, 14}, {P_S7, 26}, {P_DRIV, 18}, {P_VELF, 92},
                           {P_KEYT, 30}, {P_LEN, 40}, {P_NOTE, 36}, {P_VOL, 110}, {255, 0}}},
    {M_SYNTH, "303 SQUARE ACID", {{P_S0, 1}, {P_S3, 0}, {P_S6, 64}, {P_FTYP, FT_LADR}, {P_FREQ, 28}, {P_RESO, 112}, {P_FENV, 118},
                                  {P_FDEC, 30}, {P_FSUS, 0}, {P_ADEC, 60}, {P_ASUS, 95}, {P_AREL, 12}, {P_S7, 24}, {P_DRIV, 30},
                                  {P_VELF, 96}, {P_LEN, 40}, {P_NOTE, 36}, {P_VOL, 105}, {255, 0}}},
    {M_SYNTH, "ACID SCREAMER", {{P_S0, 0}, {P_S3, 0}, {P_FTYP, FT_LADR}, {P_FREQ, 42}, {P_RESO, 122}, {P_FENV, 120}, {P_FDEC, 44},
                                {P_FSUS, 0}, {P_ASUS, 100}, {P_AREL, 16}, {P_S7, 30}, {P_ITYP, INS_DRIVE}, {P_IMIX, 100}, {P_DRIV, 70},
                                {P_L1 + LF_DEST, P_FREQ + 1}, {P_L1 + LF_SYNC, 3}, {P_L1 + LF_WAVE, LW_TRI}, {P_L1 + LF_DEPTH, 84},
                                {P_DLY, 35}, {P_VELF, 90}, {P_LEN, 40}, {P_NOTE, 36}, {P_VOL, 127}, {255, 0}}},
    {M_SYNTH, "ACID DEEP", {{P_S0, 0}, {P_S3, 0}, {P_S4, 40}, {P_FTYP, FT_LADR}, {P_FREQ, 22}, {P_RESO, 96}, {P_FENV, 100}, {P_FDEC, 55},
                            {P_FSUS, 0}, {P_ASUS, 100}, {P_AREL, 20}, {P_S7, 34}, {P_DRIV, 25}, {P_VELF, 85}, {P_REV, 20},
                            {P_LEN, 40}, {P_NOTE, 36}, {P_VOL, 115}, {255, 0}}},
    // Reese: zwei leicht verstimmte Saegezaehne schweben gegeneinander (Jungle/DnB), Filter bewegt sich langsam
    {M_SYNTH, "REESE BASS", {{P_S0, 0}, {P_S1, 0}, {P_S2, 80}, {P_S3, 64}, {P_S4, 45}, {P_FTYP, FT_LP24}, {P_FREQ, 52}, {P_RESO, 25},
                             {P_FENV, 72}, {P_FDEC, 70}, {P_ADEC, 90}, {P_ASUS, 120}, {P_AREL, 30}, {P_DRIV, 30},
                             {P_L1 + LF_DEST, P_FREQ + 1}, {P_L1 + LF_RATE, 30}, {P_L1 + LF_WAVE, LW_SIN}, {P_L1 + LF_DEPTH, 76},
                             {P_NOTE, 36}, {P_VOL, 110}, {255, 0}}},
    {M_SYNTH, "REESE GROWL", {{P_S0, 0}, {P_S1, 0}, {P_S2, 92}, {P_S3, 64}, {P_S4, 35}, {P_FTYP, FT_LP24}, {P_FREQ, 48}, {P_RESO, 60},
                              {P_FENV, 70}, {P_ASUS, 120}, {P_AREL, 25}, {P_ITYP, INS_PHASER}, {P_IP1, 40}, {P_IP2, 90}, {P_IMIX, 90},
                              {P_DRIV, 45}, {P_L1 + LF_DEST, P_FREQ + 1}, {P_L1 + LF_SYNC, 4}, {P_L1 + LF_WAVE, LW_TRI},
                              {P_L1 + LF_DEPTH, 90}, {P_NOTE, 36}, {P_VOL, 105}, {255, 0}}},
    // Hoover: verstimmte Saegezaehne + Oktave tiefer, starker Chorus, Tonhoehe schwingt beim Anschlag von unten ein, Glide
    {M_SYNTH, "HOOVER", {{P_S0, 0}, {P_S1, 0}, {P_S2, 98}, {P_S3, 70}, {P_S4, 40}, {P_S7, 45}, {P_X1, 2}, {P_FTYP, FT_LP24},
                         {P_FREQ, 92}, {P_RESO, 20}, {P_FENV, 70}, {P_ADEC, 90}, {P_ASUS, 115}, {P_AREL, 50},
                         {P_ITYP, INS_CHORUS}, {P_IP1, 55}, {P_IP2, 110}, {P_IP3, 110}, {P_IMIX, 110},
                         {P_L1 + LF_DEST, P_NOTE + 1}, {P_L1 + LF_RATE, 92}, {P_L1 + LF_WAVE, LW_EXPDN}, {P_L1 + LF_MODE, LM_ONE},
                         {P_L1 + LF_DEPTH, 50}, {P_L2 + LF_DEST, P_S2 + 1}, {P_L2 + LF_RATE, 45}, {P_L2 + LF_DEPTH, 84}}},
    {M_SYNTH, "HOOVER LEAD", {{P_S0, 0}, {P_S1, 1}, {P_S2, 104}, {P_S3, 60}, {P_S6, 40}, {P_S7, 55}, {P_X1, 2}, {P_FTYP, FT_LP24},
                              {P_FREQ, 100}, {P_RESO, 25}, {P_AATK, 4}, {P_ASUS, 115}, {P_AREL, 55},
                              {P_ITYP, INS_CHORUS}, {P_IP1, 60}, {P_IP2, 120}, {P_IMIX, 115},
                              {P_L1 + LF_DEST, P_NOTE + 1}, {P_L1 + LF_RATE, 88}, {P_L1 + LF_WAVE, LW_EXPDN}, {P_L1 + LF_MODE, LM_ONE},
                              {P_L1 + LF_DEPTH, 46}, {P_L2 + LF_DEST, P_S6 + 1}, {P_L2 + LF_RATE, 50}, {P_L2 + LF_DEPTH, 100},
                              {P_REV, 45}, {P_DLY, 30}, {P_NOTE, 60}}},
    // Stabs: Akkord auf einer Taste (ARP-Modus AKKD), kurz und hell, mit Hall
    {M_SYNTH, "RAVE STAB", {{P_S0, 0}, {P_S1, 1}, {P_S2, 76}, {P_S3, 50}, {P_FTYP, FT_LP24}, {P_FREQ, 78}, {P_RESO, 35}, {P_FENV, 92},
                            {P_FDEC, 35}, {P_FSUS, 0}, {P_ADEC, 42}, {P_ASUS, 0}, {P_AREL, 35}, {P_AMOD, ARP_CHORD}, {P_ACHD, 3},
                            {P_REV, 50}, {P_DLY, 25}, {P_NOTE, 60}, {P_VOL, 90}, {255, 0}}},
    {M_SYNTH, "DUB CHORD", {{P_S0, 0}, {P_S1, 0}, {P_S2, 72}, {P_S3, 64}, {P_FTYP, FT_LP24}, {P_FREQ, 60}, {P_RESO, 45}, {P_FENV, 96},
                            {P_FDEC, 30}, {P_FSUS, 0}, {P_ADEC, 36}, {P_ASUS, 0}, {P_AREL, 30}, {P_AMOD, ARP_CHORD}, {P_ACHD, 6},
                            {P_DLY, 75}, {P_REV, 65}, {P_NOTE, 60}, {P_VOL, 120}, {255, 0}}},
    // Supersaw / Trance
    {M_SYNTH, "SUPERSAW", {{P_S0, 0}, {P_S1, 0}, {P_S2, 100}, {P_S3, 64}, {P_S4, 25}, {P_FTYP, FT_LP24}, {P_FREQ, 100}, {P_RESO, 15},
                           {P_AATK, 2}, {P_ASUS, 110}, {P_AREL, 60}, {P_ITYP, INS_CHORUS}, {P_IP1, 50}, {P_IP2, 100}, {P_IP3, 127},
                           {P_IMIX, 110}, {P_REV, 50}, {P_DLY, 35}, {P_NOTE, 72}, {P_VOL, 115}, {255, 0}}},
    {M_SYNTH, "TRANCE PLUCK", {{P_S0, 0}, {P_S1, 0}, {P_S2, 84}, {P_S3, 64}, {P_FTYP, FT_LP24}, {P_FREQ, 45}, {P_RESO, 40}, {P_FENV, 110},
                               {P_FDEC, 30}, {P_FSUS, 0}, {P_ADEC, 45}, {P_ASUS, 0}, {P_AREL, 30}, {P_DLY, 55}, {P_REV, 40},
                               {P_NOTE, 72}, {P_VOL, 100}, {255, 0}}},
    {M_SYNTH, "WOBBLE BASS", {{P_S0, 0}, {P_S1, 1}, {P_S2, 70}, {P_S3, 50}, {P_S4, 60}, {P_FTYP, FT_LP24}, {P_FREQ, 40}, {P_RESO, 70},
                              {P_ASUS, 120}, {P_AREL, 20}, {P_DRIV, 50}, {P_L1 + LF_DEST, P_FREQ + 1}, {P_L1 + LF_SYNC, 6},
                              {P_L1 + LF_WAVE, LW_SIN}, {P_L1 + LF_MODE, LM_TRIG}, {P_L1 + LF_DEPTH, 110}, {P_NOTE, 36}, {P_VOL, 110}, {255, 0}}},
    {M_SYNTH, "JUNGLE SUB", {{P_S0, 3}, {P_S3, 0}, {P_S4, 30}, {P_S7, 40}, {P_FTYP, FT_LP24}, {P_FREQ, 60}, {P_FENV, 64}, {P_ADEC, 90},
                             {P_ASUS, 120}, {P_AREL, 35}, {P_DRIV, 20}, {P_NOTE, 36}, {P_VOL, 120}, {255, 0}}},
    // House-Orgel (90er Orgel-Bass): 4 Traeger als Zugriegel 1/2/3/4, perkussiv, etwas Feedback fuer Biss
    {M_FM, "HOUSE ORGAN", {{P_S0, 7}, {P_S1, 30}, {P_X0 + OP_RAT, 3}, {P_X0 + OP_LVL, 120}, {P_X0 + OP_DEC, 60}, {P_X0 + OP_SUS, 30},
                           {P_Y0 + OP_RAT, 7}, {P_Y0 + OP_LVL, 90}, {P_Y0 + OP_DEC, 50}, {P_Y0 + OP_SUS, 20},
                           {P_Z0 + OP_RAT, 9}, {P_Z0 + OP_LVL, 70}, {P_Z0 + OP_DEC, 40}, {P_Z0 + OP_SUS, 10},
                           {P_W0 + OP_RAT, 11}, {P_W0 + OP_LVL, 55}, {P_W0 + OP_DEC, 30}, {P_W0 + OP_SUS, 0},
                           {P_ADEC, 60}, {P_ASUS, 50}, {P_AREL, 25}, {P_REV, 30}, {P_NOTE, 48}, {P_VOL, 95}, {255, 0}}},
};
enum { NFACTORY = sizeof FACTORY / sizeof *FACTORY };

static inline void applyFactory(Pattern& P, int t, int idx) {
    const FSound& f = FACTORY[idx];
    Engine::initTrack(P, t, f.machine);
    for (int i = 0; i < 28 && f.set[i].p != 255 && (f.set[i].p || f.set[i].v); i++) P.p[t][f.set[i].p] = f.set[i].v;   // Ende: 255 oder leerer Eintrag
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
    J.bus[0][BX_P1] = 6; J.bus[0][BX_P2] = 55; J.bus[1][BX_P1] = 95;   // Bus A = Delay, Bus B = Reverb
    static const uint8_t chain[4][2] = {{0, 2}, {1, 4}, {2, 2}, {3, 4}};
    J.songLen = 4;
    for (int i = 0; i < 4; i++) { Engine::rowFromPattern(J, J.row[i], chain[i][0]); J.row[i].rep = chain[i][1]; }
}
