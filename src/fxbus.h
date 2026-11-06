// Effekt-Busse (A/B/C): jeder Bus traegt einen frei waehlbaren Effekt (BusType in params.h).
// Eingang mono (Summe der Spur-Sends + Weiterleitung vom vorigen Bus), Ausgang stereo.
#pragma once
#include <stdint.h>

struct FxBus {
    int sr = 44100;
    int type = -1;                 // zuletzt gerechneter Typ (Wechsel -> Zustand leeren)
    // gemeinsamer Verzoegerungsspeicher (Delay/Tape: 2 Kanaele, Chorus: kurzer Bereich)
    float* bufL = nullptr; float* bufR = nullptr; int bufLen = 0, pos = 0, filled = 0;
    // Reverb: 8 Leitungen (FDN) + Eingangs-Diffusion + Predelay
    float* fdn[8] = {nullptr}; int fdnLen[8] = {0}, fdnPos[8] = {0}; float fdnLp[8] = {0};
    float* dif[4] = {nullptr}; int difLen[4] = {0}, difPos[4] = {0};
    float* pre = nullptr; int preLen = 0, prePos = 0;
    // Spring: 2 Federn, je Dispersions-Allpasskette + Rueckkopplungsleitung
    float* spr[2] = {nullptr}; int sprLen = 0, sprPos = 0;
    float apZ[2][16] = {{0}}, sprLp[2] = {0}, sprHp[2] = {0}, sprFb[2] = {0};
    // Filter-/Modulationszustaende
    float lpL = 0, lpR = 0, hpL = 0, hpR = 0, inHp = 0, inHpX = 0;
    float ph1 = 0, ph2 = 0, ph3 = 0, wowRnd = 0, wowTarget = 0, dSm = 0, satL = 0, satR = 0;
    uint32_t rng = 0x2545F491u;
    float peak = 0;                // groesster Ausgangswert im letzten Block (Anzeige)
    int quiet = 1 << 30;           // Samples ohne Eingang und mit leisem Ausgang (dann wird nicht gerechnet)
    void init(int sampleRate);
    void reset();
    // in: n Samples mono; outL/outR werden ueberschrieben. p = 8 Bus-Regler (Typ in p[0]); stepSamples fuer TIME
    void process(const float* in, float* outL, float* outR, int n, const uint8_t* p, double stepSamples);
};
