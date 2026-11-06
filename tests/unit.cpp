// Unit-Tests fuer Sequencer, Engines, Filter und Projektformat (ohne UI/SDL-Fenster).
//   make test            -> baut und startet tests/unit
//   make test SAN=1      -> mit AddressSanitizer/UBSan (Speicherfehler, undefiniertes Verhalten)
#include "../src/engine.h"
#include "../src/samples.h"
#include "../src/factory.h"
#include "../src/pcm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int nPass = 0, nFail = 0;
static const char* curTest = "";
#define CHECK(c, ...) do { if (c) nPass++; else { nFail++; printf("  FEHLER [%s] %s:%d: ", curTest, __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static Engine E;   // gross (Projekt + Puffer): statisch
static float buf[44100 * 2 * 4];

// leeres Projekt, alle Spuren still (KICK ohne Trigs), keine Sends
static void fresh(int bpm = 120) {
    E.stop();
    E.defaultProject(false);
    E.proj.bpm = (uint16_t)bpm;
    E.songOn = false;
    for (int t = 0; t < NTRACKS; t++) { E.proj.pat[0].p[t][P_DLY] = 0; E.proj.pat[0].p[t][P_REV] = 0; }
    E.curPat = 0; E.nextPat = -1;
}
static uint32_t trigs(int t) { return E.v[t].trigs; }
// rendert n Frames in Bloecken wie die Soundkarte (256), liefert Spitze, prueft auf NaN/Inf
static float run(int frames, bool* bad = nullptr) {
    float pk = 0;
    while (frames > 0) {
        int n = frames > 256 ? 256 : frames;
        E.render(buf, n);
        for (int i = 0; i < n * 2; i++) {
            float x = buf[i];
            if (!(x == x) || x > 1e6f || x < -1e6f) { if (bad) *bad = true; }
            else if (fabsf(x) > pk) pk = fabsf(x);
        }
        frames -= n;
    }
    return pk;
}
static int barFrames(int bpm) { return (int)(44100.0 * 60.0 / bpm * 4.0); }   // 16 Sechzehntel

static void testTiming() {
    curTest = "Timing";
    static const int BPMS[4] = {60, 120, 174, 300};
    for (int bpm : BPMS) {
        fresh(bpm);
        for (int s = 0; s < 16; s++) E.proj.pat[0].s[0][s].on = 1;
        uint32_t t0 = trigs(0);
        E.start();
        run(barFrames(bpm) * 2 - 200);   // knapp 2 Takte
        CHECK(trigs(0) - t0 == 32, "BPM %d: %u statt 32 Trigs in 2 Takten", bpm, trigs(0) - t0);
    }
    // Geschwindigkeit 2x und 1/2x
    fresh(120);
    for (int s = 0; s < 16; s++) { E.proj.pat[0].s[0][s].on = 1; E.proj.pat[0].s[1][s].on = 1; }
    E.proj.pat[0].speed[0] = 6;   // 2x
    E.proj.pat[0].speed[1] = 2;   // 1/2x
    uint32_t a = trigs(0), b = trigs(1);
    E.start();
    run(barFrames(120) - 200);
    CHECK(trigs(0) - a == 32, "2x: %u statt 32", trigs(0) - a);
    CHECK(trigs(1) - b == 8, "1/2x: %u statt 8", trigs(1) - b);
}

static void testPolymeter() {
    curTest = "Polymetrie";
    fresh(120);
    E.proj.pat[0].len[0] = 3;
    E.proj.pat[0].s[0][0].on = 1;   // jeder 3. Step
    uint32_t a = trigs(0);
    E.start();
    run(barFrames(120) * 3 - 200);   // 48 Steps -> 16 Trigs
    CHECK(trigs(0) - a == 16, "Laenge 3: %u statt 16", trigs(0) - a);
}

static void testProbCond() {
    curTest = "Wahrscheinlichkeit/Bedingung";
    fresh(120);
    for (int s = 0; s < 16; s++) {
        Step& st = E.proj.pat[0].s[0][s]; st.on = 1; st.lock[P_PROB] = 0;
        Step& s2 = E.proj.pat[0].s[1][s]; s2.on = 1; s2.lock[P_PROB] = 50;
    }
    uint32_t a = trigs(0), b = trigs(1);
    E.start();
    run(barFrames(120) * 8 - 200);
    CHECK(trigs(0) - a == 0, "PROB 0: %u Trigs", trigs(0) - a);
    unsigned n = trigs(1) - b;
    CHECK(n > 40 && n < 88, "PROB 50: %u von 128 (erwartet ~64)", n);
    // 1:2 = nur jeder zweite Durchlauf
    fresh(120);
    E.proj.pat[0].s[0][0].on = 1;
    int a2 = -1, b2 = 0; bool ng = false;
    for (int v = CND_AB; v < CND_COUNT; v++) { int x, y; bool neg; condDecode(v, x, y, neg); if (x == 1 && y == 2 && !neg) { a2 = v; b2 = y; ng = neg; break; } }
    (void)b2; (void)ng;
    CHECK(a2 >= 0, "Bedingung 1:2 nicht gefunden");
    E.proj.pat[0].s[0][0].lock[P_COND] = (uint8_t)a2;
    uint32_t c = trigs(0);
    E.start();
    run(barFrames(120) * 4 - 200);
    CHECK(trigs(0) - c == 2, "1:2 in 4 Takten: %u statt 2", trigs(0) - c);
}

static void testEuclid() {
    curTest = "Euklid";
    fresh(120);
    E.proj.pat[0].p[0][P_PULS] = 5;
    uint32_t a = trigs(0);
    E.start();
    run(barFrames(120) * 2 - 200);
    CHECK(trigs(0) - a == 10, "5 aus 16: %u statt 10 in 2 Takten", trigs(0) - a);
}

static void testTrigless() {
    curTest = "Trigless-Locks";
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    P.s[4][0].on = 1;
    P.s[4][4].on = 2; P.s[4][4].lock[P_FREQ] = 11;
    uint32_t a = trigs(4);
    E.start();
    run(barFrames(120) / 16 * 5 + 100);   // bis kurz nach Step 5
    CHECK(trigs(4) - a == 1, "trigless darf nicht ausloesen (%u Trigs)", trigs(4) - a);
    CHECK(E.v[4].lk[P_FREQ] == 11, "Lock FREQ nicht uebernommen (%d)", E.v[4].lk[P_FREQ]);
}

static void testSong() {
    curTest = "Song-Modus";
    fresh(120);
    E.proj.songLen = 2;
    Engine::rowFromPattern(E.proj, E.proj.row[0], 0); E.proj.row[0].rep = 2;
    Engine::rowFromPattern(E.proj, E.proj.row[1], 3);
    E.songStart(0); E.songOn = true;
    E.start();
    run(barFrames(120) * 2 + 2000);
    CHECK(E.patOf(0) == 3 && E.patOf(7) == 3, "nach 2 Takten Pattern %d statt 4", E.patOf(0) + 1);
    run(barFrames(120));
    CHECK(E.patOf(0) == 0, "nach 3 Takten Pattern %d statt 1 (Schleife)", E.patOf(0) + 1);
}

// Clip-Matrix: jede Spur spielt ihr eigenes Pattern, leere Zelle und Mute schweigen
static void testClipMatrix() {
    curTest = "Clip-Matrix";
    fresh(120);
    for (int pi = 0; pi < 3; pi++) for (int t = 0; t < 3; t++) for (int st = 0; st < NSTEPS; st++) E.proj.pat[pi].s[t][st].on = 0;
    for (int st = 0; st < 16; st += 4) E.proj.pat[0].s[0][st].on = 1;    // Pattern 1, Spur 1: 4 Trigs
    for (int st = 0; st < 16; st += 2) E.proj.pat[5].s[1][st].on = 1;    // Pattern 6, Spur 2: 8 Trigs
    E.proj.pat[5].len[1] = 16;
    for (int st = 0; st < 16; st++) E.proj.pat[0].s[2][st].on = 1;       // Spur 3 in Pattern 1 voll, aber Zelle leer
    E.proj.songLen = 1;
    Project::SongRow& r = E.proj.row[0];
    Engine::rowFromPattern(E.proj, r, 0);
    r.pat[1] = 5; r.pat[2] = SONG_EMPTY;
    E.songStart(0);
    uint32_t a0 = trigs(0), a1 = trigs(1), a2 = trigs(2);
    E.start();
    run(barFrames(120) - 500);
    CHECK(trigs(0) - a0 == 4, "Spur 1: %u Trigs statt 4", trigs(0) - a0);
    CHECK(trigs(1) - a1 == 8, "Spur 2 (Pattern 6): %u Trigs statt 8", trigs(1) - a1);
    CHECK(trigs(2) - a2 == 0, "leere Zelle spielt (%u Trigs)", trigs(2) - a2);
    E.stop();
    // Zeilen-Mute
    r.mute = 1u << 1;
    E.songStart(0); a1 = trigs(1);
    E.start(); run(barFrames(120) - 500); E.stop();
    CHECK(trigs(1) - a1 == 0, "Zeilen-Mute wirkt nicht (%u Trigs)", trigs(1) - a1);
    // Pattern-Mute (ohne Song) und Uebernahme in neue Zeile
    E.songOn = false; E.curPat = 0;
    E.toggleMute(0);
    CHECK(E.proj.patMute[0] == 1 && E.muted(0), "Pattern-Mute nicht gesetzt");
    a0 = trigs(0);
    E.start(); run(barFrames(120) - 500); E.stop();
    CHECK(trigs(0) - a0 == 0, "Pattern-Mute wirkt nicht (%u Trigs)", trigs(0) - a0);
    Project::SongRow r2; Engine::rowFromPattern(E.proj, r2, 0);
    CHECK(r2.mute == 1 && r2.pat[3] == 0, "neue Zeile uebernimmt Pattern-Mutes nicht (%02x)", r2.mute);
    E.toggleMute(0);
}

static void testArp() {
    curTest = "Arpeggiator";
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    P.s[4][0].on = 1;
    P.p[4][P_LEN] = 127;          // lang halten
    P.p[4][P_AMOD] = 1; P.p[4][P_ACHD] = 2; P.p[4][P_ARAT] = 2;   // UP, MAJ, 1/16
    uint32_t a = trigs(4);
    E.start();
    run(barFrames(120) / 4);      // 4 Sechzehntel
    CHECK(trigs(4) - a >= 3, "Arp: nur %u Noten in 4 Sechzehnteln", trigs(4) - a);
}

static void testChord() {
    curTest = "Akkord";
    fresh(120);
    Pattern& P = E.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    P.s[4][0].on = 1; P.p[4][P_LEN] = 100;
    P.p[4][P_AMOD] = ARP_CHORD; P.p[4][P_ACHD] = 5;   // MAJ7: 4 Toene
    E.start();
    bool bad = false;
    run(4410, &bad);
    int act = E.v[4].active;
    for (int k = 0; k < NCHORDV; k++) act += E.cv[4][k].active;
    CHECK(act == 4, "MAJ7-Akkord: %d statt 4 klingende Stimmen", act);
    CHECK(!bad, "Akkord: NaN");
    E.stop(); run(44100 * 3);
    act = 0; for (int k = 0; k < NCHORDV; k++) act += E.cv[4][k].active;
    CHECK(act == 0, "Akkordstimmen klingen nach Stop weiter (%d)", act);
}

static void testFactory() {
    curTest = "Werksklaenge";
    for (int i = 0; i < NFACTORY; i++) {
        fresh(120);
        Pattern& P = E.proj.pat[0];
        applyFactory(P, 0, i);
        for (int s = 0; s < 16; s += 4) P.s[0][s].on = 1;
        E.start();
        bool bad = false;
        float pk = run(44100, &bad);
        printf("  %-18s Spitze %.2f\n", FACTORY[i].name, pk);
        CHECK(!bad, "%s: NaN", FACTORY[i].name);
        CHECK(pk > 0.05f, "%s zu leise (%.3f)", FACTORY[i].name, pk);
        CHECK(pk < 0.999f, "%s uebersteuert (%.2f)", FACTORY[i].name, pk);
    }
    curTest = "Demo-Song";
    E.stop();
    buildDemo(E);
    E.songOn = true; E.songStart(0);
    E.start();
    bool bad = false; float pk = 0; int seen = 0;
    for (int bar = 0; bar < 12; bar++) { float p = run(barFrames(E.proj.bpm), &bad); if (p > pk) pk = p; seen |= 1 << E.curPat; }
    printf("  Demo: Spitze %.2f, Patterns gespielt %x\n", pk, seen);
    CHECK(!bad, "Demo: NaN");
    CHECK(pk > 0.2f && pk < 1.01f, "Demo-Pegel %.2f", pk);
    CHECK(seen == 0xF, "Demo spielt nicht alle 4 Patterns (%x)", seen);
    E.songOn = false;
}

// alle Engines x alle Filtertypen x Extremwerte: endlich, begrenzt
static void testStability() {
    curTest = "Stabilitaet";
    int worstM = -1, worstF = -1; float worst = 0;
    for (int m = 0; m < M_COUNT; m++)
        for (int ft = 0; ft < FT_COUNT; ft++)
            for (int ex = 0; ex < 3; ex++) {
                fresh(140);
                Pattern& P = E.proj.pat[0];
                Engine::initTrack(P, 0, m);
                for (int s = 0; s < 16; s++) P.s[0][s].on = 1;
                P.p[0][P_FTYP] = (uint8_t)ft;
                P.p[0][P_RESO] = 127;
                P.p[0][P_FREQ] = ex == 0 ? 0 : (ex == 1 ? 64 : 127);
                P.p[0][P_FENV] = ex == 2 ? 127 : 64;
                P.p[0][P_DRIV] = ex == 2 ? 127 : 0;
                P.p[0][P_VOL] = 127;
                if (ex == 2) for (int i = P_S0; i <= P_S7; i++) P.p[0][i] = (uint8_t)paramDef(m, i).max;
                bool bad = false;
                E.start();
                float pk = run(44100 / 2, &bad);
                CHECK(!bad, "Engine %s Filter %d Fall %d: NaN/Inf", MACHINE_NAMES[m], ft, ex);
                if (pk > worst) { worst = pk; worstM = m; worstF = ft; }
            }
    CHECK(worst < 4.f, "Spitze %.2f (Engine %s, Filter %d) zu hoch", worst, worstM >= 0 ? MACHINE_NAMES[worstM] : "-", worstF);
    printf("  Stabilitaet: groesste Spitze %.2f (%s, Filter %d)\n", worst, worstM >= 0 ? MACHINE_NAMES[worstM] : "-", worstF);
}

// nach dem Ausklingen muss Stille kommen (keine Denormal-/Rauschreste)
static void testDecay() {
    curTest = "Ausklingen";
    fresh(120);
    for (int t = 0; t < NTRACKS; t++) E.proj.pat[0].s[t][0].on = 1;
    E.start();
    run(barFrames(120) / 16);
    E.stop();
    run(44100 * 6);
    float pk = run(4410);
    CHECK(pk < 1e-3f, "nach Stop + 6 s noch Pegel %.5f", pk);
}

// altes Projekt (Format v9-v12, 96 Parameter je Spur) Byte fuer Byte erzeugen
struct OldProj {
    uint8_t* b; uint32_t ver; long size;
    explicit OldProj(uint32_t v) : ver(v) {
        size = v == 12 ? PROJECT_V12_SIZE : PROJECT_V11_SIZE;
        b = (uint8_t*)calloc(1, size);
        memcpy(b, "GBX1", 4); memcpy(b + 4, &ver, 4); uint16_t bpm = 120; memcpy(b + 8, &bpm, 2);
        for (int i = 0; i < 8; i++) b[10 + i] = MST_DEFS[i].def;
        for (int pi = 0; pi < NPATTERNS; pi++) for (int t = 0; t < NTRACKS; t++) {
            machine(pi, t, M_KICK); pat(pi)[8 + t] = 16; pat(pi)[16 + t] = SPEED_1X;
            for (int st = 0; st < NSTEPS; st++) memset(lock(pi, t, st), NO_LOCK, OLD_NPARAMS);
        }
        for (int i = 0; i < 8; i++) { tail()[i] = CMP_DEFS[i].def; fx()[i] = FX_DEFS[i].def; }
    }
    ~OldProj() { free(b); }
    uint8_t* pat(int pi) { return b + 20 + (long)pi * OLD_PAT_SIZE; }
    void machine(int pi, int t, int m) {
        pat(pi)[t] = (uint8_t)m;
        for (int i = 0; i < OLD_NPARAMS; i++) { int j = Engine::mapOldParam(m, i); p(pi, t)[i] = 0; if (j >= 0 && m != M_FM) p(pi, t)[i] = paramDef(m, j).def; }
    }
    uint8_t* p(int pi, int t) { return pat(pi) + 24 + t * OLD_NPARAMS; }
    uint8_t* lock(int pi, int t, int st) { return pat(pi) + 24 + 8 * OLD_NPARAMS + (t * NSTEPS + st) * (1 + OLD_NPARAMS) + 1; }
    uint8_t* tail() { return b + 20 + (long)NPATTERNS * OLD_PAT_SIZE; }
    uint8_t* fx() { return tail() + 8; }
    uint8_t* song(int i) { return tail() + 16 + i * 2; }
    uint8_t& songLen() { return tail()[144]; }
    uint8_t* patMute() { return tail() + 148; }
    uint8_t* row(int i) { return tail() + 148 + NPATTERNS + i * 12; }
    bool write(const char* path, bool trailer = false) {
        if (ver == 12) for (int i = 0; i < 64; i++) { uint8_t* r = row(i); if (!r[NTRACKS]) r[NTRACKS] = 1; }
        FILE* g = fopen(path, "wb"); if (!g) return false;
        bool ok = fwrite(b, 1, size, g) == (size_t)size;
        if (trailer) ok = ok && fwrite("SPL1\0\0", 1, 6, g) == 6;
        fclose(g); return ok;
    }
};

// Effekt-Busse: jeder Typ antwortet, bleibt endlich und begrenzt; Delay bringt das Echo zur eingestellten Zeit
static void testFxBus() {
    curTest = "Effekt-Busse";
    static FxBus fb;
    fb.init(44100);
    static float in[44100 * 6], oL[44100 * 6], oR[44100 * 6];
    for (int ty = 1; ty < BF_COUNT; ty++) {
        uint8_t p[8];
        for (int k = 0; k < 8; k++) p[k] = busDef(0, ty, k).def;
        p[BX_TYPE] = (uint8_t)ty;
        int N = 44100 * 4;
        for (int i = 0; i < N; i++) in[i] = 0.f;
        for (int k = 0; k < 4; k++) for (int i = 0; i < 400; i++) in[k * 22050 + i] = (i & 1 ? 0.5f : -0.5f) * (1.f - i / 400.f);
        double stepS = 44100 * 60.0 / (120 * 4.0);
        for (int i = 0; i < N; i += 32) fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, p, stepS);
        float pk = 0, energy = 0; bool fin = true;
        for (int i = 0; i < N; i++) { if (!isfinite(oL[i]) || !isfinite(oR[i])) fin = false; pk = fmaxf(pk, fmaxf(fabsf(oL[i]), fabsf(oR[i]))); energy += oL[i] * oL[i]; }
        CHECK(fin, "%s: NaN/Inf im Ausgang", BUS_TYPE_NAMES[ty]);
        CHECK(pk > 0.01f && pk < 2.f, "%s: Spitze %.3f", BUS_TYPE_NAMES[ty], pk);
        CHECK(energy > 1e-3f, "%s: kein Ausgang", BUS_TYPE_NAMES[ty]);
        printf("  %-8s Spitze %.2f\n", BUS_TYPE_NAMES[ty], pk);
    }
    {   // Delay 1/4 (= 4 Steps): erstes Echo nach 0,5 s bei 120 BPM
        uint8_t p[8]; for (int k = 0; k < 8; k++) p[k] = busDef(0, BF_DELAY, k).def;
        p[BX_TYPE] = BF_DELAY; p[BX_P1] = 10; p[BX_P2] = 0; p[BX_P4] = 0;
        fb.reset();
        int N = 44100;
        for (int i = 0; i < N; i++) in[i] = i < 64 ? 0.8f : 0.f;
        for (int i = 0; i < N; i += 32) fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, p, 44100 * 60.0 / (120 * 4.0));
        int at = -1; for (int i = 0; i < N && at < 0; i++) if (fabsf(oL[i]) > 0.05f) at = i;
        CHECK(at > 22050 - 300 && at < 22050 + 300, "Delay-Echo bei %d statt 22050", at);
    }
    {   // Reverb: lange Hallfahne klingt aus, kein Aufschaukeln bei maximaler Groesse
        uint8_t p[8]; for (int k = 0; k < 8; k++) p[k] = busDef(0, BF_REVERB, k).def;
        p[BX_TYPE] = BF_REVERB; p[BX_P1] = 127;
        fb.reset();
        int N = 44100 * 6;
        for (int i = 0; i < N; i++) in[i] = i < 2000 ? (i & 1 ? 0.5f : -0.5f) : 0.f;
        for (int i = 0; i < N; i += 32) fb.process(in + i, oL + i, oR + i, N - i < 32 ? N - i : 32, p, 5512.5);
        float e1 = 0, e2 = 0; for (int i = 44100; i < 88200; i++) e1 += oL[i] * oL[i]; for (int i = N - 44100; i < N; i++) e2 += oL[i] * oL[i];
        CHECK(e2 < e1, "Reverb waechst statt abzuklingen (%.4f -> %.4f)", e1, e2);
    }
}

static void testSaveLoad() {
    curTest = "Speichern/Laden";
    fresh(133);
    Pattern& P = E.proj.pat[2];
    P.s[5][77].on = 2; P.s[5][77].lock[P_FREQ] = 42; P.len[5] = 99; P.speed[5] = 6;
    E.proj.songLen = 3; Engine::rowFromPattern(E.proj, E.proj.row[1], 7); E.proj.row[1].pat[2] = SONG_EMPTY; E.proj.row[1].mute = 0x30; E.proj.patMute[4] = 0x81;
    static Project ref;
    memcpy(&ref, &E.proj, sizeof(Project));
    const char* path = "/tmp/gb-unit-test.gbx";
    CHECK(E.save(path), "Speichern fehlgeschlagen");
    E.defaultProject(false);
    CHECK(E.load(path), "Laden fehlgeschlagen");
    CHECK(!memcmp(&ref, &E.proj, sizeof(Project)), "Projekt nach Laden verschieden");
    // v9-Datei: SYNTH mit LP12 -> LP24
    {
        OldProj o(9);
        o.machine(0, 4, M_SYNTH); o.p(0, 4)[P_FTYP] = FT_LP12;
        CHECK(o.write(path), "v9 schreiben");
        CHECK(E.load(path), "Laden v9");
        CHECK(E.proj.version == PROJECT_VERSION, "Version %u", E.proj.version);
        CHECK(E.proj.pat[0].p[4][P_FTYP] == FT_LP24, "Migration LP12->LP24 fehlt (%d)", E.proj.pat[0].p[4][P_FTYP]);
    }
    // v11-Datei (Song als Kette, ohne Clip-Matrix): alter Teil + Sample-Liste dahinter
    {
        OldProj o(11);
        o.songLen() = 2; o.song(0)[0] = 3; o.song(0)[1] = 4; o.song(1)[0] = 9; o.song(1)[1] = 1;
        CHECK(o.write(path, true), "v11 schreiben");
        memset(E.proj.row, 0x77, sizeof E.proj.row);
        CHECK(E.load(path), "Laden v11");
        CHECK(E.proj.songLen == 2 && E.proj.row[0].pat[0] == 3 && E.proj.row[0].pat[7] == 3 && E.proj.row[0].rep == 4 && E.proj.row[1].pat[5] == 9,
              "Song v11 nicht in Zeilen umgestellt (%d %d %d)", E.proj.row[0].pat[0], E.proj.row[0].rep, E.proj.row[1].pat[5]);
        CHECK(E.proj.row[0].mute == 0 && E.proj.patMute[0] == 0, "Mutes nach Umstellung nicht leer");
    }
    // v12-Datei: FM mit 4 Operatoren, Sends auf die SEND-Seite, SYNTH-2. Seite, LFO-Ziele, Locks, Effekte -> Busse
    {
        OldProj o(12);
        uint8_t* f = o.p(2, 0);
        o.machine(2, 0, M_FM);
        f[P_S0] = 2; f[P_S1] = 30; f[P_S2] = 3; f[P_S3] = 10; f[P_S4] = 3; f[P_S5] = 3; f[P_S6] = 96; f[P_S7] = 10;
        f[P_X0] = 70; f[P_X1] = 0; f[P_X2] = 0; f[P_X3] = 127; f[P_X4] = 40; f[P_X5] = 50; f[P_X6] = 50; f[P_X7] = 90;
        o.lock(2, 0, 5)[P_S3] = 7;                       // Lock: RAT2 = 3.0
        uint8_t* y = o.p(2, 1);
        o.machine(2, 1, M_SYNTH); y[OLD_P_DLY] = 50; y[OLD_P_REV] = 60; y[P_L1 + LF_DEST] = OLD_P_DLY + 1;
        o.lock(2, 1, 3)[OLD_P_REV] = 99;
        o.fx()[FX_DTIM] = 6; o.fx()[FX_DFBK] = 70; o.fx()[FX_RSIZ] = 100;
        o.patMute()[2] = 0x81; o.songLen() = 1; o.row(0)[0] = 2;
        CHECK(o.write(path, true), "v12 schreiben");
        CHECK(E.load(path), "Laden v12");
        const Pattern& P = E.proj.pat[2];
        const uint8_t* q = P.p[0];
        CHECK(P.machine[0] == M_FM && q[P_S0] == 2 && q[P_S1] == 30 && q[P_S2] == 90 && q[P_S5] == 10, "FM: ALGO/FBK/VELO/NOIS falsch (%d %d %d %d)", q[P_S0], q[P_S1], q[P_S2], q[P_S5]);
        CHECK(FM_RATIOS[q[FM_OPB[0] + OP_RAT]] == 1.f && FM_RATIOS[q[FM_OPB[1] + OP_RAT]] == 5.f, "FM: Ratios falsch (%g %g)", FM_RATIOS[q[FM_OPB[0] + OP_RAT]], FM_RATIOS[q[FM_OPB[1] + OP_RAT]]);
        CHECK(q[FM_OPB[0] + OP_LVL] == 127 && q[FM_OPB[1] + OP_LVL] == 70 && q[FM_OPB[2] + OP_LVL] == 0, "FM: Pegel falsch");
        CHECK(q[FM_OPB[0] + OP_SUS] == 127 && q[FM_OPB[1] + OP_SUS] == 0 && q[FM_OPB[1] + OP_DEC] > 0, "FM: Huellkurven falsch (SUS1 %d SUS2 %d DEC2 %d)", q[FM_OPB[0] + OP_SUS], q[FM_OPB[1] + OP_SUS], q[FM_OPB[1] + OP_DEC]);
        CHECK(q[FM_OPB[1] + OP_DETN] > 64 && q[FM_OPB[0] + OP_DETN] == 64, "FM: FINE nicht in DETN umgerechnet (%d)", q[FM_OPB[1] + OP_DETN]);
        CHECK(FM_RATIOS[P.s[0][5].lock[FM_OPB[1] + OP_RAT]] == 3.f, "FM: Ratio-Lock falsch (%d)", P.s[0][5].lock[FM_OPB[1] + OP_RAT]);
        const uint8_t* w = P.p[1];
        CHECK(w[P_DLY] == 50 && w[P_REV] == 60 && w[P_A6] == 0 && w[P_A7] == 0, "Sends nicht verschoben (%d %d)", w[P_DLY], w[P_REV]);
        CHECK(w[P_L1 + LF_DEST] == P_DLY + 1, "LFO-Ziel nicht mitgezogen (%d)", w[P_L1 + LF_DEST]);
        CHECK(P.s[1][3].lock[P_REV] == 99 && P.s[1][3].lock[OLD_P_REV] == NO_LOCK, "Send-Lock nicht verschoben");
        CHECK(w[P_X0] == 3 && w[P_X2] == 127 && w[P_X3] == 127, "SYNTH 2. Seite nicht auf Grundwerte (%d %d)", w[P_X0], w[P_X2]);
        CHECK(E.proj.bus[0][BX_TYPE] == BF_DELAY && E.proj.bus[0][BX_P1] == 6 && E.proj.bus[0][BX_P2] == 70, "Delay nicht auf Bus A");
        CHECK(E.proj.bus[1][BX_TYPE] == BF_REVERB && E.proj.bus[1][BX_P1] == 100, "Reverb nicht auf Bus B");
        CHECK(E.proj.patMute[2] == 0x81 && E.proj.songLen == 1 && E.proj.row[0].pat[0] == 2, "v12: Mutes/Song verloren");
    }
    // kaputte Datei darf nicht geladen werden
    FILE* f = fopen(path, "wb"); fwrite("GBX1xxxx", 1, 8, f); fclose(f);
    CHECK(!E.load(path), "kaputte Datei wurde geladen");
    remove(path);
}

static Engine MC[2];   // zwei frische Engines: gleicher Startzustand, 1 bzw. 2 Kerne
static void testMultiCore() {
    curTest = "Mehrkern";
    static float out[2][44100 * 2];
    static const int ms[NTRACKS] = {M_SYNTH, M_WTBL, M_FM, M_GRAN, M_SYNTH, M_SAMPLE, M_FM, M_SNARE};
    for (int k = 0; k < 2; k++) {
        Engine& X = MC[k];
        X.init(44100);
        if (k && !X.helperReady()) { printf("  Mehrkern: nur ein Kern verfuegbar - uebersprungen\n"); return; }
        X.audioThreads = k ? 2 : 1;
        X.defaultProject(false);
        X.proj.bpm = 150;
        Pattern& P = X.proj.pat[0];
        for (int t = 0; t < NTRACKS; t++) {
            Engine::initTrack(P, t, ms[t]);
            for (int s = 0; s < 16; s++) P.s[t][s].on = 1;
            P.p[t][P_LEN] = 100; P.p[t][P_ITYP] = (uint8_t)(1 + t % 6); P.p[t][P_IMIX] = 100;
            if (t < 3) { P.p[t][P_AMOD] = ARP_CHORD; P.p[t][P_ACHD] = 5; }
        }
        X.proj.cmp[CM_SRC] = 3; X.proj.cmp[CM_THR] = 40;   // Sidechain-Spur kann auf beiden Kernen landen
        X.start();
        for (int i = 0; i < 44100; i += 256) X.render(out[k] + i * 2, 44100 - i < 256 ? 44100 - i : 256);
    }
    float md = 0; for (int i = 0; i < 44100 * 2; i++) { float d = fabsf(out[0][i] - out[1][i]); if (d > md) md = d; }
    printf("  Mehrkern: %ld Bloecke zweikernig, max. Abweichung %.2e\n", MC[1].wkBlocks, md);
    CHECK(MC[1].wkBlocks > 0, "zweiter Kern hat nie mitgerechnet");
    CHECK(MC[0].wkBlocks == 0, "1-Kern-Modus hat trotzdem aufgeteilt");
    CHECK(md < 1e-4f, "1 Kern und 2 Kerne klingen verschieden (%.2e)", md);
}

// Routing: Spur 1 + Spur 2 auf dasselbe Paar = Summe der einzelnen Ausgaenge
// LFO laeuft frei weiter, auch wenn die Stimme zwischen kurzen Noten stumm ist
static void testLfoFreeRun() {
    curTest = "LFO frei";
    static Engine X; static float out[512];
    X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
    Pattern& P = X.proj.pat[0];
    Engine::initTrack(P, 4, M_SYNTH);
    for (int t = 0; t < NTRACKS; t++) for (int s2 = 0; s2 < 64; s2++) P.s[t][s2].on = 0;
    for (int s2 = 0; s2 < 16; s2 += 4) P.s[4][s2].on = 1;
    P.p[4][P_LEN] = 32;
    P.p[4][P_L1 + LF_DEST] = P_FREQ + 1; P.p[4][P_L1 + LF_DEPTH] = 120; P.p[4][P_L1 + LF_RATE] = 70;
    P.p[4][P_L1 + LF_MODE] = LM_FREE; P.p[4][P_L1 + LF_SYNC] = 0;
    X.start();
    int idle = 0; float moved = 0, prev = -1;
    for (int i = 0; i < 44100 * 3; i += 256) {
        X.render(out, 256);
        float ph = X.v[4].lfPh[0];
        if (!X.v[4].active && prev >= 0) { idle++; float d = ph - prev; if (d < 0) d += 1.f; moved += d; }
        prev = ph;
    }
    printf("  LFO frei: %d stumme Bloecke, Phase dabei um %.2f Zyklen weiter\n", idle, moved);
    CHECK(idle > 50 && moved > 1.f, "LFO steht zwischen den Noten still (%.2f Zyklen in %d Bloecken)", moved, idle);
}

static void testMultiRoute() {
    curTest = "USB-Routing";
    static float out[44100 * 2], a[44100 * 8], b[44100 * 8];
    for (int run = 0; run < 2; run++) {
        static Engine RT[2];   // zwei frische Engines mit gleicher Vorgeschichte -> gleicher Klang
        Engine& X = RT[run];
        X.init(44100); X.stop(); X.audioThreads = 1; X.defaultProject(false); X.proj.bpm = 120;
        Pattern& P = X.proj.pat[0];
        Engine::initTrack(P, 0, M_KICK); Engine::initTrack(P, 1, M_SNARE);
        for (int s2 = 0; s2 < 16; s2 += 2) { P.s[0][s2].on = 1; P.s[1][s2 + 1].on = 1; }
        for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
        if (run == 0) { X.multiRoute[0] = 0; X.multiRoute[1] = 1; } else { X.multiRoute[0] = 2; X.multiRoute[1] = 2; }
        X.multiCh = 8; X.start();
        float* m = run ? b : a;
        for (int i = 0; i < 44100; i += 256) { X.multiBuf = m + (size_t)i * 8; X.render(out + i * 2, 44100 - i < 256 ? 44100 - i : 256); }
        X.multiBuf = nullptr; X.multiCh = 2;
        for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
    }
    float md = 0, e = 0;
    for (int i = 0; i < 44100; i++) for (int c = 0; c < 2; c++) {
        float sum = a[i * 8 + 2 + c] + a[i * 8 + 4 + c];
        float d = fabsf(b[i * 8 + 6 + c] - sum); if (d > md) md = d; e += fabsf(sum);
    }
    float rest = 0; for (int i = 0; i < 44100; i++) for (int c = 2; c < 6; c++) rest += fabsf(b[i * 8 + c]);
    printf("  USB-Routing: Summe auf einem Paar Abweichung %.2e (Energie %.0f), andere Paare %.2e\n", md, e, rest);
    CHECK(e > 1.f && md < 1e-4f, "zwei Spuren auf einem Paar ergeben nicht die Summe (%.2e)", md);
    CHECK(rest == 0.f, "nicht belegte Paare sind nicht still");
}

// 8 Kanaele: Master + FX A/B/C - die Bus-Kanaele muessen den Effektanteil tragen
static void testMultiOut8() {
    curTest = "Mehrkanal 8";
    static float out[44100 * 2], mb[44100 * 8];
    Engine& X = MC[0];
    X.init(44100); X.stop(); X.audioThreads = 1;
    X.defaultProject(false); X.proj.bpm = 140;
    Pattern& P = X.proj.pat[0];
    for (int t = 0; t < 4; t++) { Engine::initTrack(P, t, t ? M_SNARE : M_KICK); for (int s2 = 0; s2 < 16; s2 += 4) P.s[t][s2].on = 1; P.p[t][P_DLY] = 60; P.p[t][P_REV] = 60; P.p[t][P_SNDC] = 60; }
    X.multiCh = 8; X.start();
    for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = (signed char)(q >= NTRACKS ? q - NTRACKS : -1);
    for (int i = 0; i < 44100; i += 256) { X.multiBuf = mb + (size_t)i * 8; X.render(out + i * 2, 44100 - i < 256 ? 44100 - i : 256); }
    X.multiBuf = nullptr; X.multiCh = 2;
    for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
    float e[8] = {0};
    for (int i = 0; i < 44100; i++) for (int c = 0; c < 8; c++) e[c] += fabsf(mb[i * 8 + c]);
    printf("  Mehrkanal 8: Energie je Kanal %.0f %.0f | A %.0f %.0f | B %.0f %.0f | C %.0f %.0f\n", e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7]);
    for (int c = 2; c < 8; c++) CHECK(e[c] > 1.f, "8 Kanaele: Kanal %d still", c + 1);
}

// Mehrkanal-Ausgang (USB): Master gleich wie ohne Mehrkanal, jede Spur einzeln hoerbar - auf 1 und 2 Kernen
static void testMultiOut() {
    curTest = "Mehrkanal";
    static float ref[44100 * 2], out[44100 * 2], mb[44100 * 24];
    static const int ms[NTRACKS] = {M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_FM, M_WTBL, M_SYNTH};
    // 0: Referenz ohne Mehrkanal (MC[0]), 1: Mehrkanal 1 Kern (MC[1], gleiche Vorgeschichte -> gleicher Klang), 2: Mehrkanal 2 Kerne (MC[0], nur Kanaele)
    for (int run = 0; run < 3; run++) {
        Engine& X = MC[run == 1 ? 1 : 0];
        if (run == 2 && !X.helperReady()) { printf("  Mehrkanal: nur ein Kern - 2-Kern-Lauf uebersprungen\n"); break; }
        X.init(44100); X.stop(); X.audioThreads = run == 2 ? 2 : 1;
        X.defaultProject(false); X.proj.bpm = 140;
        Pattern& P = X.proj.pat[0];
        for (int t = 0; t < NTRACKS; t++) { Engine::initTrack(P, t, ms[t]); for (int s2 = 0; s2 < 16; s2 += 2) P.s[t][s2].on = 1; P.p[t][P_PAN] = (uint8_t)(t * 16); P.p[t][P_REV] = 40; }
        X.multiBuf = run ? mb : nullptr; X.multiCh = run ? 24 : 2;
        for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = (signed char)q;   // jede Quelle ein eigenes Paar
        X.start();
        float* o = run ? out : ref;
        for (int i = 0; i < 44100; i += 256) { if (run) X.multiBuf = mb + (size_t)i * 24; X.render(o + i * 2, 44100 - i < 256 ? 44100 - i : 256); }
        X.multiBuf = nullptr; X.multiCh = 2;
        for (int q = 0; q < NTRACKS + NBUS; q++) X.multiRoute[q] = -1;
        if (!run) continue;
        float md = 0, mm = 0;
        for (int i = 0; i < 44100 * 2; i++) { float d = fabsf(out[i] - ref[i]); if (d > md) md = d; }
        for (int i = 0; i < 44100; i++) for (int c = 0; c < 2; c++) { float d = fabsf(mb[i * 24 + c] - out[i * 2 + c]); if (d > mm) mm = d; }
        int silent = 0; float busE = 0;
        for (int t = 0; t < NTRACKS; t++) { float e = 0; for (int i = 0; i < 44100; i++) e += fabsf(mb[i * 24 + 2 + t * 2]) + fabsf(mb[i * 24 + 3 + t * 2]); if (e < 1.f) silent++; }
        for (int i = 0; i < 44100; i++) for (int c = 18; c < 24; c++) busE += fabsf(mb[i * 24 + c]);
        printf("  Mehrkanal (%d Kern%s): Abweichung zur Referenz %.2e, Kanal 1-2 = Ausgang %.2e, stumme Spuren %d, Bus-Energie %.0f\n", run, run > 1 ? "e" : "", md, mm, silent, busE);
        if (run == 1) CHECK(md < 1e-4f, "Mehrkanal veraendert den Master (%.2e)", md);
        CHECK(mm == 0.f, "Kanal 1-2 ist nicht der Master");
        CHECK(silent == 0, "%d Spuren fehlen im Mehrkanal-Ausgang", silent);
        CHECK(busE > 1.f, "Effekt-Bus-Kanaele sind still");
    }
}

static void testDeterminism() {
    curTest = "Reproduzierbarkeit";
    static float a[44100 * 2], b[44100 * 2];
    for (int k = 0; k < 2; k++) {
        E.init(44100);
        fresh(120);
        for (int s = 0; s < 16; s += 2) E.proj.pat[0].s[0][s].on = 1;
        E.start();
        E.render(k ? b : a, 22050);
    }
    CHECK(!memcmp(a, b, sizeof(float) * 44100), "zwei gleiche Durchlaeufe klingen verschieden (Zufall ohne festen Startwert)");
}

// 24-Bit-Ausgabe (WAV/USB): Rundung, Begrenzung, Dither, leise Signale bleiben erhalten, WAV liest sich wieder ein
static void testPcm24(const char* sdir) {
    curTest = "PCM 24 Bit";
    Dither d;
    float maxErr = 0;
    for (int i = 0; i < 20000; i++) {
        float x = sinf(i * 0.0137f) * 0.97f;
        float y = pcmQuant(x, 24, d) / 8388607.f;
        maxErr = fmaxf(maxErr, fabsf(x - y));
    }
    CHECK(maxErr < 3.f / 8388607.f, "24-Bit-Rundung zu ungenau (%.3g)", maxErr);
    CHECK(pcmQuant(1.5f, 24, d) == 8388607 && pcmQuant(-1.5f, 24, d) == -8388608, "24 Bit nicht begrenzt");
    CHECK(pcmQuant(1.5f, 16, d) == 32767 && pcmQuant(-1.5f, 16, d) == -32768, "16 Bit nicht begrenzt");
    uint8_t b3[3];
    put24(b3, -8388608); CHECK(get24(b3) == -8388608, "put24/get24 negativ");
    put24(b3, 1234567); CHECK(get24(b3) == 1234567, "put24/get24 positiv");
    // Stille -> nur Dither (+-1 LSB), mittelwertfrei
    long sum = 0; int amax = 0;
    for (int i = 0; i < 100000; i++) { int v = pcmQuant(0.f, 24, d); sum += v; if (abs(v) > amax) amax = abs(v); }
    CHECK(amax <= 1 && labs(sum) < 2000, "Dither zu stark/unsymmetrisch (max %d, Summe %ld)", amax, sum);
    // Sinus bei -100 dBFS: in 16 Bit fast weg (0,3 LSB), in 24 Bit sauber (84 LSB)
    double sig = 0, err = 0;
    for (int i = 0; i < 44100; i++) {
        float x = 1e-5f * sinf(i * 2.f * 3.14159265f * 997.f / 44100.f);
        float y = pcmQuant(x, 24, d) / 8388607.f;
        sig += (double)x * x; err += (double)(x - y) * (x - y);
    }
    double snr = 10 * log10(sig / err);
    CHECK(snr > 30, "leises Signal in 24 Bit verrauscht (SNR %.1f dB)", snr);
    // WAV schreiben (Stereo, 24 Bit, 48 kHz) und mit dem Sample-Leser wieder einlesen
    char path[600]; snprintf(path, sizeof path, "%s/zz-unit-24bit.wav", sdir);
    FILE* f = fopen(path, "wb");
    if (!f) { printf("  (WAV-Test uebersprungen: %s nicht beschreibbar)\n", sdir); return; }
    enum { N = 4800 };
    static float st[N * 2]; static uint8_t raw[N * 6];
    for (int i = 0; i < N; i++) { st[i * 2] = 0.5f * sinf(i * 0.05f); st[i * 2 + 1] = 0.25f; }
    uint8_t h[44]; wavHeader(h, 48000, 2, 24, N);
    pcmTo24(raw, st, N * 2, d);
    fwrite(h, 1, 44, f); fwrite(raw, 6, N, f); fclose(f);
    Sample s;
    bool ok = sampleRead("zz-unit-24bit", s);
    remove(path);
    CHECK(ok, "24-Bit-WAV nicht lesbar");
    if (!ok) return;
    CHECK(s.rate == 48000 && s.len == N, "WAV-Kopf falsch (Rate %d, Laenge %d)", s.rate, s.len);
    float me = 0;
    for (int i = 0; i < s.len && i < N; i++) me = fmaxf(me, fabsf(s.data[i] - (st[i * 2] + st[i * 2 + 1]) * 0.5f));
    CHECK(me < 1e-5f, "24-Bit-WAV liest sich anders zurueck (%.3g)", me);
    free(s.data);
}

int main(int argc, char** argv) {
    const char* sdir = argc > 1 ? argv[1] : "samples";
    printf("Samples: %d aus %s\n", samplesLoad(sdir), sdir);
    E.init(44100);
    testTiming();
    testPolymeter();
    testProbCond();
    testEuclid();
    testTrigless();
    testSong();
    testClipMatrix();
    testArp();
    testChord();
    testFactory();
    testStability();
    testDecay();
    testFxBus();
    testSaveLoad();
    testDeterminism();
    testMultiCore();
    testMultiOut();
    testMultiOut8();
    testMultiRoute();
    testLfoFreeRun();
    testPcm24(sdir);
    printf("%s: %d Pruefungen ok, %d Fehler\n", nFail ? "FEHLGESCHLAGEN" : "OK", nPass, nFail);
    return nFail ? 1 : 0;
}
