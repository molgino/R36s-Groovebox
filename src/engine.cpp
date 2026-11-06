#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stddef.h>
#include <unistd.h>
#include <sched.h>

static float SINT[2049];
// Wavetables: 4 Tabellen x 32 Frames x 256 Samples (beim Start berechnet)
enum { WT_TABS = 4, WT_FR = 32, WT_N = 256 };
static float WT[WT_TABS][WT_FR][WT_N + 1];
static void buildWavetables() {
    const float PI2 = 6.2831853f;
    for (int f = 0; f < WT_FR; f++) {
        float x = f / (float)(WT_FR - 1);
        for (int i = 0; i <= WT_N; i++) {
            float p = (i % WT_N) / (float)WT_N;
            // 0 BASIC: Sinus -> Dreieck -> Saege -> Rechteck (bandbegrenzt, 24 Obertoene)
            float sn = 0, tr = 0, sw = 0, sq = 0;
            for (int h = 1; h <= 24; h++) {
                float s1 = sinf(PI2 * h * p);
                if (h == 1) sn = s1;
                sw += s1 / h * 0.6f;
                if (h & 1) { sq += s1 / h * 0.9f; tr += ((h / 2) & 1 ? -1.f : 1.f) * s1 / (h * h) * 0.8f; }
            }
            float seg = x * 3.f; int si = (int)seg; float fr = seg - si;
            float sh[4] = {sn, tr, sw, sq};
            WT[0][f][i] = si >= 3 ? sq : sh[si] + (sh[si + 1] - sh[si]) * fr;
            // 1 PWM: Pulsbreite 50 % -> 5 %
            float pw = 0.5f - x * 0.45f, pv = 0;
            for (int h = 1; h <= 24; h++) pv += sinf(3.14159265f * h * pw) / h * cosf(PI2 * h * (p - pw * 0.5f));
            WT[1][f][i] = pv * 0.9f;
            // 2 VOKAL: Formanten A -> E -> I -> O -> U (110 Hz Grundton gedacht)
            static const float F1[5] = {730, 530, 270, 570, 300}, F2[5] = {1090, 1840, 2290, 840, 870};
            float vs = x * 4.f; int vi = (int)vs; if (vi > 3) vi = 3; float vf = vs - vi;
            float f1 = F1[vi] + (F1[vi + 1] - F1[vi]) * vf, f2 = F2[vi] + (F2[vi + 1] - F2[vi]) * vf;
            float vo = 0;
            for (int h = 1; h <= 30; h++) {
                float fh = 110.f * h;
                float a = expf(-((fh - f1) / 90.f) * ((fh - f1) / 90.f)) + 0.6f * expf(-((fh - f2) / 120.f) * ((fh - f2) / 120.f)) + 0.15f / h;
                vo += a * sinf(PI2 * h * p);
            }
            WT[2][f][i] = vo * 0.5f;
            // 3 OBERTOENE: Frame k = Summe der ersten k+1 Harmonischen
            float ov = 0;
            for (int h = 1; h <= f + 1; h++) ov += sinf(PI2 * h * p) / sqrtf((float)h);
            WT[3][f][i] = ov;
        }
    }
    for (int t = 0; t < WT_TABS; t++)   // jede Frame auf +-1 normieren
        for (int f = 0; f < WT_FR; f++) {
            float m = 1e-6f;
            for (int i = 0; i <= WT_N; i++) if (fabsf(WT[t][f][i]) > m) m = fabsf(WT[t][f][i]);
            for (int i = 0; i <= WT_N; i++) WT[t][f][i] /= m;
        }
}
static inline float wtRead(int t, float pos, float ph) {
    float fp = pos * (WT_FR - 1); int f0 = (int)fp; if (f0 >= WT_FR - 1) f0 = WT_FR - 2; float ff = fp - f0;
    float x = ph * WT_N; int i0 = (int)x; float fx = x - i0; i0 &= WT_N - 1;
    const float* a = WT[t][f0]; const float* b = WT[t][f0 + 1];
    float va = a[i0] + (a[i0 + 1] - a[i0]) * fx, vb = b[i0] + (b[i0 + 1] - b[i0]) * fx;
    return va + (vb - va) * ff;
}
// eigene Wavetable aus einem Sample: Frames zu 2048 Werten (Standard vieler Wavetable-Synths), sonst 256, sonst ein Zyklus
static inline int wtUserFrame(const Sample* sm) {
    if (sm->len >= 2048 && sm->len % 2048 == 0) return 2048;
    if (sm->len >= 256 && sm->len % 256 == 0) return 256;
    return sm->len;
}
static inline float wtReadUser(const Sample* sm, float pos, float ph) {
    int F = wtUserFrame(sm), nf = sm->len / F;
    float fp = pos * (nf - 1); int f0 = (int)fp; if (f0 > nf - 2) f0 = nf > 1 ? nf - 2 : 0; float ff = nf > 1 ? fp - f0 : 0.f;
    float x = ph * F; int i0 = (int)x; float fx = x - i0; if (i0 >= F) i0 = F - 1;
    int i1 = i0 + 1 >= F ? 0 : i0 + 1;
    const float* a = sm->data + (long)f0 * F; const float* b = nf > 1 ? a + F : a;
    float va = a[i0] + (a[i1] - a[i0]) * fx, vb = b[i0] + (b[i1] - b[i0]) * fx;
    return va + (vb - va) * ff;
}
const float* wavetableFrame(int t, int f) { return WT[t & 3][f < 0 ? 0 : (f >= WT_FR ? WT_FR - 1 : f)]; }
static inline float sin01(float ph) {
    ph -= (float)(int)ph;          // schneller als floorf (Phase meist in [0,1))
    if (ph < 0.f) ph += 1.f;
    float x = ph * 2048.f;
    int i = (int)x;
    float f = x - i;
    return SINT[i] + (SINT[i + 1] - SINT[i]) * f;
}
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
// schnelle 2^x bzw. log2 (Fehler < 0,01 %, fuer Tonhoehen-Huellkurven und Wavetable-Warp pro Sample)
static inline float fastExp2(float x) {
    if (x < -120.f) return 0.f;
    if (x > 120.f) x = 120.f;
    int ii = (int)x; if ((float)ii > x) ii--;
    float xi = (float)ii, f = x - xi;
    float p = 1.f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    union { float f; int32_t i; } u; u.f = p; u.i += ii * (1 << 23);
    return u.f;
}
static inline float fastLog2(float x) {
    union { float f; int32_t i; } u; u.f = x;
    float e = (float)(((u.i >> 23) & 255) - 128);
    u.i = (u.i & ~(255 << 23)) | (127 << 23);
    float m = u.f;
    return e + (-0.34484843f * m + 2.02466578f) * m - 0.67487759f;
}
static inline float fastPow01(float b, float e) { return b <= 0.f ? 0.f : fastExp2(e * fastLog2(b)); }
// Sample lesen mit 4-Punkt-Hermite-Interpolation (kubisch): deutlich weniger Spiegelfrequenzen ("Zischeln")
// als linear, wenn ein Sample transponiert oder mit anderer Samplerate abgespielt wird
static inline float smpRead(const Sample* sm, double pos) {
    int n = sm->len, i = (int)pos;
    if (i < 0) i = 0;
    if (i > n - 1) i = n - 1;
    float t = (float)(pos - i);
    const float* d = sm->data;
    float y0 = d[i > 0 ? i - 1 : 0], y1 = d[i], y2 = d[i + 1 < n ? i + 1 : n - 1], y3 = d[i + 2 < n ? i + 2 : n - 1];
    float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}
static inline float fastTanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}
static inline float fastExpE(float x) { return fastExp2(x * 1.4426950f); }
static inline float envT(int v) { return 0.0005f * fastExp2(v * (15.f / 127.f)); }
static inline float decayCo(float t, float sr) { return fastExpE(-6.9f / (t * sr)); }
static inline float envTime(int v) { return 0.0005f * fastExp2(v * (15.f / 127.f)); }   // Huellkurvenzeit wie AMP/FILTER (0,5 ms .. 16 s)
static inline float blep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}
static inline float osc(int w, float ph, float dt, float pw) {
    switch (w) {
    case 0: return 2.f * ph - 1.f - blep(ph, dt);
    case 1: {
        float v = ph < pw ? 1.f : -1.f;
        v += blep(ph, dt);
        float p2 = ph - pw; if (p2 < 0) p2 += 1.f;
        v -= blep(p2, dt);
        return v;
    }
    case 2: return 1.f - 4.f * fabsf(ph - 0.5f);
    default: return sin01(ph);
    }
}

void SVF::set(float fc, float res, float sr) {
    fc = clampf(fc, 10.f, sr * 0.45f);
    float g = tanf(3.14159265f * fc / sr);
    k = 2.f - 1.96f * clampf(res, 0.f, 1.f);
    a1 = 1.f / (1.f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

void ADSR::set(float a, float d, float sus, float r, float sr) {
    ainc = 1.f / (a * sr + 1.f);
    dco = decayCo(d, sr);
    s = sus;
    rco = decayCo(r, sr);
}

// ---------------------------------------------------------------- Voice

void Voice::noteOn(int m, const uint8_t* b, const uint8_t* locks, float velScale, int gateSamples, float sr) {
    bool wasActive = active && machine == m;
    if (!active) lastS = 0.f;   // verklungen: von Stille aus starten
    dcArm = true;               // erster neuer Sample-Wert wird an den letzten alten angeschlossen
    machine = m;
    base = b;
    for (int i = 0; i < NPARAMS; i++) lk[i] = locks ? locks[i] : NO_LOCK;
    vel = clampf(P(P_VEL) / 127.f * velScale, 0.f, 1.f);
    note = (float)quantizeNote(P(P_NOTE), scaleVal);
    gate = gateSamples;
    if (!wasActive || (m != M_SYNTH && m != M_WTBL) || P(P_S7) == 0) curNote = note;
    active = true;
    lfoTrigger();
    if (m == M_CHOP || m == M_GRAN) {
        smp = sampleGet(P(P_S0));
        if (!smp || smp->len < 64) { active = false; return; }
        if (m == M_CHOP) {
            int n = P(P_S1) + 1, sl = P(P_S2) ? (P(P_S2) - 1) % n : chopStep % n;
            int segLen = smp->len / n;
            int a = sl * segLen, L = (int)(segLen * (P(P_S5) / 100.f));
            if (L < 32) L = 32;
            sRev = P(P_S6) != 0;
            sStart = sRev ? a + L - 1 : a; sEnd = sRev ? a : a + L - 1;
            spos = sStart; sDone = false; sLoop = false;
        } else {
            for (auto& g : gr) g.on = false;
            grTimer = 0;
        }
    }
    if (m == M_SAMPLE) {
        smp = sampleGet(P(P_S0));
        if (!smp) { active = false; return; }
        int a = (int)(P(P_S3) / 127.f * (smp->len - 1)), e = (int)(P(P_S4) / 127.f * (smp->len - 1));
        sRev = e < a;
        sStart = a; sEnd = e; spos = a; sDone = false;
        sLoop = P(P_S5) != 0;
        if (sStart == sEnd) { active = false; return; }
    }
    update(0, sr, 120);
    aenv.on();
    fenv.on();
    switch (m) {
    case M_KICK: ph1 = 0; penv = 1; denv = 1; cenv = 1; aenv.v = 0; break;
    case M_SNARE: ph1 = ph2 = 0; denv = 1; nenv = 1; penv = 1; break;
    case M_HAT: denv = 1; break;
    case M_CLAP: clapLeft = P(P_S3) + 1; clapTimer = 0; clapTail = false; burst = 0; denv = 0; break;
    case M_FM:
        for (int k = 0; k < 4; k++) { fmSt[k] = 1; if (!wasActive) { fmPh[k] = 0; fmEnv[k] = 0.f; } }   // Attack ab aktuellem Pegel
        if (!wasActive) fmFbPrev = 0;
        fmPenv = 1.f;
        break;
    case M_SAMPLE: case M_CHOP: aenv.v = 0; break;
    case M_TOM: ph1 = 0; penv = 1; denv = 1; nenv = 1; aenv.v = 0; break;
    case M_RIM: ph1 = ph2 = 0; denv = 1; cenv = 1; aenv.v = 0; break;
    case M_SHKR: shkStage = 1; shkLvl = 0; denv = 1; break;
    case M_WTBL: if (!wasActive) { ph1 = 0; ph2 = 0.37f; } break;
    default: break;
    }
    trigs = trigs + 1;
}

// ---- LFOs (3 pro Stimme, Elektron-Modi FREE/TRIG/HOLD/ONE/HALF)
float Voice_lfoWave(const Voice& v, int k, int wave, float p) {
    p -= floorf(p);
    switch (wave) {
    case LW_SIN: return sin01(p);
    case LW_TRI: return 1.f - 4.f * fabsf(p - 0.5f);
    case LW_SAWUP: return 2.f * p - 1.f;
    case LW_SAWDN: return 1.f - 2.f * p;
    case LW_SQR: return p < 0.5f ? 1.f : -1.f;
    case LW_EXPUP: return (exp2f(p * 5.f) - 1.f) / 31.f * 2.f - 1.f;
    case LW_EXPDN: return (exp2f((1.f - p) * 5.f) - 1.f) / 31.f * 2.f - 1.f;
    case LW_SH: return v.lfSH[k];
    default: { float x = (1.f - cosf(p * 3.14159265f)) * 0.5f; return v.lfSHp[k] + (v.lfSH[k] - v.lfSHp[k]) * x; }
    }
}

void Voice::lfoTrigger() {
    for (int k = 0; k < NLFO; k++) {
        int b = LFO_BASE[k];
        int mode = PR(b + LF_MODE), fd = PR(b + LF_FADE) - 64;
        if (mode == LM_HOLD) lfHeld[k] = Voice_lfoWave(*this, k, PR(b + LF_WAVE) % LW_COUNT, lfPh[k]);
        else if (mode != LM_FREE) { lfPh[k] = PR(b + LF_PHASE) / 128.f; lfRun[k] = 0; }
        lfFade[k] = fd < 0 ? 0.f : 1.f;
    }
}

void Voice::lfoUpdate(int n, float sr) {
    noteMod = 0;
    lfAny = false; lfMask[0] = lfMask[1] = 0;
    for (int k = 0; k < NLFO; k++) {
        int b = LFO_BASE[k];
        int dest = PR(b + LF_DEST) - 1;
        float depth = (PR(b + LF_DEPTH) - 64) / 64.f;
        if (dest < 0 || depth == 0.f) { lfDst[k] = -1; lfVal[k] = 0; continue; }
        int sync = PR(b + LF_SYNC) % 12, wave = PR(b + LF_WAVE) % LW_COUNT, mode = PR(b + LF_MODE);
        float hz = sync ? 1.f / (LFO_SYNC_STEPS[sync] * stepSec) : 0.05f * fastExp2(PR(b + LF_RATE) * (9.f / 127.f));
        float inc = hz * n / sr;
        float lim = mode == LM_ONE ? 1.f : (mode == LM_HALF ? 0.5f : 1e9f);
        if (lfRun[k] < lim) {
            if (lfRun[k] + inc > lim) inc = lim - lfRun[k] - 1e-5f;
            lfRun[k] += inc;
            lfPh[k] += inc;
            if (lfPh[k] >= 1.f) { lfPh[k] -= floorf(lfPh[k]); lfSHp[k] = lfSH[k]; lfSH[k] = rng.next(); }
        }
        float w = mode == LM_HOLD ? lfHeld[k] : Voice_lfoWave(*this, k, wave, lfPh[k]);
        int fd = PR(b + LF_FADE) - 64;
        if (fd) {
            float ft = 0.05f * fastExp2(fabsf((float)fd) / 64.f * 6.f);
            float st = n / (ft * sr);
            lfFade[k] = fd < 0 ? fminf(1.f, lfFade[k] + st) : fmaxf(0.f, lfFade[k] - st);
        } else lfFade[k] = 1.f;
        float out = w * depth * lfFade[k];
        lfVal[k] = out;
        if (dest == P_NOTE) { noteMod += out * 12.f; lfDst[k] = -1; }
        else if (dest < NPARAMS) {
            lfDst[k] = dest; lfAny = true; lfMask[dest >> 6] |= 1ull << (dest & 63);
            lfMax[k] = paramDef(machine, dest).max;
            lfOff[k] = out * (lfMax[k] < 1 ? 1 : lfMax[k]);
        } else lfDst[k] = -1;
    }
}

void Voice::update(int n, float sr, float bpm) {
    // Schutz: Phasen bleiben in [0,1) (nach Engine-Wechsel oder Schrittweite > 1 konnten sie sonst endlos wachsen -> NaN),
    // ein NaN im Filter (z.B. nach extremen Werten) wird geleert statt die Spur dauerhaft zu verstummen
    if (!(ph1 >= 0.f && ph1 < 1.f)) ph1 = 0.f;
    if (!(ph2 >= 0.f && ph2 < 1.f)) ph2 = 0.f;
    if (!(phs >= 0.f && phs < 1.f)) phs = 0.f;
    for (int k = 0; k < 4; k++) if (!(fmPh[k] >= 0.f && fmPh[k] < 1.f)) fmPh[k] = 0.f;
    if (!(filt.ic1 == filt.ic1) || !(filt.ic2 == filt.ic2) || !(filt2.ic1 == filt2.ic1) || !(filt3.ic1 == filt3.ic1) || !(lad[3] == lad[3]) || !(cmbLp == cmbLp)) {
        filt.reset(); filt2.reset(); filt3.reset(); aux.reset(); bwHp.reset(); bwLp.reset();
        for (int k = 0; k < 4; k++) lad[k] = 0.f;
        for (int k = 0; k < 2048; k++) cmb[k] = 0.f;
        cmbLp = 0.f; lastS = 0.f; dcOff = 0.f;
    }
    (void)bpm;
    lfoUpdate(n, sr);
    aenv.set(envT(P(P_AATK)), envT(P(P_ADEC)), P(P_ASUS) / 127.f, envT(P(P_AREL)), sr);
    fenv.set(envT(P(P_FATK)), envT(P(P_FDEC)), P(P_FSUS) / 127.f, envT(P(P_FREL)), sr);
    // Filter 1: Key-Tracking (100 % = eine Oktave pro Oktave) und Velocity
    fBase = (float)P(P_FREQ) + P(P_KEYT) / 100.f * (note - 60.f) * (127.f / 10.3f / 12.f)
            + (P(P_VELF) - 64) / 64.f * (vel - 0.5f) * 80.f;
    // Filter 2: Base-Width (BASE = Hochpass, Tiefpass = BASE + WDTH Oktaven)
    {
        int bs = P(P_BASE), wd = P(P_WDTH);
        bwOn = bs > 0 || wd < 127;
        bwLo = 20.f * fastExp2(bs * (10.f / 127.f));
        bwHi = bwLo * fastExp2(wd * (10.f / 127.f));
        if (bwHi > 20000.f) bwHi = 20000.f;
    }
    fEnv = (P(P_FENV) - 64) * 2.f;
    fCrvK = fastExp2((P(P_FCRV) - 64) / 32.f);
    aCrvK = fastExp2((P(P_ACRV) - 64) / 32.f);
    reso = clampf(P(P_RESO) / 127.f, 0.f, 1.f);
    ftype = P(P_FTYP) % FT_COUNT;

    // Tonhoehe inkl. Glide
    if ((machine == M_SYNTH || machine == M_WTBL) && P(P_S7) > 0 && n > 0) {
        float gt = 0.001f * fastExp2(P(P_S7) * (11.f / 127.f));
        float co = 1.f - fastExpE(-(float)n / (gt * sr));
        curNote += (note - curNote) * co;
    } else {
        curNote = note;
    }
    float nn = curNote + noteMod;
    float ratio = fastExp2((nn - 60.f) / 12.f);
    switch (machine) {
    case M_KICK:
        pitchHz = 30.f * fastExp2(P(P_S0) * (2.f / 127.f)) * ratio;
        ddco = decayCo(0.03f * fastExp2(P(P_S1) * (6.f / 127.f)), sr);
        pco = decayCo(0.002f * fastExp2(P(P_S3) * (7.f / 127.f)), sr);
        cco = decayCo(0.004f, sr);
        break;
    case M_SNARE:
        pitchHz = 120.f * fastExp2(P(P_S0) * (1.74f / 127.f)) * ratio;
        ddco = decayCo(0.03f * fastExp2(P(P_S1) * (5.f / 127.f)), sr);
        nco = decayCo(0.03f * fastExp2(P(P_S3) * (5.f / 127.f)), sr);
        aux.set(1500.f * fastExp2(P(P_S4) * (3.2f / 127.f)), 0.15f, sr);   // TONE = Tiefpass des Rauschens
        pco = decayCo(0.012f, sr);                                       // kurzer Pitch-Abfall der Fell-Oszillatoren
        break;
    case M_HAT:
        pitchHz = 200.f * fastExp2(P(P_S0) * (1.6f / 127.f)) * ratio;
        ddco = decayCo(0.01f * fastExp2(P(P_S1) * (7.5f / 127.f)), sr);
        aux.set(5000.f * fastExp2(P(P_S2) * (1.3f / 127.f)), 0.35f, sr);   // 808: Bandpass um 7-12 kHz
        break;
    case M_CLAP:
        aux.set(600.f * fastExp2(P(P_S0) * (2.5f / 127.f)) * ratio, 0.55f, sr);
        ddco = decayCo(0.05f * fastExp2(P(P_S1) * (5.f / 127.f)), sr);
        clapInterval = (int)((0.004f + P(P_S2) / 127.f * 0.025f) * sr);
        bco = decayCo(0.012f, sr);
        break;
    case M_FM: {
        // Pitch-Huellkurve (P.ENV +-24 Halbtoene, P.DEC Abklingzeit), blockweise
        float pamt = (P(P_S3) - 64) / 64.f * 24.f;
        if (n > 0) fmPenv *= fastExpE(-6.9f * n / (envTime(P(P_S4)) * sr));
        pitchHz = 440.f * fastExp2((nn - 69.f + pamt * fmPenv) / 12.f);
        fmAlgo = P(P_S0) & 7;
        fmFbAmt = P(P_S1) / 127.f * 0.5f;
        float velS = P(P_S2) / 127.f;
        float vg = 1.f - velS + velS * vel;
        for (int k = 0; k < 4; k++) {
            int b = FM_OPB[k];
            float r = FM_RATIOS[P(b + OP_RAT) % FM_NRAT] + (P(b + OP_FINE) - 64) / 64.f * 0.5f;   // FINE: +-0,5 (unharmonisch)
            if (r < 0.0625f) r = 0.0625f;
            float det = fastExp2((P(b + OP_DETN) - 64) / 64.f * (50.f / 1200.f));                // DETN: +-50 Cent
            fmInc[k] = pitchHz * r * det / sr;
            float lv = P(b + OP_LVL) / 127.f;
            fmMod[k] = lv * 2.2f * vg;     // als Modulator: Index in Perioden
            fmCar[k] = lv;                 // als Traeger: Ausgangspegel
            fmAtk[k] = 1.f / (envTime(P(b + OP_ATK)) * sr);
            fmDco[k] = decayCo(envTime(P(b + OP_DEC)), sr);
            fmSus[k] = P(b + OP_SUS) / 127.f;
            fmRco[k] = decayCo(envTime(P(b + OP_REL)), sr);
        }
        int nc = 0; for (int k = 0; k < 4; k++) if (FM_CARRIERS[fmAlgo] >> k & 1) nc++;
        float norm = 1.f / sqrtf((float)(nc ? nc : 1));
        for (int k = 0; k < 4; k++) fmCar[k] *= norm;
    } break;
    case M_TOM:
        pitchHz = 60.f * fastExp2(P(P_S0) * (2.5f / 127.f)) * ratio;
        ddco = decayCo(0.05f * fastExp2(P(P_S1) * (5.f / 127.f)), sr);
        pco = decayCo(0.06f, sr);
        nco = decayCo(0.02f, sr);
        break;
    case M_RIM:
        pitchHz = 300.f * fastExp2(P(P_S0) * (1.6f / 127.f)) * ratio;
        ddco = decayCo(0.008f * fastExp2(P(P_S1) * (3.f / 127.f)), sr);
        cco = decayCo(0.003f, sr);
        aux.set(800.f * fastExp2(P(P_S2) * (2.5f / 127.f)), 0.6f, sr);
        break;
    case M_SHKR:
        aux.set(2000.f * fastExp2(P(P_S0) * (2.6f / 127.f)) * ratio, 0.35f, sr);
        ddco = decayCo(0.02f * fastExp2(P(P_S1) * (4.5f / 127.f)), sr);
        sAtkInc = 1.f / ((0.001f + P(P_S2) / 127.f * 0.08f) * sr);
        shkGrit = P(P_S3) / 127.f;
        break;
    case M_WTBL:
        pitchHz = 440.f * fastExp2((nn - 69.f) / 12.f);
        wtTab = P(P_S0) % 5;
        wtUser = nullptr;
        if (wtTab == 4) { const Sample* us = sampleGet(P(P_X3)); if (us && us->data && us->len >= 16) wtUser = us; else wtTab = 0; }   // USER: Sample UWAV
        wtPos = P(P_S1) / 127.f;
        wtWarp = (P(P_S2) - 64) / 64.f;
        wtDet = fastExp2(P(P_S3) / 127.f * 0.4f / 12.f);
        wtPenv = (P(P_S6) - 64) / 64.f;
        break;
    case M_CHOP:
        if (smp) {
            float semis = (nn - 60.f) + (P(P_S3) - 24) + (P(P_S4) - 64) / 64.f;
            sinc = smp->rate / sr * fastExp2(semis / 12.f) * (sRev ? -1.f : 1.f);
            float g = P(P_S7) / 64.f; sGain = g * g;
        }
        break;
    case M_GRAN:
        if (smp) {
            grPosF = P(P_S1) / 127.f;
            grSpray = P(P_S2) / 127.f;
            grLen = (0.01f * fastExp2(P(P_S3) * (5.6f / 127.f))) * sr;          // 10 ms .. 0.5 s
            float dHz = 2.f * fastExp2(P(P_S4) * (6.f / 127.f));                 // 2 .. 128 Koerner/s
            grInterval = sr / dHz;
            grPitch = smp->rate / sr * fastExp2(((nn - 60.f) + (P(P_S5) - 24)) / 12.f);
            grPSpr = P(P_S6) / 127.f;
            float g = P(P_S7) / 64.f; sGain = g * g;
        }
        break;
    case M_SAMPLE:
        if (smp) {
            float semis = (nn - 60.f) + (P(P_S1) - 24) + (P(P_S2) - 64) / 64.f;
            sinc = smp->rate / sr * fastExp2(semis / 12.f) * (sRev ? -1.f : 1.f);
            float g = P(P_S6) / 64.f;
            sGain = g * g;
        }
        break;
    default:
        pitchHz = 440.f * fastExp2((nn - 69.f) / 12.f);
        break;
    }

    float vg = P(P_VOL) / 100.f;
    float vol = vg * vg * vel;
    float pan = clampf((P(P_PAN) - 64) / 64.f, -1.f, 1.f);
    float a = (pan + 1.f) * 0.78539816f;
    gainL = vol * cosf(a) * 1.41421356f;
    gainR = vol * sinf(a) * 1.41421356f;
    snd[0] = vol * P(P_DLY) / 127.f;
    snd[1] = vol * P(P_REV) / 127.f;
    snd[2] = vol * P(P_SNDC) / 127.f;
    drive = P(P_DRIV) / 127.f;
    float cr = (float)P(P_CRSH);
    crushQ = cr > 0.5f ? fastExp2(15.f - cr / 127.f * 13.f) : 0.f;
    srrN = 1 + P(P_SRR) / 6;

    // Insert-Effekt
    insType = P(P_ITYP);
    float p1 = P(P_IP1) / 127.f, p2 = P(P_IP2) / 127.f, p3 = P(P_IP3) / 127.f;
    insMix = P(P_IMIX) / 127.f;
    insP2 = p2; insP3 = p3;
    switch (insType) {
    case INS_CHORUS: case INS_FLANGER: case INS_APAN:
        insRate = 0.05f * fastExp2(p1 * 8.f);
        insFb = (p3 - 0.5f) * 1.8f;
        break;
    case INS_PHASER: {
        insRate = 0.05f * fastExp2(p1 * 8.f);
        insPh += insRate * n / sr; if (insPh >= 1.f) insPh -= floorf(insPh);
        insFb = p3 * 0.85f;
        // zwei Allpass-Koeffizienten (L/R) aus dem LFO
        for (int c = 0; c < 2; c++) {
            float lv = sin01(insPh + c * 0.25f);
            float f = 300.f * fastExp2(p2 * 3.5f * (0.5f + 0.5f * lv));
            float t = tanf(3.14159265f * fminf(f, sr * 0.45f) / sr);
            apA[c] = (t - 1.f) / (t + 1.f);
        }
    } break;
    case INS_DRIVE:
        odG = 1.f + p1 * p1 * 40.f;
        odCo = 1.f - fastExpE(-6.2831853f * (400.f * fastExp2(p2 * 5.f)) / sr);
        odLvl = (0.1f + p3 * 0.9f) * 0.4f;
        break;
    case INS_RING:
        insRate = 20.f * fastExp2(p1 * 7.f) * fastExp2((p2 - 0.5f) / 12.f * 2.f);
        break;
    default: break;
    }

    if (gate > 0) {
        gate -= n;
        if (gate <= 0) { gate = 0; if (gated()) noteOff(); }
    }
}

// Filter 1 pro Sample (Typ ftype, Koeffizienten aus render())
inline float Voice::filter1(float x) {
    float lo, bp, hp;
    switch (ftype) {
    case FT_LP24: {
        filt.proc(x, lo, bp, hp);
        float l2, b2, h2; filt2.proc(lo, l2, b2, h2);
        return fastTanh(l2 * 1.3f) * 0.85f;   // Ausgangsstufe mit leichter Saettigung
    }
    case FT_HP24: {
        filt.proc(x, lo, bp, hp);
        float l2, b2, h2; filt2.proc(hp, l2, b2, h2);
        return fastTanh(h2 * 1.2f) * 0.9f;
    }
    case FT_LADR: {   // Zero-Delay-Feedback-Loesung der 4-Pol-Kaskade, Saettigung am Eingang
        float G = ladG, b = 1.f - G;
        float S = G * G * G * b * lad[0] + G * G * b * lad[1] + G * b * lad[2] + b * lad[3];
        float G4 = G * G * G * G;
        float u = (x * (1.f + ladK * 0.5f) - ladK * S) / (1.f + ladK * G4);
        u = fastTanh(u * 1.1f);
        for (int j = 0; j < 4; j++) { float v = (u - lad[j]) * G; float y = v + lad[j]; lad[j] = y + v; u = y; }
        return u * 1.5f;
    }
    case FT_NTCH: filt.proc(x, lo, bp, hp); return lo + hp;
    case FT_PEAK: filt.proc(x, lo, bp, hp); return (lo - hp) * 0.7f;
    case FT_CMBP: case FT_CMBN: {
        float rp = (float)cmbW - cmbD; if (rp < 0.f) rp += 2048.f;
        int i0 = (int)rp; float fr = rp - i0; int i1 = (i0 + 1) & 2047;
        float d = cmb[i0] + (cmb[i1] - cmb[i0]) * fr;
        cmbLp += (d - cmbLp) * 0.7f;               // leichte Daempfung im Rueckkopplungsweg
        float y = x + cmbFb * cmbLp;
        y = fastTanh(y * 0.8f) * 1.25f;
        cmb[cmbW] = y; cmbW = (cmbW + 1) & 2047;
        return y * (1.f - fabsf(cmbFb) * 0.55f);
    }
    case FT_VOWL: {
        float a = 0.f;
        filt.proc(x, lo, bp, hp);  a += bp * fmtG[0];
        filt2.proc(x, lo, bp, hp); a += bp * fmtG[1];
        filt3.proc(x, lo, bp, hp); a += bp * fmtG[2];
        return a * 1.6f;
    }
    default:
        filt.proc(x, lo, bp, hp);
        return ftype == FT_BP ? bp : (ftype == FT_HP12 ? hp : lo);
    }
}

void Voice::render(float* L, float* R, float (*S)[32], int n, float sr) {
    if (!active) return;
    static const float HAT_R[6] = {1.f, 1.4836f, 1.8016f, 2.5478f, 2.6321f, 3.8996f};
    const float inv = 1.f / sr;
    const float dcCo = fastExpE(-1.f / (0.002f * sr));   // Knackschutz klingt in ~2 ms aus
    float fev = fCrvK == 1.f || fenv.v <= 0.f ? fenv.v : fastPow01(fenv.v, fCrvK);
    float units = fBase + fEnv * fev;
    bool bypassF = (ftype == FT_LP12 && units >= 126.f && reso < 0.01f);
    if (!bypassF) {
        float fcHz = 16.f * fastExp2(units * (10.3f / 127.f));
        switch (ftype) {
        case FT_LP24: case FT_HP24:   // zwei SVF-Stufen = 24 dB/Okt. (Resonanz in der zweiten)
            filt.set(fcHz, 0.3f, sr); filt2.set(fcHz, reso * 0.98f, sr); break;
        case FT_LADR: {               // Transistor-Kaskade: 4 Tiefpass-Stufen mit Rueckkopplung
            float g = tanf(3.14159265f * clampf(fcHz, 10.f, sr * 0.45f) / sr);
            ladG = g / (1.f + g); ladK = reso * 4.15f;
        } break;
        case FT_CMBP: case FT_CMBN: { // Kammfilter: Verzoegerung = 1 / FREQ
            float d = sr / clampf(fcHz, 25.f, 12000.f);
            cmbD = clampf(d, 2.f, 2040.f);
            cmbFb = (ftype == FT_CMBP ? 1.f : -1.f) * (0.2f + reso * 0.77f);
        } break;
        case FT_VOWL: {               // FREQ wandert durch A-E-I-O-U, RESO = Schaerfe der Formanten
            float pos = clampf(units / 127.f, 0.f, 1.f) * 4.f;
            int i0 = (int)pos; if (i0 > 3) i0 = 3;
            float fr = pos - i0;
            float q = 0.55f + reso * 0.42f;
            SVF* fs[3] = {&filt, &filt2, &filt3};
            for (int j = 0; j < 3; j++) fs[j]->set(VOWEL_F[i0][j] + (VOWEL_F[i0 + 1][j] - VOWEL_F[i0][j]) * fr, q, sr);
        } break;
        default: filt.set(fcHz, reso * 0.97f, sr); break;
        }
    }
    // analoge Drift: langsamer Zufallsweg je Oszillator (wenige Cent)
    if (machine == M_SYNTH) {
        drift1 += rng.next() * 0.35f - drift1 * 0.02f;
        drift2 += rng.next() * 0.35f - drift2 * 0.02f;
    }
    if (bwOn) { bwHp.set(bwLo, 0.2f, sr); bwLp.set(bwHi, 0.2f, sr); }

    // maschinenspezifische Konstanten
    float sweep = P(P_S2) / 127.f * 6.f, click = P(P_S4) / 127.f, body = P(P_S5) / 127.f;
    float snap = P(P_S2) / 127.f, metl = P(P_S3) / 127.f;
    float pw = 0.5f + (P(P_S6) - 64) / 140.f;
    float detune = fastExp2((P(P_S2) - 64) / 64.f * 0.5f / 12.f + (drift2 - drift1) * 0.25f / 1200.f);
    float mix = P(P_S3) / 127.f, sub = P(P_S4) / 127.f, noise = P(P_S5) / 127.f;
    float fmNoise = P(P_S7) / 127.f;
    int w1 = P(P_S0) & 3, w2 = P(P_S1) & 3;
    float dt = pitchHz * inv * (machine == M_SYNTH ? fastExp2(drift1 * 0.25f / 1200.f) : 1.f);
    // zweite Klangseite: Oktave und Pegel je Oszillator, Sub-Oktave, Sync, Kreuz- und Ringmodulation
    float o1m = 1.f, o2m = 1.f, lv1 = 1.f, lv2 = 1.f, subDiv = 0.5f, xmod = 0.f, ring = 0.f; bool sync = false;
    if (machine == M_SYNTH) {
        o1m = fastExp2((float)(P(P_X0) - 3)); o2m = fastExp2((float)(P(P_X1) - 3));
        lv1 = P(P_X2) / 127.f; lv2 = P(P_X3) / 127.f; subDiv = P(P_X4) ? 0.25f : 0.5f;
        sync = P(P_X5) != 0; xmod = P(P_X6) / 127.f; ring = P(P_X7) / 127.f;
    } else if (machine == M_WTBL) {
        o1m = fastExp2((float)(P(P_X0) - 3)); lv1 = P(P_X1) / 127.f; subDiv = P(P_X2) ? 0.25f : 0.5f;
    }
    float bmax = 0.f;

    for (int i = 0; i < n; i++) {
        float s = 0.f;
        switch (machine) {
        case M_KICK: {
            // 808/909: exponentieller Pitch-Abfall (SWEP bis 3 Oktaven), Sinus, Anschlag aus Puls + Rauschen
            float se = sweep * 0.5f * penv;   // Pitch-Sweep nur solange er hoerbar ist
            float f = se > 1e-4f ? pitchHz * fastExp2(se) : pitchHz;
            penv *= pco;
            ph1 += f * inv; if (ph1 >= 1.f) ph1 -= 1.f;
            s = sin01(ph1) * denv;
            s = fastTanh(s * (1.15f + body * 5.f)) * (body > 0 ? 1.f : 1.05f);   // etwas Waerme, BODY = Drive (909)
            if (cenv > 1e-5f) {   // Klick klingt nach wenigen ms aus
                float clk = (ph1 < 0.5f ? 1.f : -1.f) * 0.5f + rng.next() * 0.5f;
                s += clk * cenv * click * 0.8f;
            }
            denv *= ddco; cenv *= cco;
        } break;
        case M_SNARE: {
            // 909: zwei Fell-Oszillatoren (~180/330 Hz) mit kurzem Pitch-Abfall, Rauschen tief- und hochpassgefiltert
            float pd = 1.f + 0.6f * penv;
            penv *= pco;
            ph1 += dt * pd; if (ph1 >= 1.f) ph1 -= 1.f;
            ph2 += dt * 1.83f * pd; if (ph2 >= 1.f) ph2 -= 1.f;
            float tri1 = 1.f - 4.f * fabsf(ph1 - 0.5f);
            float tone = (0.65f * sin01(ph1) + 0.25f * tri1 + 0.5f * sin01(ph2)) * 0.75f * denv;
            float lo, bp, hp;
            aux.proc(rng.next(), lo, bp, hp);
            nzHp += (lo - nzHp) * 0.09f;          // Hochpass ~650 Hz
            float nz = (lo - nzHp) * 1.6f * nenv * snap;
            s = fastTanh((tone * (1.f - snap * 0.4f) + nz) * 1.2f);
            denv *= ddco; nenv *= nco;
        } break;
        case M_HAT: {
            float sum = 0;
            for (int k = 0; k < 6; k++) {
                hph[k] += dt * HAT_R[k]; if (hph[k] >= 1.f) hph[k] -= 1.f;
                sum += hph[k] < 0.5f ? 1.f : -1.f;
            }
            float src = sum * (1.f / 6.f) * metl + rng.next() * (1.f - metl) * 0.7f;
            float lo, bp, hp;
            aux.proc(src, lo, bp, hp);
            hatHp += (bp - hatHp) * 0.45f;        // zusaetzlicher Hochpass ~5 kHz: luftiger, metallischer
            s = (bp - hatHp) * denv * 3.6f;
            denv *= ddco;
        } break;
        case M_CLAP: {
            if (!clapTail) {
                if (clapTimer <= 0) {
                    if (clapLeft > 0) { burst = 1.f; clapLeft--; clapTimer = clapInterval; }
                    else { clapTail = true; denv = 0.7f; }
                }
                clapTimer--;
            }
            float env = burst > denv ? burst : denv;
            burst = burst > 0.f ? burst - 1.f / (clapInterval > 1 ? clapInterval : 1) : 0.f;   // saegezahnfoermige Einzelschlaege (808)
            if (burst < 0.f) burst = 0.f;
            if (clapTail) denv *= ddco;
            float lo, bp, hp;
            aux.proc(rng.next(), lo, bp, hp);
            s = bp * env * 1.9f;
        } break;
        case M_SYNTH: {
            float dt1 = dt * o1m, dt2 = dt * detune * o2m;
            float o2 = osc(w2, ph2, dt2, pw);
            float p1 = ph1;
            if (xmod > 0.f) { p1 += o2 * xmod * 0.5f; p1 -= (float)(int)p1; if (p1 < 0.f) p1 += 1.f; }   // OSC2 moduliert OSC1 (FM)
            float o1 = osc(w1, p1, dt1, pw);
            ph2 += dt2; if (ph2 >= 1.f) ph2 -= 1.f;
            ph1 += dt1;
            if (ph1 >= 1.f) { ph1 -= 1.f; if (sync) ph2 = ph1 * (dt2 / dt1); }   // Hard-Sync: OSC2 startet mit OSC1 neu
            phs += dt1 * subDiv; if (phs >= 1.f) phs -= 1.f;
            s = o1 * (1.f - mix) * lv1 + o2 * mix * lv2;
            if (ring > 0.f) s += o1 * o2 * ring * 0.8f;
            s += sub * (phs < 0.5f ? 0.8f : -0.8f);
            s += noise * rng.next();
            s = fastTanh(s * 0.9f) * 0.95f;      // warme Saettigung vor dem Filter, lauter als vorher
        } break;
        case M_FM: {
            const uint8_t* mods = FM_MODS[fmAlgo];
            uint8_t car = FM_CARRIERS[fmAlgo];
            float o[4], out = 0.f;
            for (int k = 3; k >= 0; k--) {   // OP4 zuerst: Modulatoren haben immer hoehere Nummern
                float m = 0.f;
                for (int j = k + 1; j < 4; j++) if (mods[k] >> j & 1) m += o[j] * fmMod[j] * fmEnv[j];
                if (k == 3) m += fmFbPrev * fmFbAmt;
                fmPh[k] += fmInc[k]; if (fmPh[k] >= 1.f) fmPh[k] -= 1.f;
                o[k] = sin01(fmPh[k] + m);
                if (k == 3) fmFbPrev = o[3];
                if (car >> k & 1) out += o[k] * fmCar[k] * fmEnv[k];
                float e = fmEnv[k];   // Operator-Huellkurve: Attack linear, Decay/Release exponentiell
                if (fmSt[k] == 1) { e += fmAtk[k]; if (e >= 1.f) { e = 1.f; fmSt[k] = 2; } }
                else if (fmSt[k] == 2) e = fmSus[k] + (e - fmSus[k]) * fmDco[k];
                else e *= fmRco[k];
                fmEnv[k] = e;
            }
            s = out * 0.7f + fmNoise * rng.next() * 0.3f;
        } break;
        case M_TOM: {
            float f = pitchHz * (1.f + 0.8f * sweep * 0.25f * penv);
            penv *= pco;
            ph1 += f * inv; if (ph1 >= 1.f) ph1 -= 1.f;
            s = sin01(ph1) * denv;
            s += rng.next() * nenv * mix * 0.5f;              // mix = P_S3 (NOIS), sub = P_S4 (BODY)
            if (sub > 0.f) s = fastTanh(s * (1.f + sub * 4.f));
            denv *= ddco; nenv *= nco;
        } break;
        case M_RIM: {
            ph1 += dt; if (ph1 >= 1.f) ph1 -= 1.f;
            ph2 += dt * 3.66f; if (ph2 >= 1.f) ph2 -= 1.f;   // 808: 455 / 1667 Hz
            float src = (ph1 < 0.5f ? 1.f : -1.f) + 0.7f * (ph2 < 0.5f ? 1.f : -1.f);
            float lo, bp, hp;
            aux.proc(src, lo, bp, hp);
            s = bp * denv * 1.3f + rng.next() * cenv * mix;   // mix = P_S3 (CLCK)
            denv *= ddco; cenv *= cco;
        } break;
        case M_SHKR: {
            if (shkStage == 1) { shkLvl += sAtkInc; if (shkLvl >= 1.f) { shkLvl = 1.f; shkStage = 2; } }
            else { shkLvl *= ddco; denv = shkLvl; }
            if (shkGrit > 0 && rng.uni() < 0.002f + shkGrit * 0.02f) shkGain = 0.25f + rng.uni() * 0.75f * (1.f - shkGrit * 0.5f) + shkGrit * 0.3f;
            float lo, bp, hp;
            aux.proc(rng.next(), lo, bp, hp);
            s = (hp * 0.6f + bp) * shkLvl * (shkGrit > 0 ? shkGain : 1.f) * 6.f;
        } break;
        case M_WTBL: {
            float pos = clampf(wtPos + wtPenv * fenv.v, 0.f, 1.f);
            float p1 = ph1, p2 = ph2;
            if (wtWarp > 0.01f) { p1 = fastPow01(p1, 1.f + wtWarp * 3.f); p2 = fastPow01(p2, 1.f + wtWarp * 3.f); }
            else if (wtWarp < -0.01f) { p1 = 1.f - fastPow01(1.f - p1, 1.f - wtWarp * 3.f); p2 = 1.f - fastPow01(1.f - p2, 1.f - wtWarp * 3.f); }
            s = wtUser ? wtReadUser(wtUser, pos, p1) : wtRead(wtTab, pos, p1);
            if (wtDet > 1.0001f) s = (s + (wtUser ? wtReadUser(wtUser, pos, p2) : wtRead(wtTab, pos, p2))) * 0.6f;
            s *= lv1;
            float dtw = dt * o1m;
            ph1 += dtw; if (ph1 >= 1.f) ph1 -= 1.f;
            ph2 += dtw * wtDet; if (ph2 >= 1.f) ph2 -= 1.f;
            phs += dtw * subDiv; if (phs >= 1.f) phs -= 1.f;
            s += sub * 0.6f * sin01(phs);
            s += noise * 0.4f * rng.next();
            s *= 0.55f;
        } break;
        case M_CHOP: {
            if (sDone || !smp || !smp->data || smp->len < 2) break;   // Pool-Platz kann waehrend des Spielens geleert werden
            float rem = (float)(sRev ? spos - sEnd : sEnd - spos);
            float fade = rem < 64.f ? rem / 64.f : 1.f;           // kein Knacken am Scheibenende
            s = smpRead(smp, spos) * sGain * (fade < 0 ? 0 : fade);
            spos += sinc;
            if (sRev ? spos <= sEnd : spos >= sEnd) sDone = true;
        } break;
        case M_GRAN: {
            if (!smp || !smp->data || smp->len < 4) break;
            grTimer -= 1.f;
            if (grTimer <= 0.f) {   // neues Korn
                grTimer += grInterval * (0.75f + 0.5f * rng.uni());
                for (auto& g : gr) if (!g.on) {
                    float pos = grPosF + grSpray * rng.next() * 0.5f;
                    pos -= floorf(pos);
                    g.on = true; g.age = 0; g.len = grLen;
                    g.pos = pos * (smp->len - 2);
                    g.inc = grPitch * exp2f(grPSpr * rng.next() * 12.f / 12.f);
                    break;
                }
            }
            float acc = 0; int na = 0;
            for (auto& g : gr) {
                if (!g.on) continue;
                int ip = (int)g.pos;
                if (ip < 0 || ip >= smp->len - 1) { g.on = false; continue; }
                float v2 = smpRead(smp, g.pos);
                acc += v2 * sin01(g.age / g.len * 0.5f);   // Halbsinus-Fenster
                g.pos += g.inc; g.age += 1.f; na++;
                if (g.age >= g.len) g.on = false;
            }
            s = acc * sGain / sqrtf((float)(na > 1 ? na : 1)) * 1.2f;
        } break;
        case M_SAMPLE: {
            if (sDone || !smp || !smp->data || smp->len < 2) break;
            float rem = (float)(sRev ? spos - sEnd : sEnd - spos);
            float fade = sLoop || rem >= 64.f ? 1.f : (rem > 0.f ? rem / 64.f : 0.f);   // Ende (z. B. END < Dateiende) ohne Knacken
            s = smpRead(smp, spos) * sGain * fade;
            spos += sinc;
            bool past = sRev ? spos <= sEnd : spos >= sEnd;
            if (past) {
                if (sLoop) spos = sStart + (spos - sEnd);
                else sDone = true;
                if (sRev ? spos < sEnd : spos > sEnd) spos = sStart;
            }
        } break;
        }

        fenv.tick();
        if (!bypassF) s = filter1(s);
        if (bwOn) {   // Base-Width: Hochpass, dann Tiefpass
            float lo, bp, hp;
            bwHp.proc(s, lo, bp, hp); s = hp;
            bwLp.proc(s, lo, bp, hp); s = lo;
        }
        { float av = aenv.tick(); s *= aCrvK == 1.f ? av : fastPow01(av, aCrvK); }
        if (drive > 0.f) s = fastTanh(s * (1.f + drive * 6.f)) * (1.f - drive * 0.3f);
        if (srrN > 1) { if (holdC <= 0) { hold = s; holdC = srrN; } holdC--; s = hold; }
        if (crushQ > 0.f) s = floorf(s * crushQ + 0.5f) / crushQ;
        // Neuanschlag, waehrend die Stimme noch klingt (Kick in den Kick-Ausklang, Sample in Sample): ohne das
        // springt die Wellenform -> Knacken auf dem Beat. Sprung als schnell abklingenden Versatz ausgleichen.
        if (dcArm) { dcOff = lastS - s; dcArm = false; }
        if (dcOff != 0.f) { s += dcOff; dcOff *= dcCo; if (dcOff > -1e-5f && dcOff < 1e-5f) dcOff = 0.f; }
        lastS = s;

        // Insert-Effekt (mono rein, stereo raus)
        float sl = s, sr2 = s;
        if (insType != INS_OFF) {
            float wl = s, wr = s;
            switch (insType) {
            case INS_CHORUS: {
                insPh += insRate * inv; if (insPh >= 1.f) insPh -= 1.f;
                ib[iw] = s;
                float dep = insP2 * 0.008f * sr, base = 0.012f * sr;
                float dl = base + dep * (0.5f + 0.5f * sin01(insPh));
                float dr = base + dep * (0.5f + 0.5f * sin01(insPh + 0.25f));
                float a = iw - dl; if (a < 0) a += 2048.f;
                float b = iw - dr; if (b < 0) b += 2048.f;
                int ia = (int)a, ib2 = (int)b;
                wl = ib[ia & 2047] + (ib[(ia + 1) & 2047] - ib[ia & 2047]) * (a - ia);
                wr = ib[ib2 & 2047] + (ib[(ib2 + 1) & 2047] - ib[ib2 & 2047]) * (b - ib2);
                float m = (wl + wr) * 0.5f;      // WIDTH: 0 = mono, 1 = volle Breite
                wl = m + (wl - m) * insP3; wr = m + (wr - m) * insP3;
                iw = (iw + 1) & 2047;
            } break;
            case INS_FLANGER: {
                insPh += insRate * inv; if (insPh >= 1.f) insPh -= 1.f;
                float d = 0.0005f * sr + insP2 * 0.004f * sr * (0.5f + 0.5f * sin01(insPh));
                float a = iw - d; if (a < 0) a += 2048.f;
                int ia = (int)a;
                float y = ib[ia & 2047] + (ib[(ia + 1) & 2047] - ib[ia & 2047]) * (a - ia);
                ib[iw] = s + y * insFb;
                iw = (iw + 1) & 2047;
                wl = wr = y;
            } break;
            case INS_PHASER: {
                float xl = s + phFbL * insFb, xr = s + phFbR * insFb;
                for (int k = 0; k < 6; k++) {
                    float yl = -apA[0] * xl + apZL[k]; apZL[k] = xl + apA[0] * yl; xl = yl;
                    float yr = -apA[1] * xr + apZR[k]; apZR[k] = xr + apA[1] * yr; xr = yr;
                }
                phFbL = xl; phFbR = xr;
                wl = (s + xl) * 0.5f; wr = (s + xr) * 0.5f;
            } break;
            case INS_DRIVE: {
                float x = s * odG;
                x = x > 0 ? fastTanh(x) : fastTanh(x * 0.8f) * 1.1f;   // leicht asymmetrisch
                odLp += (x - odLp) * odCo;
                wl = wr = odLp * odLvl;
            } break;
            case INS_RING: {
                insPh += insRate * inv; if (insPh >= 1.f) insPh -= 1.f;
                wl = wr = s * sin01(insPh);
            } break;
            case INS_APAN: {
                insPh += insRate * inv; if (insPh >= 1.f) insPh -= 1.f;
                float lv = sin01(insPh) * insP2;
                wl = s * (1.f - lv); wr = s * (1.f + lv);
            } break;
            }
            sl = s + (wl - s) * insMix;
            sr2 = s + (wr - s) * insMix;
        }
        float as = fabsf(sl) > fabsf(sr2) ? fabsf(sl) : fabsf(sr2);
        if (as > bmax) bmax = as;
        L[i] += sl * gainL;
        R[i] += sr2 * gainR;
        float sm = (sl + sr2) * 0.5f;
        S[0][i] += sm * snd[0];
        S[1][i] += sm * snd[1];
        S[2][i] += sm * snd[2];
    }

    float pk = bmax * (gainL > gainR ? gainL : gainR);
    if (pk > peakHold) peakHold = pk;
    // Stimme beenden, wenn verklungen
    switch (machine) {
    case M_KICK: case M_HAT: if (denv < 1e-4f) active = false; break;
    case M_SNARE: if (denv < 1e-4f && nenv < 1e-4f) active = false; break;
    case M_CLAP: if (clapTail && denv < 1e-4f && burst < 1e-4f) active = false; break;
    case M_SAMPLE: case M_CHOP: if (sDone || aenv.st == 0) active = false; break;
    case M_TOM: case M_RIM: if (denv < 1e-4f) active = false; break;
    case M_SHKR: if (shkStage == 2 && shkLvl < 1e-4f) active = false; break;
    default: if (aenv.st == 0) active = false; break;
    }
    if (aenv.st == 3 && aenv.s <= 0.f) active = false;
}

// ---------------------------------------------------------------- Reverb

static const int COMB_T[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
static const int AP_T[4] = {556, 441, 341, 225};

void Reverb::init(int sr) {
    float sc = sr / 44100.f;
    for (int k = 0; k < 8; k++) {
        cl[k].n = (int)(COMB_T[k] * sc); cl[k].b = (float*)calloc(cl[k].n, sizeof(float));
        cr[k].n = (int)((COMB_T[k] + 23) * sc); cr[k].b = (float*)calloc(cr[k].n, sizeof(float));
    }
    for (int k = 0; k < 4; k++) {
        al[k].n = (int)(AP_T[k] * sc); al[k].b = (float*)calloc(al[k].n, sizeof(float));
        ar[k].n = (int)((AP_T[k] + 23) * sc); ar[k].b = (float*)calloc(ar[k].n, sizeof(float));
    }
}

static inline float combProc(Comb& c, float x, float fb, float damp) {
    float o = c.b[c.i];
    c.store = o * (1.f - damp) + c.store * damp;
    c.b[c.i] = x + c.store * fb;
    if (++c.i >= c.n) c.i = 0;
    return o;
}
static inline float apProc(Allpass& a, float x) {
    float bo = a.b[a.i];
    a.b[a.i] = x + bo * 0.5f;
    if (++a.i >= a.n) a.i = 0;
    return bo - x;
}

void Reverb::process(float in, float size, float damp, float& oL, float& oR) {
    float x = in * 0.015f;
    float fb = 0.7f + size * 0.28f, dp = damp * 0.4f;
    float l = 0, r = 0;
    for (int k = 0; k < 8; k++) { l += combProc(cl[k], x, fb, dp); r += combProc(cr[k], x, fb, dp); }
    for (int k = 0; k < 4; k++) { l = apProc(al[k], l); r = apProc(ar[k], r); }
    oL = l * 3.f; oR = r * 3.f;
}

// ---------------------------------------------------------------- Engine

static inline void flushDenormals();
// naechste Stimme des Blocks 'tag' abholen (beide Threads); false = Block vorbei oder alles vergeben
bool Engine::wkClaimOne(uint32_t tag, int& idx) {
    uint32_t c = __atomic_load_n(&wkClaim, __ATOMIC_ACQUIRE);
    for (;;) {
        if ((c >> 16) != tag || (c & 0xFFFF) == 0xFFFF || (int)(c & 0xFFFF) >= __atomic_load_n(&wkN, __ATOMIC_RELAXED)) return false;
        if (__atomic_compare_exchange_n(&wkClaim, &c, c + 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) { idx = (int)(c & 0xFFFF); return true; }
    }
}

void* Engine::wkMain(void* arg) {
    Engine* E = (Engine*)arg;
    int seen = 0;
    for (;;) {
        // naechsten Block abwarten: kurz aktiv (Bloecke eines Puffers folgen dicht), sonst schlafen
        int spins = 0;
        while (__atomic_load_n(&E->wkGoSeq, __ATOMIC_ACQUIRE) == seen) {
            if (++spins < 20000) { __asm__ __volatile__("" ::: "memory"); continue; }
            __atomic_store_n(&E->wkSleeping, 1, __ATOMIC_SEQ_CST);
            if (__atomic_load_n(&E->wkGoSeq, __ATOMIC_SEQ_CST) == seen) while (sem_wait(&E->wkGo) != 0) {}
            __atomic_store_n(&E->wkSleeping, 0, __ATOMIC_SEQ_CST);
            spins = 0;
        }
        seen = __atomic_load_n(&E->wkGoSeq, __ATOMIC_ACQUIRE);
        flushDenormals();
        uint32_t tag = (uint32_t)seen & 0xFFFF;
        int idx;
        while (E->wkClaimOne(tag, idx)) {   // so viele Stimmen wie moeglich mitrechnen (Puffer vom Audio-Thread geleert)
            int n = E->wkNs;
            Voice& v = *E->wkList[idx].v;
            v.update(n, (float)E->sr, E->proj.bpm);
            if (E->wkList[idx].key) v.render(E->wKL, E->wKR, E->wS, n, (float)E->sr);
            else v.render(E->wL, E->wR, E->wS, n, (float)E->sr);
            __atomic_add_fetch(&E->wkDone, 1, __ATOMIC_RELEASE);
        }
    }
    return nullptr;
}

void Engine::init(int sampleRate) {
    if (!wkOn) {   // zweiter Audio-Kern (einmalig)
        long cpus = sysconf(_SC_NPROCESSORS_ONLN);
        const char* env = getenv("GB_AUDIO_THREADS");
        if (cpus >= 2 && !(env && atoi(env) == 1) && sem_init(&wkGo, 0, 0) == 0
            && pthread_create(&wkThread, nullptr, wkMain, this) == 0) { wkOn = true; audioThreads = 2; }
    }
    static bool tablesBuilt = false;   // globale Tabellen nur einmal (eine zweite Engine, z. B. Song-Export, darf sie nicht neu schreiben)
    if (!tablesBuilt) {
        buildWavetables();
        for (int i = 0; i <= 2048; i++) SINT[i] = sinf(i / 2048.f * 6.2831853f);
        tablesBuilt = true;
    }
    initFx(sampleRate);
    for (int t = 0; t < NTRACKS; t++) {
        v[t].rng.s = 0x1234567u * (t + 1);
        v[t].lastS = v[t].dcOff = 0.f; v[t].dcArm = false;
        for (int k = 0; k < NCHORDV; k++) { cv[t][k].lastS = cv[t][k].dcOff = 0.f; cv[t][k].dcArm = false; }
        for (int k = 0; k < NCHORDV; k++) cv[t][k].rng.s = 0x7654321u * (t + 1) + k * 977u;
        midiNote[t] = liveNote[t] = skipStep[t] = -1;
        midiGate[t] = 0;
    }
    defaultProject();
}

void Engine::initFx(int sampleRate) {
    sr = sampleRate;
    for (int b = 0; b < NBUS; b++) bus[b].init(sr);   // Effekt-Busse A/B/C
    memset(limXL, 0, sizeof limXL); memset(limXR, 0, sizeof limXR);   // Limiter: leerer Vorausschau-Puffer
    dcXL = dcXR = dcYL = dcYR = 0.f;
    limReady = false; limPos = 0; limRel = 1.f;
}

void Engine::setRate(int sampleRate) {
    if (sampleRate == sr) return;
    double ratio = (double)sampleRate / sr;
    clock *= ratio; nextTickAt *= ratio;
    for (int t = 0; t < NTRACKS; t++) { stepStartT[t] *= ratio; stepLenT[t] *= ratio; }
    initFx(sampleRate);
}

void Engine::allNotesOff() {
    for (int t = 0; t < NTRACKS; t++) {
        if (midiNote[t] >= 0) emit(0x80 | t, (uint8_t)midiNote[t], 0, curOff);
        midiNote[t] = -1;
        midiGate[t] = 0;
    }
}

void Engine::liveNoteOn(int t, int note, int vel) {
    Pattern& P = TP(t);
    note = quantizeNote(note, proj.mst[MS_SCALE]);
    v[t].scaleVal = proj.mst[MS_SCALE];
    memset(liveLk[t], NO_LOCK, NPARAMS);
    liveLk[t][P_NOTE] = (uint8_t)note;
    liveLk[t][P_VEL] = (uint8_t)vel;
    v[t].noteOn(P.machine[t], P.p[t], liveLk[t], 1.f, -1, (float)sr);
    liveNote[t] = note;
    if (!recording) return;
    int len = P.len[t] < 1 ? 1 : P.len[t];
    int s;
    if (playing) {
        s = playedStep[t] % len;
        if (clock - stepStartT[t] > stepLenT[t] * 0.5) { s = (s + 1) % len; skipStep[t] = s; }
    } else {
        s = uiCursor % len;
        stepRecCount++;
    }
    Step& st = P.s[t][s];
    if (st.on != 1) { if (st.on == 0) memset(st.lock, NO_LOCK, NPARAMS); st.on = 1; }
    st.lock[P_NOTE] = (uint8_t)note;
    st.lock[P_VEL] = (uint8_t)vel;
}

void Engine::clockTick() {
    if (tickHistN < 25) tickHist[tickHistN++] = clock;
    else { memmove(tickHist, tickHist + 1, sizeof(double) * 24); tickHist[24] = clock; }
    if (tickHistN >= 13) {
        double per = (tickHist[tickHistN - 1] - tickHist[0]) / (tickHistN - 1);
        if (per > 0) {
            double bpm = sr * 60.0 / (24.0 * per);
            if (bpm >= 30 && bpm <= 300) proj.bpm = (uint16_t)(bpm + 0.5);
        }
    }
    if (!playing) return;
    doTick(); doTick();   // 24 ppqn = 6 pro 16tel, Raster = 12 Ticks pro 16tel
    tickCount++;
}

void Engine::midiIn(uint8_t st, uint8_t d1, uint8_t d2) {
    if (st == 0xF8) { if (clockMode == 2) clockTick(); return; }
    if (st == 0xFA || st == 0xFB) {
        if (clockMode != 2) return;
        if (st == 0xFA) {
            for (int t = 0; t < NTRACKS; t++) resetSeq(t);
            patTick = 0; globalTick = 0; tickCount = 0;
        }
        playing = true;
        return;
    }
    if (st == 0xFC) { if (clockMode == 2) stop(); return; }
    if (!midiInOn) return;
    int type = st & 0xF0, ch = st & 0x0F;
    int t = chMode == 0 ? uiTrack : (ch < NTRACKS ? ch : -1);
    if (t < 0 || t >= NTRACKS) return;
    if (type == 0x90 && d2 > 0) liveNoteOn(t, d1, d2);
    else if (type == 0x80 || type == 0x90) {
        if (liveNote[t] == d1) { if (v[t].gated()) v[t].noteOff(); liveNote[t] = -1; }
    } else if (type == 0xB0) {
        if (d1 == 1) { dj = (d2 - 64) / 64.f; return; }        // Modulationsrad = DJ-Filter
        if (d1 < 70 || d1 > 77 || uiPage < 0 || uiPage >= NPAGES) return;   // CC 70-77 = Regler 1-8 (uiPage = Speicherseite)
        Pattern& P = TP(t);
        int idx = uiPage * 8 + (d1 - 70);
        const ParamDef& d = paramDef(P.machine[t], idx);
        if (d.kind == K_NONE) return;
        uint8_t val = (uint8_t)((d2 * d.max + 63) / 127);
        if (recording && playing) {
            Step& s = P.s[t][playedStep[t]];
            if (s.on) { s.lock[idx] = val; return; }
        }
        P.p[t][idx] = val;
    }
}

static void resetTrack(Pattern& p, int t, int m) {
    p.machine[t] = (uint8_t)m;
    for (int i = 0; i < NPARAMS; i++) p.p[t][i] = paramDef(m, i).def;
    if (m == M_SYNTH) {
        p.p[t][P_FREQ] = 70; p.p[t][P_RESO] = 40; p.p[t][P_FENV] = 96; p.p[t][P_FDEC] = 50;
        p.p[t][P_ADEC] = 60; p.p[t][P_ASUS] = 90; p.p[t][P_AREL] = 40;
        p.p[t][P_FTYP] = FT_LP24;
    } else if (m == M_WTBL) {
        p.p[t][P_FTYP] = FT_LP24;
    } else if (m == M_FM) {
        p.p[t][P_ADEC] = 70; p.p[t][P_ASUS] = 40; p.p[t][P_AREL] = 60;
    }
}

void Engine::initTrack(Pattern& p, int t, int m) { resetTrack(p, t, m); }

void Engine::setMachine(Pattern& p, int t, int m) {
    p.machine[t] = (uint8_t)m;
    for (int i = P_S0; i <= P_S7; i++) {
        p.p[t][i] = paramDef(m, i).def;
        for (int s = 0; s < NSTEPS; s++) p.s[t][s].lock[i] = NO_LOCK;
    }
}

void Engine::initPattern(Pattern& p) {
    static const int defM[NTRACKS] = {M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_SYNTH, M_FM, M_SAMPLE};
    memset(&p, 0, sizeof(p));
    for (int t = 0; t < NTRACKS; t++) {
        resetTrack(p, t, defM[t]);
        p.len[t] = 16;
        p.speed[t] = SPEED_1X;
        for (int s = 0; s < NSTEPS; s++) memset(p.s[t][s].lock, NO_LOCK, NPARAMS);
    }
    // sinnvolle Startklaenge
    p.p[4][P_NOTE] = 36;                                          // Bass
    p.p[5][P_S0] = 1; p.p[5][P_DLY] = 50; p.p[5][P_FREQ] = 85;    // Lead
    p.p[6][P_NOTE] = 72; p.p[6][P_DLY] = 40; p.p[6][P_REV] = 50;  // FM-Glocke
    p.p[7][P_S0] = 11; p.p[7][P_DLY] = 35; p.p[7][P_REV] = 45;   // Sample (Start-Paket: keys/stab-minor)
}

void Engine::defaultProject(bool demo) {
    memset(&proj, 0, sizeof(proj));
    memcpy(proj.magic, "GBX1", 4);
    proj.version = PROJECT_VERSION;
    proj.bpm = 120;
    for (int i = 0; i < 8; i++) proj.mst[i] = MST_DEFS[i].def;
    for (int i = 0; i < 8; i++) proj.cmp[i] = CMP_DEFS[i].def;
    for (int b = 0; b < NBUS; b++) setBusType(proj, b, b == 0 ? BF_DELAY : (b == 1 ? BF_REVERB : BF_CHORUS));
    for (int i = 0; i < NPATTERNS; i++) initPattern(proj.pat[i]);
    if (!demo) return;   // neues Projekt: leere Init-Patterns
    // Demo-Beat in Pattern 1
    Pattern& p = proj.pat[0];
    for (int s = 0; s < 16; s += 4) p.s[0][s].on = 1;
    p.s[1][4].on = p.s[1][12].on = 1;
    for (int s = 2; s < 16; s += 4) p.s[2][s].on = 1;
    p.s[2][15].on = 1; p.s[2][15].lock[P_VEL] = 60;
    p.s[3][12].on = 1; p.s[3][12].lock[P_VEL] = 70;
    const int bs[] = {0, 3, 6, 8, 11, 14}; const int bn[] = {36, 36, 48, 36, 39, 43};
    for (int k = 0; k < 6; k++) { p.s[4][bs[k]].on = 1; p.s[4][bs[k]].lock[P_NOTE] = (uint8_t)bn[k]; }
    p.s[6][0].on = 1; p.s[6][10].on = 1; p.s[6][10].lock[P_NOTE] = 79;
    p.s[7][6].on = 1; p.s[7][14].on = 1; p.s[7][14].lock[P_NOTE] = 63;
}

void Engine::start() {
    if (songActive()) { applyRow(songPos); songRep = 0; nextPat = -1; rowPending = false; }
    if (nextPat >= 0) { curPat = nextPat; nextPat = -1; }
    for (int t = 0; t < NTRACKS; t++) { resetSeq(t); skipStep[t] = -1; }
    patTick = 0; globalTick = 0;
    tickCount = 0;
    nextTickAt = clock;
    if (clockMode == 1) sendStart = true;
    if (clockMode == 2) return;   // externer Takt startet per MIDI-Start
    playing = true;
}

void Engine::stop() {
    playing = false;
    for (int t = 0; t < NTRACKS; t++) { if (v[t].gated()) v[t].noteOff(); for (int k = 0; k < NCHORDV; k++) if (cv[t][k].active) cv[t][k].noteOff(); rt[t].left = 0; ar[t].left = 0; pend[t][0].on = pend[t][1].on = false; preDone[t] = -1; }
    sendStop = true;
}

void Engine::preview(int t, const Step* s) {
    v[t].chopStep = uiCursor;
    trigger(t, (s && s->on) ? s->lock : nullptr, 1.f, false, false);
}

void Engine::trigger(int t, const uint8_t* lk, float velScale, bool fromSeq, bool toMidi) {
    Pattern& P = TP(t);
    const uint8_t* b = P.p[t];
    auto val = [&](int i) { return (lk && lk[i] != NO_LOCK) ? lk[i] : b[i]; };
    float ls = lenSteps(val(P_LEN));
    double tss = trackStepSamples(t);
    int gs = ls < 0 ? -1 : (int)(ls * tss) + 1;
    // Arpeggiator (nur Klangerzeuger mit Tonhoehe): statt eines Tons die Akkordtoene im Tempo RATE
    if (fromSeq && val(P_AMOD) == ARP_CHORD && !machineIsDrum(P.machine[t])) {   // Akkord: Toene gleichzeitig
        int base = val(P_NOTE), chord = val(P_ACHD) % 10;
        int sv = proj.mst[MS_SCALE];
        int notes[4], n = 0;
        if (chord == 9) {   // Skala: Terzschichtung 1-3-5-7 der Skala
            int x = sv ? quantizeNote(base, sv) : base;
            for (int k = 0; k < 4 && x <= 127; k++) { notes[n++] = x; x = sv ? stepScale(stepScale(x, 1, sv), 1, sv) : x + (k % 2 ? 3 : 4); }
        } else for (int k = 0; k < 5 && ARP_IV[chord][k] >= 0 && n < 4; k++) if (base + ARP_IV[chord][k] <= 127) notes[n++] = base + ARP_IV[chord][k];
        if (n == 0) notes[n++] = base;
        ar[t].left = 0; rt[t].left = 0;
        uint8_t L[NPARAMS];
        for (int i = 0; i < NPARAMS; i++) L[i] = lk ? lk[i] : NO_LOCK;
        L[P_NOTE] = (uint8_t)notes[0];
        playNote(t, L, velScale, gs, toMidi);
        for (int k = 0; k < NCHORDV; k++) {
            Voice& c = cv[t][k];
            if (k + 1 < n) {
                L[P_NOTE] = (uint8_t)notes[k + 1];
                c.scaleVal = sv; c.stepSec = v[t].stepSec;
                c.noteOn(P.machine[t], b, L, velScale * 0.8f, gs, (float)sr);
            } else if (c.active) c.noteOff();
        }
        return;
    }
    if (fromSeq) for (int k = 0; k < NCHORDV; k++) if (cv[t][k].active) cv[t][k].noteOff();   // normaler Trig beendet einen Akkord
    if (fromSeq && val(P_AMOD) > 0 && !machineIsDrum(P.machine[t])) {
        Arp& a = ar[t];
        int base = val(P_NOTE), chord = val(P_ACHD) % 10, oct = val(P_AOCT) % 4 + 1;
        int sv = proj.mst[MS_SCALE];
        uint8_t L[48]; int n = 0;
        if (chord == 9) {   // Skalentoene (ohne Skala: Dur)
            static const int maj[7] = {0, 2, 4, 5, 7, 9, 11};
            if (sv) { int x = quantizeNote(base, sv); while (x < base + 12 * oct && n < 48 && x <= 127) { L[n++] = (uint8_t)x; x = stepScale(x, 1, sv); } }
            else for (int o = 0; o < oct; o++) for (int k = 0; k < 7 && n < 48; k++) { int x = base + 12 * o + maj[k]; if (x <= 127) L[n++] = (uint8_t)x; }
        } else {
            for (int o = 0; o < oct; o++)
                for (int k = 0; k < 5 && ARP_IV[chord][k] >= 0 && n < 48; k++) { int x = base + 12 * o + ARP_IV[chord][k]; if (x <= 127) L[n++] = (uint8_t)x; }
        }
        if (n == 0) L[n++] = (uint8_t)base;
        a.mode = val(P_AMOD);
        a.n = 0;
        if (a.mode == 2) for (int i = n - 1; i >= 0; i--) a.notes[a.n++] = L[i];                 // DOWN
        else { for (int i = 0; i < n; i++) a.notes[a.n++] = L[i];                                    // UP / RAND
               if (a.mode == 3) for (int i = n - 2; i >= 1 && a.n < 48; i--) a.notes[a.n++] = L[i]; }  // UPDN
        a.interval = (int)(tss * ARP_STEPS[val(P_ARAT) % 6]);
        if (a.interval < 16) a.interval = 16;
        a.gate = (int)(a.interval * val(P_AGAT) / 100.f);
        if (a.gate < 16) a.gate = 16;
        a.left = gs > 0 ? gs : (1 << 30);   // unendliche Laenge: bis zum naechsten Trig
        a.timer = 0; a.idx = 0;
        a.vel = velScale; a.acc = val(P_AACC) / 100.f;
        a.rng.s ^= (uint32_t)(t * 7919 + base);
        for (int i = 0; i < NPARAMS; i++) a.lk[i] = lk ? lk[i] : NO_LOCK;
        rt[t].left = 0;
        arpFire(t);
        return;
    }
    if (fromSeq) ar[t].left = 0;   // neuer normaler Trig beendet einen laufenden Arp
    playNote(t, lk, velScale, gs, toMidi);
    if (fromSeq) {
        int r = val(P_RTRG);
        if (r > 0) {
            int div = RTRG_DIV[r];
            Retrig& q = rt[t];
            q.left = div - 1;
            q.interval = (int)(tss / div);
            q.timer = q.interval;
            q.vel = velScale;
            q.fade = 0.f;
            for (int i = 0; i < NPARAMS; i++) q.lk[i] = lk ? lk[i] : NO_LOCK;
        } else {
            rt[t].left = 0;
        }
    }
}

void Engine::playNote(int t, const uint8_t* lk, float velScale, int gs, bool toMidi) {
    Pattern& P = TP(t);
    const uint8_t* b = P.p[t];
    auto val = [&](int i) { return (lk && lk[i] != NO_LOCK) ? lk[i] : b[i]; };
    v[t].scaleVal = proj.mst[MS_SCALE];
    v[t].stepSec = (float)(stepSamples() / sr);
    v[t].noteOn(P.machine[t], b, lk, velScale, gs, (float)sr);
    if (toMidi && notesOut && midiOut) {
        if (midiNote[t] >= 0) emit(0x80 | t, (uint8_t)midiNote[t], 0, curOff);
        int nn = quantizeNote(val(P_NOTE), proj.mst[MS_SCALE]);
        int vv = (int)(val(P_VEL) * velScale + 0.5f);
        emit(0x90 | t, (uint8_t)nn, (uint8_t)(vv < 1 ? 1 : (vv > 127 ? 127 : vv)), curOff);
        midiNote[t] = nn;
        midiGate[t] = gs > 0 ? gs : (int)trackStepSamples(t);
    }
}

// naechsten Arp-Ton spielen
void Engine::arpFire(int t) {
    Arp& a = ar[t];
    if (a.n <= 0) return;
    int i = a.mode == 4 ? (int)(a.rng.uni() * a.n) % a.n : a.idx % a.n;
    a.lk[P_NOTE] = a.notes[i];
    float vel = a.vel * ((a.idx % a.n) == 0 ? 1.f : 1.f - 0.6f * a.acc);
    a.idx++;
    playNote(t, a.lk, vel, a.gate, true);
}

void Engine::applyRow(int pos) {
    if (!proj.songLen) return;
    const Project::SongRow& r = proj.row[((pos % proj.songLen) + proj.songLen) % proj.songLen];
    int first = -1;
    for (int t = 0; t < NTRACKS; t++) if (r.pat[t] != SONG_EMPTY) { trkPat[t] = (uint8_t)(r.pat[t] % NPATTERNS); if (first < 0) first = trkPat[t]; }
    if (first >= 0) for (int t = 0; t < NTRACKS; t++) if (r.pat[t] == SONG_EMPTY) trkPat[t] = (uint8_t)first;   // leere Spur: Klang egal (stumm)
    if (first >= 0) curPat = first;
}

void Engine::setBusType(Project& J, int b, int type) {
    if (b < 0 || b >= NBUS) return;
    type = (type % BF_COUNT + BF_COUNT) % BF_COUNT;
    J.bus[b][BX_TYPE] = (uint8_t)type;
    for (int k = 1; k < 8; k++) J.bus[b][k] = busDef(b, type, k).def;
}

void Engine::rowFromPattern(Project& J, Project::SongRow& r, int pat) {
    for (int t = 0; t < NTRACKS; t++) r.pat[t] = (uint8_t)pat;
    r.rep = 1; r.mute = J.patMute[pat % NPATTERNS]; r.pad[0] = r.pad[1] = 0;
}

void Engine::songStart(int pos) {
    if (proj.songLen == 0) { songOn = false; return; }
    songPos = ((pos % proj.songLen) + proj.songLen) % proj.songLen;
    songRep = 0;
    songOn = true;
    nextPat = -1;
    if (playing) rowPending = true; else { applyRow(songPos); rowPending = false; }
}

// Trig-Bedingung pruefen (Digitone-II-Stil). Ergebnis wird fuer PRE/NEI gemerkt.
bool Engine::condOk(int t, int c) {
    if (c <= CND_NONE || c >= CND_COUNT) return true;
    bool r;
    switch (c) {
    case CND_FILL: r = fill; break;
    case CND_NFILL: r = !fill; break;
    case CND_PRE: r = lastCond[t]; break;
    case CND_NPRE: r = !lastCond[t]; break;
    case CND_NEI: r = t > 0 ? lastCond[t - 1] : false; break;
    case CND_NNEI: r = t > 0 ? !lastCond[t - 1] : true; break;
    case CND_1ST: r = loopCount[t] == 0; break;
    case CND_N1ST: r = loopCount[t] != 0; break;
    default: {
        int a, b; bool ng; condDecode(c, a, b, ng);
        r = (loopCount[t] % b) == a - 1;
        if (ng) r = !r;
    }
    }
    if (c != CND_PRE && c != CND_NPRE && c != CND_NEI && c != CND_NNEI) lastCond[t] = r;
    return r;
}

int Engine::seqP(int t, int i) {
    int r = TP(t).p[t][i];
    const Voice& vo = v[t];
    for (int k = 0; k < NLFO; k++)
        if (vo.lfDst[k] == i) { r += (int)vo.lfOff[k]; if (r < 0) r = 0; if (r > vo.lfMax[k]) r = vo.lfMax[k]; }
    return r;
}

void Engine::resetSeq(int t) {
    pend[t][0].on = pend[t][1].on = false; preDone[t] = -1;
    trackPos[t] = 0; trackTick[t] = 0; loopCount[t] = 0; lastCond[t] = false; dirSt[t] = 1; stepCnt[t] = 0;
    int dens = TP(t).p[t][P_DENS];
    tmReg[t] = 0;
    for (int b = 0; b < 32; b++) if (rng.uni() * 100.f < dens) tmReg[t] |= 1u << b;
}

static bool euclidHit(int s, int k, int n, int rot) {
    if (k <= 0 || n <= 0) return false;
    if (k > n) k = n;
    int pos = ((s - rot) % n + n) % n;
    return (pos * k) % n < k;
}

// Bedingung/Wahrscheinlichkeit pruefen und Locks (inkl. generierter Note) fuer einen Step bereitstellen
bool Engine::trigFor(int t, int s, bool on, uint32_t reg, uint8_t* outLk) {
    Pattern& P = TP(t);
    Step& st = P.s[t][s];
    if (!on) return false;
    int cond = st.lock[P_COND] != NO_LOCK ? st.lock[P_COND] : P.p[t][P_COND];
    bool ok = condOk(t, cond);
    int prob = st.lock[P_PROB] != NO_LOCK ? st.lock[P_PROB] : P.p[t][P_PROB];
    if (!(ok && audible(t) && (prob >= 100 || rng.uni() * 100.f < prob))) return false;
    memcpy(outLk, st.lock, NPARAMS);
    int range = seqP(t, P_RANGE);
    if (range > 0) {   // generierte Melodie aus dem Register, in die Skala eingerastet
        int len = P.len[t] < 1 ? 1 : P.len[t], L = len < 32 ? len : 32;
        uint32_t win = (uint32_t)(((uint64_t)reg | ((uint64_t)reg << L)) >> 1 & 0xFF);   // 64 Bit: L = 32 war UB
        int baseN = st.lock[P_NOTE] != NO_LOCK ? st.lock[P_NOTE] : P.p[t][P_NOTE];
        int nn = baseN + (int)(win * range / 255);
        outLk[P_NOTE] = (uint8_t)quantizeNote(nn > 127 ? 127 : nn, proj.mst[MS_SCALE]);
    }
    return true;
}

void Engine::schedule(int t, double at, int s, const uint8_t* lk) {
    Pend* p = !pend[t][0].on ? &pend[t][0] : &pend[t][1];
    p->on = true; p->at = at; p->step = s;
    memcpy(p->lk, lk, NPARAMS);
}

static bool euclidHit(int s, int k, int n, int rot);

void Engine::stepTrack(int t) {
    Pattern& P = TP(t);
    int len = P.len[t] < 1 ? 1 : (P.len[t] > NSTEPS ? NSTEPS : P.len[t]);
    int dir = P.p[t][P_DIR] % 6;
    int s = trackPos[t] % len;
    if (stepCnt[t] == 0 && dir == 1) s = len - 1;      // rueckwaerts beginnt am Ende
    playedStep[t] = s;
    stepStartT[t] = clock;
    stepLenT[t] = trackStepSamples(t);
    Step& st = P.s[t][s];
    bool skip = skipStep[t] == s;
    if (skip) skipStep[t] = -1;

    // Generator: Turing-Register weiterschieben (CHNG = Mutation, DENS = Dichte neuer Bits)
    int puls = seqP(t, P_PULS), rot = seqP(t, P_EROT), dens = seqP(t, P_DENS), chng = seqP(t, P_CHNG);
    int L = len < 32 ? len : 32;
    uint32_t mask = L >= 32 ? 0xFFFFFFFFu : ((1u << L) - 1u);
    uint32_t r0 = tmReg[t] & mask;                       // Schleife = Spurlaenge (max. 32)
    uint32_t bit = r0 & 1u, top = bit;
    if (chng > 0 && rng.uni() * 100.f < chng) top = rng.uni() * 100.f < dens ? 1u : 0u;
    tmReg[t] = ((r0 >> 1) | (top << (L - 1))) & mask;
    auto isOn = [&](int pos, uint32_t b) {
        if (puls > 0 || dens > 0) return (puls > 0 && euclidHit(pos, puls, len, rot)) || (dens > 0 && b);
        return P.s[t][pos].on == 1;   // 2 = trigless (nur Locks, kein Ton)
    };
    auto micro = [&](int pos) {
        const Step& q = P.s[t][pos];
        int m = q.lock[P_MTIM] != NO_LOCK ? q.lock[P_MTIM] : P.p[t][P_MTIM];
        return (m - 23) / 24.0;
    };

    // aktueller Step (wurde er schon vorgezogen ausgeloest, nichts tun)
    if (preDone[t] == s) preDone[t] = -1;
    else if (!skip) {
        uint8_t lk[NPARAMS];
        if (trigFor(t, s, isOn(s, bit), r0, lk)) {
            double off = micro(s);
            v[t].chopStep = s;
            if (off > 0.0) schedule(t, clock + off * stepLenT[t], s, lk);
            else trigger(t, lk, 1.f, true);
        }
    }
    // Trigless Lock: Parameter des klingenden Tons aendern, ohne neu anzuschlagen (gilt bis zum naechsten Trig)
    if (st.on == 2 && !skip) {
        for (int i = 0; i < NPARAMS; i++)
            if (st.lock[i] != NO_LOCK && i != P_NOTE && i != P_LEN && i != P_COND && i != P_PROB) {
                v[t].lk[i] = st.lock[i];
                ar[t].lk[i] = st.lock[i];
            }
    }
    // naechste Position je nach Richtung
    int next = s;
    switch (dir) {
    case 0: next = (s + 1) % len; break;
    case 1: next = (s - 1 + len) % len; break;
    case 2: {   // PING: Enden nicht doppelt
        int d = dirSt[t];
        if (len > 1 && (s + d < 0 || s + d >= len)) d = -d;
        dirSt[t] = d; next = len > 1 ? s + d : 0;
    } break;
    case 3: {   // PEND: Enden doppelt
        int d = dirSt[t];
        if (s + d < 0 || s + d >= len) { dirSt[t] = -d; next = s; } else next = s + d;
    } break;
    case 4: next = (int)(rng.uni() * len) % len; break;
    default: next = (s + (rng.uni() < 0.5f ? -1 : 1) + len) % len; break;
    }
    trackPos[t] = next;
    stepCnt[t]++;
    if (stepCnt[t] % len == 0) loopCount[t] = loopCount[t] + 1;
    // negatives Mikrotiming: naechsten Step schon jetzt pruefen und vorgezogen einplanen
    double offN = micro(next);
    if (offN < 0.0 && skipStep[t] != next) {
        uint8_t lk[NPARAMS];
        preDone[t] = next;
        if (trigFor(t, next, isOn(next, tmReg[t] & 1u), tmReg[t], lk))
            schedule(t, clock + stepLenT[t] * (1.0 + offN), next, lk);
    }
}

// ein Tick (12 pro 16tel): Spuren mit eigener Geschwindigkeit, Pattern-Ende = laengste Spur
void Engine::doTick() {
    if (patTick == 0) {
        if (songActive() && rowPending) {   // Song: Pattern je Spur aus der neuen Zeile
            rowPending = false;
            uint8_t old[NTRACKS]; for (int t = 0; t < NTRACKS; t++) old[t] = trkPat[t];
            applyRow(songPos);
            for (int t = 0; t < NTRACKS; t++) if (trkPat[t] != old[t]) resetSeq(t);   // gleiche Clips laufen weiter
        } else if (nextPat >= 0) {
            curPat = nextPat; nextPat = -1;
            for (int t = 0; t < NTRACKS; t++) resetSeq(t);
        }
    }
    const Project::SongRow* row = songRow();
    long master = 0;
    for (int t = 0; t < NTRACKS; t++) {
        const Pattern& P = TP(t);
        int tps = SPEED_TPS[P.speed[t] % 7];
        if (trackTick[t] % tps == 0) stepTrack(t);
        trackTick[t]++;
        int len = P.len[t] < 1 ? 1 : P.len[t];
        if (trackTick[t] >= len * tps) trackTick[t] = 0;
        if (row && row->pat[t] == SONG_EMPTY) continue;   // leere Zelle bestimmt die Zeilenlaenge nicht
        long l = (long)len * tps;
        if (l > master) master = l;
    }
    if (master <= 0) master = 16 * 12;
    if (++patTick >= master) {
        patTick = 0;
        if (songActive()) {   // Song: Wiederholungen zaehlen, dann naechste Zeile
            int rep = proj.row[songPos % proj.songLen].rep; if (rep < 1) rep = 1;
            if (++songRep >= rep) { songRep = 0; songPos = (songPos + 1) % proj.songLen; rowPending = true; }
        }
    }
    if (clockMode == 1 && (globalTick & 1) == 0) emit(0xF8, 0, 0, curOff);   // 24 ppqn
    globalTick++;
}

// Denormals (winzige Zahlen beim Ausklingen) auf 0 setzen: sonst rechnet die CPU dort um ein Vielfaches langsamer
static inline void flushDenormals() {
#if defined(__aarch64__)
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    if (!(fpcr & (1ull << 24))) { fpcr |= 1ull << 24; __asm__ volatile("msr fpcr, %0" : : "r"(fpcr)); }
#elif defined(__x86_64__) || defined(__i386__)
    unsigned csr;
    __asm__ volatile("stmxcsr %0" : "=m"(csr));
    if ((csr & 0x8040) != 0x8040) { csr |= 0x8040; __asm__ volatile("ldmxcsr %0" : : "m"(csr)); }
#endif
}

void Engine::render(float* out, int frames) {
    flushDenormals();
    const int BLK = 32;   // Steuerdaten (Huellkurven-Koeffizienten, LFOs) alle 32 Samples = 0,7 ms
    for (int i = 0; i < frames; i += BLK) {
        int m = frames - i < BLK ? frames - i : BLK;
        curOff = i;
        if (sendStop) { sendStop = false; allNotesOff(); if (clockMode == 1) emit(0xFC, 0, 0, i); }
        if (sendStart) { sendStart = false; if (clockMode == 1) emit(0xFA, 0, 0, i); }
        if (playing && clockMode != 2) {
            int guard = 0;
            while (clock + m > nextTickAt && guard++ < 16) {
                double o = nextTickAt - clock;
                curOff = i + (o > 0 ? (int)o : 0);
                doTick();
                // Swing: erstes 16tel eines Paares dauert 2*S*sw, das zweite 2*S*(1-sw)
                double S = stepSamples();
                double sw = 0.5 + proj.mst[MS_SWNG] / 100.0;
                int g = (int)((globalTick - 1) % 24);   // gerade ausgefuehrter Tick
                nextTickAt += (g < 12 ? 2.0 * S * sw : 2.0 * S * (1.0 - sw)) / 12.0;
            }
            curOff = i;
        }
        for (int t = 0; t < NTRACKS; t++)
            for (int k = 0; k < 2; k++) {
                Pend& pd = pend[t][k];
                if (pd.on && pd.at < clock + m) { pd.on = false; v[t].chopStep = pd.step; trigger(t, pd.lk, 1.f, true); }
            }
        for (int t = 0; t < NTRACKS; t++) {
            if (midiNote[t] >= 0 && midiGate[t] > 0) {
                midiGate[t] -= m;
                if (midiGate[t] <= 0) { emit(0x80 | t, (uint8_t)midiNote[t], 0, i); midiNote[t] = -1; }
            }
        }
        for (int t = 0; t < NTRACKS; t++) {
            Arp& a = ar[t];
            if (a.left > 0) {
                a.left -= m;
                a.timer += m;
                if (a.timer >= a.interval && a.left > 0) { a.timer -= a.interval; arpFire(t); }
            }
            Retrig& q = rt[t];
            if (q.left > 0) {
                q.timer -= m;
                if (q.timer <= 0) {
                    q.vel = clampf(q.vel + q.fade, 0.f, 1.5f);
                    trigger(t, q.lk, q.vel, false, true);
                    q.left--;
                    q.timer += q.interval;
                }
            }
        }
        processBlock(out + 2 * i, m);
        clock += m;
    }
}

void Engine::processBlock(float* out, int n) {
    float L[32] = {0}, R[32] = {0}, S[NBUS][32] = {{0}}, KL[32] = {0}, KR[32] = {0};   // = BLK in render(); S = Sends an Bus A/B/C
    int key = (int)proj.cmp[CM_SRC] - 1;   // -1 = intern (Summe), sonst Sidechain-Spur
    // klingende Stimmen sammeln; bei spuerbarer Last rechnet ein zweiter Kern mit (beide holen sich Stimmen ab,
    // der Audio-Thread wartet nie auf einen schlafenden Helfer, hoechstens auf dessen laufende Stimme)
    static const float WEIGHT[M_COUNT] = {1.9f, 2.3f, 2.f, 1.7f, 2.3f, 3.2f, 1.7f, 1.8f, 1.6f, 1.9f, 2.5f, 1.8f, 2.1f};
    int nAct = 0; float tot = 0.f;
    WkItem* act = wkList;
    for (int t = 0; t < NTRACKS; t++) {
        if (v[t].active) { act[nAct].v = &v[t]; act[nAct].key = t == key; nAct++; tot += WEIGHT[v[t].machine % M_COUNT]; }
        for (int k = 0; k < NCHORDV; k++) if (cv[t][k].active) { act[nAct].v = &cv[t][k]; act[nAct].key = t == key; nAct++; tot += WEIGHT[cv[t][k].machine % M_COUNT]; }
    }
    bool split = wkOn && audioThreads > 1 && nAct >= 2 && tot >= 8.f;
    if (!split) {
        for (int i = 0; i < nAct; i++) {
            Voice& vc = *act[i].v;
            vc.update(n, (float)sr, proj.bpm);
            if (act[i].key) vc.render(KL, KR, S, n, (float)sr); else vc.render(L, R, S, n, (float)sr);
        }
    } else {
        if (!wkPrioSet) {   // Helfer bekommt die Prioritaet des Audio-Threads
            wkPrioSet = true;
            int pol; struct sched_param sp;
            if (pthread_getschedparam(pthread_self(), &pol, &sp) == 0) pthread_setschedparam(wkThread, pol, &sp);
        }
        for (int i = 0; i < n; i++) { wL[i] = wR[i] = wKL[i] = wKR[i] = 0.f; for (int b = 0; b < NBUS; b++) wS[b][i] = 0.f; }
        __atomic_store_n(&wkN, nAct, __ATOMIC_RELAXED); wkNs = n;
        __atomic_store_n(&wkDone, 0, __ATOMIC_RELAXED);
        int seq = wkGoSeq + 1;
        uint32_t tag = (uint32_t)seq & 0xFFFF;
        __atomic_store_n(&wkClaim, tag << 16, __ATOMIC_RELEASE);
        __atomic_store_n(&wkGoSeq, seq, __ATOMIC_SEQ_CST);
        if (__atomic_load_n(&wkSleeping, __ATOMIC_SEQ_CST)) sem_post(&wkGo);
        int idx, mineN = 0;
        while (wkClaimOne(tag, idx)) {
            Voice& vc = *act[idx].v;
            vc.update(n, (float)sr, proj.bpm);
            if (act[idx].key) vc.render(KL, KR, S, n, (float)sr); else vc.render(L, R, S, n, (float)sr);
            mineN++;
        }
        __atomic_store_n(&wkClaim, (tag << 16) | 0xFFFF, __ATOMIC_RELEASE);   // Block schliessen
        int helper = nAct - mineN;
        if (helper > 0) {
            while (__atomic_load_n(&wkDone, __ATOMIC_ACQUIRE) < helper) { __asm__ __volatile__("" ::: "memory"); }
            for (int i = 0; i < n; i++) { L[i] += wL[i]; R[i] += wR[i]; KL[i] += wKL[i]; KR[i] += wKR[i]; for (int b = 0; b < NBUS; b++) S[b][i] += wS[b][i]; }
            wkBlocks++;
        }
    }
    // Kompressor-Parameter
    float cThr = cmpThrDb(proj.cmp[CM_THR]);
    float cSlope = 1.f - 1.f / CMP_RATIOS[proj.cmp[CM_RAT] > 8 ? 8 : proj.cmp[CM_RAT]];
    float cAtk = 1.f - expf(-1000.f / (cmpAtkMs(proj.cmp[CM_ATK]) * sr));
    float cRel = 1.f - expf(-1000.f / (cmpRelMs(proj.cmp[CM_REL]) * sr));
    float cMake = exp2f(cmpGainDb(proj.cmp[CM_GAIN]) / 6.0206f);
    float cMix = proj.cmp[CM_MIX] / 127.f;
    bool cOn = proj.cmp[CM_THR] < 127 || proj.cmp[CM_GAIN] > 0;
    float grMax = 0;
    // Effekt-Busse: A, dann B, dann C (FWD schickt den Ausgang eines Busses zusaetzlich in den naechsten)
    float BL[NBUS][32], BR[NBUS][32];
    {
        double ss = stepSamples();
        for (int b = 0; b < NBUS; b++) {
            if (b > 0) { float fw = proj.bus[b - 1][BX_FWD] / 127.f; if (fw > 0.f) for (int i = 0; i < n; i++) S[b][i] += (BL[b - 1][i] + BR[b - 1][i]) * 0.5f * fw; }
            bus[b].process(S[b], BL[b], BR[b], n, proj.bus[b], ss);
        }
    }
    float mv = proj.mst[MS_VOL] / 100.f; mv *= mv;
    float mdrv = proj.mst[MS_DRIV] / 127.f;
    float outG = 0.6f * mv * (1.f + 3.f * mdrv), outC = 1.f - 0.25f * mdrv;
    float cleanG = 0.6f * mv, satMix = mdrv * 8.f > 1.f ? 1.f : mdrv * 8.f;   // DRIVE 0 = sauber, ab ~16 voller Charakter
    const float LIM_CEIL = 0.891f;                                      // -1 dBFS (Reserve fuer Wandler/Zwischenwerte)
    const float limRelCo = 1.f - fastExpE(-1.f / (0.08f * sr));         // Rueckkehr ~80 ms
    const float dcR = 1.f - 6.2831853f * 3.f / sr;                      // DC-Sperre: 1-Pol-Hochpass 3 Hz (30 Hz: -0,04 dB)
    if (!limReady) { for (int k = 0; k < LIM_LA; k++) { limReq[k] = limR[k] = 1.f; } limReady = true; }
    float limMin = 1.f;

    djSm += (dj - djSm) * 0.08f;
    int djMode = djSm < -0.03f ? 1 : (djSm > 0.03f ? 2 : 0);
    if (djMode == 1) { float fc = 18000.f * exp2f(djSm * 8.5f); djL.set(fc, 0.35f, sr); djR.set(fc, 0.35f, sr); }
    else if (djMode == 2) { float fc = 20.f * exp2f(djSm * 9.f); djL.set(fc, 0.35f, sr); djR.set(fc, 0.35f, sr); }

    float pl = peakL * 0.97f, pr = peakR * 0.97f;
    for (int i = 0; i < n; i++) {
        float oL = L[i] + BL[0][i] + BL[1][i] + BL[2][i], oR = R[i] + BR[0][i] + BR[1][i] + BR[2][i];
        if (cOn) {
            float det = key >= 0 ? fmaxf(fabsf(KL[i]), fabsf(KR[i])) : fmaxf(fabsf(oL), fabsf(oR));
            cEnv += (det - cEnv) * (det > cEnv ? cAtk : cRel);
            float over = 6.0206f * log2f(cEnv + 1e-9f) - cThr;
            float gr = 0;   // weiches Knie, 6 dB
            if (over > 3.f) gr = cSlope * over;
            else if (over > -3.f) gr = cSlope * (over + 3.f) * (over + 3.f) / 12.f;
            if (gr > grMax) grMax = gr;
            float g = exp2f(-gr / 6.0206f) * cMake;
            float w = 1.f - cMix + cMix * g;
            oL *= w; oR *= w;
        }
        oL += KL[i]; oR += KR[i];
        if (djMode) {
            float lo, bp, hp;
            djL.proc(oL, lo, bp, hp); oL = djMode == 1 ? lo : hp;
            djR.proc(oR, lo, bp, hp); oR = djMode == 1 ? lo : hp;
        }
        if (satMix > 0.f) {   // MASTER DRIVE: gewollte Saettigung (wie frueher), sonst sauber
            oL = oL * cleanG + (fastTanh(oL * outG) * outC - oL * cleanG) * satMix;
            oR = oR * cleanG + (fastTanh(oR * outG) * outC - oR * cleanG) * satMix;
        } else { oL *= cleanG; oR *= cleanG; }
        {   // DC-Sperre: Gleichanteil (asymmetrische Saettigung, Samples) kostet Headroom und knackt beim Start/Stopp
            float yl = oL - dcXL + dcR * dcYL, yr = oR - dcXR + dcR * dcYR;
            dcXL = oL; dcXR = oR; dcYL = yl; dcYR = yr; oL = yl; oR = yr;
        }
        {   // Limiter: Mindest-Verstaerkung der naechsten LIM_LA Samples halten, dann ueber LIM_LA mitteln ->
            // die Kurve ist an jedem verzoegerten Sample <= Decke/Spitze (keine Uebersteuerung, kein Knacken)
            float pk = fmaxf(fabsf(oL), fabsf(oR));
            limXL[limPos] = oL; limXR[limPos] = oR;
            limReq[limPos] = pk > LIM_CEIL ? LIM_CEIL / pk : 1.f;
            float m = 1.f;
            for (int k = 0; k < LIM_LA; k++) m = fminf(m, limReq[k]);
            limRel += (1.f - limRel) * limRelCo;
            if (m < limRel) limRel = m;
            limR[limPos] = limRel;
            float g = 0.f;
            for (int k = 0; k < LIM_LA; k++) g += limR[k];
            g *= 1.f / LIM_LA;
            if (g < limMin) limMin = g;
            limPos = (limPos + 1) & (LIM_LA - 1);
            oL = limXL[limPos] * g; oR = limXR[limPos] * g;   // aeltester Wert = um LIM_LA-1 verzoegert
        }
        out[2 * i] = oL; out[2 * i + 1] = oR;
        float al = fabsf(oL), ar = fabsf(oR);
        if (al > pl) pl = al;
        if (ar > pr) pr = ar;
    }
    peakL = pl; peakR = pr;
    { float gr = limMin < 1.f ? -6.0206f * log2f(limMin) : 0.f; if (gr > limGr) limGr = gr; }
    if (grMax > grPeak) grPeak = grMax;
}

bool Engine::save(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(&proj, sizeof(proj), 1, f) == 1;
    fclose(f);
    return ok;
}

// alte FM-Spur (2 Operatoren, v1-v3) auf 4-Operator-FM umrechnen
static void migrateFm(uint8_t* p) {
    uint8_t rat = p[P_S0], idx = p[P_S1], fbk = p[P_S2], mdec = p[P_S3], fine = p[P_S4], nois = p[P_S5];
    p[P_S0] = 0; p[P_S1] = fbk / 2; p[P_S2] = 3; p[P_S3] = rat; p[P_S4] = 3; p[P_S5] = 3; p[P_S6] = fine; p[P_S7] = nois;
    p[P_X0] = idx; p[P_X1] = 0; p[P_X2] = 0; p[P_X3] = 127; p[P_X4] = mdec; p[P_X5] = 64; p[P_X6] = 64; p[P_X7] = 64;
}

// Parameter (oder Locks) eines alten Formats (40/48/56 Parameter) ins aktuelle Layout uebertragen
static void remapLegacy(const uint8_t* o, int oldNp, uint8_t* out, bool lock, int machine) {
    for (int i = 0; i < NPARAMS; i++) out[i] = lock ? NO_LOCK : paramDef(machine, i).def;
    for (int i = 0; i < 32; i++) out[i] = o[i];
    out[P_COND] = lock ? NO_LOCK : 0;                       // v3: Skala lag hier, v5: Trig-Bedingung
    out[P_MTIM] = lock ? NO_LOCK : 23;                      // bis v6 Retrig-Fade, ab v7 Mikrotiming
    auto has = [&](int i) { return !lock || o[i] != NO_LOCK; };
    static const uint8_t dm[6] = {P_NOTE + 1, P_FREQ + 1, P_VOL + 1, P_PAN + 1, P_RESO + 1, P_CRSH + 1};
    static const uint8_t wm[5] = {LW_SIN, LW_TRI, LW_SAWUP, LW_SQR, LW_SH};
    if (has(32)) out[P_L1 + LF_RATE] = o[32];
    if (has(33)) out[P_L1 + LF_DEPTH] = o[33];
    if (has(34)) out[P_L1 + LF_DEST] = dm[o[34] % 6];
    if (has(35)) out[P_L1 + LF_WAVE] = wm[o[35] % 5];
    if (has(36)) out[P_L1 + LF_MODE] = o[36] ? LM_TRIG : LM_FREE;
    for (int j = 0; j < 3; j++) if (has(37 + j)) out[P_CRSH + j] = o[37 + j];
    if (oldNp > 40) for (int i = 40; i < 45; i++) out[i] = o[i];
    if (oldNp > 48) for (int i = 48; i < 56; i++) out[i] = o[i];
}

// ---- Umstellung auf v13 (16 Seiten): Sends auf die SEND-Seite, FM mit 4 Operatorseiten, SYNTH/WTBL mit eigener 2. Seite
static int ratIdxFor(float r) {   // naechstgelegener Eintrag der neuen Ratio-Tabelle
    int best = 3; float bd = 1e9f;
    for (int i = 0; i < FM_NRAT; i++) { float d = fabsf(FM_RATIOS[i] - r); if (d < bd) { bd = d; best = i; } }
    return best;
}
static int envFromOldFmDec(int dv) {   // alte FM-Abklingzeit 0,01 s * 2^(dv*9/127) -> Wert der neuen Huellkurve (envTime)
    float t = 0.01f * exp2f(dv * (9.f / 127.f));
    int v = (int)lrintf(127.f / 15.f * log2f(t / 0.0005f));
    return v < 0 ? 0 : (v > 126 ? 126 : v);
}
// neuer Index eines Parameters aus dem Layout bis v12 (-1 = entfaellt)
int Engine::mapOldParam(int m, int i) {
    if (i == OLD_P_DLY) return P_DLY;
    if (i == OLD_P_REV) return P_REV;
    if (m == M_FM) {
        if (i >= P_S2 && i <= P_S5) return FM_OPB[i - P_S2] + OP_RAT;
        if (i == P_S6) return FM_OPB[1] + OP_DETN;
        if (i == P_S7) return P_S5;                          // NOIS
        if (i >= P_X0 && i <= P_X2) return FM_OPB[i - P_X0 + 1] + OP_LVL;
        if (i >= P_X3 && i <= P_X6) return FM_OPB[i - P_X3] + OP_DEC;
        if (i == P_X7) return P_S2;                          // VELO
    }
    if ((m == M_SYNTH || m == M_WTBL) && i >= P_X0 && i <= P_X7) return -1;   // bisher unbenutzt
    return i < OLD_NPARAMS ? i : -1;
}
// eine Spur: o = alte 96 Werte (bzw. Locks), out = 128 neue
void Engine::migrateTrack13(int m, const uint8_t* o, uint8_t* out, bool lock) {
    for (int i = 0; i < NPARAMS; i++) out[i] = lock ? NO_LOCK : paramDef(m, i).def;
    for (int i = 0; i < OLD_NPARAMS; i++) {
        if (lock && o[i] == NO_LOCK) continue;
        int j = mapOldParam(m, i);
        if (j >= 0) out[j] = o[i];
    }
    if (m == M_FM) {
        auto has = [&](int i) { return !lock || o[i] != NO_LOCK; };
        for (int k = 0; k < 4; k++) {
            int b = FM_OPB[k];
            if (has(P_S2 + k)) out[b + OP_RAT] = (uint8_t)ratIdxFor(FM_RATIOS_OLD[o[P_S2 + k] & 15]);
            if (has(P_S6)) {   // FINE verstimmte OP2-4 gestaffelt um bis zu 1/4 Halbton
                float cents = (o[P_S6] - 64) / 64.f * 25.f * k;
                out[b + OP_DETN] = (uint8_t)(k ? (cents > 50 ? 127 : (cents < -50 ? 0 : 64 + (int)lrintf(cents * 64.f / 50.f))) : 64);
                if (out[b + OP_DETN] > 127) out[b + OP_DETN] = 127;
            }
            if (!lock) { out[b + OP_FINE] = 64; out[b + OP_ATK] = 0; out[b + OP_LVL] = k ? o[P_X0 + k - 1] : 127; }
            if (has(P_X3 + k)) {
                int dv = o[P_X3 + k];
                if (dv >= 127) { out[b + OP_SUS] = 127; if (!lock) { out[b + OP_DEC] = 70; out[b + OP_REL] = 127; } else out[b + OP_DEC] = NO_LOCK; }
                else { int e = envFromOldFmDec(dv); out[b + OP_DEC] = (uint8_t)e; out[b + OP_SUS] = 0; if (!lock) out[b + OP_REL] = (uint8_t)e; }
            }
        }
        if (!lock) { out[P_S3] = 64; out[P_S6] = 0; out[P_S7] = 0; }
    }
    for (int k = 0; k < NLFO; k++) {   // LFO-Ziele (Wert = Parameter + 1) mitziehen
        int di = LFO_BASE[k] + LF_DEST, d = out[di];
        if (d == NO_LOCK || d == 0) continue;
        int j = mapOldParam(m, d - 1);
        out[di] = (uint8_t)(j >= 0 ? j + 1 : 0);
    }
}

bool Engine::load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 20 || size > (long)sizeof(Project) * 2) { fclose(f); return false; }
    uint8_t* buf = (uint8_t*)malloc(size);
    bool ok = fread(buf, 1, size, f) == (size_t)size;
    fclose(f);
    uint32_t ver = 0;
    if (ok) memcpy(&ver, buf + 4, 4);
    ok = ok && memcmp(buf, "GBX1", 4) == 0;
    uint8_t oldFx[8]; for (int i = 0; i < 8; i++) oldFx[i] = FX_DEFS[i].def;   // bis v12: Delay/Reverb
    Project::SongRow oldRows[64]; uint8_t oldMute[NPATTERNS]; bool haveRows = false;
    struct OldSong { uint8_t pat, rep; } oldSong[64]; memset(oldSong, 0, sizeof oldSong);
    // alte Formate: v1/v2 = 40 Parameter, v3 = 48, v4 = 56; immer 16 Steps; Pattern ab Offset 20
    long oldNp = ver <= 2 ? 40 : (ver == 3 ? 48 : 56);
    long oldPat = 16 + 8 * oldNp + NTRACKS * 16 * (1 + oldNp);
    long expect = 20 + NPATTERNS * oldPat + (ver >= 2 ? 16 : 0);
    // v5..v12: 72/80/88/96 Parameter, 128 Steps, speed-Feld
    long nNp = ver == 5 ? 72 : (ver <= 7 ? 80 : (ver == 8 ? 88 : OLD_NPARAMS));
    long nPat = 24 + 8 * nNp + NTRACKS * 128 * (1 + nNp), nBase = 20 + NPATTERNS * nPat;
    long nSize = ver <= 8 ? nBase + 16 : projectDataSize(ver);
    Project* np = nullptr;
    if (ok && ver == PROJECT_VERSION && size >= (long)sizeof(Project)) {   // dahinter evtl. Sample-Namensliste
        memcpy(&proj, buf, sizeof(Project));
    } else if (ok && ver >= 5 && ver <= 12 && (ver <= 8 ? size == nSize : size >= nSize)) {
        np = (Project*)calloc(1, sizeof(Project));
        memcpy(np, buf, 20);
        for (int pi = 0; pi < NPATTERNS; pi++) {
            const uint8_t* q = buf + 20 + pi * nPat;
            Pattern& P = np->pat[pi];
            memcpy(P.machine, q, 8); memcpy(P.len, q + 8, 8); memcpy(P.speed, q + 16, 8);
            for (int t = 0; t < NTRACKS; t++) {
                for (int i = 0; i < NPARAMS; i++) P.p[t][i] = paramDef(P.machine[t] % M_COUNT, i).def;
                memcpy(P.p[t], q + 24 + t * nNp, nNp);
                if (ver <= 6) P.p[t][P_MTIM] = 23;   // bis v6 Retrig-Fade
                for (int st = 0; st < NSTEPS; st++) {
                    const uint8_t* sp = q + 24 + 8 * nNp + (t * 128 + st) * (1 + nNp);
                    P.s[t][st].on = sp[0];
                    memset(P.s[t][st].lock, NO_LOCK, NPARAMS);
                    memcpy(P.s[t][st].lock, sp + 1, nNp);
                    if (ver <= 6) P.s[t][st].lock[P_MTIM] = NO_LOCK;
                }
            }
        }
        memcpy(np->cmp, buf + nBase, 8);
        memcpy(oldFx, buf + nBase + 8, 8);
        if (ver >= 9) { memcpy(oldSong, buf + nBase + 16, 128); np->songLen = buf[nBase + 144]; }
        if (ver == 12) { memcpy(oldMute, buf + nBase + 148, NPATTERNS); memcpy(oldRows, buf + nBase + 148 + NPATTERNS, sizeof oldRows); haveRows = true; }
    } else if (ok && ver >= 1 && ver <= 4 && size == expect) {
        np = (Project*)calloc(1, sizeof(Project));
        memcpy(np->magic, "GBX1", 4);
        memcpy(&np->bpm, buf + 8, 2);
        memcpy(np->mst, buf + 10, 8);
        int scaleFromV3 = 0;
        for (int pi = 0; pi < NPATTERNS; pi++) {
            const uint8_t* q = buf + 20 + pi * oldPat;
            Pattern& P = np->pat[pi];
            memcpy(P.machine, q, 8);
            memcpy(P.len, q + 8, 8);
            for (int t = 0; t < NTRACKS; t++) {
                int m = P.machine[t] % M_COUNT;
                P.speed[t] = SPEED_1X;
                if (P.len[t] < 1 || P.len[t] > 16) P.len[t] = 16;
                const uint8_t* op = q + 16 + t * oldNp;
                if (ver == 3 && !scaleFromV3 && op[7] > 0 && op[7] <= 120) scaleFromV3 = op[7];
                remapLegacy(op, oldNp, P.p[t], false, m);
                bool fm = m == M_FM && ver < 4;
                if (fm) migrateFm(P.p[t]);
                for (int st = 0; st < 16; st++) {
                    const uint8_t* sp = q + 16 + 8 * oldNp + (t * 16 + st) * (1 + oldNp);
                    P.s[t][st].on = sp[0];
                    remapLegacy(sp + 1, oldNp, P.s[t][st].lock, true, m);
                    if (fm) for (int i = P_S0; i <= P_S7; i++) P.s[t][st].lock[i] = NO_LOCK;
                }
                for (int st = 16; st < NSTEPS; st++) { P.s[t][st].on = 0; memset(P.s[t][st].lock, NO_LOCK, NPARAMS); }
            }
        }
        if (ver >= 2) {
            memcpy(np->cmp, buf + 20 + NPATTERNS * oldPat, 8);
            memcpy(oldFx, buf + 20 + NPATTERNS * oldPat + 8, 8);
        } else {   // Version 1: Effekte lagen in mst
            uint8_t old[8];
            memcpy(old, np->mst, 8);   // BPM SWNG DTIM DFBK DTON RSIZ RDMP VOL
            for (int i = 0; i < 8; i++) { np->cmp[i] = CMP_DEFS[i].def; np->mst[i] = MST_DEFS[i].def; }
            oldFx[FX_DTIM] = old[2]; oldFx[FX_DFBK] = old[3]; oldFx[FX_DTON] = old[4];
            oldFx[FX_RSIZ] = old[5]; oldFx[FX_RDMP] = old[6];
            np->mst[MS_SWNG] = old[1]; np->mst[MS_VOL] = old[7];
        }
        static const uint8_t dtMap[8] = {1, 4, 6, 7, 9, 10, 12, 13};   // alte Delay-Zeiten (8 Werte) -> neue Liste (15)
        oldFx[FX_DTIM] = dtMap[oldFx[FX_DTIM] & 7];
        oldFx[FX_DFBK] = (uint8_t)(oldFx[FX_DFBK] * 0.95f / 1.2f);   // gleiches Feedback wie vorher
        if (ver <= 3) np->mst[MS_SCALE] = (uint8_t)scaleFromV3;
    } else {
        ok = false;
    }
    if (ok && np) {   // altes Layout (96 Parameter) -> v13
        static uint8_t o[OLD_NPARAMS];
        for (int pi = 0; pi < NPATTERNS; pi++) for (int t = 0; t < NTRACKS; t++) {
            Pattern& P = np->pat[pi];
            int m = P.machine[t] % M_COUNT;
            memcpy(o, P.p[t], OLD_NPARAMS); migrateTrack13(m, o, P.p[t], false);
            for (int st = 0; st < NSTEPS; st++) { memcpy(o, P.s[t][st].lock, OLD_NPARAMS); migrateTrack13(m, o, P.s[t][st].lock, true); }
        }
        // Effekte: Bus A = Delay, Bus B = Reverb (bisherige Werte), Bus C frei
        for (int b = 0; b < NBUS; b++) setBusType(*np, b, b == 0 ? BF_DELAY : (b == 1 ? BF_REVERB : BF_OFF));
        np->bus[0][BX_P1] = oldFx[FX_DTIM]; np->bus[0][BX_P2] = oldFx[FX_DFBK]; np->bus[0][BX_P3] = oldFx[FX_DTON]; np->bus[0][BX_P4] = oldFx[FX_DWID];
        np->bus[0][BX_P5] = 0;
        np->bus[1][BX_P1] = oldFx[FX_RSIZ]; np->bus[1][BX_P2] = oldFx[FX_RDMP]; np->bus[1][BX_P3] = oldFx[FX_RPRE]; np->bus[1][BX_P4] = oldFx[FX_RWID];
        np->bus[1][BX_P5] = 0;
        if (haveRows) { memcpy(np->patMute, oldMute, NPATTERNS); memcpy(np->row, oldRows, sizeof oldRows); }
        else {   // bis v11: Song war eine Pattern-Kette -> Zeilen mit demselben Pattern auf allen Spuren
            if (np->songLen > SONG_MAX) np->songLen = 0;
            for (int i = 0; i < SONG_MAX; i++) { rowFromPattern(*np, np->row[i], oldSong[i].pat % NPATTERNS); np->row[i].rep = oldSong[i].rep < 1 ? 1 : oldSong[i].rep; }
        }
        np->version = PROJECT_VERSION;
        memcpy(&proj, np, sizeof(Project));
        free(np);
        printf("Projekt von Version %u auf %d umgestellt\n", ver, PROJECT_VERSION);
    }
    if (ok && ver < 10) {   // bis v9: SYNTH/WTBL spielten den Tiefpass mit 24 dB -> Typ LP24
        for (int pi = 0; pi < NPATTERNS; pi++) for (int t = 0; t < NTRACKS; t++) {
            Pattern& P = proj.pat[pi];
            int m = P.machine[t];
            if (m != M_SYNTH && m != M_WTBL) continue;
            if (P.p[t][P_FTYP] == FT_LP12) P.p[t][P_FTYP] = FT_LP24;
            for (int st = 0; st < NSTEPS; st++) if (P.s[t][st].lock[P_FTYP] == FT_LP12) P.s[t][st].lock[P_FTYP] = FT_LP24;
        }
    }
    if (ok && ver < 11) {   // bis v10: Huellkurvenform neu (F.KRV/A.KRV) -> neutral
        for (int pi = 0; pi < NPATTERNS; pi++) for (int t = 0; t < NTRACKS; t++) {
            Pattern& P = proj.pat[pi];
            P.p[t][P_FCRV] = 64; P.p[t][P_ACRV] = 64;
            for (int st = 0; st < NSTEPS; st++) { P.s[t][st].lock[P_FCRV] = NO_LOCK; P.s[t][st].lock[P_ACRV] = NO_LOCK; }
        }
    }
    if (ok) {   // Grundton getrennt merken (aeltere Projekte hatten ihn nur in SCALE)
        if (proj.mst[MS_SCALE] > 120) proj.mst[MS_SCALE] = 0;
        if (proj.mst[MS_SCALE]) proj.mst[MS_ROOT] = (uint8_t)((proj.mst[MS_SCALE] - 1) % 12);
        else proj.mst[MS_ROOT] %= 12;
    }
    free(buf);
    return ok;
}
