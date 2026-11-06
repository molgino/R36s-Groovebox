// Animationen fuer Flaechen, die sonst leer waeren: Oszilloskop/Spektrum der Summe, Grafiken fuer Seiten ohne
// eigene Engine-Grafik (TRIG, OSC, INSERT, SENDS, Effekt-Busse), Fuellungen fuer leere Regler, Einstellungen,
// Song, LIVE und Projekt. Eingebunden in main.cpp vor drawPageGraph (braucht pv, flash, meterV, Zeichenfunktionen).

// ---- Mitschnitt der Summe (Audio-Thread schreibt, Oberflaeche liest; Reissen einzelner Werte ist egal)
enum { SCOPE_N = 4096 };
static float scopeBuf[SCOPE_N];
static volatile unsigned scopePos = 0;
static void scopePush(const float* st, int frames) {
    unsigned p = scopePos;
    for (int i = 0; i < frames; i++) scopeBuf[p++ & (SCOPE_N - 1)] = (st[2 * i] + st[2 * i + 1]) * 0.5f;
    scopePos = p;
}
static float scopeGain = 4.f;   // automatische Verstaerkung der Anzeige (leise Signale bleiben sichtbar)

// Oszilloskop mit Trigger auf steigenden Nulldurchgang; ohne Signal atmet eine ruhige Linie
static void drawScope(int x, int y, int w, int h, Col c, int step = 2, bool glow = true) {
    unsigned p = scopePos;
    int need = w * step + 700;
    unsigned st = p - (unsigned)need;
    float pk = 0.f;
    for (int i = 0; i < w * step + 600; i += 4) { float a = fabsf(scopeBuf[(st + i) & (SCOPE_N - 1)]); if (a > pk) pk = a; }
    float tg = pk > 1e-4f ? 0.85f / pk : 4.f; if (tg > 40.f) tg = 40.f;
    scopeGain += (tg - scopeGain) * 0.15f;
    int trig = 0;
    for (int i = 1; i < 600; i++) {
        float a = scopeBuf[(st + i - 1) & (SCOPE_N - 1)], b = scopeBuf[(st + i) & (SCOPE_N - 1)];
        if (a <= 0.f && b > 0.f) { trig = i; break; }
    }
    int cy = y + h / 2, py = cy;
    bool silent = pk < 1e-4f;
    for (int i = 0; i < w; i++) {
        float v = silent ? 0.45f * sinf(i * 0.11f - animTime * 4.f) * (0.35f + 0.65f * fabsf(sinf(animTime * 0.7f + i * 0.013f)))
                         : scopeBuf[(st + trig + i * step) & (SCOPE_N - 1)] * scopeGain;
        if (v > 1.f) v = 1.f;
        if (v < -1.f) v = -1.f;
        int yy = cy - (int)(v * (h / 2 - 2));
        if (i) {
            if (glow) line(x + i - 1, py, x + i, yy, dimC(c, 0.35f), 4);
            line(x + i - 1, py, x + i, yy, c, 1);
        }
        py = yy;
    }
}

// Spektrum in Baendern (Goertzel ueber die letzten 1024 Werte, einmal pro Bild berechnet)
enum { SPEC_N = 24 };
static float specVal[SPEC_N], specHold[SPEC_N];
static Uint32 specFrame = 0;
static void specUpdate() {
    Uint32 f = SDL_GetTicks() / 30;
    if (f == specFrame) return;
    specFrame = f;
    unsigned p = scopePos - 1024;
    for (int b = 0; b < SPEC_N; b++) {
        float fr = 60.f * powf(12000.f / 60.f, b / (float)(SPEC_N - 1));
        float co = 2.f * cosf(6.2831853f * fr / 44100.f), s1 = 0, s2 = 0;
        for (int i = 0; i < 1024; i += 2) { float s0 = scopeBuf[(p + i) & (SCOPE_N - 1)] + co * s1 - s2; s2 = s1; s1 = s0; }
        float mag = sqrtf(fmaxf(0.f, s1 * s1 + s2 * s2 - co * s1 * s2)) / 256.f;
        float db = 20.f * log10f(mag + 1e-5f);
        float v = (db + 60.f) / 60.f; if (v < 0) v = 0; if (v > 1) v = 1;
        specVal[b] = v > specVal[b] ? v : specVal[b] * 0.82f + v * 0.18f;
        specHold[b] = specVal[b] > specHold[b] ? specVal[b] : specHold[b] - 0.012f;
    }
}
static void drawSpectrum(int x, int y, int w, int h, Col c, int bands = SPEC_N) {
    specUpdate();
    int bw = w / bands;
    for (int b = 0; b < bands; b++) {
        int si = b * SPEC_N / bands;
        float idle = 0.12f + 0.22f * (0.5f + 0.5f * sinf(animTime * 2.3f + b * 0.55f)) * (0.6f + 0.4f * sinf(animTime * 0.9f - b * 0.3f));   // Ruhe: sanfte Welle
        float v = fmaxf(specVal[si], idle);
        int hh = (int)(v * (h - 4)), bx = x + b * bw;
        for (int s = 0; s < hh; s += 4) fill(bx + 1, y + h - 2 - s - 3, bw - 2, 3, mix(dimC(c, 0.5f), c, s / (float)h));
        int hy = y + h - 2 - (int)(specHold[si] * (h - 4));
        fill(bx + 1, hy, bw - 2, 1, C_PLAY);
    }
}

// Pulsierende Ringe im Takt (Viertel), Farbe der Spur
static void drawBeatRings(int x, int y, int w, int h, Col c) {
    float beat = eng.playing ? fmodf(animTime * eng.proj.bpm / 60.f, 1.f) : fmodf(animTime * 0.35f, 1.f);
    int cx = x + w / 2, cy = y + h / 2, rm = (h < w ? h : w) / 2 - 2;
    for (int k = 0; k < 3; k++) {
        float t = fmodf(beat + k / 3.f, 1.f);
        int r = 2 + (int)(t * rm);
        ring(cx, cy, r, 1, dimC(c, (1.f - t) * 0.9f));
    }
    disc(cx, cy, 3 + (int)((1.f - beat) * 3.f), c);
}

// Fuellung fuer leere Felder: abwechselnd Oszilloskop, Spektrum, Takt-Ringe (gedimmt)
static void filler(int x, int y, int w, int h, Col c, int seed) {
    Col dc = dimC(c, 0.55f);
    switch (seed % 3) {
    case 0: drawScope(x + 6, y + 4, w - 12, h - 8, dc, 3, false); break;
    case 1: drawSpectrum(x + 6, y + 4, w - 12, h - 8, dc, (w - 12) / 8 > SPEC_N ? SPEC_N : (w - 12) / 8); break;
    default: drawBeatRings(x, y, w, h, dc); break;
    }
}

// ---- TRIG: Piano-Roll der Spur (aktuelle 16 Steps), Lauflicht, Anschlag, Laenge, Wahrscheinlichkeit
static void trigGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    const Pattern& P = curP();
    int len = P.len[track] < 1 ? 1 : P.len[track], base = (cursor / 16) * 16;
    int lo = 127, hi = 0;
    for (int s = 0; s < 16; s++) {
        const Step& st = P.s[track][base + s];
        if (base + s >= len || st.on != 1) continue;
        int n = st.lock[P_NOTE] != NO_LOCK ? st.lock[P_NOTE] : P.p[track][P_NOTE];
        if (n < lo) lo = n;
        if (n > hi) hi = n;
    }
    if (lo > hi) { lo = P.p[track][P_NOTE] - 6; hi = lo + 12; }
    if (hi - lo < 7) { int m = (hi + lo) / 2; lo = m - 3; hi = m + 4; }
    int x0 = gx + 34, W = gw - 44, top = gy + 6, H = gh - 34, vy = gy + gh - 24;   // unten: Anschlag-Spur
    float cw = W / 16.f;
    // Klaviatur links
    for (int n = lo; n <= hi; n++) {
        int y = top + H - (int)((n - lo + 1) * H / (float)(hi - lo + 1));
        int hh = H / (hi - lo + 1); if (hh < 1) hh = 1;
        bool blk = n % 12 == 1 || n % 12 == 3 || n % 12 == 6 || n % 12 == 8 || n % 12 == 10;
        fill(gx + 8, y, 20, hh - (hh > 2), blk ? C_OFF2 : C_DIM);
    }
    for (int s = 0; s <= 16; s += 4) fill(x0 + (int)(s * cw), top, 1, H + 26, C_SEL);
    fill(x0, vy + 19, W, 1, C_OFF2);
    text(gx + 6, vy + 2, "VEL", C_DIM);
    fill(x0 + (int)((cursor % 16) * cw), top, (int)cw, H, C_SEL);   // Cursor-Spalte
    int ps = eng.playing ? eng.playedStep[track] : -1;
    for (int s = 0; s < 16; s++) {
        const Step& st = P.s[track][base + s];
        if (base + s >= len || st.on == 0) continue;
        auto val = [&](int i) { return st.lock[i] != NO_LOCK ? st.lock[i] : P.p[track][i]; };
        int n = val(P_NOTE), vel = val(P_VEL), prob = val(P_PROB);
        float ls = lenSteps(val(P_LEN)); if (ls < 0 || ls > 16 - s) ls = (float)(16 - s);
        int y = top + H - (int)((n - lo + 1) * H / (float)(hi - lo + 1)), hh = H / (hi - lo + 1); if (hh < 9) hh = 9; if (y + hh > top + H) y = top + H - hh;
        {   // Anschlag als Saeule unten
            int vh = vel * 18 / 127; Col vc2 = st.on == 2 ? dimC(c, 0.3f) : dimC(c, 0.5f + 0.5f * vel / 127.f);
            fill(x0 + (int)(s * cw) + 2, vy + 18 - vh, (int)cw - 4, vh > 1 ? vh : 1, base + s == ps ? C_PLAY : vc2);
        }
        int x = x0 + (int)(s * cw) + (int)((val(P_MTIM) - 23) / 24.f * cw), w = (int)(ls * cw) - 2; if (w < 4) w = 4;
        bool playing = base + s == ps;
        Col nc = mix(dimC(c, 0.35f + 0.65f * vel / 127.f), C_PLAY, playing ? flash[track] * 0.8f : 0.f);
        if (st.on == 2) { rrect(x, y, w, hh, 3, dimC(c, 0.3f)); continue; }   // trigless
        rrect(x, y, w, hh, 3, nc);
        if (w >= 22 && hh >= 9) { char nb[8]; fmtParam(COMMON_DEFS[P_NOTE], quantizeNote(n, curScale()), nb, sizeof nb); if (textW(nb) < w - 2) text(x + 2, y + hh / 2 - 8, nb, C_BG); }
        if (prob < 100) for (int i = 0; i < w; i += 4) fill(x + i, y, 2, 1, C_BG);   // Wahrscheinlichkeit: gestrichelt
        int rt = val(P_RTRG); if (rt) { int nn = RTRG_DIV[rt % 7]; for (int i = 1; i < nn && i < 8; i++) fill(x + i * w / nn, y, 1, hh, C_BG); }
        if (playing) for (int g = 1; g <= 3; g++) rrect(x - g, y - g, w + 2 * g, hh + 2 * g, 4, c, (Uint8)(90 / g));
    }
    if (ps >= base && ps < base + 16) {   // Lauflicht
        int px = x0 + (int)((ps - base) * cw + cw * 0.5f);
        fill(px, top, 2, H, C_PLAY, 150);
    }
}

// ---- SYNTH Seite OSC: beide Oszillatoren (Oktave, Pegel), Sub, Ring/X-Mod/Sync, Summe laeuft durch
static void oscPageGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int w1 = pv(P_S0) & 3, w2 = pv(P_S1) & 3;
    float m1 = exp2f((float)(pv(P_X0) - 3)), m2 = exp2f((float)(pv(P_X1) - 3)) * exp2f((pv(P_S2) - 64) / 64.f * 0.5f / 12.f);
    float lv1 = pv(P_X2) / 127.f, lv2 = pv(P_X3) / 127.f, mixv = pv(P_S3) / 127.f, ring = pv(P_X7) / 127.f, xm = pv(P_X6) / 127.f;
    bool sync = pv(P_X5) != 0;
    float sub = pv(P_S4) / 127.f, subDiv = pv(P_X4) ? 0.25f : 0.5f;
    int x0 = gx + 8, W = gw - 16, lh = (gh - 12) / 3;
    float scroll = animTime * 0.25f;
    Col c1 = mix(c, C_CYAN, 0.3f), c2 = mix(c, C_ACC, 0.4f);
    int py1 = 0, py2 = 0, py3 = 0;
    for (int i = 0; i <= W; i++) {
        float t = i / (float)W * 2.f + scroll;   // zwei Grundschwingungen
        float p1 = t * m1, p2 = t * m2;
        if (sync) p2 = (p1 - floorf(p1)) * (m2 / m1);
        float o2 = symWave(w2, p2) * 2.f - 1.f;
        float o1 = symWave(w1, p1 + o2 * xm * 0.5f) * 2.f - 1.f;
        float s = o1 * (1.f - mixv) * lv1 + o2 * mixv * lv2 + ring * o1 * o2 * 0.8f + sub * (fmodf(t * subDiv, 1.f) < 0.5f ? 0.8f : -0.8f);
        s = tanhf(s * 0.9f);
        int y1 = gy + 4 + lh / 2 - (int)(o1 * lv1 * (lh / 2 - 2)), y2 = gy + 4 + lh + lh / 2 - (int)(o2 * lv2 * (lh / 2 - 2));
        int y3 = gy + 8 + 2 * lh + lh / 2 - (int)(s * (lh / 2 + 2));
        if (i) { line(x0 + i - 1, py1, x0 + i, y1, dimC(c1, 0.8f)); line(x0 + i - 1, py2, x0 + i, y2, dimC(c2, 0.8f)); line(x0 + i - 1, py3, x0 + i, y3, c, 2); }
        py1 = y1; py2 = y2; py3 = y3;
    }
    char b[48];
    snprintf(b, sizeof b, "OSC1 %+d", pv(P_X0) - 3); text(gx + gw - 8 - textW(b), gy + 4, b, c1);
    snprintf(b, sizeof b, "OSC2 %+d%s", pv(P_X1) - 3, sync ? " SYNC" : ""); text(gx + gw - 8 - textW(b), gy + 4 + lh, b, c2);
    text(gx + gw - 8 - textW(TR("SUMME", "SUM")), gy + 8 + 2 * lh, TR("SUMME", "SUM"), c);
}

// ---- WTBL Seite OSC: die Tabelle als 3D-Stapel, aktuelle Position leuchtet, Abtastlinie laeuft
static void wtStackGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int tab = pv(P_S0) % 5, pos = pv(P_S1);
    const Sample* us = tab == 4 ? sampleGet(pv(P_X3)) : nullptr;
    int nf = 12, x0 = gx + 20, W = gw - 120, H = gh - 30;
    int cur = pos * (nf - 1) / 127;
    float scan = fmodf(animTime * 0.6f, 1.f);
    for (int f = nf - 1; f >= 0; f--) {
        int ox = x0 + f * 7, oy = gy + gh - 14 - H / 2 - f * 4;
        bool hl = f == cur;
        Col fc = hl ? C_PLAY : dimC(c, 0.25f + 0.5f * (1.f - f / (float)nf));
        int py = oy;
        for (int i = 0; i <= W; i += 2) {
            float ph = i / (float)W, v;
            if (us && us->data && us->len >= 16) {
                int F = us->len >= 2048 && us->len % 2048 == 0 ? 2048 : (us->len % 256 == 0 ? 256 : us->len), n = us->len / F;
                int fr2 = n > 1 ? f * (n - 1) / (nf - 1) : 0;
                v = us->data[(long)fr2 * F + (int)(ph * (F - 1))];
            } else v = wavetableFrame(tab & 3, f * 31 / (nf - 1))[(int)(ph * 255)];
            int y = oy - (int)(v * H / 2.4f);
            if (i) line(ox + i - 2, py, ox + i, y, fc, hl ? 2 : 1);
            py = y;
        }
        if (hl) { int sx = ox + (int)(scan * W); fill(sx, oy - H / 2, 2, H, C_PLAY, 120); }
    }
    char b[40];
    const char* tn[5] = {"BASIC", "PWM", TR("VOKAL", "VOWEL"), TR("OBERT", "HARM"), "USER"};
    snprintf(b, sizeof b, "%s  POS %d", tn[tab], pos); text(gx + gw - 8 - textW(b), gy + 6, b, c);
    if (us) { const char* nm = sampleName(pv(P_X3)); const char* sl = strrchr(nm, '/'); text(gx + gw - 8 - textW(sl ? sl + 1 : nm), gy + 24, sl ? sl + 1 : nm, C_DIM); }
}

// ---- INSERT: Animation je Effekt
static void insGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int ty = pv(P_ITYP) % INS_COUNT;
    float p1 = pv(P_IP1) / 127.f, p2 = pv(P_IP2) / 127.f, p3 = pv(P_IP3) / 127.f, mixv = pv(P_IMIX) / 127.f;
    int x0 = gx + 10, W = gw - 20, cy = gy + gh / 2, A = gh / 2 - 8;
    float t = animTime;
    Col cd = dimC(c, 0.4f);
    switch (ty) {
    case INS_CHORUS: case INS_FLANGER: {
        float rate = 0.3f + p1 * 3.f;
        for (int v = 0; v < 3; v++) {
            int py = cy;
            for (int i = 0; i <= W; i += 2) {
                float d = sinf(t * rate + v * 2.1f) * p2 * 0.25f;
                float y = sinf((i / (float)W) * 6.2831853f * 3.f + d * 6.f + v * 0.3f);
                int yy = cy - (int)(y * A * (v ? 0.7f : 1.f));
                if (i) line(x0 + i - 2, py, x0 + i, yy, v ? cd : c, v ? 1 : 2);
                py = yy;
            }
        }
    } break;
    case INS_PHASER: {   // Frequenzgang mit wandernden Kerben
        float sweep = 0.5f + 0.4f * sinf(t * (0.3f + p1 * 2.f)) * (0.3f + p2);
        int py = cy;
        for (int i = 0; i <= W; i++) {
            float f = i / (float)W, g = 1.f;
            for (int k = 0; k < 3; k++) { float nf = sweep * (0.4f + k * 0.35f); g *= 1.f - (0.5f + p3 * 0.45f) * expf(-(f - nf) * (f - nf) * 900.f); }
            int yy = gy + gh - 8 - (int)(g * (gh - 20));
            if (i) line(x0 + i - 1, py, x0 + i, yy, c, 2);
            py = yy;
        }
    } break;
    case INS_DRIVE: {   // Kennlinie + Sinus davor (dunkel) und danach (hell)
        float g = 1.f + p1 * 9.f;
        int bx = gx + gw - 110, bs = gh - 16;
        rrect(bx, gy + 8, bs, bs, 4, C_BG);
        int py = 0;
        for (int i = 0; i <= bs; i++) { float xin = i / (float)bs * 2.f - 1.f; int yy = gy + 8 + bs / 2 - (int)(tanhf(xin * g) / tanhf(g) * bs / 2); if (i) line(bx + i - 1, py, bx + i, yy, c, 2); py = yy; }
        float ph = fmodf(t * 0.5f, 1.f); float xin = sinf(ph * 6.2831853f);
        disc(bx + (int)((xin + 1) * bs / 2), gy + 8 + bs / 2 - (int)(tanhf(xin * g) / tanhf(g) * bs / 2), 4, C_PLAY);
        int py1 = cy, py2 = cy, W2 = bx - x0 - 10;
        for (int i = 0; i <= W2; i += 2) {
            float s = sinf((i / (float)W2) * 6.2831853f * 2.f + t * 2.f);
            int y1 = cy - (int)(s * A * 0.6f), y2 = cy - (int)(tanhf(s * g) / tanhf(g) * A);
            if (i) { line(x0 + i - 2, py1, x0 + i, y1, cd); line(x0 + i - 2, py2, x0 + i, y2, c, 2); }
            py1 = y1; py2 = y2;
        }
    } break;
    case INS_RING: {
        float cf = 2.f + p1 * 14.f;
        int py = cy;
        for (int i = 0; i <= W; i++) {
            float f = i / (float)W;
            float s = sinf(f * 6.2831853f * 2.f + t) * sinf(f * 6.2831853f * cf + t * 3.f);
            int yy = cy - (int)(s * A);
            if (i) line(x0 + i - 1, py, x0 + i, yy, c, 2);
            py = yy;
        }
    } break;
    case INS_APAN: {
        float ph = sinf(t * (0.5f + p1 * 6.f)) * (0.2f + p2 * 0.8f);
        text(x0 + 4, cy - 8, "L", C_DIM); text(gx + gw - 18, cy - 8, "R", C_DIM);
        fill(x0 + 20, cy, W - 40, 1, C_SEL);
        int bx = x0 + W / 2 + (int)(ph * (W / 2 - 30));
        for (int g = 3; g >= 1; g--) disc(bx, cy, 8 + g * 4, dimC(c, 0.2f * (4 - g)));
        disc(bx, cy, 8, c);
    } break;
    default: {   // AUS: Signal fliesst ungehindert durch
        fill(x0, cy, W, 2, C_SEL);
        for (int k = 0; k < 6; k++) { float f = fmodf(t * 0.35f + k / 6.f, 1.f); disc(x0 + (int)(f * W), cy, 3, dimC(c, 0.7f)); }
        textC(gx + gw / 2, cy - 22, TR("KEIN INSERT-EFFEKT (TYPE WÄHLEN)", "NO INSERT EFFECT (CHOOSE TYPE)"), C_DIM);
    } break;
    }
    if (ty) { char b[24]; snprintf(b, sizeof b, "%s  MIX %d%%", INS_NAMES[ty], (int)(mixv * 100)); text(gx + 8, gy + 4, b, c); }
}

// ---- SENDS: Spur schickt Teilchen zu Bus A/B/C (Dichte = Send, Helligkeit = Pegel der Spur)
static void sendGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int sx = gx + 70, sy = gy + gh / 2;
    rrect(gx + 10, sy - 18, 76, 36, 8, dimC(c, 0.5f + 0.5f * flash[track]));
    char b[32]; snprintf(b, sizeof b, TR("SPUR %d", "TRACK %d"), track + 1); textC(gx + 48, sy - 8, b, C_BG);
    const int SP[3] = {P_DLY, P_REV, P_SNDC};
    for (int k = 0; k < NBUS; k++) {
        int by = gy + 8 + k * ((gh - 16) / 3), bh = (gh - 16) / 3 - 4, bx = gx + gw - 170;
        int v = pv(SP[k]);
        const uint8_t* bp = eng.proj.bus[k];
        rrect(bx, by, 160, bh, 6, v ? C_SEL : C_BG);
        snprintf(b, sizeof b, "BUS %c  %s", 'A' + k, BUS_TYPE_NAMES[bp[BX_TYPE] % BF_COUNT]);
        text(bx + 8, by + bh / 2 - 8, b, v ? C_TEXT : C_DIM);
        snprintf(b, sizeof b, "%d", v); text(bx + 152 - textW(b), by + bh / 2 - 8, b, v ? c : C_DIM);
        int ex = bx, ey = by + bh / 2;
        Col lc = v ? dimC(c, 0.25f + 0.6f * v / 127.f) : C_SEL;
        line(sx + 16, sy, ex, ey, lc, v > 60 ? 2 : 1);
        int np = v / 16;
        float act = 0.35f + 0.65f * fminf(1.f, sqrtf(meterV[track]) * 1.5f);
        for (int i = 0; i < np; i++) {
            float f = fmodf(animTime * (0.35f + k * 0.07f) + i / (float)(np ? np : 1), 1.f);
            int px = sx + 16 + (int)((ex - sx - 16) * f), py = sy + (int)((ey - sy) * f);
            disc(px, py, 2 + (int)(act * 2.f), mix(dimC(c, act), C_PLAY, act * 0.3f));
        }
    }
}

// ---- Effekt-Bus (untere Reihe): Animation je Effekt-Typ, Aktivitaet aus dem Bus-Pegel
static void busAnimGraph(int gx, int gy, int gw, int gh, int b) {
    graphBox(gx, gy, gw, gh);
    const uint8_t* p = eng.proj.bus[b];
    int ty = p[BX_TYPE] % BF_COUNT;
    float act = fminf(1.f, sqrtf(eng.bus[b].peak) * 2.f), t = animTime;
    Col c = C_ACC, cd = dimC(c, 0.4f);
    int x0 = gx + 12, W = gw - 24, cy = gy + gh / 2, A = gh / 2 - 6;
    switch (ty) {
    case BF_DELAY: {   // Echos abwechselnd links (oben) / rechts (unten), Abstand = Zeit, Hoehe = Rueckkopplung
        float fb = p[BX_P2] / 127.f * 1.2f, ping = p[BX_P4] / 127.f;
        float step = DLY_STEPS[p[BX_P1] % 15] / 4.f;   // in Takten
        int sp = (int)(W * step / 5.f); if (sp < 26) sp = 26; if (sp > W / 4) sp = W / 4;
        float run = fmodf(t * eng.proj.bpm / 240.f / (step > 0.01f ? step : 0.01f), 1.f);
        fill(x0, cy, W, 1, C_SEL);
        float amp = 1.f;
        for (int k = 0, x = x0 + 4; x < x0 + W - 4 && k < 24; k++, x += sp) {
            // jedes Echo als kleines abklingendes Wellenpaket; ein Lichtpuls laeuft im Takt der Echozeit durch
            float h = A * (0.2f + 0.8f * amp) * (0.6f + 0.4f * act);
            int dir = (k & 1) && ping > 0.3f ? 1 : -1;
            float glow = k == (int)(fmodf(t * eng.proj.bpm / 240.f / (step > 0.01f ? step : 0.01f), 6.f)) ? 1.f : 0.f;
            Col ec = mix(mix(cd, c, amp), C_PLAY, glow * 0.6f);
            int pw = sp - 6 < 60 ? sp - 6 : 60; if (pw < 6) pw = 6;
            int py = cy;
            for (int i = 0; i <= pw; i++) {
                float f = i / (float)pw, e = fminf(1.f, 1.7f * sqrtf(sinf(f * 3.14159f)) * expf(-f * 1.2f));
                int yy = cy + dir * (int)(fabsf(sinf(f * 14.f + t * 6.f)) * h * e);
                if (i) line(x + i - 1, py, x + i, yy, ec, k == 0 ? 2 : 1);
                py = yy;
            }
            rrect(x - 1, dir < 0 ? cy - (int)h - 3 : cy + (int)h + 1, 3, 3, 1, ec);
            amp *= fb > 1.f ? 1.f : fb;
            if (amp < 0.04f) break;
        }
        disc(x0 + 4 + (int)(run * sp), cy, 3, C_PLAY);
    } break;
    case BF_TAPE: {   // zwei Spulen drehen, Band dazwischen schwankt (WOW)
        float wow = p[BX_P3] / 127.f, spd = 0.6f + act * 1.4f;
        int r = A - 4, lx = gx + 30 + r, rx = gx + gw - 30 - r;
        for (int s = 0; s < 2; s++) {
            int cx = s ? rx : lx;
            ring(cx, cy, r, 2, c); disc(cx, cy, r / 4, cd);
            for (int k = 0; k < 3; k++) { float a = t * spd * (s ? 1.1f : 1.f) + k * 2.094f; line(cx, cy, cx + (int)(cosf(a) * (r - 4)), cy + (int)(sinf(a) * (r - 4)), c, 2); }
        }
        int py = cy + r;
        for (int x = lx; x <= rx; x += 3) { int y = cy + r - 2 + (int)(sinf(x * 0.05f + t * 3.f) * wow * 6.f); if (x > lx) line(x - 3, py, x, y, cd, 2); py = y; }
        int hx = (lx + rx) / 2; rrect(hx - 14, cy + r - 14, 28, 12, 3, mix(C_SEL, c, act));   // Tonkopf
    } break;
    case BF_REVERB: {   // Ringe breiten sich aus, Groesse = SIZE, Daempfung = dunkler
        float size = p[BX_P1] / 127.f, damp = p[BX_P2] / 127.f;
        int cx = gx + gw / 2, rm = (int)(A * (1.2f + size * 2.6f));
        for (int k = 0; k < 6; k++) {
            float f = fmodf(t * (0.25f + 0.2f * (1.f - size)) + k / 6.f, 1.f);
            int r = 4 + (int)(f * rm);
            float a = (1.f - f) * (0.3f + 0.7f * act) * (1.f - damp * 0.5f);
            if (r < gw / 2 - 4) ring(cx, cy, r, 1, dimC(mix(c, C_CYAN, f), a));
        }
        for (int k = 0; k < 18; k++) {   // Diffusion: Funken
            float a = k * 2.399f + t * 0.4f, d = fmodf(t * 0.3f + k * 0.137f, 1.f) * rm;
            int px = cx + (int)(cosf(a) * d), py = cy + (int)(sinf(a) * d * 0.5f);
            if (px > gx + 4 && px < gx + gw - 4 && py > gy + 4 && py < gy + gh - 4) fill(px, py, 2, 2, dimC(c, (1.f - d / rm) * (0.4f + act)));
        }
    } break;
    case BF_SPRING: {   // Feder zwischen zwei Halterungen, schwingt mit dem Signal
        float drip = p[BX_P3] / 127.f;
        rrect(x0, cy - 12, 10, 24, 3, C_DIM); rrect(x0 + W - 10, cy - 12, 10, 24, 3, C_DIM);
        int py = cy, coils = 22;
        float wob = (0.15f + act) * (A - 6);
        for (int i = 0; i <= W - 20; i++) {
            float f = i / (float)(W - 20);
            float env = sinf(f * 3.14159f);
            float y = sinf(f * coils * 6.2831853f) * 7.f + sinf(t * (8.f + drip * 10.f) - f * 9.f) * wob * env * 0.5f;
            int yy = cy + (int)y;
            if (i) line(x0 + 10 + i - 1, py, x0 + 10 + i, yy, i % 8 < 4 ? c : cd, 2);
            py = yy;
        }
    } break;
    case BF_CHORUS: {   // mehrere leicht verstimmte Stimmen wandern gegeneinander
        int nv = (p[BX_P3] % 3) + 1; float rate = 0.3f + p[BX_P1] / 127.f * 2.5f, dep = p[BX_P2] / 127.f;
        for (int v = 0; v <= nv; v++) {
            int py = cy;
            Col vc = v == 0 ? c : mix(C_CYAN, c, v / (float)(nv + 1));
            for (int i = 0; i <= W; i += 2) {
                float ph = (i / (float)W) * 6.2831853f * 3.f + (v ? sinf(t * rate + v * 2.f) * dep * 2.f : 0.f);
                int yy = cy - (int)(sinf(ph) * A * (v ? 0.8f : 1.f) * (0.5f + 0.5f * (act + 0.3f)));
                if (i) line(x0 + i - 2, py, x0 + i, yy, vc, v ? 1 : 2);
                py = yy;
            }
        }
    } break;
    default: {   // AUS: ruhiges Gitter
        for (int k = 0; k < 8; k++) { float f = fmodf(t * 0.2f + k / 8.f, 1.f); fill(x0, gy + 6 + (int)(f * (gh - 12)), W, 1, dimC(C_SEL, 1.f - f)); }
        textC(gx + gw / 2, cy - 8, TR("BUS AUS - TYPE WÄHLEN", "BUS OFF - CHOOSE TYPE"), C_DIM);
    } break;
    }
}

// ---- Einstellungen: Bild je Bereich unter der Liste
static void settingsArt(int cat, int x, int y, int w, int h) {
    if (h < 40) return;
    rrect(x, y, w, h, 8, C_BG);
    Col c = C_ACC;
    switch (cat) {
    case 0: case 1: {   // PATTERN / SPUR: Raster der Spuren mit Lauflicht
        const Pattern& P = curP();
        int rows = cat == 1 ? 1 : NTRACKS, rh = (h - 12) / rows; if (rh > 14) rh = 14;
        float cw = (w - 16) / 16.f;
        for (int r = 0; r < rows; r++) {
            int t = cat == 1 ? track : r, len = P.len[t] < 1 ? 1 : P.len[t], base = cat == 1 ? (cursor / 16) * 16 : 0;
            for (int s = 0; s < 16; s++) {
                bool on = base + s < len && P.s[t][base + s].on == 1, pl = eng.playing && eng.playedStep[t] % 16 == s;
                Col sc = on ? (pl ? C_PLAY : TCOL[t]) : (pl ? C_SEL : C_PANEL);
                rrect(x + 8 + (int)(s * cw), y + 6 + r * rh, (int)cw - 2, rh - 3 > 4 ? rh - 3 : 4, 2, sc);
            }
        }
        if (cat == 1) drawScope(x + 8, y + rh + 12, w - 16, h - rh - 18, TCOL[track]);
    } break;
    case 2: drawScope(x + 8, y + 6, w / 2 - 12, h - 12, C_CYAN); drawSpectrum(x + w / 2 + 4, y + 6, w / 2 - 12, h - 12, c); break;   // AUDIO
    case 3: {   // MIDI: Tasten leuchten mit den Trigs der Spuren
        int keys = 21, kw = (w - 16) / keys;
        for (int k = 0; k < keys; k++) {
            int t = k % NTRACKS; float f = flash[t];
            bool blk = k % 7 == 1 || k % 7 == 3 || k % 7 == 5;
            rrect(x + 8 + k * kw, y + 8 + (blk ? 0 : 0), kw - 2, h - 16, 3, mix(blk ? C_OFF2 : C_PANEL, TCOL[t], f));
        }
    } break;
    case 4: {   // USB & PC: Datenpakete zwischen Groovebox und PC
        int lx = x + 40, rx = x + w - 40, cy = y + h / 2;
        rrect(lx - 30, cy - 18, 60, 36, 6, C_SEL); textC(lx, cy - 8, "R36S", C_TEXT);
        rrect(rx - 30, cy - 18, 60, 36, 6, C_SEL); textC(rx, cy - 8, "PC", C_TEXT);
        fill(lx + 30, cy, rx - lx - 60, 2, C_OFF2);
        for (int k = 0; k < 5; k++) {
            float f = fmodf(animTime * 0.5f + k / 5.f, 1.f);
            bool back = k % 2;
            int px = back ? rx - 30 - (int)(f * (rx - lx - 60)) : lx + 30 + (int)(f * (rx - lx - 60));
            rrect(px - 4, cy - 4 + (back ? 8 : -8), 8, 6, 2, back ? C_CYAN : c);
        }
    } break;
    case 5: {   // SAMPLES: Wellenformen ziehen vorbei
        int n = sampleCount();
        for (int k = 0; k < 4 && n > 0; k++) {
            int idx = ((int)(animTime * 0.3f) + k) % n;
            const Sample* sm = sampleGet(idx); if (!sm || !sm->data) continue;
            int bx = x + 8 + k * (w - 16) / 4, bw = (w - 16) / 4 - 8, cy = y + h / 2;
            for (int i = 0; i < bw; i += 2) { float a = fabsf(sm->data[(long)sm->len * i / bw]); int hh = (int)(fminf(1.f, a * 2.f) * (h / 2 - 8)); fill(bx + i, cy - hh, 1, 2 * hh + 1, dimC(TCOL[k % NTRACKS], 0.8f)); }
        }
        if (n == 0) drawBeatRings(x, y, w, h, c);
    } break;
    default: {   // SYSTEM: Auslastung und laufende Sonne
        int cx = x + w / 2, cy = y + h - 8, r = h - 20; if (r > 70) r = 70;
        for (int k = 0; k < r; k += 3) { int hw = (int)sqrtf((float)(r * r - k * k)); fill(cx - hw, cy - k, 2 * hw, 2, mix(C_ACC, C_LOCK, k / (float)r)); }
        for (int k = 0; k < 4; k++) { int yy = cy - 6 - k * 8 - (int)(fmodf(animTime * 6.f, 8.f)); if (yy > cy - r) fill(cx - r, yy, 2 * r, 2, C_BG); }
        char b[48]; snprintf(b, sizeof b, "CPU %d%%", (int)(cpuLoad * 100)); text(x + 10, y + 8, b, C_DIM);
    } break;
    }
}

// ---- Song: Zeitleiste der Zeilen (Breite = Laenge) mit laufender Position
static void songTimeline(int x, int y, int w, int h) {
    const Project& J = eng.proj;
    rrect(x, y, w, h, 8, C_BG);
    if (!J.songLen) { drawBeatRings(x, y, w, h, C_ACC); return; }
    long tot = 0, len[SONG_MAX];
    for (int i = 0; i < J.songLen; i++) {
        long m = 0;
        for (int t = 0; t < NTRACKS; t++) { if (J.row[i].pat[t] == SONG_EMPTY) continue; const Pattern& P = J.pat[J.row[i].pat[t] % NPATTERNS]; long l = (long)(P.len[t] < 1 ? 1 : P.len[t]) * SPEED_TPS[P.speed[t] % 7]; if (l > m) m = l; }
        len[i] = (m ? m : 192) * (J.row[i].rep < 1 ? 1 : J.row[i].rep); tot += len[i];
    }
    int bx = x + 8, bw = w - 16, by = y + 8, bh = h / 2 - 6;
    long acc = 0;
    for (int i = 0; i < J.songLen; i++) {
        int xs = bx + (int)(acc * bw / tot), xe = bx + (int)((acc + len[i]) * bw / tot);
        int ap = -1; for (int t = 0; t < NTRACKS; t++) if (J.row[i].pat[t] != SONG_EMPTY) { ap = J.row[i].pat[t]; break; }
        bool cur = eng.songOn && eng.songPos == i;
        rrect(xs, by, xe - xs - 2 > 2 ? xe - xs - 2 : 2, bh, 4, cur ? C_ACC : dimC(TCOL[(ap < 0 ? 0 : ap) % NTRACKS], 0.5f));
        if (xe - xs > 26) { char b[8]; snprintf(b, sizeof b, "%02d", i + 1); text(xs + 4, by + bh / 2 - 8, b, cur ? C_BG : C_TEXT); }
        acc += len[i];
    }
    if (eng.songOn && eng.playing) {   // Abspielposition (geschaetzt aus Zeile + Wiederholung)
        long a2 = 0; for (int i = 0; i < eng.songPos && i < J.songLen; i++) a2 += len[i];
        int rp = eng.songPos < J.songLen ? J.row[eng.songPos].rep : 1; if (rp < 1) rp = 1;
        a2 += len[eng.songPos % J.songLen] * eng.songRep / rp;
        int px = bx + (int)(a2 * bw / tot);
        fill(px, by - 3, 2, bh + 6, C_PLAY);
    }
    drawScope(x + 8, by + bh + 6, w - 16, h - bh - 20, C_CYAN, 2);
}

// ---- LIVE: Spektrum oben, Takt-Ringe in der Mitte
static void liveArt() {
    drawSpectrum(10, 74, 620, 58, C_ACC, SPEC_N);
    drawBeatRings(270, 180, 100, 100, eng.fill ? C_LOCK : C_CYAN);
}

// ---- PROJEKT: animiertes Symbol je Aktion
static void prjIcon(int k, int x, int y, int s, Col c) {
    float t = animTime;
    int cx = x + s / 2, cy = y + s / 2;
    switch (k) {
    case 0: {   // LADEN: Ordner, Blatt hebt sich
        rrect(x + 2, y + s / 3, s - 4, s * 2 / 3 - 4, 4, dimC(c, 0.6f));
        int up = (int)((0.5f + 0.5f * sinf(t * 2.f)) * s / 5);
        rrect(x + 8, y + s / 3 - up, s - 16, s / 2, 2, C_TEXT);
        rrect(x + 2, y + s / 2, s - 4, s / 2 - 4, 4, c);
    } break;
    case 1: case 2: {   // SPEICHERN: Diskette, Fortschritt laeuft
        rrect(x + 4, y + 4, s - 8, s - 8, 4, c);
        rrect(x + 12, y + 4, s - 24, s / 3, 2, C_BG);
        int pw = (int)(fmodf(t * 0.6f, 1.f) * (s - 20));
        rrect(x + 10, y + s - 18, s - 20, 8, 3, C_BG); if (pw > 2) rrect(x + 10, y + s - 18, pw, 8, 3, C_PLAY);
        if (k == 2) line(x + s - 14, y + s - 4, x + s, y + s - 18, C_LOCK, 3);   // Stift
    } break;
    case 3: {   // NEU: Plus mit Funken
        fill(cx - 3, y + 6, 6, s - 12, c); fill(x + 6, cy - 3, s - 12, 6, c);
        for (int i = 0; i < 6; i++) { float a = i * 1.047f + t; int r = s / 2 - 2 + (int)(sinf(t * 3.f + i) * 3.f); fill(cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), 2, 2, C_PLAY); }
    } break;
    case 4: case 5: {   // SOUND LADEN/SPEICHERN: Welle in die Box bzw. heraus
        rrect(x + s / 2, y + 6, s / 2 - 4, s - 12, 4, dimC(c, 0.6f));
        int py = cy;
        for (int i = 0; i < s / 2; i++) { float f = i / (float)(s / 2); int yy = cy - (int)(sinf(f * 12.f + (k == 4 ? -t : t) * 5.f) * (s / 5) * (1.f - f * 0.5f)); if (i) line(x + i - 1, py, x + i, yy, c, 2); py = yy; }
    } break;
    case 6: {   // INIT: Pfeil im Kreis
        ring(cx, cy, s / 2 - 4, 2, c);
        float a = t * 2.f; disc(cx + (int)(cosf(a) * (s / 2 - 4)), cy + (int)(sinf(a) * (s / 2 - 4)), 4, C_PLAY);
    } break;
    default: {   // DEMO: Play-Dreieck pulsiert, Noten steigen
        int r = s / 3 + (int)(sinf(t * 4.f) * 2.f);
        for (int i = 0; i < r * 2; i++) fill(cx - r / 2, cy - r + i, (i < r ? i : 2 * r - i) * 3 / 2, 1, c);
        for (int i = 0; i < 3; i++) { float f = fmodf(t * 0.5f + i / 3.f, 1.f); disc(x + 6 + i * (s / 3), y + s - 4 - (int)(f * s), 3, dimC(C_PLAY, 1.f - f)); }
    } break;
    }
}
