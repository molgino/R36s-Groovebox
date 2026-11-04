// Unit-Tests fuer Sequencer, Engines, Filter und Projektformat (ohne UI/SDL-Fenster).
//   make test            -> baut und startet tests/unit
//   make test SAN=1      -> mit AddressSanitizer/UBSan (Speicherfehler, undefiniertes Verhalten)
#include "../src/engine.h"
#include "../src/samples.h"
#include "../src/factory.h"
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
    E.proj.song[0].pat = 0; E.proj.song[0].rep = 2;
    E.proj.song[1].pat = 3; E.proj.song[1].rep = 1;
    E.songStart(0); E.songOn = true;
    E.start();
    run(barFrames(120) * 2 + 2000);
    CHECK(E.curPat == 3, "nach 2 Takten Pattern %d statt 4", E.curPat + 1);
    run(barFrames(120));
    CHECK(E.curPat == 0, "nach 3 Takten Pattern %d statt 1 (Schleife)", E.curPat + 1);
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

static void testSaveLoad() {
    curTest = "Speichern/Laden";
    fresh(133);
    Pattern& P = E.proj.pat[2];
    P.s[5][77].on = 2; P.s[5][77].lock[P_FREQ] = 42; P.len[5] = 99; P.speed[5] = 6;
    E.proj.songLen = 3; E.proj.song[1].pat = 7;
    static Project ref;
    memcpy(&ref, &E.proj, sizeof(Project));
    const char* path = "/tmp/gb-unit-test.gbx";
    CHECK(E.save(path), "Speichern fehlgeschlagen");
    E.defaultProject(false);
    CHECK(E.load(path), "Laden fehlgeschlagen");
    CHECK(!memcmp(&ref, &E.proj, sizeof(Project)), "Projekt nach Laden verschieden");
    // v9-Datei: SYNTH mit LP12 -> LP24
    fresh(120);
    Engine::initTrack(E.proj.pat[0], 4, M_SYNTH);
    E.proj.pat[0].p[4][P_FTYP] = FT_LP12;
    E.proj.version = 9;
    CHECK(E.save(path), "Speichern v9");
    CHECK(E.load(path), "Laden v9");
    CHECK(E.proj.version == PROJECT_VERSION, "Version %u", E.proj.version);
    CHECK(E.proj.pat[0].p[4][P_FTYP] == FT_LP24, "Migration LP12->LP24 fehlt (%d)", E.proj.pat[0].p[4][P_FTYP]);
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
    testArp();
    testChord();
    testFactory();
    testStability();
    testDecay();
    testSaveLoad();
    testDeterminism();
    testMultiCore();
    printf("%s: %d Pruefungen ok, %d Fehler\n", nFail ? "FEHLGESCHLAGEN" : "OK", nPass, nFail);
    return nFail ? 1 : 0;
}
