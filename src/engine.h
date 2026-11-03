// Audio-Engine: Sequencer, 8 monophone Spuren, Delay + Reverb, Master
#pragma once
#include "params.h"
#include "samples.h"
#include <stdint.h>

struct Step { uint8_t on; uint8_t lock[NPARAMS]; };

struct Pattern {
    uint8_t machine[NTRACKS];
    uint8_t len[NTRACKS];
    uint8_t speed[NTRACKS];      // Index in SPEED_TPS (ab v5)
    uint8_t p[NTRACKS][NPARAMS];
    Step s[NTRACKS][NSTEPS];
};

struct Project {
    char magic[4];
    uint32_t version;
    uint16_t bpm;
    uint8_t mst[8];
    uint8_t pad[2];
    Pattern pat[NPATTERNS];
    uint8_t cmp[8];          // ab Version 2: Kompressor
    uint8_t fx[8];           // ab Version 2: Delay/Reverb (vorher in mst)
    // ab Version 9: Song (Pattern-Kette)
    struct SongEntry { uint8_t pat, rep; } song[64];
    uint8_t songLen;
    uint8_t pad2[3];
};
enum { PROJECT_VERSION = 10, SONG_MAX = 64 };

struct Rng {
    uint32_t s = 0x9E3779B9u;
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (int32_t)s * (1.0f / 2147483648.0f); }
    inline float uni() { return (next() + 1.f) * 0.5f; }
};

struct SVF {
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 2;
    void set(float fc, float res, float sr);
    inline void proc(float v0, float& lo, float& bp, float& hp) {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lo = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

struct ADSR {
    int st = 0; float v = 0, ainc = 1, dco = 0, s = 1, rco = 0;
    void set(float a, float d, float sus, float r, float sr);
    void on() { st = 1; }
    void off() { if (st) st = 4; }
    inline float tick() {
        switch (st) {
        case 1: v += ainc; if (v >= 1.f) { v = 1.f; st = 2; } break;
        case 2: v = s + (v - s) * dco; if (fabsf(v - s) < 1e-4f) { v = s; st = 3; } break;
        case 3: v = s; break;
        case 4: v *= rco; if (v < 1e-5f) { v = 0; st = 0; } break;
        }
        return v;
    }
};

struct Voice {
    int machine = 0;
    bool active = false;
    uint8_t lk[NPARAMS];
    const uint8_t* base = nullptr;
    float vel = 1, note = 60, curNote = 60;
    int gate = -1;
    // Oszillatoren
    float ph1 = 0, ph2 = 0, phs = 0, phM = 0, fbPrev = 0;
    float hph[6] = {0};
    // Drum-Huellkurven
    float denv = 0, ddco = 0, penv = 0, pco = 0, nenv = 0, nco = 0, cenv = 0, cco = 0;
    int clapLeft = 0, clapTimer = 0, clapInterval = 1; float burst = 0, bco = 0; bool clapTail = false;
    float menv = 0, mco = 0;
    // FM mit 4 Operatoren
    float fmPh[4] = {0}, fmEnv[4] = {0}, fmDco[4] = {1, 1, 1, 1}, fmInc[4] = {0}, fmMod[4] = {0}, fmCar[4] = {0};
    float fmFbAmt = 0, fmFbPrev = 0;
    int fmAlgo = 0;
    int scaleVal = 0;   // globale Tonskala (vom Engine gesetzt)
    // neue Engines
    float wtPos = 0, wtWarp = 0, wtDet = 1, wtPenv = 0, sAtkInc = 0, shkLvl = 0, shkGrit = 0, shkGain = 1;
    int shkStage = 0, wtTab = 0;
    int chopStep = 0, chopFade = 64;
    struct Grain { bool on; double pos; float inc, age, len; } gr[12];
    float grTimer = 0, grInterval = 1000, grLen = 2000, grPosF = 0, grSpray = 0, grPitch = 1, grPSpr = 0;
    ADSR aenv, fenv;
    SVF filt, aux;
    SVF filt2, filt3;          // weitere Stufen (24 dB, Formant)
    // Filter 1: Typ (FT_*), Ladder-Zustaende, Kammfilter-Puffer, Formant-Pegel
    float lad[4] = {0}, ladG = 0.5f, ladK = 0;
    float cmb[2048]; int cmbW = 0; float cmbD = 100, cmbFb = 0, cmbLp = 0;
    float fmtG[3] = {1, 0.6f, 0.4f};
    float drift1 = 0, drift2 = 0;   // analoge Tonhoehen-Drift (Cent)
    float nzHp = 0, hatHp = 0;      // Hochpass-Zustaende (Snare-Rauschen, Hi-Hat)
    // 3 LFOs pro Stimme (Ziel = beliebiger Spurparameter)
    float lfPh[NLFO] = {0}, lfVal[NLFO] = {0}, lfSH[NLFO] = {0}, lfSHp[NLFO] = {0}, lfRun[NLFO] = {0}, lfFade[NLFO] = {1, 1, 1};
    float lfHeld[NLFO] = {0}, lfOff[NLFO] = {0};
    int lfDst[NLFO] = {-1, -1, -1}, lfMax[NLFO] = {127, 127, 127};
    float noteMod = 0;
    float stepSec = 0.125f;   // Laenge eines 16tels (fuer synchrone LFOs)
    float hold = 0; int holdC = 0;
    float glideCo = 0;
    // Sample-Wiedergabe
    const Sample* smp = nullptr;
    double spos = 0; float sinc = 0, sGain = 1; int sStart = 0, sEnd = 0; bool sRev = false, sLoop = false, sDone = false;
    Rng rng;
    // pro Block berechnet
    float pitchHz = 100, gainL = 0, gainR = 0, sendD = 0, sendR = 0, drive = 0, crushQ = 0;
    int srrN = 1, ftype = 0;
    float fBase = 127, fEnv = 0, reso = 0;
    SVF bwHp, bwLp;              // Base-Width-Filter
    float bwLo = 20, bwHi = 20000; bool bwOn = false;
    // Insert-Effekt
    float ib[2048];
    int iw = 0;
    float insPh = 0, insRate = 0, insMix = 0, insP2 = 0, insP3 = 0, insFb = 0;
    float apZL[6] = {0}, apZR[6] = {0}, apA[2] = {0}, phFbL = 0, phFbR = 0, odLp = 0, odG = 1, odLvl = 1, odCo = 1;
    int insType = 0;
    // fuer die Anzeige
    volatile uint32_t trigs = 0;
    volatile float peakHold = 0;

    inline int PR(int i) const { return lk[i] != NO_LOCK ? lk[i] : base[i]; }   // ohne LFO
    inline int P(int i) const {                                                   // mit LFO-Modulation
        int r = PR(i);
        for (int k = 0; k < NLFO; k++)
            if (i == lfDst[k]) { r += (int)lfOff[k]; if (r < 0) r = 0; if (r > lfMax[k]) r = lfMax[k]; }
        return r;
    }
    void lfoUpdate(int n, float sr);
    void lfoTrigger();
    void noteOn(int machine, const uint8_t* base, const uint8_t* locks, float velScale, int gateSamples, float sr);
    void noteOff() { aenv.off(); fenv.off(); }
    bool gated() const { return machine == M_SYNTH || machine == M_FM || machine == M_WTBL || machine == M_GRAN || (machine == M_SAMPLE && P(P_S5)); }
    void update(int n, float sr, float bpm);
    void render(float* L, float* R, float* dS, float* rS, int n, float sr);
    inline float filter1(float x);
};

struct Comb { float* b = nullptr; int n = 0, i = 0; float store = 0; };
struct Allpass { float* b = nullptr; int n = 0, i = 0; };

struct Reverb {
    Comb cl[8], cr[8]; Allpass al[4], ar[4];
    void init(int sr);
    void process(float in, float size, float damp, float& oL, float& oR);
};

class Engine {
public:
    Project proj;
    int sr = 44100;
    volatile bool playing = false;
    volatile int curPat = 0, nextPat = -1;
    volatile int trackPos[NTRACKS] = {0};
    volatile int playedStep[NTRACKS] = {0};
    double clock = 0;
    volatile bool fill = false;        // FILL-Modus fuer Trig-Bedingungen
    volatile int loopCount[NTRACKS] = {0};
    uint32_t tmReg[NTRACKS] = {0};                    // Turing-Register (generativ)
    bool mute[NTRACKS] = {false};
    volatile float dj = 0;     // -1..1 DJ-Filter (rechter Stick)
    bool solo[NTRACKS] = {false};
    bool audible(int t) const {   // Mute/Solo
        if (mute[t]) return false;
        for (int i = 0; i < NTRACKS; i++) if (solo[i]) return solo[t];
        return true;
    }
    float djSm = 0;
    volatile float peakL = 0, peakR = 0;
    volatile float grPeak = 0;   // Gain-Reduktion in dB (fuer die Anzeige)
    Voice v[NTRACKS];

    // MIDI (Ausgabe ueber Callback aus dem Audio-Thread; frameOff = Position im aktuellen Puffer)
    typedef void (*MidiOutFn)(uint8_t st, uint8_t d1, uint8_t d2, int frameOff);
    MidiOutFn midiOut = nullptr;
    volatile bool notesOut = false, recording = false, midiInOn = true;
    volatile int clockMode = 0;   // 0 aus, 1 senden, 2 empfangen
    volatile int chMode = 0;      // 0 = AUTO (aktive Spur), 1 = Kanal 1-8 -> Spur 1-8
    volatile int uiTrack = 0, uiPage = 0, uiCursor = 0;
    volatile int stepRecCount = 0;
    // Song-Modus: Pattern-Kette proj.song abspielen
    volatile bool songOn = false;
    volatile int songPos = 0, songRep = 0;
    void songStart(int pos);   // ab Eintrag pos (nur Engine-Zustand; Wiedergabe startet mit start())

    void init(int sampleRate);
    void setRate(int sampleRate);
    void midiIn(uint8_t st, uint8_t d1, uint8_t d2);
    void defaultProject(bool demo = true);
    void initPattern(Pattern& p);
    static void setMachine(Pattern& p, int t, int m);
    static void initTrack(Pattern& p, int t, int m);   // Klang einer Spur auf Grundwerte der Engine
    void start();
    void stop();
    void preview(int t, const Step* s);
    void dropSamples() { for (int t = 0; t < NTRACKS; t++) if (v[t].smp) { v[t].smp = nullptr; v[t].active = false; } }   // vor dem Austausch der Sample-Bank
    void render(float* out, int frames);
    bool save(const char* path);
    bool load(const char* path);
    double stepSamples() const { return sr * 60.0 / (proj.bpm * 4.0); }

private:
    struct Retrig { int left = 0; int interval = 0; int timer = 0; float vel = 1, fade = 0; uint8_t lk[NPARAMS]; } rt[NTRACKS];
    // Arpeggiator je Spur: spielt waehrend der Trig-Laenge die Akkordtoene
    struct Arp { int left = 0; int interval = 0; int timer = 0; int idx = 0, n = 0, mode = 0, gate = 0; float vel = 1, acc = 0;
                 uint8_t notes[48]; uint8_t lk[NPARAMS]; Rng rng; } ar[NTRACKS];
    void arpFire(int t);
    void playNote(int t, const uint8_t* lk, float velScale, int gs, bool toMidi);
    float* dlyL = nullptr; float* dlyR = nullptr; int dlyLen = 0, dlyPos = 0; float dlpL = 0, dlpR = 0;
    Reverb rev;
    SVF djL, djR;
    float cEnv = 0;
    float* preBuf = nullptr; int preLen = 0, prePos = 0;
    Rng rng;
    int curOff = 0;
    int midiNote[NTRACKS], midiGate[NTRACKS], liveNote[NTRACKS], skipStep[NTRACKS];
    uint8_t liveLk[NTRACKS][NPARAMS];
    // Sequencer-Raster: 12 Ticks pro 16tel, jede Spur mit eigener Geschwindigkeit
    double nextTickAt = 0;
    long globalTick = 0, patTick = 0;
    int trackTick[NTRACKS] = {0};
    bool lastCond[NTRACKS] = {false};
    int dirSt[NTRACKS] = {1, 1, 1, 1, 1, 1, 1, 1};   // Ping-Pong-Richtung
    long stepCnt[NTRACKS] = {0};                      // gespielte Steps seit Start (fuer Durchlaeufe)
    uint8_t genLk[NTRACKS][NPARAMS];
    int seqP(int t, int i);                           // SEQ-Parameter inkl. LFO-Modulation
    void resetSeq(int t);
    // Mikrotiming: verzoegerte bzw. vorgezogene Trigs
    struct Pend { bool on; double at; int step; uint8_t lk[NPARAMS]; } pend[NTRACKS][2];
    int preDone[NTRACKS];                             // Step, der schon vorgezogen ausgeloest wurde
    bool trigFor(int t, int s, bool on, uint32_t reg, uint8_t* outLk);
    void schedule(int t, double at, int s, const uint8_t* lk);
    double stepStartT[NTRACKS] = {0}, stepLenT[NTRACKS] = {1};
    int tickCount = 0; double tickHist[25]; int tickHistN = 0;
    float dSm = 0, dhpL = 0, dhpR = 0;   // Delay: geglaettete Zeit, Hochpass in der Schleife
    void doTick();
    void stepTrack(int t);
    bool condOk(int t, int cond);
    double trackStepSamples(int t) const { return stepSamples() * SPEED_TPS[proj.pat[curPat].speed[t] % 7] / 12.0; }
    bool sendStart = false, sendStop = false;
    void initFx(int sampleRate);
    void emit(uint8_t st, uint8_t d1, uint8_t d2, int off) { if (midiOut) midiOut(st, d1, d2, off); }
    void allNotesOff();
    void liveNoteOn(int t, int note, int vel);
    void clockTick();
    void trigger(int t, const uint8_t* lk, float velScale, bool fromSeq, bool toMidi = true);
    void processBlock(float* out, int n);
};
