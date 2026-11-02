#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static float SINT[2049];
static inline float sin01(float ph) {
    ph -= floorf(ph);
    float x = ph * 2048.f;
    int i = (int)x;
    float f = x - i;
    return SINT[i] + (SINT[i + 1] - SINT[i]) * f;
}
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float fastTanh(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}
static inline float envT(int v) { return 0.0005f * exp2f(v * (15.f / 127.f)); }
static inline float decayCo(float t, float sr) { return expf(-6.9f / (t * sr)); }
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
    machine = m;
    base = b;
    for (int i = 0; i < NPARAMS; i++) lk[i] = locks ? locks[i] : NO_LOCK;
    vel = clampf(P(P_VEL) / 127.f * velScale, 0.f, 1.f);
    note = (float)P(P_NOTE);
    gate = gateSamples;
    if (!wasActive || m != M_SYNTH || P(P_S7) == 0) curNote = note;
    active = true;
    if (P(P_LTRG)) lfoPh = 0;
    update(0, sr, 120);
    aenv.on();
    fenv.on();
    switch (m) {
    case M_KICK: ph1 = 0; penv = 1; denv = 1; cenv = 1; aenv.v = 0; break;
    case M_SNARE: ph1 = ph2 = 0; denv = 1; nenv = 1; break;
    case M_HAT: denv = 1; break;
    case M_CLAP: clapLeft = P(P_S3) + 1; clapTimer = 0; clapTail = false; burst = 0; denv = 0; break;
    case M_FM: menv = 1; if (!wasActive) { ph1 = phM = 0; fbPrev = 0; } break;
    default: break;
    }
}

void Voice::update(int n, float sr, float bpm) {
    (void)bpm;
    // LFO
    float rate = 0.05f * exp2f(P(P_LSPD) * (9.f / 127.f));
    lfoPh += rate * n / sr;
    if (lfoPh >= 1.f) { lfoPh -= floorf(lfoPh); lfoSH = rng.next(); }
    switch (P(P_LWAV)) {
    case 0: lfoVal = sin01(lfoPh); break;
    case 1: lfoVal = 1.f - 4.f * fabsf(lfoPh - 0.5f); break;
    case 2: lfoVal = 2.f * lfoPh - 1.f; break;
    case 3: lfoVal = lfoPh < 0.5f ? 1.f : -1.f; break;
    default: lfoVal = lfoSH; break;
    }
    float m = lfoVal * (P(P_LDEP) - 64) / 64.f;
    int dst = P(P_LDST);
    float pitchMod = dst == 0 ? m * 12.f : 0.f;
    float freqMod = dst == 1 ? m * 64.f : 0.f;
    float volMod = dst == 2 ? clampf(1.f + m, 0.f, 2.f) : 1.f;
    float panMod = dst == 3 ? m : 0.f;
    float resMod = dst == 4 ? m * 64.f : 0.f;
    float crsMod = dst == 5 ? m * 127.f : 0.f;

    aenv.set(envT(P(P_AATK)), envT(P(P_ADEC)), P(P_ASUS) / 127.f, envT(P(P_AREL)), sr);
    fenv.set(envT(P(P_FATK)), envT(P(P_FDEC)), P(P_FSUS) / 127.f, envT(P(P_FREL)), sr);
    fBase = P(P_FREQ) + freqMod;
    fEnv = (P(P_FENV) - 64) * 2.f;
    reso = clampf((P(P_RESO) + resMod) / 127.f, 0.f, 1.f);
    ftype = P(P_FTYP);

    // Tonhoehe inkl. Glide
    if (machine == M_SYNTH && P(P_S7) > 0 && n > 0) {
        float gt = 0.001f * exp2f(P(P_S7) * (11.f / 127.f));
        float co = 1.f - expf(-(float)n / (gt * sr));
        curNote += (note - curNote) * co;
    } else {
        curNote = note;
    }
    float nn = curNote + pitchMod;
    float ratio = exp2f((nn - 60.f) / 12.f);
    switch (machine) {
    case M_KICK:
        pitchHz = 30.f * exp2f(P(P_S0) * (2.f / 127.f)) * ratio;
        ddco = decayCo(0.03f * exp2f(P(P_S1) * (6.f / 127.f)), sr);
        pco = decayCo(0.002f * exp2f(P(P_S3) * (7.f / 127.f)), sr);
        cco = decayCo(0.004f, sr);
        break;
    case M_SNARE:
        pitchHz = 120.f * exp2f(P(P_S0) * (1.74f / 127.f)) * ratio;
        ddco = decayCo(0.03f * exp2f(P(P_S1) * (5.f / 127.f)), sr);
        nco = decayCo(0.03f * exp2f(P(P_S3) * (5.f / 127.f)), sr);
        aux.set(1000.f * exp2f(P(P_S4) * (3.5f / 127.f)), 0.2f, sr);
        break;
    case M_HAT:
        pitchHz = 200.f * exp2f(P(P_S0) * (1.6f / 127.f)) * ratio;
        ddco = decayCo(0.01f * exp2f(P(P_S1) * (7.5f / 127.f)), sr);
        aux.set(3000.f * exp2f(P(P_S2) * (2.f / 127.f)), 0.3f, sr);
        break;
    case M_CLAP:
        aux.set(600.f * exp2f(P(P_S0) * (2.5f / 127.f)) * ratio, 0.55f, sr);
        ddco = decayCo(0.05f * exp2f(P(P_S1) * (5.f / 127.f)), sr);
        clapInterval = (int)((0.004f + P(P_S2) / 127.f * 0.025f) * sr);
        bco = decayCo(0.012f, sr);
        break;
    case M_FM:
        pitchHz = 440.f * exp2f((nn - 69.f) / 12.f);
        mco = decayCo(0.01f * exp2f(P(P_S3) * (9.f / 127.f)), sr);
        break;
    default:
        pitchHz = 440.f * exp2f((nn - 69.f) / 12.f);
        break;
    }

    float vg = P(P_VOL) / 100.f;
    float vol = vg * vg * vel * volMod;
    float pan = clampf((P(P_PAN) - 64) / 64.f + panMod, -1.f, 1.f);
    float a = (pan + 1.f) * 0.78539816f;
    gainL = vol * cosf(a) * 1.41421356f;
    gainR = vol * sinf(a) * 1.41421356f;
    sendD = vol * P(P_DLY) / 127.f;
    sendR = vol * P(P_REV) / 127.f;
    drive = P(P_DRIV) / 127.f;
    float cr = clampf(P(P_CRSH) + crsMod, 0.f, 127.f);
    crushQ = cr > 0.5f ? exp2f(15.f - cr / 127.f * 13.f) : 0.f;
    srrN = 1 + P(P_SRR) / 6;

    if (gate > 0) {
        gate -= n;
        if (gate <= 0) { gate = 0; if (machine >= M_SYNTH) noteOff(); }
    }
}

void Voice::render(float* L, float* R, float* dS, float* rS, int n, float sr) {
    if (!active) return;
    static const float HAT_R[6] = {1.f, 1.4836f, 1.8016f, 2.5478f, 2.6321f, 3.8996f};
    const float inv = 1.f / sr;
    float units = fBase + fEnv * fenv.v;
    bool bypassF = (ftype == 0 && units >= 126.f && reso < 0.01f);
    if (!bypassF) filt.set(16.f * exp2f(units * (10.3f / 127.f)), reso * 0.97f, sr);

    // maschinenspezifische Konstanten
    float sweep = P(P_S2) / 127.f * 6.f, click = P(P_S4) / 127.f, body = P(P_S5) / 127.f;
    float snap = P(P_S2) / 127.f, metl = P(P_S3) / 127.f;
    float pw = 0.5f + (P(P_S6) - 64) / 140.f;
    float detune = exp2f((P(P_S2) - 64) / 64.f * 0.5f / 12.f);
    float mix = P(P_S3) / 127.f, sub = P(P_S4) / 127.f, noise = P(P_S5) / 127.f;
    float fmRatio = FM_RATIOS[P(P_S0) & 15] * exp2f((P(P_S4) - 64) / 64.f / 12.f);
    float fmIndex = P(P_S1) / 127.f * 1.6f, fmFb = P(P_S2) / 127.f * 0.4f;
    int w1 = P(P_S0) & 3, w2 = P(P_S1) & 3;
    float dt = pitchHz * inv;

    for (int i = 0; i < n; i++) {
        float s = 0.f;
        switch (machine) {
        case M_KICK: {
            float f = pitchHz * (1.f + sweep * penv);
            penv *= pco;
            ph1 += f * inv; if (ph1 >= 1.f) ph1 -= 1.f;
            s = sin01(ph1) * denv;
            if (body > 0) s = fastTanh(s * (1.f + body * 5.f));
            s += rng.next() * cenv * click * 0.7f;
            denv *= ddco; cenv *= cco;
        } break;
        case M_SNARE: {
            ph1 += dt; if (ph1 >= 1.f) ph1 -= 1.f;
            ph2 += dt * 1.47f; if (ph2 >= 1.f) ph2 -= 1.f;
            float tone = (sin01(ph1) + 0.6f * sin01(ph2)) * 0.6f * denv;
            float lo, bp, hp;
            aux.proc(rng.next(), lo, bp, hp);
            float nz = (hp * 0.7f + bp) * nenv * snap;
            s = tone * (1.f - snap * 0.5f) + nz;
            denv *= ddco; nenv *= nco;
        } break;
        case M_HAT: {
            float sum = 0;
            for (int k = 0; k < 6; k++) {
                hph[k] += dt * HAT_R[k]; if (hph[k] >= 1.f) hph[k] -= 1.f;
                sum += hph[k] < 0.5f ? 1.f : -1.f;
            }
            float src = sum * (1.f / 6.f) * metl + rng.next() * (1.f - metl);
            float lo, bp, hp;
            aux.proc(src, lo, bp, hp);
            s = hp * denv * 1.4f;
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
            burst *= bco;
            if (clapTail) denv *= ddco;
            float lo, bp, hp;
            aux.proc(rng.next(), lo, bp, hp);
            s = bp * env * 2.5f;
        } break;
        case M_SYNTH: {
            float dt2 = dt * detune;
            float o1 = osc(w1, ph1, dt, pw), o2 = osc(w2, ph2, dt2, pw);
            ph1 += dt; if (ph1 >= 1.f) ph1 -= 1.f;
            ph2 += dt2; if (ph2 >= 1.f) ph2 -= 1.f;
            phs += dt * 0.5f; if (phs >= 1.f) phs -= 1.f;
            s = o1 * (1.f - mix) + o2 * mix;
            s += sub * (phs < 0.5f ? 0.8f : -0.8f);
            s += noise * rng.next();
            s *= 0.5f;
        } break;
        case M_FM: {
            phM += dt * fmRatio; if (phM >= 1.f) phM -= 1.f;
            float mod = sin01(phM + fbPrev * fmFb);
            fbPrev = mod;
            ph1 += dt; if (ph1 >= 1.f) ph1 -= 1.f;
            s = sin01(ph1 + mod * fmIndex * menv);
            menv *= mco;
            s += noise * rng.next() * 0.5f;
            s *= 0.6f;
        } break;
        }

        fenv.tick();
        if (!bypassF) {
            float lo, bp, hp;
            filt.proc(s, lo, bp, hp);
            s = ftype == 0 ? lo : (ftype == 1 ? bp : hp);
        }
        s *= aenv.tick();
        if (drive > 0.f) s = fastTanh(s * (1.f + drive * 6.f)) * (1.f - drive * 0.3f);
        if (srrN > 1) { if (holdC <= 0) { hold = s; holdC = srrN; } holdC--; s = hold; }
        if (crushQ > 0.f) s = floorf(s * crushQ + 0.5f) / crushQ;

        L[i] += s * gainL;
        R[i] += s * gainR;
        dS[i] += s * sendD;
        rS[i] += s * sendR;
    }

    // Stimme beenden, wenn verklungen
    switch (machine) {
    case M_KICK: case M_HAT: if (denv < 1e-4f) active = false; break;
    case M_SNARE: if (denv < 1e-4f && nenv < 1e-4f) active = false; break;
    case M_CLAP: if (clapTail && denv < 1e-4f && burst < 1e-4f) active = false; break;
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

void Engine::init(int sampleRate) {
    sr = sampleRate;
    for (int i = 0; i <= 2048; i++) SINT[i] = sinf(i / 2048.f * 6.2831853f);
    dlyLen = sr * 4 + 16;
    dlyL = (float*)calloc(dlyLen, sizeof(float));
    dlyR = (float*)calloc(dlyLen, sizeof(float));
    rev.init(sr);
    for (int t = 0; t < NTRACKS; t++) v[t].rng.s = 0x1234567u * (t + 1);
    defaultProject();
}

static void resetTrack(Pattern& p, int t, int m) {
    p.machine[t] = (uint8_t)m;
    for (int i = 0; i < NPARAMS; i++) p.p[t][i] = paramDef(m, i).def;
    if (m == M_SYNTH) {
        p.p[t][P_FREQ] = 70; p.p[t][P_RESO] = 40; p.p[t][P_FENV] = 96; p.p[t][P_FDEC] = 50;
        p.p[t][P_ADEC] = 60; p.p[t][P_ASUS] = 90; p.p[t][P_AREL] = 40;
    } else if (m == M_FM) {
        p.p[t][P_ADEC] = 70; p.p[t][P_ASUS] = 40; p.p[t][P_AREL] = 60;
    }
}

void Engine::setMachine(Pattern& p, int t, int m) {
    p.machine[t] = (uint8_t)m;
    for (int i = P_S0; i <= P_S7; i++) {
        p.p[t][i] = paramDef(m, i).def;
        for (int s = 0; s < NSTEPS; s++) p.s[t][s].lock[i] = NO_LOCK;
    }
}

void Engine::initPattern(Pattern& p) {
    static const int defM[NTRACKS] = {M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_SYNTH, M_FM, M_SYNTH};
    memset(&p, 0, sizeof(p));
    for (int t = 0; t < NTRACKS; t++) {
        resetTrack(p, t, defM[t]);
        p.len[t] = NSTEPS;
        for (int s = 0; s < NSTEPS; s++) memset(p.s[t][s].lock, NO_LOCK, NPARAMS);
    }
    // sinnvolle Startklaenge
    p.p[4][P_NOTE] = 36;                                          // Bass
    p.p[5][P_S0] = 1; p.p[5][P_DLY] = 50; p.p[5][P_FREQ] = 85;    // Lead
    p.p[6][P_NOTE] = 72; p.p[6][P_DLY] = 40; p.p[6][P_REV] = 50;  // FM-Glocke
    p.p[7][P_AATK] = 75; p.p[7][P_AREL] = 95; p.p[7][P_REV] = 70; // Pad
    p.p[7][P_S2] = 80; p.p[7][P_S3] = 64; p.p[7][P_LEN] = 80;
}

void Engine::defaultProject() {
    memset(&proj, 0, sizeof(proj));
    memcpy(proj.magic, "GBX1", 4);
    proj.version = 1;
    proj.bpm = 120;
    for (int i = 0; i < 8; i++) proj.mst[i] = MST_DEFS[i].def;
    for (int i = 0; i < NPATTERNS; i++) initPattern(proj.pat[i]);
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
}

void Engine::start() {
    for (int t = 0; t < NTRACKS; t++) trackPos[t] = 0;
    if (nextPat >= 0) { curPat = nextPat; nextPat = -1; }
    globalStep = 0;
    nextStepAt = clock;
    playing = true;
}

void Engine::stop() {
    playing = false;
    for (int t = 0; t < NTRACKS; t++) { v[t].noteOff(); rt[t].left = 0; }
}

void Engine::preview(int t, const Step* s) {
    trigger(t, (s && s->on) ? s->lock : nullptr, 1.f, false);
}

void Engine::trigger(int t, const uint8_t* lk, float velScale, bool fromSeq) {
    Pattern& P = proj.pat[curPat];
    const uint8_t* b = P.p[t];
    auto val = [&](int i) { return (lk && lk[i] != NO_LOCK) ? lk[i] : b[i]; };
    float ls = lenSteps(val(P_LEN));
    int gs = ls < 0 ? -1 : (int)(ls * stepSamples()) + 1;
    v[t].noteOn(P.machine[t], b, lk, velScale, gs, (float)sr);
    if (fromSeq) {
        int r = val(P_RTRG);
        if (r > 0) {
            int div = RTRG_DIV[r];
            Retrig& q = rt[t];
            q.left = div - 1;
            q.interval = (int)(stepSamples() / div);
            q.timer = q.interval;
            q.vel = velScale;
            q.fade = (val(P_RFAD) - 64) / 64.f / div;
            for (int i = 0; i < NPARAMS; i++) q.lk[i] = lk ? lk[i] : NO_LOCK;
        } else {
            rt[t].left = 0;
        }
    }
}

void Engine::doStep() {
    if (globalStep == 0 && nextPat >= 0) {
        curPat = nextPat; nextPat = -1;
        for (int t = 0; t < NTRACKS; t++) trackPos[t] = 0;
    }
    Pattern& P = proj.pat[curPat];
    for (int t = 0; t < NTRACKS; t++) {
        int len = P.len[t] < 1 ? 1 : P.len[t];
        int s = trackPos[t] % len;
        playedStep[t] = s;
        Step& st = P.s[t][s];
        if (st.on && !mute[t]) {
            int prob = st.lock[P_PROB] != NO_LOCK ? st.lock[P_PROB] : P.p[t][P_PROB];
            if (prob >= 100 || rng.uni() * 100.f < prob) trigger(t, st.lock, 1.f, true);
        }
        trackPos[t] = (s + 1) % len;
    }
    globalStep = (globalStep + 1) % NSTEPS;
}

void Engine::render(float* out, int frames) {
    const int BLK = 16;
    for (int i = 0; i < frames; i += BLK) {
        int m = frames - i < BLK ? frames - i : BLK;
        if (playing) {
            int guard = 0;
            while (clock >= nextStepAt && guard++ < 4) {
                doStep();
                double S = stepSamples();
                double sw = 0.5 + proj.mst[MS_SWNG] / 100.0;
                nextStepAt += (globalStep & 1) ? 2.0 * S * sw : 2.0 * S * (1.0 - sw);
            }
        }
        for (int t = 0; t < NTRACKS; t++) {
            Retrig& q = rt[t];
            if (q.left > 0) {
                q.timer -= m;
                if (q.timer <= 0) {
                    q.vel = clampf(q.vel + q.fade, 0.f, 1.5f);
                    trigger(t, q.lk, q.vel, false);
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
    float L[16] = {0}, R[16] = {0}, D[16] = {0}, RV[16] = {0};
    for (int t = 0; t < NTRACKS; t++) {
        if (!v[t].active) continue;
        v[t].update(n, (float)sr, proj.bpm);
        v[t].render(L, R, D, RV, n, (float)sr);
    }
    int dlen = (int)(DLY_STEPS[proj.mst[MS_DTIM] & 7] * stepSamples());
    if (dlen > dlyLen - 1) dlen = dlyLen - 1;
    if (dlen < 1) dlen = 1;
    float fb = proj.mst[MS_DFBK] / 127.f * 0.95f;
    float dtone = 0.05f + proj.mst[MS_DTON] / 127.f * 0.95f;
    float rsize = proj.mst[MS_RSIZ] / 127.f, rdamp = proj.mst[MS_RDMP] / 127.f;
    float mv = proj.mst[MS_VOL] / 100.f; mv *= mv;

    djSm += (dj - djSm) * 0.08f;
    int djMode = djSm < -0.03f ? 1 : (djSm > 0.03f ? 2 : 0);
    if (djMode == 1) { float fc = 18000.f * exp2f(djSm * 8.5f); djL.set(fc, 0.35f, sr); djR.set(fc, 0.35f, sr); }
    else if (djMode == 2) { float fc = 20.f * exp2f(djSm * 9.f); djL.set(fc, 0.35f, sr); djR.set(fc, 0.35f, sr); }

    float pl = peakL * 0.97f, pr = peakR * 0.97f;
    for (int i = 0; i < n; i++) {
        int rp = dlyPos - dlen; if (rp < 0) rp += dlyLen;
        dlpL += (dlyL[rp] - dlpL) * dtone;
        dlpR += (dlyR[rp] - dlpR) * dtone;
        dlyL[dlyPos] = D[i] + dlpR * fb;
        dlyR[dlyPos] = dlpL * fb;
        if (++dlyPos >= dlyLen) dlyPos = 0;
        float rl, rr;
        rev.process(RV[i] + (dlpL + dlpR) * 0.15f, rsize, rdamp, rl, rr);
        float oL = L[i] + dlpL + rl, oR = R[i] + dlpR + rr;
        if (djMode) {
            float lo, bp, hp;
            djL.proc(oL, lo, bp, hp); oL = djMode == 1 ? lo : hp;
            djR.proc(oR, lo, bp, hp); oR = djMode == 1 ? lo : hp;
        }
        oL = fastTanh(oL * 0.6f * mv);
        oR = fastTanh(oR * 0.6f * mv);
        out[2 * i] = oL; out[2 * i + 1] = oR;
        float al = fabsf(oL), ar = fabsf(oR);
        if (al > pl) pl = al;
        if (ar > pr) pr = ar;
    }
    peakL = pl; peakR = pr;
}

bool Engine::save(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(&proj, sizeof(proj), 1, f) == 1;
    fclose(f);
    return ok;
}

bool Engine::load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    Project* tmp = (Project*)malloc(sizeof(Project));
    bool ok = fread(tmp, sizeof(Project), 1, f) == 1 && memcmp(tmp->magic, "GBX1", 4) == 0 && tmp->version == 1;
    fclose(f);
    if (ok) memcpy(&proj, tmp, sizeof(Project));
    free(tmp);
    return ok;
}
