// Audio-Engine: Sequencer, 8 monophone Spuren, Delay + Reverb, Master
#pragma once
#include "params.h"
#include "samples.h"
#include <stdint.h>

struct Step { uint8_t on; uint8_t lock[NPARAMS]; };

struct Pattern {
    uint8_t machine[NTRACKS];
    uint8_t len[NTRACKS];
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
};

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
    ADSR aenv, fenv;
    SVF filt, aux;
    float lfoPh = 0, lfoSH = 0, lfoVal = 0;
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
    // fuer die Anzeige
    volatile uint32_t trigs = 0;
    volatile float peakHold = 0;

    inline int P(int i) const { return lk[i] != NO_LOCK ? lk[i] : base[i]; }
    void noteOn(int machine, const uint8_t* base, const uint8_t* locks, float velScale, int gateSamples, float sr);
    void noteOff() { aenv.off(); fenv.off(); }
    bool gated() const { return machine == M_SYNTH || machine == M_FM || (machine == M_SAMPLE && P(P_S5)); }
    void update(int n, float sr, float bpm);
    void render(float* L, float* R, float* dS, float* rS, int n, float sr);
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
    int globalStep = 0;
    double clock = 0, nextStepAt = 0;
    bool mute[NTRACKS] = {false};
    volatile float dj = 0;     // -1..1 DJ-Filter (rechter Stick)
    float djSm = 0;
    volatile float peakL = 0, peakR = 0;
    Voice v[NTRACKS];

    // MIDI (Ausgabe ueber Callback aus dem Audio-Thread; frameOff = Position im aktuellen Puffer)
    typedef void (*MidiOutFn)(uint8_t st, uint8_t d1, uint8_t d2, int frameOff);
    MidiOutFn midiOut = nullptr;
    volatile bool notesOut = false, recording = false, midiInOn = true;
    volatile int clockMode = 0;   // 0 aus, 1 senden, 2 empfangen
    volatile int chMode = 0;      // 0 = AUTO (aktive Spur), 1 = Kanal 1-8 -> Spur 1-8
    volatile int uiTrack = 0, uiPage = 0, uiCursor = 0;
    volatile int stepRecCount = 0;

    void init(int sampleRate);
    void setRate(int sampleRate);
    void midiIn(uint8_t st, uint8_t d1, uint8_t d2);
    void defaultProject();
    void initPattern(Pattern& p);
    static void setMachine(Pattern& p, int t, int m);
    void start();
    void stop();
    void preview(int t, const Step* s);
    void render(float* out, int frames);
    bool save(const char* path);
    bool load(const char* path);
    double stepSamples() const { return sr * 60.0 / (proj.bpm * 4.0); }

private:
    struct Retrig { int left = 0; int interval = 0; int timer = 0; float vel = 1, fade = 0; uint8_t lk[NPARAMS]; } rt[NTRACKS];
    float* dlyL = nullptr; float* dlyR = nullptr; int dlyLen = 0, dlyPos = 0; float dlpL = 0, dlpR = 0;
    Reverb rev;
    SVF djL, djR;
    Rng rng;
    int curOff = 0;
    int midiNote[NTRACKS], midiGate[NTRACKS], liveNote[NTRACKS], skipStep[NTRACKS];
    uint8_t liveLk[NTRACKS][NPARAMS];
    double stepStartClock = 0, curStepLen = 1;
    double nextTickAt = 0, tickInt = 1; int ticksLeft = 0;
    int tickCount = 0; double tickHist[25]; int tickHistN = 0;
    bool sendStart = false, sendStop = false;
    void initFx(int sampleRate);
    void emit(uint8_t st, uint8_t d1, uint8_t d2, int off) { if (midiOut) midiOut(st, d1, d2, off); }
    void allNotesOff();
    void liveNoteOn(int t, int note, int vel);
    void clockTick();
    void doStep();
    void trigger(int t, const uint8_t* lk, float velScale, bool fromSeq, bool toMidi = true);
    void processBlock(float* out, int n);
};
