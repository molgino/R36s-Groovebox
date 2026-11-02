// R36S Groovebox - Elektron-artige Groovebox fuer den R36S (ArkOS) und Linux-Desktop
#include <SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include "engine.h"
#include "input.h"
#include "font.h"
#include "samples.h"
#include "midi.h"

static const int SW = 640, SH = 480;

struct Col { Uint8 r, g, b; };
static const Col C_BG = {14, 14, 17}, C_PANEL = {28, 28, 34}, C_SEL = {44, 40, 38}, C_DIM = {80, 80, 92},
                 C_TEXT = {220, 220, 226}, C_ACC = {255, 122, 26}, C_TRIG = {232, 57, 47}, C_LOCK = {242, 194, 48},
                 C_OFF = {38, 38, 46}, C_OFF2 = {52, 52, 62}, C_PLAY = {250, 250, 250}, C_DARK = {20, 20, 24},
                 C_GREEN = {90, 220, 120}, C_KNOB = {60, 60, 70};

static SDL_Renderer* ren = nullptr;
static SDL_Texture* fontTex = nullptr;
static Uint8 textAlpha = 255;
static SDL_AudioDeviceID audioDev = 0;
static Engine eng;
static Input in;

// UI-Zustand
static int track = 0, cursor = 0, page = 0, slot = 0;
static bool aHeld = false, aJustOn = false, aEdited = false;
static int holdPat = 0, holdTrack = 0, holdStep = 0;
static bool funcUsed = false;
static float stickAcc = 0;
static char msg[64] = "";
static Uint32 msgUntil = 0;
static Uint32 editAt = 0, msgAt = 0;
static int editSlot = 0;
static Pattern clip;
static bool haveClip = false;
static bool quitReq = false;
static volatile float cpuLoad = 0;
static char basePath[512] = "./", cfgPath[600], projPath[600];
static Project lastSaved;

// System-Einstellungen (settings.cfg)
struct Settings { char dev[128]; int buf, midiIn, notesOut, clk, chm; };
static Settings cfg = {"", 1, 1, 0, 0, 0};
static char setPath[600];
static int bufOverride = 0;
static int devCount = 0;
static char devNames[16][128];
static Uint32 audioApplyAt = 0, usbApplyAt = 0;
static int usbMode = 0;   // 0 Host, 1 PC (MIDI), 2 PC (MIDI + Audio)
static int lastStepRec = 0;
static Uint32 taps[4]; static int tapN = 0;
static bool stickDj = false;
static volatile double cbTime = 0;
static volatile int cbFrames = 256;

static void lockA() { if (audioDev) SDL_LockAudioDevice(audioDev); }
static void unlockA() { if (audioDev) SDL_UnlockAudioDevice(audioDev); }
static bool saveProject() {
    lockA(); bool ok = eng.save(projPath); if (ok) memcpy(&lastSaved, &eng.proj, sizeof(Project)); unlockA();
    return ok;
}
static int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }

static void toast(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    msgUntil = SDL_GetTicks() + 1400;
    msgAt = SDL_GetTicks();
}

static void loadSettings() {
    FILE* f = fopen(setPath, "r");
    if (!f) return;
    char line[200];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char* sp = strchr(line, ' ');
        if (!sp || line[0] == '#') continue;
        *sp = 0;
        const char* v = sp + 1;
        if (!strcmp(line, "audio_device")) snprintf(cfg.dev, sizeof cfg.dev, "%s", v);
        else if (!strcmp(line, "buffer")) cfg.buf = clampi(atoi(v), 0, 3);
        else if (!strcmp(line, "midi_in")) cfg.midiIn = atoi(v) != 0;
        else if (!strcmp(line, "midi_notes_out")) cfg.notesOut = atoi(v) != 0;
        else if (!strcmp(line, "midi_clock")) cfg.clk = clampi(atoi(v), 0, 2);
        else if (!strcmp(line, "midi_channels")) cfg.chm = atoi(v) != 0;
    }
    fclose(f);
}
static void saveSettings() {
    FILE* f = fopen(setPath, "w");
    if (!f) return;
    fprintf(f, "# Groovebox Systemeinstellungen\naudio_device %s\nbuffer %d\nmidi_in %d\nmidi_notes_out %d\nmidi_clock %d\nmidi_channels %d\n",
            cfg.dev, cfg.buf, cfg.midiIn, cfg.notesOut, cfg.clk, cfg.chm);
    fclose(f);
}
static void applyMidiSettings() {
    eng.midiInOn = cfg.midiIn != 0;
    eng.notesOut = cfg.notesOut != 0;
    eng.clockMode = cfg.clk;
    eng.chMode = cfg.chm;
}
static void refreshDevices() {
    int n = SDL_GetNumAudioDevices(0);
    devCount = 0;
    for (int i = 0; i < n && devCount < 16; i++) {
        const char* nm = SDL_GetAudioDeviceName(i, 0);
        if (nm) snprintf(devNames[devCount++], 128, "%s", nm);
    }
}
static int devIndex() {
    if (!cfg.dev[0]) return 0;
    for (int i = 0; i < devCount; i++) if (!strcmp(devNames[i], cfg.dev)) return i + 1;
    return 0;
}

// ---------------------------------------------------------------- Zeichnen

static void setCol(Col c, Uint8 a = 255) { SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, a); }
static void fill(int x, int y, int w, int h, Col c, Uint8 a = 255) {
    setCol(c, a); SDL_Rect r = {x, y, w, h}; SDL_RenderFillRect(ren, &r);
}
static void makeFont() {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 95 * FONT_W, FONT_H, 32, SDL_PIXELFORMAT_RGBA32);
    Uint8* px = (Uint8*)s->pixels;
    for (int c = 0; c < 95; c++)
        for (int y = 0; y < FONT_H; y++)
            for (int x = 0; x < FONT_W; x++) {
                Uint8* p = px + y * s->pitch + (c * FONT_W + x) * 4;
                p[0] = p[1] = p[2] = 255; p[3] = FONT_DATA[c][y * FONT_W + x];
            }
    fontTex = SDL_CreateTextureFromSurface(ren, s);
    SDL_SetTextureBlendMode(fontTex, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(s);
}
static int textW(const char* s, int sc = 1) { return (int)strlen(s) * FONT_W * sc; }
static void text(int x, int y, const char* s, Col c, int sc = 1) {
    SDL_SetTextureColorMod(fontTex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(fontTex, textAlpha);
    for (; *s; s++, x += FONT_W * sc) {
        int ch = (unsigned char)*s;
        if (ch < 32 || ch > 126) ch = '?';
        SDL_Rect src = {(ch - 32) * FONT_W, 0, FONT_W, FONT_H}, dst = {x, y, FONT_W * sc, FONT_H * sc};
        SDL_RenderCopy(ren, fontTex, &src, &dst);
    }
}
static void textC(int cx, int y, const char* s, Col c, int sc = 1) { text(cx - textW(s, sc) / 2, y, s, c, sc); }

static void arc(int cx, int cy, int r, float a0, float a1, Col c, int th) {
    setCol(c);
    if (a1 < a0) { float t = a0; a0 = a1; a1 = t; }
    int segs = (int)((a1 - a0) / 6.f) + 2;
    SDL_Point pts[80];
    if (segs > 79) segs = 79;
    for (int k = 0; k < th; k++) {
        for (int i = 0; i <= segs; i++) {
            float a = (a0 + (a1 - a0) * i / segs) * 0.0174533f;
            pts[i].x = cx + (int)lroundf((r - k) * cosf(a));
            pts[i].y = cy + (int)lroundf((r - k) * sinf(a));
        }
        SDL_RenderDrawLines(ren, pts, segs + 1);
    }
}
static void knob(int cx, int cy, int r, float frac, bool bip, Col c) {
    arc(cx, cy, r, 135, 405, C_KNOB, 4);
    float av = 135 + 270 * frac;
    if (bip) arc(cx, cy, r, 270, av, c, 4);
    else arc(cx, cy, r, 135, av, c, 4);
    float a = av * 0.0174533f;
    setCol(c);
    SDL_RenderDrawLine(ren, cx + (int)((r - 9) * cosf(a)), cy + (int)((r - 9) * sinf(a)),
                       cx + (int)((r - 4) * cosf(a)), cy + (int)((r - 4) * sinf(a)));
}
static void playIcon(int x, int y, bool playing) {
    if (playing) { for (int k = 0; k < 8; k++) fill(x + k, y + k, 1, 16 - 2 * k, C_GREEN); }
    else fill(x, y + 2, 12, 12, C_DIM);
}

// ---------------------------------------------------------------- Optik-Helfer

static const Col TCOL[NTRACKS] = {
    {255, 94, 58}, {255, 168, 46}, {242, 222, 76}, {118, 214, 108},
    {56, 200, 196}, {84, 150, 255}, {168, 112, 255}, {255, 98, 184}};

static Col mix(Col a, Col b, float t) {
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return {(Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t), (Uint8)(a.b + (b.b - a.b) * t)};
}
static Col dimC(Col c, float f) { return mix(C_BG, c, f); }

// gefuelltes Rechteck mit runden Ecken
static void rrect(int x, int y, int w, int h, int r, Col c, Uint8 a = 255) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r <= 0) { fill(x, y, w, h, c, a); return; }
    fill(x, y + r, w, h - 2 * r, c, a);
    for (int i = 0; i < r; i++) {
        float dy = r - i - 0.5f;
        int in = r - (int)(sqrtf((float)r * r - dy * dy) + 0.5f);
        fill(x + in, y + i, w - 2 * in, 1, c, a);
        fill(x + in, y + h - 1 - i, w - 2 * in, 1, c, a);
    }
}

// Hinweistext: [..] wird hervorgehoben
static void hint(int x, int y, const char* s, Col key, Col norm) {
    bool k = false;
    char ch[2] = {0, 0};
    for (; *s; s++) {
        if (*s == '[') { k = true; continue; }
        if (*s == ']') { k = false; x += 3; continue; }
        ch[0] = *s;
        text(x, y, ch, k ? key : norm);
        x += FONT_W;
    }
}

// Animationszustand
static float flash[NTRACKS], meterV[NTRACKS];
static uint32_t lastTrigs[NTRACKS];
static Uint32 pageAt = 0, lastFrame = 0;
static int lastPage = -1;
static float beatPulse = 0;
static float grDisp = 0;
static int lastBeat = -1;

static void animate() {
    Uint32 now = SDL_GetTicks();
    float dt = lastFrame ? (now - lastFrame) / 1000.f : 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    lastFrame = now;
    for (int t = 0; t < NTRACKS; t++) {
        uint32_t tc = eng.v[t].trigs;
        if (tc != lastTrigs[t]) { lastTrigs[t] = tc; flash[t] = 1.f; }
        flash[t] -= dt * 5.f; if (flash[t] < 0) flash[t] = 0;
        float pk = eng.v[t].peakHold; eng.v[t].peakHold = 0;
        float dec = meterV[t] * expf(-dt * 7.f);
        meterV[t] = pk > dec ? pk : dec;
    }
    float g = eng.grPeak; eng.grPeak = 0;
    float gd = grDisp * expf(-dt * 6.f);
    grDisp = g > gd ? g : gd;
    int beat = eng.playing ? eng.playedStep[0] / 4 : -1;
    if (beat != lastBeat) { lastBeat = beat; if (beat >= 0) beatPulse = 1.f; }
    beatPulse -= dt * 4.f; if (beatPulse < 0) beatPulse = 0;
    if (page != lastPage) { if (lastPage >= 0) pageAt = now; lastPage = page; }
}

// ---------------------------------------------------------------- Bildschirm

static Pattern& curP() { return eng.proj.pat[eng.curPat]; }
static Step& holdS() { return eng.proj.pat[holdPat].s[holdTrack][holdStep]; }

static bool stepHasLock(const Step& s) {
    for (int i = 0; i < NPARAMS; i++) if (s.lock[i] != NO_LOCK) return true;
    return false;
}

static void drawHeader() {
    char b[64];
    Col tc = TCOL[track];
    fill(0, 0, SW, 40, C_PANEL);
    fill(0, 40, SW, 1, dimC(tc, 0.6f));
    snprintf(b, sizeof b, "P%02d", eng.curPat + 1);
    text(8, 4, b, C_TEXT, 2);
    if (eng.nextPat >= 0) {   // Pattern in der Warteschlange: blinkend + Fortschritt bis zum Wechsel
        if ((SDL_GetTicks() / 250) % 2) { snprintf(b, sizeof b, ">%02d", eng.nextPat + 1); text(58, 4, b, C_ACC); }
        int prog = eng.playing ? (eng.playedStep[0] + 1) * 48 / NSTEPS : 0;
        fill(8, 36, 48, 2, C_OFF); fill(8, 36, prog, 2, C_ACC);
    }
    playIcon(62, 20, eng.playing);
    snprintf(b, sizeof b, "%d BPM", eng.proj.bpm);
    text(84, 4, b, C_TEXT);
    for (int i = 0; i < 4; i++) {   // Taktanzeige
        bool on = eng.playing && lastBeat == i;
        Col c = on ? mix(C_ACC, C_PLAY, beatPulse * 0.6f) : C_OFF2;
        rrect(146 + i * 10, 8, 7, 7, 2, c);
    }
    snprintf(b, sizeof b, "SW %d%%", 50 + eng.proj.mst[MS_SWNG]);
    text(84, 21, b, C_DIM);
    if (eng.recording) {
        bool bl = (SDL_GetTicks() / 400) % 2;
        rrect(140, 24, 9, 9, 4, bl ? C_TRIG : dimC(C_TRIG, 0.5f));
        text(152, 21, "REC", C_TRIG);
    }
    snprintf(b, sizeof b, "T%d %s", track + 1, MACHINE_NAMES[curP().machine[track]]);
    text(196, 4, b, eng.mute[track] ? C_DIM : tc, 2);
    for (int p = 0; p < NUI_PAGES; p++) {
        int x = 336 + p * 30;
        if (p == page) { rrect(x - 1, 3, 31, 18, 4, C_ACC); textC(x + 15, 4, PAGE_NAMES[p], C_BG); }
        else textC(x + 15, 4, PAGE_NAMES[p], p >= PAGE_FX ? dimC(C_DIM, 0.8f) : C_DIM);
    }
    if (aHeld) {
        snprintf(b, sizeof b, "TRIG %02d HALTEN: P-LOCK", holdStep + 1);
        text(338, 23, b, C_LOCK);
    } else {
        snprintf(b, sizeof b, "CPU %2d%%  LEN %d", (int)(cpuLoad * 100), curP().len[track]);
        text(338, 23, b, C_DIM);
        if (strcmp(midiBackend(), "AUS")) {
            bool act = SDL_GetTicks() - midiActivity < 150;
            rrect(554, 27, 6, 6, 3, act ? C_GREEN : C_OFF2);
            text(564, 23, "MIDI", act ? C_GREEN : C_DIM);
        }
    }
    int ml = clampi((int)(eng.peakL * 34), 0, 34), mr = clampi((int)(eng.peakR * 34), 0, 34);
    rrect(620, 3, 6, 34, 2, C_OFF); rrect(629, 3, 6, 34, 2, C_OFF);
    if (eng.proj.cmp[CM_THR] < 127) {   // Gain-Reduktion des Kompressors (von oben nach unten)
        int gh = clampi((int)(grDisp / 24.f * 34), 0, 34);
        rrect(611, 3, 5, 34, 2, C_OFF);
        if (gh) rrect(611, 3, 5, gh, 2, C_TRIG);
    }
    if (ml) rrect(620, 37 - ml, 6, ml, 2, eng.peakL > 0.95f ? C_TRIG : C_GREEN);
    if (mr) rrect(629, 37 - mr, 6, mr, 2, eng.peakR > 0.95f ? C_TRIG : C_GREEN);
}

// Wert + Anzeige eines Reglers der aktuellen Seite ermitteln
static bool knobInfo(int k, ParamDef& d, int& val, bool& locked, int& baseVal, char* label, int ln) {
    const Pattern& P = curP();
    locked = false; label[0] = 0;
    if (page == PAGE_MST) {
        d = MST_DEFS[k];
        val = k == MS_BPM ? eng.proj.bpm : eng.proj.mst[k];
    } else if (page == PAGE_FX) {
        d = FX_DEFS[k];
        val = eng.proj.fx[k];
    } else if (page == PAGE_CMP) {
        d = CMP_DEFS[k];
        val = k == CM_GR ? 0 : eng.proj.cmp[k];
        if (k == CM_SRC && val) snprintf(label, ln, "SC: T%d %s", val, MACHINE_NAMES[P.machine[val - 1]]);
        if (k == CM_THR && val >= 127 && eng.proj.cmp[CM_GAIN] == 0) snprintf(label, ln, "THR (AUS)");
    } else if (page == PAGE_SYS) {
        d = SYS_DEFS[k];
        switch (k) {
        case SY_OUT: val = devIndex(); d.max = (uint8_t)devCount; snprintf(label, ln, "%.19s", val ? devNames[val - 1] : "STANDARD"); break;
        case SY_BUF: val = bufOverride ? 0 : cfg.buf; break;
        case SY_MIN: val = cfg.midiIn; break;
        case SY_NOUT: val = cfg.notesOut; break;
        case SY_CLK: val = cfg.clk; break;
        case SY_MCH: val = cfg.chm; break;
        case SY_USB: val = usbMode; snprintf(label, ln, "%s", usbMode ? "R36S AM PC" : "GERAETE AM R36S"); break;
        default: {
            char nm[256]; val = midiPortInfo(nm, sizeof nm);
            snprintf(label, ln, "%.19s", strcmp(midiBackend(), "AUS") ? nm : "MIDI AUS");
        } break;
        }
        if (k == SY_BUF && bufOverride) snprintf(label, ln, "--buffer %d", bufOverride);
    } else {
        int idx = page * 8 + k;
        d = paramDef(P.machine[track], idx);
        val = P.p[track][idx];
        if (d.kind == K_MACH) val = P.machine[track];
        else if (aHeld && holdS().lock[idx] != NO_LOCK) { val = holdS().lock[idx]; locked = true; }
        if (d.kind == K_SMPL) {
            d.max = (uint8_t)(sampleCount() > 0 ? sampleCount() - 1 : 0);
            snprintf(label, ln, "%.19s", sampleCount() ? sampleName(val) : "KEINE SAMPLES");
        }
    }
    baseVal = (page < PAGE_FX && d.kind != K_MACH) ? P.p[track][page * 8 + k] : val;
    return d.kind != K_NONE;
}

static void knobText(const ParamDef& d, int val, char* b, int n) {
    if (d.kind == K_GR) snprintf(b, n, "-%.1f", grDisp);
    else if (d.kind == K_SMPL) snprintf(b, n, "%d", val + 1);
    else if (d.kind == K_DEV) { if (val) snprintf(b, n, "#%d", val); else snprintf(b, n, "STD"); }
    else fmtParam(d, val, b, n);
}
static float knobFrac(const ParamDef& d, int val) {
    if (d.kind == K_BPM) return (val - 30) / 270.f;
    if (d.kind == K_GR) return grDisp / 24.f > 1.f ? 1.f : grDisp / 24.f;
    return d.max ? (float)val / d.max : 0.f;
}

static void drawKnobs() {
    char b[32], label[24];
    Col tc = TCOL[track];
    Uint32 now = SDL_GetTicks();
    for (int k = 0; k < 8; k++) {
        int x = (k % 4) * 160, y = 44 + (k / 4) * 96;
        bool sel = k == slot;
        ParamDef d; int val, baseVal; bool locked;
        bool ok = knobInfo(k, d, val, locked, baseVal, label, sizeof label);
        if (sel) { rrect(x + 3, y + 1, 154, 92, 8, C_SEL); rrect(x + 3, y + 10, 3, 74, 1, ok ? tc : C_DIM); }
        int cx = x + 80, cy = y + 38;
        if (!ok) {
            arc(cx, cy, 27, 135, 405, C_OFF, 2);
            textC(cx, cy - 8, "-", C_DIM);
            continue;
        }
        float frac = knobFrac(d, val);
        bool justEdited = sel && editAt && now - editAt < 300;
        Col kc = locked ? C_LOCK : (sel ? (page >= PAGE_FX ? C_ACC : tc) : C_TEXT);
        if (d.kind == K_GR) kc = C_TRIG;
        if (justEdited) kc = mix(kc, C_PLAY, 0.5f * (1.f - (now - editAt) / 300.f));
        knob(cx, cy, 27, frac, d.kind == K_BIP, kc);
        if (locked) {   // Grundwert als Markierung
            float a = (135 + 270 * knobFrac(d, baseVal)) * 0.0174533f;
            fill(cx + (int)(31 * cosf(a)) - 1, cy + (int)(31 * sinf(a)) - 1, 3, 3, C_DIM);
        }
        knobText(d, val, b, sizeof b);
        textC(cx, cy - 8, b, locked ? C_LOCK : C_TEXT);
        textC(cx, y + 70, label[0] ? label : d.name, sel ? (locked ? C_LOCK : C_TEXT) : C_DIM);
    }
    // grosse Werteanzeige beim Drehen bzw. Meldung (die juengste gewinnt)
    bool showEdit = editAt && (now - editAt < 900 || editAt == 1) && editAt >= msgAt;
    bool showMsg = now < msgUntil && msgAt > editAt;
    if (showEdit) {
        ParamDef d; int val, baseVal; bool locked;
        if (knobInfo(editSlot, d, val, locked, baseVal, label, sizeof label)) {
            knobText(d, val, b, sizeof b);
            Col c = locked ? C_LOCK : (page >= PAGE_FX ? C_ACC : tc);
            rrect(150, 204, 340, 34, 8, C_BG, 240);
            rrect(150, 204, 4, 34, 2, c);
            text(164, 213, label[0] ? label : d.name, C_DIM);
            int bx = 330, bw = 100;
            rrect(bx, 217, bw, 8, 3, C_OFF2);
            int fw = (int)(bw * knobFrac(d, val));
            if (fw > 0) rrect(bx, 217, fw, 8, 3, c);
            text(478 - textW(b, 1) - 2, 213, b, c);
            if (locked) text(bx, 205, "LOCK", C_LOCK);
            if (d.kind == K_MACH) {   // Auswahlleiste aller Engines
                rrect(150, 204, 340, 34, 8, C_BG, 250);
                rrect(150, 204, 4, 34, 2, c);
                for (int m = 0; m < M_COUNT; m++) {
                    int mx = 160 + m * 47;
                    if (m == val) rrect(mx - 2, 211, 46, 20, 5, c);
                    textC(mx + 21, 213, MACHINE_NAMES[m], m == val ? C_BG : C_DIM);
                }
            }
        }
    } else if (showMsg) {
        int w = textW(msg) + 28;
        rrect(320 - w / 2, 210, w, 26, 8, C_ACC);
        textC(320, 215, msg, C_BG);
    }
    // Seitenwechsel: Seitenname kurz gross einblenden
    if (pageAt && now - pageAt < 450) {
        textAlpha = (Uint8)(255 * (1.f - (now - pageAt) / 450.f));
        textC(320, 120, PAGE_NAMES[page], C_PLAY, 4);
        textAlpha = 255;
    }
}

static Col offCol(int s) { return (s / 4) % 2 ? C_OFF2 : C_OFF; }

static void drawGrid() {
    const Pattern& P = curP();
    char b[16];
    int y0 = 244;
    for (int t = 0; t < NTRACKS; t++) {
        int y = y0 + t * 18;
        Col tc = eng.mute[t] ? dimC(TCOL[t], 0.35f) : TCOL[t];
        if (t == track) rrect(0, y - 2, SW, 18, 4, C_PANEL);
        rrect(4, y, 4, 14, 1, mix(tc, C_PLAY, flash[t] * 0.7f));   // Farbchip, blitzt beim Trig
        snprintf(b, sizeof b, "%d %s", t + 1, MACHINE_NAMES[P.machine[t]]);
        text(12, y - 2, b, eng.mute[t] ? C_DIM : (t == track ? TCOL[t] : C_TEXT));
        if (eng.mute[t]) { fill(12, y + 5, textW(b), 1, C_DIM); text(68, y - 2, "M", C_TRIG); }
        int mw = clampi((int)(sqrtf(meterV[t]) * 62), 0, 62);   // Pegel der Spur
        fill(12, y + 13, 62, 2, C_OFF);
        if (mw) fill(12, y + 13, mw, 2, tc);
        int len = P.len[t];
        for (int s = 0; s < NSTEPS; s++) {
            int x = 82 + s * 34;
            if (s >= len) continue;
            const Step& st = P.s[t][s];
            bool ph = eng.playing && eng.playedStep[t] == s;
            if (t == track && s == cursor) rrect(x - 2, y - 1, 35, 16, 4, aHeld ? C_LOCK : C_TEXT);
            Col c = st.on ? tc : offCol(s);
            if (ph) c = st.on ? mix(tc, C_PLAY, 0.25f + flash[t] * 0.6f) : mix(c, C_PLAY, 0.18f);
            rrect(x, y + 1, 31, 12, 3, c);
            if (st.on && stepHasLock(st)) fill(x + 25, y + 3, 3, 3, C_BG);
        }
    }
}

static void drawSteps() {
    const Pattern& P = curP();
    char b[8];
    Col tc = eng.mute[track] ? dimC(TCOL[track], 0.35f) : TCOL[track];
    int len = P.len[track];
    if (eng.recording) rrect(2, 388, 628, 54, 8, (SDL_GetTicks() / 400) % 2 ? C_TRIG : dimC(C_TRIG, 0.5f));
    if (eng.recording) rrect(4, 390, 624, 50, 7, C_BG);
    for (int s = 0; s < NSTEPS; s++) {
        int x = 6 + s * 39, y = 392;
        if (s >= len) { rrect(x, y, 36, 46, 6, C_DARK); continue; }
        const Step& st = P.s[track][s];
        bool ph = eng.playing && eng.playedStep[track] == s;
        if (s == cursor) rrect(x - 3, y - 3, 42, 52, 8, aHeld ? C_LOCK : C_TEXT);
        Col c = st.on ? tc : offCol(s);
        if (ph && st.on) c = mix(tc, C_PLAY, 0.2f + flash[track] * 0.6f);
        rrect(x, y, 36, 46, 6, c);
        if (ph) rrect(x + 6, y + 4, 24, 4, 2, st.on ? C_BG : C_PLAY);
        if (st.on && stepHasLock(st)) rrect(x + 27, y + 11, 5, 5, 2, C_BG);
        if (st.on && st.lock[P_NOTE] != NO_LOCK) {   // gelockte Note auf der Taste
            char nb[8];
            fmtParam(COMMON_DEFS[P_NOTE], st.lock[P_NOTE], nb, sizeof nb);
            textC(x + 18, y + 13, nb, dimC(C_BG, 0.85f));
        }
        snprintf(b, sizeof b, "%d", s + 1);
        textC(x + 18, y + 27, b, st.on ? C_BG : C_DIM);
    }
    const char* h;
    if (aHeld) h = "[X/Y/STICK] LOCK SETZEN   [B] LOCKS LOESCHEN   [A] LOSLASSEN = FERTIG";
    else if (in.down[BT_SELECT]) h = "[</>]PTN [^]REC [v]TAP [A]MUTE [B]CLR [X/Y]MASCH [L1/R1]CPY/PST";
    else if (page == PAGE_SYS) h = "[X/Y] AENDERN  [L1/R1] EINSTELLUNG  [L2/R2] SEITE  [START] PLAY";
    else if (page == PAGE_FX) h = "[X/Y] WERT [L1/R1] REGLER   SENDS PRO SPUR: AMP-SEITE (DLY/REV)";
    else if (page == PAGE_CMP) h = "[X/Y] WERT [L1/R1] REGLER   SC=T1 + RATIO HOCH = KICK DUCKT ALLES";
    else if (page == PAGE_MST) h = "[X/Y] WERT  [L1/R1] REGLER  [L2/R2] SEITE  [SEL+v] TAP-TEMPO";
    else h = "[A]TRIG [B]HOER [X/Y]WERT [L1/R1]REGLER [L2/R2]SEITE [SEL]FUNC";
    hint(6, 452, h, C_ACC, C_DIM);
}

static void drawHelp() {
    rrect(30, 30, 580, 420, 12, C_ACC);
    rrect(32, 32, 576, 416, 11, C_BG, 245);
    static const char* lines[] = {
        "SELECT (FUNC) GEHALTEN +",
        "  LINKS/RECHTS   Pattern wechseln",
        "  HOCH / RUNTER  Aufnahme an/aus / Tap-Tempo",
        "  A              Spur stumm",
        "  B              Spur loeschen",
        "  X / Y          Maschine wechseln",
        "  L1 / R1        Pattern kopieren / einfuegen",
        "  L2 / R2        Spurlaenge -/+",
        "  START          Speichern (2s halten: Ende)",
        "",
        "OHNE FUNC",
        "  A              Trig setzen/loeschen",
        "  A halten+X/Y   Parameter-Lock auf Step",
        "  A halten+B     Locks des Steps loeschen",
        "  B              Spur vorhoeren",
        "  Linker Stick   Wert stufenlos",
        "  Rechter Stick  DJ-Filter (LP/HP)",
        "  START          Play / Stop",
    };
    for (unsigned i = 0; i < sizeof lines / sizeof *lines; i++)
        text(48, 44 + i * 21, lines[i], i == 0 || i == 10 ? C_ACC : C_TEXT);
}

static void drawAll(bool help) {
    animate();
    setCol(C_BG);
    SDL_RenderClear(ren);
    drawHeader();
    drawKnobs();
    drawGrid();
    drawSteps();
    if (help) drawHelp();
}

// ---------------------------------------------------------------- Bedienung

static void editValue(int delta) {
    editAt = SDL_GetTicks(); editSlot = slot;
    if (page == PAGE_SYS) {
        int d = delta > 0 ? 1 : -1;
        switch (slot) {
        case SY_OUT: {
            refreshDevices();
            int i = clampi(devIndex() + d, 0, devCount);
            snprintf(cfg.dev, sizeof cfg.dev, "%s", i ? devNames[i - 1] : "");
            audioApplyAt = SDL_GetTicks() + 800;
        } break;
        case SY_BUF: cfg.buf = clampi(cfg.buf + d, 0, 3); bufOverride = 0; audioApplyAt = SDL_GetTicks() + 800; break;
        case SY_MIN: cfg.midiIn = d > 0; break;
        case SY_NOUT: cfg.notesOut = d > 0; break;
        case SY_CLK: cfg.clk = clampi(cfg.clk + d, 0, 2); break;
        case SY_MCH: cfg.chm = d > 0; break;
        case SY_USB: usbMode = clampi(usbMode + d, 0, 2); usbApplyAt = SDL_GetTicks() + 1200; return;
        default: return;
        }
        applyMidiSettings();
        saveSettings();
        return;
    }
    if (page == PAGE_FX) {
        eng.proj.fx[slot] = (uint8_t)clampi(eng.proj.fx[slot] + delta, 0, FX_DEFS[slot].max);
        return;
    }
    if (page == PAGE_CMP) {
        if (slot != CM_GR) eng.proj.cmp[slot] = (uint8_t)clampi(eng.proj.cmp[slot] + delta, 0, CMP_DEFS[slot].max);
        return;
    }
    if (page == PAGE_MST) {
        if (slot == MS_BPM) eng.proj.bpm = (uint16_t)clampi(eng.proj.bpm + delta, 30, 300);
        else if (MST_DEFS[slot].kind != K_NONE) eng.proj.mst[slot] = (uint8_t)clampi(eng.proj.mst[slot] + delta, 0, MST_DEFS[slot].max);
        return;
    }
    Pattern& P = curP();
    int idx = page * 8 + slot;
    ParamDef d = paramDef(P.machine[track], idx);
    if (d.kind == K_NONE) return;
    if (d.kind == K_SMPL) d.max = (uint8_t)(sampleCount() > 0 ? sampleCount() - 1 : 0);
    if (d.kind == K_MACH) {   // Engine der Spur (nicht lockbar)
        if (aHeld) return;
        int m = clampi(P.machine[track] + (delta > 0 ? 1 : -1), 0, M_COUNT - 1);
        if (m != P.machine[track]) { lockA(); Engine::setMachine(P, track, m); unlockA(); lockA(); eng.preview(track, nullptr); unlockA(); }
        return;
    }
    if (aHeld) {
        Step& s = holdS();
        int cur = s.lock[idx] != NO_LOCK ? s.lock[idx] : eng.proj.pat[holdPat].p[holdTrack][idx];
        s.lock[idx] = (uint8_t)clampi(cur + delta, 0, d.max);
        aEdited = true;
    } else {
        P.p[track][idx] = (uint8_t)clampi(P.p[track][idx] + delta, 0, d.max);
    }
    if (idx == P_NOTE || d.kind == K_SMPL) { lockA(); eng.preview(track, aHeld ? &holdS() : nullptr); unlockA(); }
}

static int curMax() {
    if (page == PAGE_MST) return slot == MS_BPM ? 270 : MST_DEFS[slot].max;
    if (page == PAGE_SYS) return 1;
    if (page == PAGE_CMP) return CMP_DEFS[slot].max;
    if (page == PAGE_FX) return FX_DEFS[slot].max;
    return paramDef(curP().machine[track], page * 8 + slot).max;
}

static void handleInput(Uint32 now, float dt) {
    Input& I = in;
    bool func = I.down[BT_SELECT];
    if (I.pressed(BT_SELECT)) funcUsed = false;

    if (func) {
        if (I.pressed(BT_LEFT) || I.pressed(BT_RIGHT)) {
            funcUsed = true;
            int d = I.pressed(BT_RIGHT) ? 1 : -1;
            int from = eng.nextPat >= 0 ? eng.nextPat : eng.curPat;
            int to = (from + d + NPATTERNS) % NPATTERNS;
            lockA();
            if (eng.playing) eng.nextPat = to; else { eng.curPat = to; eng.nextPat = -1; }
            unlockA();
            toast("PATTERN %02d", to + 1);
        }
        if (I.pressed(BT_UP)) {
            funcUsed = true;
            eng.recording = !eng.recording;
            toast(eng.recording ? "AUFNAHME AN" : "AUFNAHME AUS");
        }
        if (I.pressed(BT_DOWN)) {   // Tap-Tempo
            funcUsed = true;
            if (tapN && now - taps[(tapN - 1) % 4] > 2000) tapN = 0;
            taps[tapN % 4] = now; tapN++;
            int n = tapN < 4 ? tapN : 4;
            if (n >= 2) {
                Uint32 newest = taps[(tapN - 1) % 4], oldest = taps[(tapN - n) % 4];
                float iv = (newest - oldest) / (float)(n - 1);
                if (iv > 0) eng.proj.bpm = (uint16_t)clampi((int)(60000.f / iv + 0.5f), 30, 300);
                toast("TAP %d BPM", eng.proj.bpm);
            } else toast("TAP ...");
        }
        if (I.pressed(BT_A)) { funcUsed = true; eng.mute[track] = !eng.mute[track]; toast(eng.mute[track] ? "SPUR %d STUMM" : "SPUR %d AN", track + 1); }
        if (I.pressed(BT_B)) {
            funcUsed = true;
            lockA();
            for (int s = 0; s < NSTEPS; s++) { curP().s[track][s].on = 0; memset(curP().s[track][s].lock, NO_LOCK, NPARAMS); }
            unlockA();
            toast("SPUR %d GELOESCHT", track + 1);
        }
        if (I.pressed(BT_X) || I.pressed(BT_Y)) {
            funcUsed = true;
            int m = (curP().machine[track] + (I.pressed(BT_X) ? 1 : M_COUNT - 1)) % M_COUNT;
            lockA(); Engine::setMachine(curP(), track, m); unlockA();
            toast("MASCHINE: %s", MACHINE_NAMES[m]);
        }
        if (I.pressed(BT_L1)) { funcUsed = true; clip = curP(); haveClip = true; toast("PATTERN KOPIERT"); }
        if (I.pressed(BT_R1)) {
            funcUsed = true;
            if (haveClip) { lockA(); curP() = clip; unlockA(); toast("PATTERN EINGEFUEGT"); }
            else toast("ZWISCHENABLAGE LEER");
        }
        if (I.pressed(BT_L2) || I.pressed(BT_R2)) {
            funcUsed = true;
            Pattern& P = curP();
            P.len[track] = (uint8_t)clampi(P.len[track] + (I.pressed(BT_R2) ? 1 : -1), 1, NSTEPS);
            toast("SPURLAENGE %d", P.len[track]);
        }
        if (I.pressed(BT_START)) {
            funcUsed = true;
            bool ok = saveProject();
            toast(ok ? "GESPEICHERT" : "SPEICHERN FEHLGESCHLAGEN");
        }
        if (I.down[BT_START] && I.heldMs(BT_START, now) > 2000) quitReq = true;
    } else {
        if (!aHeld) {
            if (I.repeat(BT_LEFT, now, 250, 70)) cursor = (cursor + NSTEPS - 1) % NSTEPS;
            if (I.repeat(BT_RIGHT, now, 250, 70)) cursor = (cursor + 1) % NSTEPS;
            if (I.repeat(BT_UP, now)) track = (track + NTRACKS - 1) % NTRACKS;
            if (I.repeat(BT_DOWN, now)) track = (track + 1) % NTRACKS;
        }
        if (I.pressed(BT_A)) {
            holdPat = eng.curPat; holdTrack = track; holdStep = cursor;
            Step& s = holdS();
            aJustOn = !s.on;
            if (!s.on) { memset(s.lock, NO_LOCK, NPARAMS); s.on = 1; }
            aHeld = true; aEdited = false;
        }
        if (I.pressed(BT_B)) {
            if (aHeld) { memset(holdS().lock, NO_LOCK, NPARAMS); aEdited = true; toast("LOCKS GELOESCHT"); }
            else { lockA(); eng.preview(track, &curP().s[track][cursor]); unlockA(); }
        }
        if (I.repeat(BT_L1, now)) slot = (slot + 7) % 8;
        if (I.repeat(BT_R1, now)) slot = (slot + 1) % 8;
        if (I.pressed(BT_L2)) page = (page + NUI_PAGES - 1) % NUI_PAGES;
        if (I.pressed(BT_R2)) page = (page + 1) % NUI_PAGES;
        if (I.pressed(BT_START)) {
            lockA();
            if (eng.playing) eng.stop(); else eng.start();
            unlockA();
            if (!eng.playing && eng.clockMode == 2) toast("WARTE AUF MIDI-START");
        }
        int big = curMax() >= 100;
        int mul = (big && (I.heldMs(BT_X, now) > 1200 || I.heldMs(BT_Y, now) > 1200)) ? 5 : 1;
        if (I.repeat(BT_X, now, 300, 45)) editValue(mul);
        if (I.repeat(BT_Y, now, 300, 45)) editValue(-mul);
        float ly = I.ly;
        if (fabsf(ly) > 0.25f) {
            float dz = (fabsf(ly) - 0.25f) / 0.75f;
            float rate = (big ? 90.f : 8.f) * dz * dz + 2.f;
            stickAcc += (ly > 0 ? 1.f : -1.f) * rate * dt;
            int d = (int)stickAcc;
            if (d) { stickAcc -= d; editValue(d); }
        } else stickAcc = 0;
    }
    if (I.released(BT_A) && aHeld) {
        Step& s = holdS();
        if (!aJustOn && !aEdited) { s.on = 0; memset(s.lock, NO_LOCK, NPARAMS); }
        aHeld = false;
    }
    if (aHeld && !I.down[BT_A]) aHeld = false;
    float ry = I.ry;
    if (fabsf(ry) > 0.2f) { eng.dj = ry > 0 ? (ry - 0.2f) / 0.8f : (ry + 0.2f) / 0.8f; stickDj = true; }
    else if (stickDj) { eng.dj = 0.f; stickDj = false; }
    // Zustand fuer MIDI-Eingang / Step-Aufnahme
    eng.uiTrack = track; eng.uiPage = page; eng.uiCursor = cursor;
    if (eng.stepRecCount != lastStepRec) {
        lastStepRec = eng.stepRecCount;
        int len = curP().len[track] < 1 ? 1 : curP().len[track];
        cursor = (cursor + 1) % len;
    }
    if (I.pressed(BT_R2) || I.pressed(BT_L2)) if (page == PAGE_SYS) refreshDevices();
}

// ---------------------------------------------------------------- Lernmodus

static bool runLearn() {
    SDL_Joystick* j = in.joy;
    if (!j) { printf("Kein Gamepad gefunden - Lernmodus uebersprungen\n"); return false; }
    static const char* prompts[BT_COUNT + 2] = {
        "STEUERKREUZ HOCH", "STEUERKREUZ RUNTER", "STEUERKREUZ LINKS", "STEUERKREUZ RECHTS",
        "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "SELECT", "START",
        "LINKEN STICK NACH OBEN", "RECHTEN STICK NACH OBEN"};
    int nb = SDL_JoystickNumButtons(j), nh = SDL_JoystickNumHats(j), na = SDL_JoystickNumAxes(j);
    Binding res[BT_COUNT];
    int axL = -1, axR = -1, sL = -1, sR = -1;
    for (int item = 0; item < BT_COUNT + 2; item++) {
        bool stick = item >= BT_COUNT;
        Uint8 bb[64] = {0}; Uint8 bh[8] = {0}; float ba[16] = {0};
        SDL_PumpEvents();
        for (int i = 0; i < nb && i < 64; i++) bb[i] = SDL_JoystickGetButton(j, i);
        for (int i = 0; i < nh && i < 8; i++) bh[i] = SDL_JoystickGetHat(j, i);
        for (int i = 0; i < na && i < 16; i++) ba[i] = SDL_JoystickGetAxis(j, i) / 32767.f;
        Uint32 t0 = SDL_GetTicks();
        bool done = false;
        while (!done) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) return false;
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) return false;
            }
            if (!stick) {
                for (int i = 0; i < nb && i < 64 && !done; i++)
                    if (SDL_JoystickGetButton(j, i) && !bb[i]) { res[item].type = 1; res[item].idx = i; done = true; }
                for (int i = 0; i < nh && i < 8 && !done; i++) {
                    int h = SDL_JoystickGetHat(j, i) & ~bh[i];
                    if (h) { res[item].type = 2; res[item].idx = i; res[item].val = h; done = true; }
                }
            }
            for (int i = 0; i < na && i < 16 && !done; i++) {
                float a = SDL_JoystickGetAxis(j, i) / 32767.f;
                if (fabsf(a - ba[i]) > 0.6f) {
                    int sg = a > ba[i] ? 1 : -1;
                    if (stick) { if (item == BT_COUNT) { axL = i; sL = sg; } else { axR = i; sR = sg; } }
                    else { res[item].type = 3; res[item].idx = i; res[item].val = sg; }
                    done = true;
                }
            }
            int left = stick ? 8 - (int)((SDL_GetTicks() - t0) / 1000) : 99;
            if (stick && left <= 0) done = true;

            setCol(C_BG); SDL_RenderClear(ren);
            textC(SW / 2, 40, "TASTEN LERNEN", C_ACC, 2);
            textC(SW / 2, 150, "DRUECKE:", C_DIM);
            textC(SW / 2, 190, prompts[item], C_TEXT, 2);
            char b[64];
            snprintf(b, sizeof b, "%d / %d", item + 1, BT_COUNT + 2);
            textC(SW / 2, 260, b, C_DIM);
            if (stick) { snprintf(b, sizeof b, "Kein Stick? Ueberspringen in %ds", left); textC(SW / 2, 300, b, C_DIM); }
            SDL_RenderPresent(ren);
            SDL_Delay(10);
        }
        // warten bis alles losgelassen
        Uint32 t1 = SDL_GetTicks();
        for (;;) {
            SDL_PumpEvents();
            bool any = false;
            for (int i = 0; i < nb && i < 64; i++) if (SDL_JoystickGetButton(j, i) && !bb[i]) any = true;
            for (int i = 0; i < nh && i < 8; i++) if (SDL_JoystickGetHat(j, i) & ~bh[i]) any = true;
            for (int i = 0; i < na && i < 16; i++) if (fabsf(SDL_JoystickGetAxis(j, i) / 32767.f - ba[i]) > 0.3f) any = true;
            if (!any || SDL_GetTicks() - t1 > 5000) break;
            SDL_Delay(10);
        }
        SDL_Delay(120);
    }
    for (int b = 0; b < BT_COUNT; b++) in.map[b] = res[b];
    in.axLY = axL; in.sgnLY = sL; in.axRY = axR; in.sgnRY = sR;
    in.haveMap = true;
    if (in.gc) { in.gc = nullptr; }  // ab jetzt Rohdaten des Joysticks benutzen
    bool ok = in.saveMap(cfgPath);
    printf("Tastenbelegung %s: %s\n", ok ? "gespeichert" : "NICHT gespeichert", cfgPath);
    return true;
}

// ---------------------------------------------------------------- Audio / Hilfen

static void midiOutHook(uint8_t st, uint8_t d1, uint8_t d2, int off) {
    MidiMsg m = {st, d1, d2, 0, cbTime + (double)(cbFrames + off) / eng.sr};
    midiOutPush(m);
}

static void audioCb(void*, Uint8* stream, int len) {
    Uint64 t0 = SDL_GetPerformanceCounter();
    int frames = len / (int)(sizeof(float) * 2);
    cbTime = midiNow();
    cbFrames = frames;
    MidiMsg mm;
    while (midiInPop(mm)) eng.midiIn(mm.st, mm.d1, mm.d2);
    eng.render((float*)stream, frames);
    double el = (double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency();
    double avail = (double)frames / eng.sr;
    cpuLoad = cpuLoad * 0.9f + 0.1f * (float)(el / avail);
}

static bool writeWav(const char* path, const float* data, int frames, int sr) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int bytes = frames * 4;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(sr); w32(sr * 4); w16(4); w16(16);
    fwrite("data", 1, 4, f); w32(bytes);
    for (int i = 0; i < frames * 2; i++) {
        float x = data[i] < -1 ? -1 : (data[i] > 1 ? 1 : data[i]);
        w16((uint16_t)(int16_t)(x * 32767));
    }
    fclose(f);
    return true;
}

static void openAudio() {
    if (audioDev) { SDL_CloseAudioDevice(audioDev); audioDev = 0; }
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100; want.format = AUDIO_F32SYS; want.channels = 2;
    want.samples = (Uint16)(bufOverride ? bufOverride : BUF_SIZES[cfg.buf]);
    want.callback = audioCb;
    const char* name = cfg.dev[0] ? cfg.dev : nullptr;
    audioDev = SDL_OpenAudioDevice(name, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!audioDev && name) {
        printf("Audio: '%s' nicht verfuegbar (%s) - nehme Standardausgang\n", name, SDL_GetError());
        toast("AUSGANG NICHT VERFUEGBAR");
        audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    }
    if (!audioDev) { printf("Audio: %s\n", SDL_GetError()); return; }
    printf("Audio: %s, %d Hz, Puffer %d Frames (%.1f ms)\n", name ? name : "Standard", have.freq, have.samples,
           have.samples * 1000.0 / have.freq);
    eng.setRate(have.freq);
    SDL_PauseAudioDevice(audioDev, 0);
}

static int detectUsbMode() {
    FILE* f = fopen("/sys/kernel/config/usb_gadget/groovebox/UDC", "r");
    if (!f) return 0;
    char b[64] = "";
    bool on = fgets(b, sizeof b, f) && b[0] > ' ';
    fclose(f);
    if (!on) return 0;
    struct stat st;
    return stat("/sys/kernel/config/usb_gadget/groovebox/configs/c.1/uac2.usb0", &st) == 0 ? 2 : 1;
}

static void applyUsbMode() {
    char cmd[800];
    static const char* args[3] = {"aus", "an", "an audio"};
    snprintf(cmd, sizeof cmd, "bash '%spc-modus.sh' %s nosleep", basePath, args[usbMode]);
    toast(usbMode ? "PC-MODUS WIRD AKTIVIERT ..." : "PC-MODUS AUS ...");
    drawAll(false); SDL_RenderPresent(ren);
    int rc = system(cmd);
    int now = detectUsbMode();
    printf("USB-Modus %d: rc=%d, erkannt=%d\n", usbMode, rc, now);
    if (usbMode && !now) { usbMode = 0; toast("PC-MODUS NICHT MOEGLICH - SIEHE PC-MODUS.LOG"); }
    else toast(usbMode ? "PC-MODUS AKTIV" : "USB: HOST-MODUS");
    usbMode = now;
    refreshDevices();
}

static void setupPaths() {
    char* bp = SDL_GetBasePath();
    if (bp) { snprintf(basePath, sizeof basePath, "%s", bp); SDL_free(bp); }
    char dir[600];
    snprintf(dir, sizeof dir, "%sprojects", basePath);
    mkdir(dir, 0755);
    if (!projPath[0]) snprintf(projPath, sizeof projPath, "%s/project1.gbx", dir);
    snprintf(cfgPath, sizeof cfgPath, "%sinput.cfg", basePath);
    snprintf(setPath, sizeof setPath, "%ssettings.cfg", basePath);
    char sdir[600];
    snprintf(sdir, sizeof sdir, "%ssamples", basePath);
    mkdir(sdir, 0755);
    int ns = samplesLoad(sdir);
    printf("%d Samples geladen aus %s\n", ns, sdir);
}

int main(int argc, char** argv) {
    bool learn = false, windowed = false;
    const char* shot = nullptr; const char* renderOut = nullptr; float renderSecs = 8; int shotPage = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--learn")) learn = true;
        else if (!strcmp(argv[i], "--windowed")) windowed = true;
        else if (!strcmp(argv[i], "--buffer") && i + 1 < argc) bufOverride = clampi(atoi(argv[++i]), 64, 4096);
        else if (!strcmp(argv[i], "--project") && i + 1 < argc) snprintf(projPath, sizeof projPath, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) { shot = argv[++i]; if (i + 1 < argc && argv[i + 1][0] != '-') shotPage = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--render") && i + 1 < argc) { renderOut = argv[++i]; if (i + 1 < argc && argv[i + 1][0] != '-') renderSecs = (float)atof(argv[++i]); }
        else { printf("Optionen: --learn --windowed --buffer FRAMES --project DATEI --render WAV [sek] --shot BMP [seite]\n"); return 0; }
    }
    setupPaths();

    if (renderOut) {   // Offline-Bounce ohne Grafik/Audiogeraet
        eng.init(44100);
        if (eng.load(projPath)) printf("Projekt geladen: %s\n", projPath);
        eng.start();
        int frames = (int)(renderSecs * 44100);
        float* buf = (float*)malloc(sizeof(float) * 2 * frames);
        Uint64 t0 = SDL_GetPerformanceCounter();
        eng.render(buf, frames);
        double el = (double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency();
        printf("%.1fs Audio in %.3fs gerechnet (%.1f%% Echtzeit-Last)\n", renderSecs, el, el / renderSecs * 100);
        bool ok = writeWav(renderOut, buf, frames, 44100);
        free(buf);
        return ok ? 0 : 1;
    }

    if (shot) {        // Screenshot (Software-Renderer) zum Testen
        eng.init(44100);
        eng.load(projPath);
        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, SW, SH, 32, SDL_PIXELFORMAT_RGBA32);
        ren = SDL_CreateSoftwareRenderer(surf);
        makeFont();
        eng.start();
        static float tmp[44100];
        for (int k = 0; k < 6; k++) eng.render(tmp, 22050 / 2);
        // Seiten 0-7, 10 = Hilfe, 11 = P-Lock-Ansicht, 12 = Werte-Overlay
        page = shotPage % NUI_PAGES; track = page == 1 ? 7 : (page == 0 ? 4 : 0); cursor = 3; slot = page == 1 ? 0 : 1;
        refreshDevices();
        if (shotPage == 11) { holdPat = 0; holdTrack = 4; holdStep = 3; aHeld = true; page = 0; track = 4; slot = 0; }
        if (shotPage == 13) { page = 0; slot = 6; track = 4; editSlot = 6; editAt = 1; }
        if (shotPage == 6) {   // Sidechain-Beispiel
            eng.proj.cmp[CM_SRC] = 1; eng.proj.cmp[CM_THR] = 40; eng.proj.cmp[CM_RAT] = 6; slot = 5;
            for (int k = 0; k < 4; k++) eng.render(tmp, 22050 / 2);
            animate();
        }
        if (shotPage == 12) { page = 2; slot = 0; track = 4; animate(); editAt = SDL_GetTicks(); editSlot = 0; eng.recording = true; }
        lastPage = page;
        if (shotPage == 10) { page = 0; drawAll(true); } else drawAll(false);
        SDL_RenderPresent(ren);
        SDL_SaveBMP(surf, shot);
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    char dbPath[600];
    snprintf(dbPath, sizeof dbPath, "%sgamecontrollerdb.txt", basePath);
    SDL_GameControllerAddMappingsFromFile(dbPath);
    if (in.loadMap(cfgPath)) printf("Tastenbelegung geladen: %s\n", cfgPath);
    in.open();
    SDL_ShowCursor(SDL_DISABLE);

    SDL_Window* win = SDL_CreateWindow("R36S Groovebox", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SW, SH,
                                       windowed ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { printf("Fenster: %s\n", SDL_GetError()); return 1; }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) { printf("Renderer: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, SW, SH);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    makeFont();

    if (learn || (!in.haveMap && in.joy && !in.gc)) runLearn();

    loadSettings();
    usbMode = detectUsbMode();
    refreshDevices();
    for (int i = 0; i < devCount; i++) printf("Audio-Ausgang #%d: %s\n", i + 1, devNames[i]);
    eng.init(44100);
    if (eng.load(projPath)) printf("Projekt geladen: %s\n", projPath);
    memcpy(&lastSaved, &eng.proj, sizeof(Project));
    applyMidiSettings();
    eng.midiOut = midiOutHook;
    midiStart();
    openAudio();
    toast("%d SAMPLES GELADEN", sampleCount());
    Uint32 lastAuto = SDL_GetTicks();

    Uint32 last = SDL_GetTicks();
    while (!quitReq) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quitReq = true;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) quitReq = true;
        }
        in.poll();
        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.f;
        last = now;
        handleInput(now, dt);
        if (audioApplyAt && now >= audioApplyAt) { audioApplyAt = 0; openAudio(); }
        if (usbApplyAt && now >= usbApplyAt) { usbApplyAt = 0; applyUsbMode(); }
        if (now - lastAuto > 120000) {   // Autosave alle 2 Minuten bei Aenderungen
            lastAuto = now;
            if (memcmp(&lastSaved, &eng.proj, sizeof(Project))) saveProject();
        }
        bool help = in.down[BT_SELECT] && !funcUsed && in.heldMs(BT_SELECT, now) > 700;
        drawAll(help);
        SDL_RenderPresent(ren);
        Uint32 spent = SDL_GetTicks() - now;
        if (spent < 16) SDL_Delay(16 - spent);
    }

    lockA(); eng.stop(); unlockA();
    if (saveProject()) printf("Projekt gespeichert: %s\n", projPath);
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    midiStop();
    SDL_Quit();
    return 0;
}
