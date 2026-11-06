// Wavetable-Editor (WTBL, Regler DRAW auf der Seite OSC): 4 Wellen zu je 64 Punkten, die ueber POS ineinander
// uebergehen. Beim Bearbeiten klingt die Tabelle sofort (Vorhoer-Platz des Pools), START speichert sie als
// samples/wavetables/draw-NN.wav (16 Frames x 2048 Werte, 32-Bit-Float) und weist sie der Spur zu.
// Eingebunden in main.cpp nach filedlg.inc.

enum { WTE_K = 4, WTE_N = 64, WTE_FR = 16, WTE_F = 2048 };
static bool wteOpen = false, wteMenu = false, wteDirty = false;
static int wteKey = 0, wteCur = 0, wteMenuSel = 0, wteOrigTab = 0, wteOrigWav = 0, wteTrack = 0;
static float wtePts[WTE_K][WTE_N];
static Uint32 wteDirtyAt = 0;
static float wteStickAcc = 0;

static float wteCubic(const float* p, float x) {   // periodische Catmull-Rom-Interpolation
    int i1 = (int)x; float t = x - i1;
    int i0 = (i1 + WTE_N - 1) % WTE_N, i2 = (i1 + 1) % WTE_N, i3 = (i1 + 2) % WTE_N; i1 %= WTE_N;
    float a = p[i0], b = p[i1], c = p[i2], d = p[i3];
    return b + 0.5f * t * (c - a + t * (2.f * a - 5.f * b + 4.f * c - d + t * (3.f * (b - c) + d - a)));
}
// 16 Frames: Frame f liegt zwischen Welle floor(f/5) und der naechsten
static float* wteRender() {
    float* d = (float*)malloc(sizeof(float) * WTE_FR * WTE_F);
    if (!d) return nullptr;
    for (int f = 0; f < WTE_FR; f++) {
        float kp = f * (WTE_K - 1) / (float)(WTE_FR - 1);
        int k0 = (int)kp; if (k0 > WTE_K - 2) k0 = WTE_K - 2; float fr = kp - k0;
        for (int j = 0; j < WTE_F; j++) {
            float x = j * (float)WTE_N / WTE_F;
            float a = wteCubic(wtePts[k0], x), b = wteCubic(wtePts[k0 + 1], x);
            float v = a + (b - a) * fr;
            d[f * WTE_F + j] = v > 1.f ? 1.f : (v < -1.f ? -1.f : v);
        }
    }
    return d;
}
// Vorhoeren: Tabelle in den Vorhoer-Platz legen und der Spur zuweisen
static void wteApply() {
    Sample s; memset(&s, 0, sizeof s);
    s.data = wteRender(); if (!s.data) return;
    s.len = WTE_FR * WTE_F; s.rate = 44100;
    snprintf(s.name, sizeof s.name, "%s", TR("(Entwurf)", "(draft)"));
    lockA();
    poolSwap(PREVIEW_SLOT, s);
    Pattern& P = curP();
    P.p[wteTrack][P_S0] = 4; P.p[wteTrack][P_X3] = PREVIEW_SLOT;
    unlockA();
    sampleFree(s);
    wteDirty = false;
}
static void wteShape(int tool) {   // Werkzeuge auf die aktuelle Welle
    float* p = wtePts[wteKey];
    float tmp[WTE_N];
    for (int i = 0; i < WTE_N; i++) {
        float ph = i / (float)WTE_N;
        switch (tool) {
        case 0: p[i] = sinf(6.2831853f * ph); break;                                   // Sinus
        case 1: p[i] = 1.f - 4.f * fabsf(ph - 0.5f); p[i] = -p[i]; break;               // Dreieck
        case 2: p[i] = 1.f - 2.f * ph; break;                                           // Saege
        case 3: p[i] = ph < 0.5f ? 0.9f : -0.9f; break;                                 // Rechteck
        case 4: p[i] = ph < 0.25f ? 0.9f : -0.9f; break;                                // Puls 25 %
        case 5: { static uint32_t r = 12345; r = r * 1664525u + 1013904223u; p[i] = (r >> 8) * (2.f / 16777216.f) - 1.f; } break;   // Rauschen
        default: break;
        }
    }
    if (tool == 6) {   // glaetten
        for (int i = 0; i < WTE_N; i++) tmp[i] = (p[(i + WTE_N - 1) % WTE_N] + 2.f * p[i] + p[(i + 1) % WTE_N]) * 0.25f;
        memcpy(p, tmp, sizeof tmp);
    } else if (tool == 7) {   // normalisieren
        float m = 1e-4f; for (int i = 0; i < WTE_N; i++) m = fmaxf(m, fabsf(p[i]));
        for (int i = 0; i < WTE_N; i++) p[i] *= 0.98f / m;
    } else if (tool == 8) { for (int i = 0; i < WTE_N; i++) p[i] = -p[i]; }   // umkehren
    else if (tool == 9) { for (int i = 0; i < WTE_N; i++) p[i] = 0.f; }      // leeren
    else if (tool == 10 && wteKey > 0) memcpy(p, wtePts[wteKey - 1], sizeof(float) * WTE_N);   // von der vorigen Welle
    else if (tool == 11) {   // alle 4 Wellen gleich
        for (int k = 0; k < WTE_K; k++) if (k != wteKey) memcpy(wtePts[k], p, sizeof(float) * WTE_N);
    }
    wteDirty = true; wteDirtyAt = SDL_GetTicks();
}
static const char* wteToolName(int i) {
    const char* const DE[12] = {"SINUS", "DREIECK", "SÄGE", "RECHTECK", "PULS 25%", "RAUSCHEN", "GLÄTTEN", "NORMALISIEREN", "UMKEHREN", "LEEREN",
                                "VON VORIGER WELLE", "AUF ALLE 4 WELLEN"};
    const char* const EN[12] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "PULSE 25%", "NOISE", "SMOOTH", "NORMALIZE", "INVERT", "CLEAR",
                                "FROM PREVIOUS WAVE", "TO ALL 4 WAVES"};
    return g_lang ? EN[i] : DE[i];
}
enum { WTE_TOOLS = 12 };

static void openWtEditor() {
    Pattern& P = curP();
    wteTrack = track; wteOrigTab = P.p[track][P_S0]; wteOrigWav = P.p[track][P_X3];
    wteKey = 0; wteCur = 0; wteMenu = false;
    // Startwerte: vorhandene eigene Tabelle (4 Frames gleichmaessig verteilt) oder die eingebaute Tabelle
    const Sample* us = (wteOrigTab % 5) == 4 ? sampleGet(wteOrigWav) : nullptr;
    for (int k = 0; k < WTE_K; k++) {
        for (int i = 0; i < WTE_N; i++) {
            float v;
            if (us && us->data && us->len >= 16) {
                int F = us->len >= 2048 && us->len % 2048 == 0 ? 2048 : (us->len % 256 == 0 ? 256 : us->len), nf = us->len / F;
                int f = nf > 1 ? k * (nf - 1) / (WTE_K - 1) : 0;
                v = us->data[(long)f * F + (long)i * F / WTE_N];
            } else {
                const float* fr = wavetableFrame(wteOrigTab & 3, k * 31 / (WTE_K - 1));
                v = fr[i * 256 / WTE_N];
            }
            wtePts[k][i] = v > 1.f ? 1.f : (v < -1.f ? -1.f : v);
        }
    }
    wteOpen = true;
    wteApply();
}
static void wteClose(bool keep) {
    if (keep) {   // speichern: WAV schreiben, in die Bibliothek aufnehmen, Spur zuweisen
        float* d = wteRender();
        char dir[700], name[96], path[800];
        snprintf(dir, sizeof dir, "%ssamples", basePath); mkdir(dir, 0755);
        snprintf(dir, sizeof dir, "%ssamples/wavetables", basePath); mkdir(dir, 0755);
        struct stat st; name[0] = 0;
        for (int i = 1; i < 1000; i++) { snprintf(name, sizeof name, "wavetables/draw-%02d", i); snprintf(path, sizeof path, "%ssamples/%s.wav", basePath, name); if (stat(path, &st)) break; }
        bool ok = false;
        FILE* f = d ? fopen(path, "wb") : nullptr;
        if (f) {   // 32-Bit-Float mono
            uint32_t n = WTE_FR * WTE_F, bytes = n * 4, sr = 44100, br = sr * 4;
            uint16_t fmt = 3, ch = 1, ba = 4, bits = 32; uint32_t c16 = 16, riff = 36 + bytes;
            fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&c16, 4, 1, f);
            fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
            fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
            ok = fwrite(d, 4, n, f) == n;
            if (fclose(f) != 0) ok = false;
        }
        free(d);
        poolSetSlot(PREVIEW_SLOT, "");
        int sl = ok ? poolAssign(name) : -1;
        lockA();
        if (sl >= 0) { curP().p[wteTrack][P_S0] = 4; curP().p[wteTrack][P_X3] = (uint8_t)sl; }
        else { curP().p[wteTrack][P_S0] = (uint8_t)wteOrigTab; curP().p[wteTrack][P_X3] = (uint8_t)wteOrigWav; }
        unlockA();
        if (sl >= 0) { reloadSamples(); toast(TR("GESPEICHERT: SAMPLES/%s", "SAVED: SAMPLES/%s"), name); }
        else toast(TR("SPEICHERN FEHLGESCHLAGEN", "SAVING FAILED"));
    } else {
        lockA(); curP().p[wteTrack][P_S0] = (uint8_t)wteOrigTab; curP().p[wteTrack][P_X3] = (uint8_t)wteOrigWav; unlockA();
        poolSetSlot(PREVIEW_SLOT, "");
        toast(TR("WAVETABLE VERWORFEN", "WAVETABLE DISCARDED"));
    }
    wteOpen = false;
}

static void wteInput(Input& I, Uint32 now, float dt) {
    if (wteMenu) {
        int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0);
        if (dy) wteMenuSel = (wteMenuSel + dy + WTE_TOOLS) % WTE_TOOLS;
        if (I.pressed(BT_A)) { wteShape(wteMenuSel); wteMenu = false; toast("%s", wteToolName(wteMenuSel)); }
        if (I.pressed(BT_B) || I.pressed(BT_SELECT)) wteMenu = false;
        return;
    }
    int dx = (I.repeat(BT_RIGHT, now, 250, 40) ? 1 : 0) - (I.repeat(BT_LEFT, now, 250, 40) ? 1 : 0);
    if (fabsf(I.lx) > 0.4f) {   // linker Stick: Cursor schnell bewegen (zum Zeichnen mit dem rechten Stick)
        wteStickAcc += I.lx * dt * 40.f;
        int d = (int)wteStickAcc; if (d) { wteStickAcc -= d; dx += d; }
    } else wteStickAcc = 0;
    if (dx) wteCur = ((wteCur + dx) % WTE_N + WTE_N) % WTE_N;
    float* p = wtePts[wteKey];
    int dv = (I.repeat(BT_UP, now, 250, 50) ? 1 : 0) - (I.repeat(BT_DOWN, now, 250, 50) ? 1 : 0);
    if (dv) { p[wteCur] = fminf(1.f, fmaxf(-1.f, p[wteCur] + dv / 32.f)); wteDirty = true; wteDirtyAt = now; }
    if (fabsf(I.ry) > 0.15f) { p[wteCur] = fminf(1.f, fmaxf(-1.f, I.ry)); wteDirty = true; wteDirtyAt = now; }   // rechter Stick: Wert direkt
    int dk = (I.pressed(BT_R1) ? 1 : 0) - (I.pressed(BT_L1) ? 1 : 0);
    if (dk) wteKey = (wteKey + dk + WTE_K) % WTE_K;
    if (I.pressed(BT_A)) { wteMenu = true; }
    if (I.pressed(BT_X)) { if (wteDirty) wteApply(); lockA(); eng.preview(wteTrack, nullptr); unlockA(); }
    if (I.pressed(BT_START)) { if (wteDirty) wteApply(); wteClose(true); return; }
    if (I.pressed(BT_B) || I.pressed(BT_SELECT)) { wteClose(false); return; }
    if (wteDirty && now - wteDirtyAt > 120) wteApply();   // kurz nach der letzten Aenderung hoerbar machen
}

static void drawWtEditor() {
    Col tc = TCOL[wteTrack];
    rrect(6, 30, 628, 418, 12, tc);
    rrect(8, 32, 624, 414, 11, C_BG);
    text(20, 40, TR("WAVETABLE ZEICHNEN", "DRAW WAVETABLE"), tc, 2);
    char b[64];
    // vier Wellen als Reiter mit Vorschau
    for (int k = 0; k < WTE_K; k++) {
        int x = 344 + k * 70, y = 38, w = 64, h = 34;
        bool sel = k == wteKey;
        rrect(x, y, w, h, 6, sel ? tc : C_PANEL);
        for (int i = 0; i < WTE_N; i += 2) { int yy = y + h / 2 - (int)(wtePts[k][i] * (h / 2 - 3)); fill(x + 4 + i * (w - 8) / WTE_N, yy, 2, 2, sel ? C_BG : C_TEXT); }
        snprintf(b, sizeof b, "W%d", k + 1); text(x + 3, y + 1, b, sel ? C_BG : C_DIM);
    }
    // Zeichenflaeche
    int x0 = 24, W = 592, cy = 220, A = 120;
    rrect(x0 - 6, cy - A - 10, W + 12, 2 * A + 20, 8, C_PANEL);
    fill(x0, cy, W, 1, C_OFF2);
    fill(x0, cy - A, W, 1, C_OFF); fill(x0, cy + A, W, 1, C_OFF);
    float bw = W / (float)WTE_N;
    // Nachbarwelle blass dahinter (Uebergang sichtbar)
    int nk = wteKey < WTE_K - 1 ? wteKey + 1 : wteKey - 1;
    for (int i = 0; i < WTE_N; i++) { int yy = cy - (int)(wtePts[nk][i] * A); fill(x0 + (int)(i * bw + bw / 2), yy, 2, 2, C_OFF2); }
    for (int i = 0; i < WTE_N; i++) {
        float v = wtePts[wteKey][i];
        int x = x0 + (int)(i * bw), h = (int)(fabsf(v) * A);
        bool cur = i == wteCur;
        Col c = cur ? C_PLAY : dimC(tc, 0.75f);
        fill(x + 1, v >= 0 ? cy - h : cy, (int)bw - 2, h > 0 ? h : 1, c);
    }
    // interpolierte Kurve (so klingt sie)
    int py = cy;
    for (int xx = 0; xx < W; xx++) {
        float v = wteCubic(wtePts[wteKey], xx * (float)WTE_N / W);
        int y = cy - (int)(fminf(1.f, fmaxf(-1.f, v)) * A);
        if (xx) { int a = py < y ? py : y, h2 = abs(y - py) + 1; fill(x0 + xx, a, 1, h2, C_TEXT); }
        py = y;
    }
    snprintf(b, sizeof b, TR("WELLE %d  PUNKT %d/%d  WERT %+.2f", "WAVE %d  POINT %d/%d  VALUE %+.2f"), wteKey + 1, wteCur + 1, WTE_N, wtePts[wteKey][wteCur]);
    text(24, 352, b, C_TEXT);
    text(24, 372, TR("POS blendet von W1 (0) nach W4 (127) über.", "POS morphs from W1 (0) to W4 (127)."), C_DIM);
    text(24, 392, TR("Zeichnen: rechter Stick setzt den Wert, linker Stick fährt.", "Drawing: right stick sets the value, left stick moves."), C_DIM);
    if (wteMenu) {   // Werkzeuge
        int mx = 360, my = 80, mw = 250, mh = WTE_TOOLS * 22 + 16;
        rrect(mx, my, mw, mh, 8, tc); rrect(mx + 2, my + 2, mw - 4, mh - 4, 7, C_BG);
        for (int i = 0; i < WTE_TOOLS; i++) {
            bool sel = i == wteMenuSel;
            if (sel) rrect(mx + 6, my + 6 + i * 22, mw - 12, 20, 5, C_SEL);
            text(mx + 14, my + 8 + i * 22, wteToolName(i), sel ? tc : C_TEXT);
        }
    }
    hint(16, 424, wteMenu ? TR("[^v]WÄHLEN [A]ANWENDEN [B]ZURÜCK", "[^v]SELECT [A]APPLY [B]BACK")
                          : TR("[</>]PUNKT [^v]WERT [L1/R1]WELLE [A]WERKZEUG [X]HÖREN [START]SPEICHERN [B]ABBRUCH",
                               "[</>]POINT [^v]VALUE [L1/R1]WAVE [A]TOOLS [X]LISTEN [START]SAVE [B]CANCEL"), C_ACC, C_DIM);
}
