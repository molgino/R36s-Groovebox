#include "engine.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stddef.h>

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
    case M_SNARE: ph1 = ph2 = 0; denv = 1; nenv = 1; break;
    case M_HAT: denv = 1; break;
    case M_CLAP: clapLeft = P(P_S3) + 1; clapTimer = 0; clapTail = false; burst = 0; denv = 0; break;
    case M_FM: menv = 1; if (!wasActive) { ph1 = phM = 0; fbPrev = 0; } break;
    case M_SAMPLE: aenv.v = 0; break;
    default: break;
    }
    trigs = trigs + 1;
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
    case M_SAMPLE:
        if (smp) {
            float semis = (nn - 60.f) + (P(P_S1) - 24) + (P(P_S2) - 64) / 64.f;
            sinc = smp->rate / sr * exp2f(semis / 12.f) * (sRev ? -1.f : 1.f);
            float g = P(P_S6) / 64.f;
            sGain = g * g;
        }
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

    // Insert-Effekt
    insType = P(P_ITYP);
    float p1 = P(P_IP1) / 127.f, p2 = P(P_IP2) / 127.f, p3 = P(P_IP3) / 127.f;
    insMix = P(P_IMIX) / 127.f;
    insP2 = p2; insP3 = p3;
    switch (insType) {
    case INS_CHORUS: case INS_FLANGER: case INS_APAN:
        insRate = 0.05f * exp2f(p1 * 8.f);
        insFb = (p3 - 0.5f) * 1.8f;
        break;
    case INS_PHASER: {
        insRate = 0.05f * exp2f(p1 * 8.f);
        insPh += insRate * n / sr; if (insPh >= 1.f) insPh -= floorf(insPh);
        insFb = p3 * 0.85f;
        // zwei Allpass-Koeffizienten (L/R) aus dem LFO
        for (int c = 0; c < 2; c++) {
            float lv = sin01(insPh + c * 0.25f);
            float f = 300.f * exp2f(p2 * 3.5f * (0.5f + 0.5f * lv));
            float t = tanf(3.14159265f * fminf(f, sr * 0.45f) / sr);
            apA[c] = (t - 1.f) / (t + 1.f);
        }
    } break;
    case INS_DRIVE:
        odG = 1.f + p1 * p1 * 40.f;
        odCo = 1.f - expf(-6.2831853f * (400.f * exp2f(p2 * 5.f)) / sr);
        odLvl = (0.1f + p3 * 0.9f) * 0.4f;
        break;
    case INS_RING:
        insRate = 20.f * exp2f(p1 * 7.f) * exp2f((p2 - 0.5f) / 12.f * 2.f);
        break;
    default: break;
    }

    if (gate > 0) {
        gate -= n;
        if (gate <= 0) { gate = 0; if (gated()) noteOff(); }
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
    float bmax = 0.f;

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
        case M_SAMPLE: {
            if (sDone || !smp) break;
            int ip = (int)spos;
            if (ip < 0) ip = 0;
            if (ip >= smp->len) ip = smp->len - 1;
            float fr = (float)(spos - ip);
            float a = smp->data[ip], b = ip + 1 < smp->len ? smp->data[ip + 1] : a;
            s = (a + (b - a) * fr) * sGain;
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
        if (!bypassF) {
            float lo, bp, hp;
            filt.proc(s, lo, bp, hp);
            s = ftype == 0 ? lo : (ftype == 1 ? bp : hp);
        }
        s *= aenv.tick();
        if (drive > 0.f) s = fastTanh(s * (1.f + drive * 6.f)) * (1.f - drive * 0.3f);
        if (srrN > 1) { if (holdC <= 0) { hold = s; holdC = srrN; } holdC--; s = hold; }
        if (crushQ > 0.f) s = floorf(s * crushQ + 0.5f) / crushQ;

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
        dS[i] += sm * sendD;
        rS[i] += sm * sendR;
    }

    float pk = bmax * (gainL > gainR ? gainL : gainR);
    if (pk > peakHold) peakHold = pk;
    // Stimme beenden, wenn verklungen
    switch (machine) {
    case M_KICK: case M_HAT: if (denv < 1e-4f) active = false; break;
    case M_SNARE: if (denv < 1e-4f && nenv < 1e-4f) active = false; break;
    case M_CLAP: if (clapTail && denv < 1e-4f && burst < 1e-4f) active = false; break;
    case M_SAMPLE: if (sDone || aenv.st == 0) active = false; break;
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
    for (int i = 0; i <= 2048; i++) SINT[i] = sinf(i / 2048.f * 6.2831853f);
    initFx(sampleRate);
    for (int t = 0; t < NTRACKS; t++) {
        v[t].rng.s = 0x1234567u * (t + 1);
        midiNote[t] = liveNote[t] = skipStep[t] = -1;
        midiGate[t] = 0;
    }
    defaultProject();
}

void Engine::initFx(int sampleRate) {
    sr = sampleRate;
    free(dlyL); free(dlyR);
    dlyLen = sr * 4 + 16;
    dlyL = (float*)calloc(dlyLen, sizeof(float));
    dlyR = (float*)calloc(dlyLen, sizeof(float));
    dlyPos = 0;
    free(preBuf);
    preLen = sr / 4 + 2;
    preBuf = (float*)calloc(preLen, sizeof(float));
    prePos = 0;
    for (int k = 0; k < 8; k++) { free(rev.cl[k].b); free(rev.cr[k].b); rev.cl[k] = Comb(); rev.cr[k] = Comb(); }
    for (int k = 0; k < 4; k++) { free(rev.al[k].b); free(rev.ar[k].b); rev.al[k] = Allpass(); rev.ar[k] = Allpass(); }
    rev.init(sr);
}

void Engine::setRate(int sampleRate) {
    if (sampleRate == sr) return;
    double ratio = (double)sampleRate / sr;
    clock *= ratio; nextStepAt *= ratio; nextTickAt *= ratio; stepStartClock *= ratio;
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
    Pattern& P = proj.pat[curPat];
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
        if (clock - stepStartClock > curStepLen * 0.5) { s = (s + 1) % len; skipStep[t] = s; }
    } else {
        s = uiCursor % len;
        stepRecCount++;
    }
    Step& st = P.s[t][s];
    if (!st.on) { memset(st.lock, NO_LOCK, NPARAMS); st.on = 1; }
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
    if (tickCount % 6 == 0) {
        stepStartClock = clock;
        curStepLen = stepSamples();
        doStep();
    }
    tickCount++;
}

void Engine::midiIn(uint8_t st, uint8_t d1, uint8_t d2) {
    if (st == 0xF8) { if (clockMode == 2) clockTick(); return; }
    if (st == 0xFA || st == 0xFB) {
        if (clockMode != 2) return;
        if (st == 0xFA) { for (int t = 0; t < NTRACKS; t++) trackPos[t] = 0; globalStep = 0; tickCount = 0; }
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
        if (d1 < 70 || d1 > 77 || uiPage >= PAGE_FX) return;   // CC 70-77 = Regler 1-8
        Pattern& P = proj.pat[curPat];
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
    static const int defM[NTRACKS] = {M_KICK, M_SNARE, M_HAT, M_CLAP, M_SYNTH, M_SYNTH, M_FM, M_SAMPLE};
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
    p.p[7][P_S0] = 11; p.p[7][P_DLY] = 35; p.p[7][P_REV] = 45;   // Sample (Start-Paket: keys/stab-minor)
}

void Engine::defaultProject() {
    memset(&proj, 0, sizeof(proj));
    memcpy(proj.magic, "GBX1", 4);
    proj.version = 3;
    proj.bpm = 120;
    for (int i = 0; i < 8; i++) proj.mst[i] = MST_DEFS[i].def;
    for (int i = 0; i < 8; i++) proj.cmp[i] = CMP_DEFS[i].def;
    for (int i = 0; i < 8; i++) proj.fx[i] = FX_DEFS[i].def;
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
    p.s[7][6].on = 1; p.s[7][14].on = 1; p.s[7][14].lock[P_NOTE] = 63;
}

void Engine::start() {
    for (int t = 0; t < NTRACKS; t++) { trackPos[t] = 0; skipStep[t] = -1; }
    if (nextPat >= 0) { curPat = nextPat; nextPat = -1; }
    globalStep = 0;
    tickCount = 0;
    nextStepAt = clock;
    if (clockMode == 1) sendStart = true;
    if (clockMode == 2) return;   // externer Takt startet per MIDI-Start
    playing = true;
}

void Engine::stop() {
    playing = false;
    for (int t = 0; t < NTRACKS; t++) { if (v[t].gated()) v[t].noteOff(); rt[t].left = 0; }
    ticksLeft = 0;
    sendStop = true;
}

void Engine::preview(int t, const Step* s) {
    trigger(t, (s && s->on) ? s->lock : nullptr, 1.f, false, false);
}

void Engine::trigger(int t, const uint8_t* lk, float velScale, bool fromSeq, bool toMidi) {
    Pattern& P = proj.pat[curPat];
    const uint8_t* b = P.p[t];
    auto val = [&](int i) { return (lk && lk[i] != NO_LOCK) ? lk[i] : b[i]; };
    float ls = lenSteps(val(P_LEN));
    int gs = ls < 0 ? -1 : (int)(ls * stepSamples()) + 1;
    v[t].noteOn(P.machine[t], b, lk, velScale, gs, (float)sr);
    if (toMidi && notesOut && midiOut) {
        if (midiNote[t] >= 0) emit(0x80 | t, (uint8_t)midiNote[t], 0, curOff);
        int nn = val(P_NOTE);
        int vv = (int)(val(P_VEL) * velScale + 0.5f);
        emit(0x90 | t, (uint8_t)nn, (uint8_t)(vv < 1 ? 1 : (vv > 127 ? 127 : vv)), curOff);
        midiNote[t] = nn;
        midiGate[t] = gs > 0 ? gs : (int)stepSamples();
    }
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
        bool skip = skipStep[t] == s;
        if (skip) skipStep[t] = -1;
        if (st.on && !mute[t] && !skip) {
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
        curOff = i;
        if (sendStop) { sendStop = false; allNotesOff(); if (clockMode == 1) emit(0xFC, 0, 0, i); }
        if (sendStart) { sendStart = false; if (clockMode == 1) emit(0xFA, 0, 0, i); }
        if (playing && clockMode != 2) {
            int guard = 0;
            while (clock >= nextStepAt && guard++ < 4) {
                stepStartClock = nextStepAt;
                doStep();
                double S = stepSamples();
                double sw = 0.5 + proj.mst[MS_SWNG] / 100.0;
                double d = (globalStep & 1) ? 2.0 * S * sw : 2.0 * S * (1.0 - sw);
                nextStepAt += d;
                curStepLen = d;
                if (clockMode == 1) { ticksLeft = 6; tickInt = d / 6.0; nextTickAt = stepStartClock; }
            }
        }
        while (ticksLeft > 0 && nextTickAt < clock + m) {
            double o = nextTickAt - clock;
            emit(0xF8, 0, 0, i + (o > 0 ? (int)o : 0));
            nextTickAt += tickInt;
            ticksLeft--;
        }
        for (int t = 0; t < NTRACKS; t++) {
            if (midiNote[t] >= 0 && midiGate[t] > 0) {
                midiGate[t] -= m;
                if (midiGate[t] <= 0) { emit(0x80 | t, (uint8_t)midiNote[t], 0, i); midiNote[t] = -1; }
            }
        }
        for (int t = 0; t < NTRACKS; t++) {
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
    float L[16] = {0}, R[16] = {0}, D[16] = {0}, RV[16] = {0}, KL[16] = {0}, KR[16] = {0};
    int key = (int)proj.cmp[CM_SRC] - 1;   // -1 = intern (Summe), sonst Sidechain-Spur
    for (int t = 0; t < NTRACKS; t++) {
        if (!v[t].active) continue;
        v[t].update(n, (float)sr, proj.bpm);
        if (t == key) v[t].render(KL, KR, D, RV, n, (float)sr);
        else v[t].render(L, R, D, RV, n, (float)sr);
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
    int dlen = (int)(DLY_STEPS[proj.fx[FX_DTIM] & 7] * stepSamples());
    if (dlen > dlyLen - 1) dlen = dlyLen - 1;
    if (dlen < 1) dlen = 1;
    float fb = proj.fx[FX_DFBK] / 127.f * 0.95f;
    float dtone = 0.05f + proj.fx[FX_DTON] / 127.f * 0.95f;
    float dwid = proj.fx[FX_DWID] / 127.f, rwid = proj.fx[FX_RWID] / 127.f;
    float rsize = proj.fx[FX_RSIZ] / 127.f, rdamp = proj.fx[FX_RDMP] / 127.f;
    int pre = (int)(fxPreMs(proj.fx[FX_RPRE]) * sr / 1000.f);
    if (pre > preLen - 1) pre = preLen - 1;
    float mv = proj.mst[MS_VOL] / 100.f; mv *= mv;
    float mdrv = proj.mst[MS_DRIV] / 127.f;
    float outG = 0.6f * mv * (1.f + 3.f * mdrv), outC = 1.f - 0.25f * mdrv;

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
        // Delay-Stereobreite: 0 = mono, 127 = volles Ping-Pong
        float dm = (dlpL + dlpR) * 0.5f;
        float wdL = dm + (dlpL - dm) * dwid, wdR = dm + (dlpR - dm) * dwid;
        // Reverb mit Predelay
        float rin = RV[i] + (dlpL + dlpR) * 0.15f;
        if (pre > 0) {
            int rp2 = prePos - pre; if (rp2 < 0) rp2 += preLen;
            preBuf[prePos] = rin; rin = preBuf[rp2];
            if (++prePos >= preLen) prePos = 0;
        }
        float rl, rr;
        rev.process(rin, rsize, rdamp, rl, rr);
        float rm = (rl + rr) * 0.5f;
        rl = rm + (rl - rm) * rwid; rr = rm + (rr - rm) * rwid;
        float oL = L[i] + wdL + rl, oR = R[i] + wdR + rr;
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
        oL = fastTanh(oL * outG) * outC;
        oR = fastTanh(oR * outG) * outC;
        out[2 * i] = oL; out[2 * i + 1] = oR;
        float al = fabsf(oL), ar = fabsf(oR);
        if (al > pl) pl = al;
        if (ar > pr) pr = ar;
    }
    peakL = pl; peakR = pr;
    if (grMax > grPeak) grPeak = grMax;
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
    // alte Formate (Version 1/2): 40 Parameter pro Spur, Pattern = 5584 Bytes ab Offset 20
    const long OLD_NP = 40, OLD_PAT = 16 + 8 * OLD_NP + NTRACKS * NSTEPS * (1 + OLD_NP);
    const long V1 = 20 + NPATTERNS * OLD_PAT, V2 = V1 + 16;
    if (ok && ver == 3 && size == (long)sizeof(Project)) {
        memcpy(&proj, buf, sizeof(Project));
    } else if (ok && ((ver == 1 && size == V1) || (ver == 2 && size == V2))) {
        Project* np = (Project*)malloc(sizeof(Project));
        memset(np, 0, sizeof(Project));
        memcpy(np->magic, "GBX1", 4);
        np->version = 3;
        memcpy(&np->bpm, buf + 8, 2);
        memcpy(np->mst, buf + 10, 8);
        for (int pi = 0; pi < NPATTERNS; pi++) {
            const uint8_t* q = buf + 20 + pi * OLD_PAT;
            Pattern& P = np->pat[pi];
            memcpy(P.machine, q, 8);
            memcpy(P.len, q + 8, 8);
            for (int t = 0; t < NTRACKS; t++) {
                memcpy(P.p[t], q + 16 + t * OLD_NP, OLD_NP);
                for (int i = OLD_NP; i < NPARAMS; i++) P.p[t][i] = COMMON_DEFS[i].def;
                for (int st = 0; st < NSTEPS; st++) {
                    const uint8_t* sp = q + 16 + 8 * OLD_NP + (t * NSTEPS + st) * (1 + OLD_NP);
                    P.s[t][st].on = sp[0];
                    memcpy(P.s[t][st].lock, sp + 1, OLD_NP);
                    memset(P.s[t][st].lock + OLD_NP, NO_LOCK, NPARAMS - OLD_NP);
                }
            }
        }
        if (ver == 2) {
            memcpy(np->cmp, buf + V1, 8);
            memcpy(np->fx, buf + V1 + 8, 8);
        } else {   // Version 1: Effekte lagen in mst
            uint8_t old[8];
            memcpy(old, np->mst, 8);   // BPM SWNG DTIM DFBK DTON RSIZ RDMP VOL
            for (int i = 0; i < 8; i++) { np->cmp[i] = CMP_DEFS[i].def; np->fx[i] = FX_DEFS[i].def; np->mst[i] = MST_DEFS[i].def; }
            np->fx[FX_DTIM] = old[2]; np->fx[FX_DFBK] = old[3]; np->fx[FX_DTON] = old[4];
            np->fx[FX_RSIZ] = old[5]; np->fx[FX_RDMP] = old[6];
            np->mst[MS_SWNG] = old[1]; np->mst[MS_VOL] = old[7];
        }
        memcpy(&proj, np, sizeof(Project));
        free(np);
        printf("Projekt von Version %u auf 3 umgestellt\n", ver);
    } else {
        ok = false;
    }
    free(buf);
    return ok;
}
