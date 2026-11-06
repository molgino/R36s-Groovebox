// Effekt-Busse: Delay (Ping-Pong), Tape-Delay (Wow/Flutter, Saettigung, Alterung), Reverb (8-Leitungs-FDN mit
// Diffusion und Modulation), Spring (Federhall: Dispersions-Allpaesse in einer Rueckkopplungsschleife), Chorus.
#include "fxbus.h"
#include "params.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static inline float fx_tanh(float x) {   // schnelle weiche Saettigung
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}
static inline float fx_sin(float p) { return sinf(6.2831853f * p); }

void FxBus::init(int sampleRate) {
    sr = sampleRate;
    free(bufL); free(bufR);
    bufLen = sr * 9 + 16;
    bufL = (float*)calloc(bufLen, sizeof(float));
    bufR = (float*)calloc(bufLen, sizeof(float));
    static const float fdnMs[8] = {31.7f, 37.9f, 41.3f, 45.7f, 53.9f, 61.1f, 68.3f, 74.9f};
    for (int k = 0; k < 8; k++) { free(fdn[k]); fdnLen[k] = (int)(fdnMs[k] * 1.6f * sr / 1000.f) + 64; fdn[k] = (float*)calloc(fdnLen[k], sizeof(float)); }
    static const float difMs[4] = {4.77f, 3.59f, 12.73f, 9.31f};
    for (int k = 0; k < 4; k++) { free(dif[k]); difLen[k] = (int)(difMs[k] * sr / 1000.f) + 1; dif[k] = (float*)calloc(difLen[k], sizeof(float)); }
    free(pre); preLen = sr / 4 + 2; pre = (float*)calloc(preLen, sizeof(float));
    sprLen = (int)(0.09f * sr) + 4;
    for (int k = 0; k < 2; k++) { free(spr[k]); spr[k] = (float*)calloc(sprLen, sizeof(float)); }
    type = -1;
    reset();
}

// Zustand leeren; die grossen Verzoegerungsspeicher nicht loeschen (zu teuer im Audio-Thread): `filled` sperrt alte Daten
void FxBus::reset() {
    pos = 0; filled = 0;
    for (int k = 0; k < 8; k++) { if (fdn[k]) memset(fdn[k], 0, fdnLen[k] * sizeof(float)); fdnPos[k] = 0; fdnLp[k] = 0; }
    for (int k = 0; k < 4; k++) { if (dif[k]) memset(dif[k], 0, difLen[k] * sizeof(float)); difPos[k] = 0; }
    if (pre) memset(pre, 0, preLen * sizeof(float));
    prePos = 0;
    for (int k = 0; k < 2; k++) { if (spr[k]) memset(spr[k], 0, sprLen * sizeof(float)); sprLp[k] = sprHp[k] = sprFb[k] = 0; for (int j = 0; j < 16; j++) apZ[k][j] = 0; }
    sprPos = 0;
    lpL = lpR = hpL = hpR = inHp = inHpX = 0;
    ph1 = ph2 = ph3 = 0; wowRnd = wowTarget = 0; dSm = 0; satL = satR = 0;
    quiet = 1 << 30; peak = 0;
}

// Lesen aus dem Ringspeicher d Samples zurueck (linear), nie Daten von vor dem letzten Leeren
static inline float rd(const float* b, int len, int pos, int filled, float d) {
    if (d >= (float)filled) return 0.f;
    float rp = pos - d; while (rp < 0) rp += len;
    int i0 = (int)rp; float fr = rp - i0; int i1 = i0 + 1 >= len ? 0 : i0 + 1;
    return b[i0] + (b[i1] - b[i0]) * fr;
}

void FxBus::process(const float* in, float* outL, float* outR, int n, const uint8_t* p, double stepSamples) {
    int t = p[BX_TYPE] % BF_COUNT;
    if (t != type) { reset(); type = t; }
    for (int i = 0; i < n; i++) outL[i] = outR[i] = 0.f;
    if (t == BF_OFF || !bufL) { peak = 0; return; }
    bool any = false;
    for (int i = 0; i < n; i++) if (in[i] != 0.f) { any = true; break; }
    // Ausklingzeit, nach der ein Bus ohne Eingang und ohne hoerbaren Ausgang pausieren darf
    double tSteps = DLY_STEPS[p[BX_P1] % 15] * stepSamples;
    int tail = (t == BF_DELAY || t == BF_TAPE) ? (int)tSteps + sr / 4 : (t == BF_CHORUS ? sr / 10 : sr / 2);
    if (!any && quiet > tail) { peak = 0; return; }
    const float lvl = p[BX_LVL] / 100.f;
    float pk = 0.f;
    switch (t) {
    case BF_DELAY: {
        float target = (float)tSteps; if (target > bufLen - 4) target = (float)(bufLen - 4); if (target < 2) target = 2;
        if (dSm < 1) dSm = target;
        float fb = p[BX_P2] / 127.f * 1.2f;                       // > 100 %: Saettigung haelt die Schleife stabil
        float tone = 0.05f + p[BX_P3] / 127.f * 0.95f;
        float wid = p[BX_P4] / 127.f;
        float hpc = 1.f - expf(-6.2831853f * (20.f * exp2f(p[BX_P5] / 127.f * 6.f)) / sr);
        for (int i = 0; i < n; i++) {
            dSm += (target - dSm) * 0.0004f;                       // Zeitwechsel gleitet wie beim Bandecho
            float a = rd(bufL, bufLen, pos, filled, dSm), b = rd(bufR, bufLen, pos, filled, dSm);
            lpL += (a - lpL) * tone; lpR += (b - lpR) * tone;
            hpL += (lpL - hpL) * hpc; hpR += (lpR - hpR) * hpc;
            float fL = lpL - hpL, fR = lpR - hpR;
            bufL[pos] = fx_tanh(in[i] + fR * fb);                  // Ping-Pong
            bufR[pos] = fx_tanh(fL * fb);
            if (++pos >= bufLen) pos = 0;
            if (filled < bufLen) filled++;
            float m = (fL + fR) * 0.5f;
            outL[i] = (m + (fL - m) * wid) * lvl; outR[i] = (m + (fR - m) * wid) * lvl;
        }
    } break;
    case BF_TAPE: {
        float target = (float)tSteps; if (target > bufLen - 600) target = (float)(bufLen - 600); if (target < 64) target = 64;
        if (dSm < 1) dSm = target;
        float fb = p[BX_P2] / 127.f * 1.15f;
        float wow = p[BX_P3] / 127.f, sat = p[BX_P4] / 127.f, age = p[BX_P5] / 127.f;
        float lpc = 1.f - expf(-6.2831853f * (11000.f * exp2f(-age * 2.6f)) / sr);    // 11 kHz .. 1,8 kHz
        float hpc = 1.f - expf(-6.2831853f * (40.f + age * 260.f) / sr);
        float drive = 1.f + sat * 4.f;
        float wIncS = 0.55f / sr, wIncF = 6.3f / sr;
        float wDepth = wow * 0.0045f * sr, fDepth = wow * 0.00035f * sr;
        for (int i = 0; i < n; i++) {
            dSm += (target - dSm) * 0.00025f;                      // Zeitwechsel: Tonhoehe gleitet (Bandmaschine)
            ph1 += wIncS; if (ph1 >= 1.f) { ph1 -= 1.f; rng = rng * 1664525u + 1013904223u; wowTarget = (rng >> 9) * (1.f / 8388608.f) - 1.f; }
            ph2 += wIncF; if (ph2 >= 1.f) ph2 -= 1.f;
            wowRnd += (wowTarget - wowRnd) * 0.00005f;
            float mod = wDepth * (0.6f * fx_sin(ph1) + 0.4f * wowRnd) + fDepth * fx_sin(ph2);
            float d = dSm + mod;
            float a = rd(bufL, bufLen, pos, filled, d), b = rd(bufL, bufLen, pos, filled, d * 1.011f + 23.f);   // zwei Tonkoepfe: Stereo
            lpL += (a - lpL) * lpc; lpR += (b - lpR) * lpc;
            hpL += (lpL - hpL) * hpc; hpR += (lpR - hpR) * hpc;
            float eL = lpL - hpL, eR = lpR - hpR;
            float w = in[i] + (eL + eR) * 0.5f * fb;
            bufL[pos] = fx_tanh(w * drive) / drive;                // Band saettigt weich (Kleinsignal unveraendert)
            if (++pos >= bufLen) pos = 0;
            if (filled < bufLen) filled++;
            rng = rng * 1664525u + 1013904223u;
            float hiss = ((rng >> 9) * (1.f / 8388608.f) - 1.f) * age * age * 0.0015f * (filled > (int)d ? 1.f : 0.f);
            outL[i] = (eL + hiss) * lvl; outR[i] = (eR + hiss) * lvl;
        }
    } break;
    case BF_REVERB: {
        float size = p[BX_P1] / 127.f, damp = p[BX_P2] / 127.f, wid = p[BX_P4] / 127.f, md = p[BX_P5] / 127.f;
        int pd = (int)(fxPreMs(p[BX_P3]) * sr / 1000.f); if (pd > preLen - 1) pd = preLen - 1;
        float scale = 0.35f + size * 1.2f;
        float rt60 = 0.25f * exp2f(size * 5.5f);                   // 0,25 s .. 11 s
        float g[8]; int len[8];
        static const float fdnMs[8] = {31.7f, 37.9f, 41.3f, 45.7f, 53.9f, 61.1f, 68.3f, 74.9f};
        for (int k = 0; k < 8; k++) {
            len[k] = (int)(fdnMs[k] * scale * sr / 1000.f); if (len[k] > fdnLen[k] - 40) len[k] = fdnLen[k] - 40;
            g[k] = powf(10.f, -3.f * len[k] / (rt60 * sr));
        }
        float dco = 0.08f + (1.f - damp) * 0.88f;                   // Daempfung: Tiefpass in jeder Leitung
        float mInc = 0.31f / sr, mDepth = md * 0.0011f * sr;
        for (int i = 0; i < n; i++) {
            float x = in[i];
            if (pd > 0) { pre[prePos] = x; int rp = prePos - pd; if (rp < 0) rp += preLen; x = pre[rp]; if (++prePos >= preLen) prePos = 0; }
            for (int k = 0; k < 4; k++) {   // Eingangs-Diffusion (Allpaesse)
                float z = dif[k][difPos[k]];
                float y = -0.62f * x + z;
                dif[k][difPos[k]] = x + 0.62f * y;
                if (++difPos[k] >= difLen[k]) difPos[k] = 0;
                x = y;
            }
            ph1 += mInc; if (ph1 >= 1.f) ph1 -= 1.f;
            float o[8];
            for (int k = 0; k < 8; k++) {
                float d = (float)len[k];
                if (k == 0) d += mDepth * fx_sin(ph1);
                else if (k == 5) d += mDepth * fx_sin(ph1 + 0.37f);
                float rp = fdnPos[k] - d; while (rp < 0) rp += fdnLen[k];
                int i0 = (int)rp; float fr = rp - i0; int i1 = i0 + 1 >= fdnLen[k] ? 0 : i0 + 1;
                float v = fdn[k][i0] + (fdn[k][i1] - fdn[k][i0]) * fr;
                fdnLp[k] += (v - fdnLp[k]) * dco;
                o[k] = fdnLp[k] * g[k];
            }
            // Hadamard-Matrix 8x8 (schnell), normiert
            for (int h = 1; h < 8; h <<= 1)
                for (int a = 0; a < 8; a += h << 1)
                    for (int b = a; b < a + h; b++) { float u = o[b], w = o[b + h]; o[b] = u + w; o[b + h] = u - w; }
            const float nrm = 0.35355339f;
            for (int k = 0; k < 8; k++) {
                fdn[k][fdnPos[k]] = o[k] * nrm + x * ((k & 1) ? -0.5f : 0.5f);
                if (++fdnPos[k] >= fdnLen[k]) fdnPos[k] = 0;
            }
            float l = (fdnLp[0] - fdnLp[2] + fdnLp[4] - fdnLp[6]) * 0.42f;
            float r = (fdnLp[1] - fdnLp[3] + fdnLp[5] - fdnLp[7]) * 0.42f;
            float m = (l + r) * 0.5f;
            outL[i] = (m + (l - m) * wid) * lvl; outR[i] = (m + (r - m) * wid) * lvl;
        }
    } break;
    case BF_SPRING: {
        float decay = 0.55f + p[BX_P1] / 127.f * 0.42f;             // Rueckkopplung der Feder
        float tone = 1.f - expf(-6.2831853f * (1800.f * exp2f(p[BX_P2] / 127.f * 2.4f)) / sr);
        float a = -0.45f - p[BX_P3] / 127.f * 0.45f;                // Dispersion: staerker = mehr "Drip"/Zirpen
        float wid = p[BX_P4] / 127.f;
        float hpc = 1.f - expf(-6.2831853f * (60.f * exp2f(p[BX_P5] / 127.f * 3.5f)) / sr);
        int dl[2] = {(int)(0.0371f * sr), (int)(0.0457f * sr)};
        for (int i = 0; i < n; i++) {
            float x0 = in[i];
            inHp += (x0 - inHp) * hpc; x0 -= inHp;                   // Federn uebertragen kaum Bass
            float s[2];
            for (int k = 0; k < 2; k++) {
                int rp = sprPos - dl[k]; if (rp < 0) rp += sprLen;
                float y = spr[k][rp];
                sprLp[k] += (y - sprLp[k]) * tone;
                float x = x0 + sprLp[k] * decay;
                for (int j = 0; j < 12; j++) {   // Kette erster Ordnung: verzoegert hohe Frequenzen -> "Boing"
                    float yy = a * x + apZ[k][j];
                    apZ[k][j] = x - a * yy;
                    x = yy;
                }
                spr[k][sprPos] = fx_tanh(x);
                s[k] = sprLp[k];
            }
            if (++sprPos >= sprLen) sprPos = 0;
            float l = s[0] * (0.5f + 0.5f * wid) + s[1] * (0.5f - 0.5f * wid);
            float r = s[1] * (0.5f + 0.5f * wid) + s[0] * (0.5f - 0.5f * wid);
            outL[i] = l * 0.9f * lvl; outR[i] = r * 0.9f * lvl;
        }
    } break;
    case BF_CHORUS: {
        float rate = 0.05f * exp2f(p[BX_P1] / 127.f * 7.f);         // 0,05 .. 6,4 Hz
        float depth = p[BX_P2] / 127.f * 0.0065f * sr, base = 0.011f * sr;
        int nv = (p[BX_P3] % 3) + 1;
        float wid = p[BX_P4] / 127.f, fb = p[BX_P5] / 127.f * 0.7f;
        float inc = rate / sr;
        for (int i = 0; i < n; i++) {
            ph1 += inc; if (ph1 >= 1.f) ph1 -= 1.f;
            float l = 0, r = 0;
            for (int k = 0; k < nv; k++) {
                float ofs = k / (float)nv;
                l += rd(bufL, bufLen, pos, filled, base + depth * (0.5f + 0.5f * fx_sin(ph1 + ofs)));
                r += rd(bufL, bufLen, pos, filled, base + depth * (0.5f + 0.5f * fx_sin(ph1 + ofs + 0.25f)));
            }
            l /= nv; r /= nv;
            bufL[pos] = in[i] + (l + r) * 0.5f * fb;
            if (++pos >= bufLen) pos = 0;
            if (filled < bufLen) filled++;
            float m = (l + r) * 0.5f;
            outL[i] = (m + (l - m) * wid) * lvl; outR[i] = (m + (r - m) * wid) * lvl;
        }
    } break;
    }
    for (int i = 0; i < n; i++) { float a = fabsf(outL[i]) + fabsf(outR[i]); if (a > pk) pk = a; }
    peak = pk;
    if (any || pk > 1e-5f) quiet = 0; else quiet += n;
}
