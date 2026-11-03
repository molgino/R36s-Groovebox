// R36S Groovebox - Elektron-artige Groovebox fuer den R36S (ArkOS) und Linux-Desktop
#include <SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <dirent.h>
#include "engine.h"
#include "input.h"
#include "font.h"
#include "samples.h"
#include "web.h"
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
static bool lockLatch = false;   // Lock-Modus eingerastet (A lange gedrueckt)
static Uint32 aDownAt = 0;
static int rsAxis = 0;            // rechter Stick: 0 frei, 1 senkrecht, 2 waagerecht (rastet bis zur Mitte ein)
static int holdPat = 0, holdTrack = 0, holdStep = 0;
static bool funcUsed = false;
static float stickAcc = 0;
static char msg[64] = "";
static Uint32 msgUntil = 0;
static Uint32 editAt = 0, msgAt = 0;
static int editSlot = 0;
static Pattern clip;
static bool haveClip = false;
static int prjSel = 0, presetSel = 0, mixRow = 0;
static const uint8_t MIX_P[4] = {P_VOL, P_PAN, P_DLY, P_REV};
static const char* const MIX_NAMES[4] = {"VOL", "PAN", "DLY", "REV"};
static bool menuOpen = false; static int menuSel = 0;
static bool confirmOpen = false; static char confirmText[80]; static int confirmAct = 0, confirmArg = 0;
static Uint32 selDownAt = 0;
static bool quitReq = false;
static volatile float cpuLoad = 0;
static char basePath[512] = "./", cfgPath[600], projPath[600];
static Project lastSaved;

// System-Einstellungen (settings.cfg)
struct Settings { char dev[128]; int buf, midiIn, notesOut, clk, chm, usb, song; };
static Settings cfg = {"", 1, 1, 0, 0, 0, 0, 0};
static bool projFromArg = false;
static char setPath[600];
static int bufOverride = 0;
static int devCount = 0;
static char devNames[16][128];
static Uint32 audioApplyAt = 0, usbApplyAt = 0;
static int usbMode = 0;
static char netIp[32] = "";
static void readNetIp() {
    char p[700]; snprintf(p, sizeof p, "%snetmidi.ip", basePath);
    netIp[0] = 0;
    FILE* f = fopen(p, "r");
    if (f) { if (fgets(netIp, sizeof netIp, f)) netIp[strcspn(netIp, "\r\n")] = 0; fclose(f); }
}   // 0 Host, 1 PC (MIDI), 2 PC (MIDI + Audio)
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
        else if (!strcmp(line, "usb_mode")) cfg.usb = clampi(atoi(v), 0, 2);
        else if (!strcmp(line, "song")) cfg.song = clampi(atoi(v), 0, 31);
    }
    fclose(f);
}
static void saveSettings() {
    FILE* f = fopen(setPath, "w");
    if (!f) return;
    fprintf(f, "# Groovebox Systemeinstellungen\naudio_device %s\nbuffer %d\nmidi_in %d\nmidi_notes_out %d\nmidi_clock %d\nmidi_channels %d\nusb_mode %d\nsong %d\n",
            cfg.dev, cfg.buf, cfg.midiIn, cfg.notesOut, cfg.clk, cfg.chm, cfg.usb, cfg.song);
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
static int lastPage = -1, lastTrackA = -1;
static float beatPulse = 0;
static float grDisp = 0;
static int lastBeat = -1;
static float animDt = 0.016f, animTime = 0;
static float slideOff = 0;                  // Seitenwechsel: Inhalt schiebt ein
static float knobAnim[8];                   // Regler gleiten zum Zielwert
static float hlY = -1, curXg = -1;          // Spur-Markierung und Cursor gleiten
static float uiLfoPh = 0, oscDrift = 0;
static int batPct = -1; static bool batChg = false; static Uint32 batAt = 0;
static char batPath[300] = "";

static void readBattery() {
    if (!batPath[0]) {
        DIR* d = opendir("/sys/class/power_supply");
        if (!d) { batPct = -2; return; }
        struct dirent* e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char p[300], ty[32] = "";
            snprintf(p, sizeof p, "/sys/class/power_supply/%s/type", e->d_name);
            FILE* f = fopen(p, "r");
            if (!f) continue;
            if (fgets(ty, sizeof ty, f) && !strncmp(ty, "Battery", 7)) snprintf(batPath, sizeof batPath, "/sys/class/power_supply/%s", e->d_name);
            fclose(f);
            if (batPath[0]) break;
        }
        closedir(d);
        if (!batPath[0]) { batPct = -2; return; }
    }
    char p[340], b[32] = "";
    snprintf(p, sizeof p, "%s/capacity", batPath);
    FILE* f = fopen(p, "r");
    if (f) { if (fgets(b, sizeof b, f)) batPct = atoi(b); fclose(f); }
    snprintf(p, sizeof p, "%s/status", batPath);
    f = fopen(p, "r");
    if (f) { if (fgets(b, sizeof b, f)) batChg = !strncmp(b, "Charging", 8) || !strncmp(b, "Full", 4); fclose(f); }
}

static void animate() {
    Uint32 now = SDL_GetTicks();
    float dt = lastFrame ? (now - lastFrame) / 1000.f : 0.016f;
    if (dt > 0.1f) dt = 0.1f;
    lastFrame = now;
    animDt = dt; animTime += dt;
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
    beatPulse -= dt * 3.f; if (beatPulse < 0) beatPulse = 0;
    if (page != lastPage) {
        if (lastPage >= 0) {
            pageAt = now;
            int fwd = (page - lastPage + NUI_PAGES) % NUI_PAGES;
            slideOff = fwd <= NUI_PAGES / 2 ? 90.f : -90.f;
        }
        lastPage = page;
    }
    if (track != lastTrackA) { if (lastTrackA >= 0 && slideOff == 0) slideOff = 0; lastTrackA = track; }
    slideOff *= expf(-dt * 16.f);
    if (fabsf(slideOff) < 0.5f) slideOff = 0;
    float k = 1.f - expf(-dt * 18.f);
    float ty = 244.f + track * 18.f;
    hlY = hlY < 0 ? ty : hlY + (ty - hlY) * k;
    {
        static int lastPg = -1;
        int pg = cursor / 16, cx = cursor % 16;
        if (pg != lastPg || curXg < 0) curXg = (float)cx; else curXg += (cx - curXg) * k;
        lastPg = pg;
    }
    oscDrift += dt;
    if (now - batAt > 10000 || batAt == 0) { batAt = now; readBattery(); }
}

// ---------------------------------------------------------------- Bildschirm

static Pattern& curP() { return eng.proj.pat[eng.curPat]; }
static Step& holdS() { return eng.proj.pat[holdPat].s[holdTrack][holdStep]; }

// Seiten: Spurseiten haben eine Speicherseite (je 8 Parameter), globale nicht
static bool trackPage(int p) { return p < FIRST_GLOBAL_PAGE; }
static int sp() { return PG_STORE[page]; }
static bool pageAvail(int p) { return p != PG_SRC2 || machineHasSrc2(curP().machine[track]); }
static void gotoPage(int dir) {
    for (int i = 0; i < NUI_PAGES; i++) { page = (page + dir + NUI_PAGES) % NUI_PAGES; if (pageAvail(page)) break; }
}
static int curScale() { return eng.proj.mst[MS_SCALE]; }

static bool stepHasLock(const Step& s) {
    for (int i = 0; i < NPARAMS; i++) if (s.lock[i] != NO_LOCK) return true;
    return false;
}

static void battery(int x, int y) {
    if (batPct < 0) return;
    Col c = batPct <= 15 ? C_TRIG : (batPct <= 30 ? C_LOCK : C_DIM);
    if (batChg) c = C_GREEN;
    if (batPct <= 10 && !batChg && (SDL_GetTicks() / 500) % 2) c = dimC(C_TRIG, 0.4f);
    rrect(x, y, 22, 11, 2, c);
    rrect(x + 1, y + 1, 20, 9, 2, C_PANEL);
    fill(x + 22, y + 3, 2, 5, c);
    int w = clampi(batPct * 18 / 100, 0, 18);
    if (w) rrect(x + 2, y + 2, w, 7, 1, c);
    if (batChg) {   // Blitz
        fill(x + 11, y + 2, 2, 4, C_PANEL); fill(x + 9, y + 5, 4, 1, C_PANEL); fill(x + 9, y + 6, 2, 3, C_PANEL);
    }
    char b[8]; snprintf(b, sizeof b, "%d%%", batPct);
    text(x + 27, y - 3, b, c);
}

static void drawIcon(int m, int x, int y, int s, Col c);
static void drawHeader() {
    char b[64];
    Col tc = TCOL[track];
    fill(0, 0, SW, 40, C_PANEL);
    // pulsierende Linie im Takt
    fill(0, 40, SW, 1, mix(dimC(tc, 0.5f), tc, beatPulse));
    if (beatPulse > 0.01f) fill(0, 41, SW, 1, tc, (Uint8)(120 * beatPulse));
    snprintf(b, sizeof b, "P%02d", eng.curPat + 1);
    text(8, 4, b, C_TEXT, 2);
    if (eng.nextPat >= 0) {
        if ((SDL_GetTicks() / 250) % 2) { snprintf(b, sizeof b, ">%02d", eng.nextPat + 1); text(58, 4, b, C_ACC); }
        int l0 = curP().len[0] < 1 ? 1 : curP().len[0];
        int prog = eng.playing ? (eng.playedStep[0] + 1) * 48 / l0 : 0;
        fill(8, 36, 48, 2, C_OFF); fill(8, 36, prog, 2, C_ACC);
    }
    playIcon(62, 20, eng.playing);
    snprintf(b, sizeof b, "%d BPM", eng.proj.bpm);
    text(84, 4, b, C_TEXT);
    for (int i = 0; i < 4; i++) {
        bool on = eng.playing && lastBeat == i;
        int grow = on ? (int)(beatPulse * 3) : 0;
        Col c = on ? mix(C_ACC, C_PLAY, beatPulse * 0.6f) : C_OFF2;
        rrect(146 + i * 10 - grow / 2, 8 - grow / 2, 7 + grow, 7 + grow, 3, c);
    }
    snprintf(b, sizeof b, "SW %d%%", 50 + eng.proj.mst[MS_SWNG]);
    text(84, 21, b, C_DIM);
    if (curScale() && !eng.recording && !eng.fill) { snprintf(b, sizeof b, "%s%s", NOTE_NAMES[(curScale() - 1) % 12], SCALE_NAMES[scaleType(curScale()) - 1]); text(140, 21, b, dimC(C_ACC, 0.8f)); }
    if (eng.recording) {
        float pl = 0.5f + 0.5f * sinf(animTime * 6.f);
        rrect(140, 24, 9, 9, 4, mix(dimC(C_TRIG, 0.4f), C_TRIG, pl));
        text(152, 21, "REC", C_TRIG);
    } else if (eng.fill) {
        float pl = 0.5f + 0.5f * sinf(animTime * 8.f);
        rrect(138, 21, 40, 16, 4, mix(dimC(C_LOCK, 0.5f), C_LOCK, pl));
        text(142, 21, "FILL", C_BG);
    }
    {
        int m = curP().machine[track] % M_COUNT;
        Col hc = eng.mute[track] ? C_DIM : mix(tc, C_PLAY, flash[track] * 0.35f);
        int is = 30 + (int)(flash[track] * 4);
        drawIcon(m, 206 - is / 2, 20 - is / 2, is, hc);
        text(226, 4, MACHINE_NAMES[m], hc, 2);
        snprintf(b, sizeof b, "T%d", track + 1);
        text(310, 2, b, C_DIM);
    }
    {   // Reiter nur der aktuellen Gruppe (Seitenmenue: SELECT kurz antippen)
        int g = PG_GROUP[page], x = 334;
        for (int p = 0; p < NUI_PAGES; p++) {
            if (PG_GROUP[p] != g || !pageAvail(p)) continue;
            int w = textW(PAGE_NAMES[p]) + 4;
            if (p == page) { rrect(x, 3, w, 18, 4, C_ACC); text(x + 2, 4, PAGE_NAMES[p], C_BG); }
            else text(x + 2, 4, PAGE_NAMES[p], C_DIM);
            x += w + 1;
        }
    }
    if (aHeld) {
        snprintf(b, sizeof b, lockLatch ? "LOCK-MODUS STEP %02d" : "TRIG %02d HALTEN: P-LOCK", holdStep + 1);
        text(332, 23, b, C_LOCK);
    } else {
        text(334, 23, GROUP_NAMES[PG_GROUP[page]], dimC(C_ACC, 0.8f));
        snprintf(b, sizeof b, "CPU %d%%", (int)(cpuLoad * 100));
        text(334 + textW(GROUP_NAMES[PG_GROUP[page]]) + 10, 23, b, C_DIM);
        if (strcmp(midiBackend(), "AUS")) {
            bool act = SDL_GetTicks() - midiActivity < 150;
            rrect(462, 27, 6, 6, 3, act ? C_GREEN : C_OFF2);
            text(471, 23, "MIDI", act ? C_GREEN : C_DIM);
        }
        battery(530, 26);
    }
    int ml = clampi((int)(eng.peakL * 34), 0, 34), mr = clampi((int)(eng.peakR * 34), 0, 34);
    rrect(620, 3, 6, 34, 2, C_OFF); rrect(629, 3, 6, 34, 2, C_OFF);
    if (eng.proj.cmp[CM_THR] < 127) {
        int gh = clampi((int)(grDisp / 24.f * 34), 0, 34);
        rrect(611, 3, 5, 34, 2, C_OFF);
        if (gh) rrect(611, 3, 5, gh, 2, C_TRIG);
    }
    if (ml) rrect(620, 37 - ml, 6, ml, 2, eng.peakL > 0.95f ? C_TRIG : C_GREEN);
    if (mr) rrect(629, 37 - mr, 6, mr, 2, eng.peakR > 0.95f ? C_TRIG : C_GREEN);
}

static bool songExists(int i);
static bool presetExists(int m, int i);

// Wert + Anzeige eines Reglers der aktuellen Seite ermitteln
static bool knobInfo(int k, ParamDef& d, int& val, bool& locked, int& baseVal, char* label, int ln) {
    const Pattern& P = curP();
    locked = false; label[0] = 0;
    if (page == PAGE_MST) {
        d = MST_DEFS[k];
        val = k == MS_BPM ? eng.proj.bpm : eng.proj.mst[k];
    } else if (page == PG_PRJ) {
        static const ParamDef PRJ_DEFS[8] = {
            {"SONG", K_SLOT, 0, 31}, {"LADEN", K_ACT, 0, 0}, {"SPEICHERN", K_ACT, 0, 0}, {"NEU", K_ACT, 0, 0},
            {"PRESET", K_SLOT, 0, 31}, {"P.LADEN", K_ACT, 0, 0}, {"P.SPEICHERN", K_ACT, 0, 0}, {"INIT", K_ACT, 0, 0}};
        d = PRJ_DEFS[k];
        val = k == 0 ? prjSel : (k == 4 ? presetSel : 0);
        if (k == 0) snprintf(label, ln, "SONG %02d %s", prjSel + 1, prjSel == cfg.song ? "(AKTIV)" : (songExists(prjSel) ? "" : "(LEER)"));
        if (k == 4) snprintf(label, ln, "%s %02d%s", MACHINE_NAMES[P.machine[track]], presetSel + 1, presetExists(P.machine[track], presetSel) ? "" : " (LEER)");
    } else if (page == PG_MIX) {
        d = PDEF_NONE;
        val = 0;
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
        case SY_USB: val = usbMode;
            if (usbMode && netIp[0]) snprintf(label, ln, "IP %s", netIp);
            else snprintf(label, ln, "%s", usbMode ? "R36S AM PC" : "GERAETE AM R36S");
            break;
        default: {
            char nm[256]; val = midiPortInfo(nm, sizeof nm);
            snprintf(label, ln, "%.19s", strcmp(midiBackend(), "AUS") ? nm : "MIDI AUS");
        } break;
        }
        if (k == SY_BUF && bufOverride) snprintf(label, ln, "--buffer %d", bufOverride);
    } else {
        int idx = sp() * 8 + k;
        d = paramDef(P.machine[track], idx);
        val = P.p[track][idx];
        if (d.kind == K_MACH) val = P.machine[track];
        else if (d.kind == K_TLEN) val = P.len[track];
        else if (d.kind == K_TSPD) val = P.speed[track] % 7;
        else if (aHeld && holdS().lock[idx] != NO_LOCK) { val = holdS().lock[idx]; locked = true; }
        if (page == PAGE_INS && k >= 1 && k <= 4) {
            int ty = (aHeld && holdS().lock[P_ITYP] != NO_LOCK) ? holdS().lock[P_ITYP] : P.p[track][P_ITYP];
            if (ty <= 0 || ty >= INS_COUNT || (k <= 3 && INS_PNAMES[ty][k - 1][0] == '-')) d = PDEF_NONE;
            else if (k <= 3) snprintf(label, ln, "%s", INS_PNAMES[ty][k - 1]);
        }
        if (idx == P_NOTE) {   // Note in die Skala der Spur einrasten
            val = quantizeNote(val, curScale());
        }
        if (d.kind == K_LDEST) snprintf(label, ln, "%s", val > 0 ? "ZIEL" : "DEST");
        if (d.kind == K_SCALE && val > 0) snprintf(label, ln, "%s", SCALE_LONG[((val - 1) / 12) % NSCALES]);
        if (d.kind == K_ROOT) snprintf(label, ln, "GRUNDTON");
        if (d.kind == K_SMPL) {
            d.max = (uint8_t)(sampleCount() > 0 ? sampleCount() - 1 : 0);
            snprintf(label, ln, "%.19s", sampleCount() ? sampleName(val) : "KEINE SAMPLES");
        }
    }
    baseVal = (trackPage(page) && d.kind != K_MACH && d.kind != K_TLEN && d.kind != K_TSPD) ? P.p[track][sp() * 8 + k] : val;
    return d.kind != K_NONE;
}

static float envSec(int v) { return 0.0005f * exp2f(v * (15.f / 127.f)); }
static bool isEnvTime(int k) {
    return (page == PG_AMP && (k == 0 || k == 1 || k == 3)) || (page == PG_FLT && (k == 4 || k == 5 || k == 7));
}
static bool isEnvSus(int k) { return (page == PG_AMP && k == 2) || (page == PG_FLT && k == 6); }

static void knobText(const ParamDef& d, int val, char* b, int n, int k = -1) {
    if (d.kind == K_LDEST) { if (val > 0) snprintf(b, n, "%.5s", paramDef(curP().machine[track], val - 1).name); else snprintf(b, n, "---"); return; }
    if (k >= 0 && trackPage(page) && isEnvTime(k)) {
        float s = envSec(val);
        if (s < 1.f) snprintf(b, n, "%.0fms", s * 1000.f); else snprintf(b, n, "%.1fs", s);
    } else if (k >= 0 && trackPage(page) && isEnvSus(k)) snprintf(b, n, "%d%%", val * 100 / 127);
    else if (d.kind == K_GR) snprintf(b, n, "-%.1f", grDisp);
    else if (d.kind == K_SMPL) snprintf(b, n, "%d", val + 1);
    else if (d.kind == K_DEV) { if (val) snprintf(b, n, "#%d", val); else snprintf(b, n, "STD"); }
    else fmtParam(d, val, b, n);
}
static float knobFrac(const ParamDef& d, int val) {
    if (d.kind == K_BPM) return (val - 30) / 270.f;
    if (d.kind == K_GR) return grDisp / 24.f > 1.f ? 1.f : grDisp / 24.f;
    return d.max ? (float)val / d.max : 0.f;
}

// ---------------------------------------------------------------- Grafiken

// Kurve aus Spaltenwerten zeichnen: gefuellt + dicke Linie
static void curve(int x0, const int* ys, int n, int base, Col c, Uint8 fillA, const bool* hi = nullptr) {
    for (int i = 0; i < n; i++) {
        int y = ys[i];
        if (fillA) fill(x0 + i, y, 1, base - y, c, fillA);
        int py = i ? ys[i - 1] : y;
        int a = py < y ? py : y, b = py < y ? y : py;
        Col cc = (hi && hi[i]) ? mix(c, C_PLAY, 0.45f) : c;
        fill(x0 + i, a - 1, 1, b - a + 3, cc);
    }
}
static void glowDot(int x, int y, Col c) {
    rrect(x - 7, y - 7, 14, 14, 7, c, 60);
    rrect(x - 4, y - 4, 9, 9, 4, C_PLAY);
}
static void handle(int x, int y, Col c, bool sel) {
    if (sel) {
        float p = 0.5f + 0.5f * sinf(animTime * 7.f);
        int r = 6 + (int)(p * 2);
        rrect(x - r, y - r, 2 * r + 1, 2 * r + 1, r, c, 90);
    }
    rrect(x - 3, y - 3, 7, 7, 3, sel ? C_PLAY : c);
}

static void graphBox(int x, int y, int w, int h) {
    rrect(x, y, w, h, 8, C_PANEL);
    for (int i = 1; i < 4; i++) fill(x + 8, y + h * i / 4, w - 16, 1, C_SEL);
}

// ADSR-Huellkurve mit Live-Punkt
static void envGraph(int gx, int gy, int gw, int gh, int a, int d, int s, int r, int sel, const ADSR* live, Col c) {
    graphBox(gx, gy, gw, gh);
    int x0 = gx + 10, top = gy + 8, bot = gy + gh - 8, H = bot - top;
    float span = (gw - 20) * 0.27f;
    int wA = 4 + (int)(a / 127.f * span), wD = 4 + (int)(d / 127.f * span), wR = 4 + (int)(r / 127.f * span);
    int wS = (int)((gw - 20) * 0.16f);
    float sv = s / 127.f;
    int n = wA + wD + wS + wR;
    static int ys[700]; static bool hi[700];
    if (n > 690) n = 690;
    for (int i = 0; i < n; i++) {
        float v; int seg;
        if (i < wA) { v = (float)i / wA; seg = 0; }
        else if (i < wA + wD) { float t = (float)(i - wA) / wD; v = sv + (1.f - sv) * expf(-6.9f * t); seg = 1; }
        else if (i < wA + wD + wS) { v = sv; seg = 2; }
        else { float t = (float)(i - wA - wD - wS) / wR; v = sv * expf(-6.9f * t); seg = 3; }
        ys[i] = bot - (int)(v * H);
        hi[i] = seg == sel;
    }
    curve(x0, ys, n, bot, c, 40, hi);
    handle(x0 + wA, top, c, sel == 0);
    handle(x0 + wA + wD, bot - (int)(sv * H), c, sel == 1 || sel == 2);
    handle(x0 + wA + wD + wS, bot - (int)(sv * H), c, sel == 2);
    handle(x0 + n - 1, bot, c, sel == 3);
    // Live: aktueller Huellkurvenwert der Stimme
    if (live && live->st) {
        float v = live->v, px;
        switch (live->st) {
        case 1: px = v * wA; break;
        case 2: px = wA + (sv < 0.999f ? clampi((int)(logf(fmaxf((v - sv) / (1.f - sv), 1e-4f)) / -6.9f * wD), 0, wD) : 0); break;
        case 3: px = wA + wD + wS * 0.5f; break;
        default: px = wA + wD + wS + (sv > 0.001f ? clampi((int)(logf(fmaxf(v / sv, 1e-4f)) / -6.9f * wR), 0, wR) : 0); break;
        }
        int ly = bot - (int)(v * H);
        fill(gx + 8, ly, gw - 16, 1, c, 70);
        glowDot(x0 + (int)px, ly, c);
    }
}

// Filterkurve (SVF LP/BP/HP) mit Live-Cutoff
static void filterGraph(int gx, int gy, int gw, int gh, int freq, int res, int type, int env, int sel, float liveUnits, Col c) {
    graphBox(gx, gy, gw, gh);
    int x0 = gx + 10, W = gw - 20, top = gy + 8, bot = gy + gh - 8, H = bot - top;
    float k = 2.f - 1.96f * (res / 127.f) * 0.97f;
    auto mag = [&](float f, float units) {
        float fc = 16.f * exp2f(units * (10.3f / 127.f));
        float w = f / fc, den = sqrtf((1 - w * w) * (1 - w * w) + (w * k) * (w * k));
        float m = type == 0 ? 1.f / den : (type == 1 ? (w * k) / den : (w * w) / den);
        float db = 20.f * log10f(m + 1e-6f);
        return bot - (int)((clampi((int)db, -30, 18) + 30) / 48.f * H);
    };
    static int ys[700];
    if (W > 690) W = 690;
    // Ziel der Filterhuellkurve (gestrichelt)
    if (env != 64) {
        float tu = freq + (env - 64) * 2.f;
        for (int i = 0; i < W; i += 3) { float f = 20.f * powf(1000.f, (float)i / W); int y = mag(f, tu); fill(x0 + i, y, 2, 2, c, 110); }
    }
    for (int i = 0; i < W; i++) ys[i] = mag(20.f * powf(1000.f, (float)i / W), (float)freq);
    curve(x0, ys, W, bot, c, 45);
    float fc = 16.f * exp2f(freq * (10.3f / 127.f));
    int xc = x0 + (int)(log10f(fminf(fmaxf(fc, 20.f), 20000.f) / 20.f) / 3.f * W);
    for (int y = top; y < bot; y += 4) fill(xc, y, 1, 2, sel == 0 ? C_PLAY : c, 160);
    handle(xc, ys[clampi(xc - x0, 0, W - 1)], c, sel == 0 || sel == 1);
    if (liveUnits >= 0) {
        float lf = 16.f * exp2f(liveUnits * (10.3f / 127.f));
        int xl = x0 + (int)(log10f(fminf(fmaxf(lf, 20.f), 20000.f) / 20.f) / 3.f * W);
        fill(xl, top, 2, H, C_PLAY, 90);
        glowDot(xl, ys[clampi(xl - x0, 0, W - 1)], c);
    }
    static const char* tn[3] = {"LOWPASS", "BANDPASS", "HIGHPASS"};
    text(gx + gw - 10 - textW(tn[type % 3]), gy + 6, tn[type % 3], sel == 2 ? C_PLAY : dimC(c, 0.8f));
}

// Base-Width-Filter: Filter 1 (grau) und Gesamtkurve mit Band
static void bwGraph(int gx, int gy, int gw, int gh, int freq, int res, int type, int base, int wdth, int sel, Col c) {
    graphBox(gx, gy, gw, gh);
    int x0 = gx + 10, W = gw - 20, top = gy + 8, bot = gy + gh - 8, H = bot - top;
    if (W > 690) W = 690;
    float k = 2.f - 1.96f * (res / 127.f) * 0.97f;
    float fc = 16.f * exp2f(freq * (10.3f / 127.f));
    float lo = 20.f * exp2f(base * (10.f / 127.f)), hi = fminf(lo * exp2f(wdth * (10.f / 127.f)), 20000.f);
    bool on = base > 0 || wdth < 127;
    auto f1 = [&](float f) {
        float w = f / fc, den = sqrtf((1 - w * w) * (1 - w * w) + (w * k) * (w * k));
        return type == 0 ? 1.f / den : (type == 1 ? (w * k) / den : (w * w) / den);
    };
    auto bw = [&](float f) {
        if (!on) return 1.f;
        float a = f / lo, b = f / hi;
        float hp = (a * a) / sqrtf((1 - a * a) * (1 - a * a) + 2.f * a * a);
        float lp = 1.f / sqrtf((1 - b * b) * (1 - b * b) + 2.f * b * b);
        return hp * lp;
    };
    auto toY = [&](float m) { float db = 20.f * log10f(m + 1e-6f); return bot - (int)((clampi((int)db, -30, 18) + 30) / 48.f * H); };
    auto toX = [&](float f) { return x0 + (int)(log10f(fminf(fmaxf(f, 20.f), 20000.f) / 20.f) / 3.f * W); };
    int xl = toX(lo), xh = toX(hi);
    fill(xl, top, xh - xl, H, c, 28);
    for (int y = top; y < bot; y += 4) { fill(xl, y, 1, 2, sel == 0 ? C_PLAY : c, 150); fill(xh, y, 1, 2, sel == 1 ? C_PLAY : c, 150); }
    static int ys[700];
    for (int i = 0; i < W; i += 2) { float f = 20.f * powf(1000.f, (float)i / W); fill(x0 + i, toY(f1(f)), 1, 1, C_DIM, 255); }
    for (int i = 0; i < W; i++) { float f = 20.f * powf(1000.f, (float)i / W); ys[i] = toY(f1(f) * bw(f)); }
    curve(x0, ys, W, bot, c, 45);
    handle(xl, ys[clampi(xl - x0, 0, W - 1)], c, sel == 0);
    handle(xh - 1, ys[clampi(xh - 1 - x0, 0, W - 1)], c, sel == 1);
    const char* t = on ? "BASE-WIDTH" : "BASE-WIDTH AUS";
    text(gx + gw - 10 - textW(t), gy + 6, t, dimC(c, 0.8f));
}

static float waveAt(int w, float ph, float pw) {
    ph -= floorf(ph);
    switch (w) {
    case 0: return 2.f * ph - 1.f;
    case 1: return ph < pw ? 1.f : -1.f;
    case 2: return 1.f - 4.f * fabsf(ph - 0.5f);
    default: return sinf(ph * 6.2831853f);
    }
}

// LFO-Welle mit laufender Phase
static int pv(int idx);
static float lfoWaveUi(int wave, float p) {   // wie in der Engine, Zufall fest fuer die Anzeige
    p -= floorf(p);
    switch (wave) {
    case LW_SIN: return sinf(p * 6.2831853f);
    case LW_TRI: return 1.f - 4.f * fabsf(p - 0.5f);
    case LW_SAWUP: return 2.f * p - 1.f;
    case LW_SAWDN: return 1.f - 2.f * p;
    case LW_SQR: return p < 0.5f ? 1.f : -1.f;
    case LW_EXPUP: return (exp2f(p * 5.f) - 1.f) / 31.f * 2.f - 1.f;
    case LW_EXPDN: return (exp2f((1.f - p) * 5.f) - 1.f) / 31.f * 2.f - 1.f;
    case LW_SH: { int seg = (int)(p * 4); return sinf(seg * 12.9898f + 1.3f) * 0.9f; }
    default: { float q = p * 4; int seg = (int)q; float x = (1.f - cosf((q - seg) * 3.14159f)) * 0.5f;
               float a = sinf(seg * 12.9898f + 1.3f) * 0.9f, b = sinf((seg + 1) * 12.9898f + 1.3f) * 0.9f; return a + (b - a) * x; }
    }
}
static float uiLfoPhase[NLFO];

// LFO: Kurve bei voller Tiefe (grau), wirksame Kurve (farbig), laufende Phase, Ziel und Tempo
static void lfoGraph(int gx, int gy, int gw, int gh, int k, int base, float livePh, bool live, Col c) {
    graphBox(gx, gy, gw, gh);
    int x0 = gx + 10, W = gw - 20, mid = gy + gh / 2, A = (gh - 18) / 2;
    int rate = pv(base + LF_RATE), sync = pv(base + LF_SYNC) % 12, dep = pv(base + LF_DEPTH), wave = pv(base + LF_WAVE) % LW_COUNT;
    int mode = pv(base + LF_MODE), dest = pv(base + LF_DEST);
    float amp = (dep - 64) / 64.f;
    float hz = sync ? 1.f / (LFO_SYNC_STEPS[sync] * 15.f / eng.proj.bpm) : 0.05f * exp2f(rate * (9.f / 127.f));
    if (!live) uiLfoPhase[k] += hz * animDt;
    float ph = live ? livePh : uiLfoPhase[k];
    fill(x0, mid, W, 1, C_OFF2);
    static int ys[700];
    if (W > 690) W = 690;
    for (int i = 0; i < W; i += 3) fill(x0 + i, mid - (int)(lfoWaveUi(wave, (float)i / W * 2.f) * A), 2, 1, C_OFF2);
    for (int i = 0; i < W; i++) ys[i] = mid - (int)(lfoWaveUi(wave, (float)i / W * 2.f) * amp * A);
    curve(x0, ys, W, mid, c, 35);
    float pp = ph - floorf(ph);
    int px = x0 + (int)(pp * W / 2.f);
    glowDot(px, ys[clampi(px - x0, 0, W - 1)], c);
    char b[48];
    static const char* mn[5] = {"FREE", "TRIG", "HOLD", "ONE", "HALF"};
    if (sync) { char sb[12]; fmtParam(COMMON_DEFS[P_L1 + LF_SYNC], sync, sb, sizeof sb); snprintf(b, sizeof b, "%s  %s", sb, mn[mode % 5]); }
    else snprintf(b, sizeof b, "%.2f HZ  %s", hz, mn[mode % 5]);
    text(gx + gw - 10 - textW(b), gy + 4, b, dimC(c, 0.8f));
    if (dest > 0) snprintf(b, sizeof b, "-> %s", paramDef(curP().machine[track], dest - 1).name);
    else snprintf(b, sizeof b, "KEIN ZIEL");
    text(gx + 10, gy + 4, b, dest > 0 && dep != 64 ? c : C_DIM);
}

// Oszillator: eine gemischte Wellenform (OSC1 + OSC2 + SUB) wie sie klingt; Verstimmung = Bewegung
static void oscGraph(int gx, int gy, int gw, int gh, int w1, int w2, int det, int mixv, int pwv, int subv, int sel, Col c) {
    graphBox(gx, gy, gw, gh);
    int x0 = gx + 10, W = gw - 20, mid = gy + gh / 2, A = (gh - 18) / 2;
    float pw = 0.5f + (pwv - 64) / 140.f, m = mixv / 127.f, sub = subv / 127.f * 0.8f;
    float ratio = exp2f((det - 64) / 64.f * 0.5f / 12.f);
    float drift = oscDrift * (ratio - 1.f) * 40.f;   // sichtbares "Wandern" von OSC2
    static float v[700]; static int ys[700];
    if (W > 690) W = 690;
    float mx = 0.001f;
    for (int i = 0; i < W; i++) {
        float p = (float)i / W * 2.f;   // 2 Perioden von OSC1
        float a = waveAt(w1, p, pw), b = waveAt(w2, p * ratio + drift, pw);
        float sq = waveAt(1, p * 0.5f, 0.5f);
        v[i] = a * (1.f - m) + b * m + sub * sq;
        if (fabsf(v[i]) > mx) mx = fabsf(v[i]);
    }
    for (int i = 0; i < W; i++) ys[i] = mid - (int)(v[i] / mx * A);
    fill(x0, mid, W, 1, C_OFF2);
    curve(x0, ys, W, mid, c, 45);
    static const char* wn[4] = {"SAW", "SQR", "TRI", "SIN"};
    char b2[48];
    snprintf(b2, sizeof b2, "%s + %s  MIX %d%%%s", wn[w1 & 3], wn[w2 & 3], mixv * 100 / 127, subv ? "  +SUB" : "");
    text(gx + gw - 10 - textW(b2), gy + 4, b2, sel >= 0 ? c : dimC(c, 0.8f));
}

// FM: Algorithmus-Diagramm. Traeger gefuellt (rechts der Ausgang), Modulatoren als Rahmen, leuchten beim Spielen
static void fmGraph(int gx, int gy, int gw, int gh, int algo, const int* rat, const int* lvl, int selOp, const Voice* vo, Col c) {
    graphBox(gx, gy, gw, gh);
    algo &= 7;
    int bx[4], by[4], bw = 92, bh = 16;
    for (int k = 0; k < 4; k++) {
        bx[k] = gx + 30 + FM_LAYOUT[algo][k][0] * 128;
        by[k] = gy + 5 + FM_LAYOUT[algo][k][1] * 19;
    }
    // Verbindungen Modulator -> Ziel
    for (int k = 0; k < 4; k++)
        for (int j = 0; j < 4; j++)
            if (FM_MODS[algo][k] >> j & 1) {
                int x1 = bx[j] + bw, y1 = by[j] + bh / 2, x2 = bx[k], y2 = by[k] + bh / 2, xm = (x1 + x2) / 2;
                Col lc = dimC(c, 0.6f);
                fill(x1, y1, xm - x1, 1, lc);
                fill(xm, y1 < y2 ? y1 : y2, 1, abs(y2 - y1) + 1, lc);
                fill(xm, y2, x2 - xm, 1, lc);
                fill(x2 - 4, y2 - 2, 3, 5, lc);
            }
    // Ausgang
    int ox = gx + gw - 26;
    fill(ox, gy + 8, 2, gh - 16, c);
    text(ox - 6, gy + gh - 18, "OUT", dimC(c, 0.8f));
    for (int k = 0; k < 4; k++) {
        bool car = FM_CARRIERS[algo] >> k & 1;
        float glow = (vo && vo->active && vo->machine == M_FM) ? vo->fmEnv[k] : 0.f;
        float lv = k == 0 ? 1.f : lvl[k] / 127.f;
        Col bc = car ? mix(dimC(c, 0.35f + 0.65f * lv), C_PLAY, glow * 0.4f) : mix(C_SEL, c, glow * 0.6f);
        if (k == selOp) { float p = 0.5f + 0.5f * sinf(animTime * 7.f); rrect(bx[k] - 3, by[k] - 3, bw + 6, bh + 6, 6, c, (Uint8)(60 + 60 * p)); }
        rrect(bx[k], by[k], bw, bh, 4, bc);
        if (!car) { rrect(bx[k] + 1, by[k] + 1, bw - 2, bh - 2, 3, C_PANEL); fill(bx[k] + 2, by[k] + bh - 3, (int)((bw - 4) * lv), 2, c); }
        char b[24];
        snprintf(b, sizeof b, "OP%d x%.2g", k + 1, FM_RATIOS[rat[k] & 15]);
        text(bx[k] + 6, by[k], b, car ? C_BG : C_TEXT);
        if (car) { int x1 = bx[k] + bw; fill(x1, by[k] + bh / 2, ox - x1, 1, dimC(c, 0.5f)); }
        if (k == 3) { rrect(bx[3] + bw - 14, by[3] - 4, 12, 6, 3, dimC(c, 0.6f)); }   // Feedback-Schleife
    }
    char b[16]; snprintf(b, sizeof b, "ALGO %d", algo + 1);
    text(gx + 8, gy + gh - 18, b, selOp == 4 ? C_PLAY : dimC(c, 0.8f));
}

// Sample-Wellenform mit Start/Ende und Abspielposition
static void sampleGraph(int gx, int gy, int gw, int gh, int idx, int st, int en, bool loop, int sel, Col c) {
    graphBox(gx, gy, gw, gh);
    const Sample* sm = sampleGet(idx);
    if (!sm) { textC(gx + gw / 2, gy + gh / 2 - 8, "KEINE SAMPLES IM ORDNER samples/", C_DIM); return; }
    int x0 = gx + 10, W = gw - 20, mid = gy + gh / 2, A = (gh - 16) / 2;
    static int cIdx = -1, cW = 0; static float pk[700];
    if (W > 690) W = 690;
    if (cIdx != idx || cW != W) {   // Peaks einmal pro Sample berechnen
        cIdx = idx; cW = W;
        for (int i = 0; i < W; i++) {
            int a = (int)((long)sm->len * i / W), b = (int)((long)sm->len * (i + 1) / W);
            float m = 0;
            int stp = (b - a) / 64 + 1;
            for (int j = a; j < b; j += stp) { float v = fabsf(sm->data[j]); if (v > m) m = v; }
            pk[i] = m;
        }
    }
    int xs = x0 + st * (W - 1) / 127, xe = x0 + en * (W - 1) / 127;
    int lo = xs < xe ? xs : xe, hi = xs < xe ? xe : xs;
    for (int i = 0; i < W; i++) {
        int h = (int)(sqrtf(pk[i]) * A);
        bool in = x0 + i >= lo && x0 + i <= hi;
        fill(x0 + i, mid - h, 1, 2 * h + 1, in ? c : C_OFF2);
    }
    fill(xs, gy + 4, 2, gh - 8, sel == 3 ? C_PLAY : C_GREEN);
    fill(xe, gy + 4, 2, gh - 8, C_TRIG);
    if (loop) text(lo + 4, gy + gh - 20, "LOOP", C_PLAY);
    if (en < st) text(lo + 4, gy + 4, "<< RUECKWAERTS", C_PLAY);
    const Voice& v = eng.v[track];
    if (v.active && v.machine == M_SAMPLE && v.smp == sm) {
        int px = x0 + (int)(v.spos / sm->len * (W - 1));
        fill(px, gy + 4, 2, gh - 8, C_PLAY);
    }
    char b[64];
    snprintf(b, sizeof b, "%.30s  %.2fs", sm->name, (float)sm->len / sm->rate);
    text(gx + gw - 10 - textW(b), gy + 4, b, dimC(c, 0.8f));
}

static bool euclidHitUi(int s, int k, int n, int rot) {
    if (k <= 0 || n <= 0) return false;
    if (k > n) k = n;
    int pos = ((s - rot) % n + n) % n;
    return (pos * k) % n < k;
}

// SEQ: Euklid-Kreis mit Playhead, Richtung, Turing-Register
static void seqGraph(int gx, int gy, int gw, int gh, int sel, Col c) {
    graphBox(gx, gy, gw, gh);
    const Pattern& P = curP();
    int len = P.len[track] < 1 ? 1 : P.len[track];
    int dir = P.p[track][P_DIR] % 6, puls = pv(P_PULS), rot = pv(P_EROT), dens = pv(P_DENS), chng = pv(P_CHNG), range = pv(P_RANGE);
    int cx = gx + 42, cy = gy + gh / 2, r = 24;
    int ps = eng.playing ? eng.playedStep[track] : -1;
    for (int i = 0; i < len; i++) {
        float a = (i / (float)len) * 6.2831853f - 1.5707963f;
        int x = cx + (int)(r * cosf(a)), y = cy + (int)(r * sinf(a));
        bool hit = puls > 0 ? euclidHitUi(i, puls, len, rot) : P.s[track][i].on;
        if (i == ps) { glowDot(x, y, c); continue; }
        if (hit) rrect(x - 3, y - 3, 7, 7, 3, c);
        else rrect(x - 1, y - 1, 3, 3, 1, C_DIM);
    }
    if (puls > 0) {   // Rotationsmarke
        float a = ((rot % len) / (float)len) * 6.2831853f - 1.5707963f;
        fill(cx + (int)((r + 6) * cosf(a)) - 1, cy + (int)((r + 6) * sinf(a)) - 1, 3, 3, sel == 0 + 4 ? C_PLAY : C_TEXT);
    }
    static const char* dn[6] = {"VORWAERTS >>", "<< RUECKWAERTS", "PING <->", "PENDEL <=>", "ZUFALL ?", "BETRUNKEN ~"};
    char b[48];
    text(gx + 84, gy + 6, dn[dir], sel == 0 ? C_PLAY : c);
    if (puls > 0) snprintf(b, sizeof b, "EUKLID E(%d,%d) ROT %d", puls > len ? len : puls, len, rot % len);
    else snprintf(b, sizeof b, "%d STEPS  %s", len, SPEED_NAMES[P.speed[track] % 7]);
    text(gx + 84, gy + 24, b, puls > 0 ? c : C_DIM);
    if (dens > 0 || range > 0) snprintf(b, sizeof b, "GEN %d%%  MUT %d%%  MEL +%d", dens, chng, range);
    else snprintf(b, sizeof b, "GENERATOR AUS");
    text(gx + 84, gy + 42, b, dens > 0 || range > 0 ? c : C_DIM);
    // Turing-Register: naechste 16 Bits (links = naechster Step), Balken = erzeugte Tonhoehe
    uint32_t reg = eng.tmReg[track];
    int rx = gx + 380;
    for (int i = 0; i < 16; i++) {
        bool on = (reg >> i) & 1;
        int x = rx + i * 15;
        if (range > 0) { int h = (int)(((reg >> i) & 0xFF) / 255.f * 26); fill(x, gy + 34 - h, 11, h, dimC(c, 0.5f)); }
        rrect(x, gy + 38, 11, 11, 3, on && dens > 0 ? c : C_OFF2);
    }
    text(rx, gy + gh - 14, "REGISTER", C_DIM);
}

// ---------------------------------------------------------------- Symbole (gezeichnet, skalierbar)

static void disc(int cx, int cy, int r, Col c, Uint8 a = 255) {   // gefuellter Kreis
    for (int dy = -r; dy <= r; dy++) { int w = (int)sqrtf((float)(r * r - dy * dy) + 0.5f); fill(cx - w, cy + dy, 2 * w + 1, 1, c, a); }
}
static void ellipse(int cx, int cy, int rx, int ry, Col c, Uint8 a = 255) {
    for (int dy = -ry; dy <= ry; dy++) { float f = 1.f - (float)(dy * dy) / (ry * ry + 0.01f); int w = (int)(rx * sqrtf(f > 0 ? f : 0)); fill(cx - w, cy + dy, 2 * w + 1, 1, c, a); }
}
static void ring(int cx, int cy, int r, int th, Col c) {
    for (int dy = -r; dy <= r; dy++) {
        int wo = (int)sqrtf((float)(r * r - dy * dy) + 0.5f);
        int ri = r - th, wi = abs(dy) <= ri ? (int)sqrtf((float)(ri * ri - dy * dy) + 0.5f) : -1;
        if (wi < 0) fill(cx - wo, cy + dy, 2 * wo + 1, 1, c);
        else { fill(cx - wo, cy + dy, wo - wi, 1, c); fill(cx + wi + 1, cy + dy, wo - wi, 1, c); }
    }
}
static void line(int x1, int y1, int x2, int y2, Col c, int th = 1) {
    setCol(c);
    for (int k = 0; k < th; k++) { SDL_RenderDrawLine(ren, x1 + k, y1, x2 + k, y2); SDL_RenderDrawLine(ren, x1, y1 + k, x2, y2 + k); }
}

// Symbol einer Engine im Quadrat (x, y, Groesse s)
static void drawIcon(int m, int x, int y, int s, Col c) {
    int cx = x + s / 2, cy = y + s / 2, th = s / 12 > 0 ? s / 12 : 1;
    Col d = dimC(c, 0.45f);
    switch (m) {
    case M_KICK:   // Bassdrum von vorne: Fell, Logo, Fuesse
        ring(cx, cy - s / 16, s * 7 / 16, th + 1, c);
        disc(cx, cy - s / 16, s / 5, d);
        fill(cx - s * 5 / 16, y + s - th * 2, th * 2, th * 2, c); fill(cx + s * 5 / 16 - th * 2, y + s - th * 2, th * 2, th * 2, c);
        break;
    case M_SNARE: case M_TOM: {   // Trommel seitlich
        int top = m == M_TOM ? y + s / 6 : y + s / 4, bot = y + s - s / 6, rx = s * 7 / 16, ry = s / 9 + 1;
        fill(cx - rx, top, 2 * rx + 1, bot - top, d);
        ellipse(cx, bot, rx, ry, d);
        ellipse(cx, top, rx, ry, c);
        if (m == M_SNARE) for (int i = 0; i < 4; i++) line(cx - rx + 3 + i * rx / 2, bot - 2, cx - rx + 3 + i * rx / 2 + rx / 4, top + ry + 2, c);
        else for (int i = 0; i < 3; i++) fill(cx - rx + 2 + i * rx * 2 / 3 + rx / 3 - 1, top + (bot - top) / 2 - 2, 3, 5, c);
    } break;
    case M_HAT:   // zwei Becken auf Staender
        ellipse(cx, y + s / 4, s * 7 / 16, s / 12 + 1, c);
        ellipse(cx, y + s / 4 + s / 7, s * 7 / 16, s / 12 + 1, d);
        fill(cx - th / 2, y + s / 4, th, s * 3 / 4 - 2, c);
        line(cx, y + s - 3, cx - s / 4, y + s - 1, c); line(cx, y + s - 3, cx + s / 4, y + s - 1, c);
        break;
    case M_CLAP: {   // Hand mit Bewegungslinien
        int pw = s / 2, px = cx - pw / 2 - s / 10, py = cy - s / 12;
        rrect(px, py, pw, s / 2, s / 10, c);
        for (int i = 0; i < 4; i++) rrect(px + 1 + i * pw / 4, py - s / 4, pw / 4 - 1, s / 4 + 2, 2, c);
        rrect(px - s / 8, py + s / 10, s / 7, s / 4, 2, c);
        for (int i = 0; i < 3; i++) line(px + pw + 3 + i * 3, py - s / 8 + i * s / 8, px + pw + s / 6 + i * 3, py - s / 6 + i * s / 8, d);
    } break;
    case M_RIM: {   // Trommelrand mit Stick
        ellipse(cx, cy + s / 8, s * 7 / 16, s / 6, d);
        ellipse(cx, cy + s / 8, s * 7 / 16 - th - 1, s / 6 - th - 1, C_BG);
        line(x + s / 8, y + s / 6, x + s - s / 6, y + s * 5 / 8, c, th + 1);
        disc(x + s / 8, y + s / 6, th + 1, c);
    } break;
    case M_SHKR:   // Ei mit Schuettel-Linien
        ellipse(cx, cy, s / 5, s * 3 / 8, c);
        for (int i = -1; i <= 1; i += 2) for (int k = 0; k < 2; k++)
            line(cx + i * (s / 4 + 2 + k * 4), cy - s / 6, cx + i * (s / 4 + 2 + k * 4), cy + s / 6, d);
        break;
    case M_SYNTH: {   // Saegezahn
        rrect(x + 1, y + 1, s - 2, s - 2, s / 6, C_SEL);
        int x0 = x + s / 8, y0 = y + s * 3 / 4, w = (s * 3 / 4) / 2, h = s / 2;
        for (int i = 0; i < 2; i++) { line(x0 + i * w, y0, x0 + (i + 1) * w, y0 - h, c, th); line(x0 + (i + 1) * w, y0 - h, x0 + (i + 1) * w, y0, c, th); }
    } break;
    case M_FM: {   // zwei Operatoren mit Verbindung
        int b = s * 3 / 8;
        rrect(x + 1, y + s / 8, b, b, 3, d); rrect(x + s - b - 1, y + s - b - s / 8, b, b, 3, c);
        line(x + 1 + b, y + s / 8 + b / 2, x + s - b - 1, y + s - b / 2 - s / 8, c, th);
        ring(x + 1 + b / 2, y + s / 8 - 1, b / 3 + 1, 1, d);
    } break;
    case M_WTBL:   // gestapelte Wellen (3D)
        for (int k = 2; k >= 0; k--) {
            int ox = x + k * s / 8, oy = y + s / 2 - k * s / 8 + s / 10;
            Col cc = k == 0 ? c : dimC(c, 0.35f + 0.2f * (2 - k));
            int px = ox, py = oy;
            for (int i = 1; i <= s * 5 / 8; i++) {
                int nx = ox + i, ny = oy - (int)(sinf(i / (float)(s * 5 / 8) * 6.2831853f) * s / 6);
                line(px, py, nx, ny, cc); px = nx; py = ny;
            }
        }
        break;
    case M_SAMPLE: case M_CHOP:   // Wellenform-Balken (CHOP: mit Schnitten)
        for (int i = 0; i < 9; i++) {
            float a = fabsf(sinf(i * 1.7f + 0.4f)) * (1.f - i / 12.f);
            int h = (int)(a * s * 0.42f) + 1, bx = x + s / 10 + i * s / 11;
            fill(bx, cy - h, s / 18 + 1, 2 * h, c);
        }
        if (m == M_CHOP) for (int k = 1; k <= 2; k++) for (int yy = y; yy < y + s; yy += 4) fill(x + k * s / 3, yy, 1, 2, C_LOCK);
        break;
    case M_GRAN: {   // Koerner-Wolke
        static const float gx[9] = {0.2f, 0.45f, 0.75f, 0.3f, 0.62f, 0.85f, 0.15f, 0.5f, 0.7f};
        static const float gy[9] = {0.25f, 0.18f, 0.3f, 0.5f, 0.55f, 0.6f, 0.75f, 0.82f, 0.78f};
        for (int i = 0; i < 9; i++) disc(x + (int)(gx[i] * s), y + (int)(gy[i] * s), (i % 3) + s / 20, i % 2 ? c : d);
    } break;
    }
}

// ---------------------------------------------------------------- Wert-Symbole (statt Balken/Drehregler)

const float* wavetableFrame(int t, int f);
static bool nameIs(const char* n, const char* const* list) { for (; *list; list++) if (!strcmp(n, *list)) return true; return false; }

// Kurve aus Funktionswerten v(t) in [0,1] in ein Rechteck zeichnen
template <typename Fn> static void symCurve(int x, int y, int w, int h, Col c, Fn f, bool fillUnder = true) {
    int py = y + h - (int)(f(0.f) * h);
    for (int i = 1; i <= w; i++) {
        float v = f((float)i / w);
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        int ny = y + h - (int)(v * h);
        if (fillUnder) fill(x + i, ny, 1, y + h - ny, c, 45);
        line(x + i - 1, py, x + i, ny, c, 2);
        py = ny;
    }
}
static float symWave(int w, float p) {   // 0 SAW 1 SQR 2 TRI 3 SIN (wie Oszillator) -> 0..1
    p -= floorf(p);
    switch (w & 3) {
    case 0: return p;
    case 1: return p < 0.5f ? 1.f : 0.f;
    case 2: return 1.f - 2.f * fabsf(p - 0.5f);
    default: return 0.5f + 0.5f * sinf(p * 6.2831853f);
    }
}

static void paramSymbol(const ParamDef& d, int val, float fr, int x, int y, int w, int h, Col c) {
    const char* n = d.name;
    Col cd = dimC(c, 0.4f);
    int cx = x + w / 2, cy = y + h / 2;
    static const char* DEC[] = {"DCAY", "DEC", "REL", "N.DEC", "M.DEC", "F.DEC", "F.REL", "DEC1", "DEC2", "DEC3", "DEC4", "R.DAMP", 0};
    static const char* ATK[] = {"ATK", "F.ATK", 0};
    static const char* SUS[] = {"SUS", "F.SUS", 0};
    static const char* PITCH[] = {"TUNE", "PTCH", "FINE", "DETN", "RAT1", "RAT2", "RAT3", "RAT4", "RATE", "SPD", 0};
    static const char* LEVEL[] = {"VOL", "GAIN", "LVL2", "LVL3", "LVL4", 0};
    static const char* NOISE[] = {"NOIS", "GRIT", "SNAP", "CLCK", "METL", "SPRY", "P.SPR", 0};
    static const char* DRIVE[] = {"DRIV", "DRIVE", "BODY", 0};
    static const char* GLIDE[] = {"GLID", "SWEP", "S.TIM", 0};
    static const char* BRIGHT[] = {"TONE", "D.TONE", 0};
    static const char* BIPARROW[] = {"ENV", "P.ENV", "VELF", "DEPTH", "WARP", 0};
    static const char* WIDTH[] = {"D.PING", "R.WIDE", "WIDTH", 0};
    static const char* ROOM[] = {"REV", "R.SIZE", 0};
    static const char* MIXS[] = {"MIX", 0};

    if (d.kind == K_MACH) { drawIcon(val, cx - h / 2, y, h, c); return; }
    if (d.kind == K_WAVE || (d.kind == K_LWAV)) {
        if (d.kind == K_WAVE) symCurve(x, y + 4, w, h - 8, c, [&](float t) { return symWave(val, t * 2.f); }, false);
        else symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + 0.5f * lfoWaveUi(val, t * 2.f); }, false);
        return;
    }
    if (!strcmp(n, "NOTE")) {   // Klaviatur, Taste der Note hervorgehoben
        static const int wk[7] = {0, 2, 4, 5, 7, 9, 11}, bk[5] = {1, 3, 6, 8, 10}, bp[5] = {0, 1, 3, 4, 5};
        int kw = w / 7, pc = val % 12;
        for (int i = 0; i < 7; i++) rrect(x + i * kw, y + 4, kw - 1, h - 6, 2, wk[i] == pc ? c : C_TEXT);
        for (int i = 0; i < 5; i++) rrect(x + bp[i] * kw + kw * 2 / 3, y + 4, kw * 2 / 3, (h - 6) * 3 / 5, 2, bk[i] == pc ? c : C_BG);
        return;
    }
    if (!strcmp(n, "VEL")) { for (int i = 0; i < w; i += 4) { int hh = (int)((float)i / w * h); fill(x + i, y + h - hh, 3, hh, i < fr * w ? c : C_OFF2); } return; }
    if (d.kind == K_LEN) {   // Notenlaenge als Balken ueber einem Step-Raster
        for (int i = 0; i < 4; i++) fill(x + i * w / 4, y + h - 6, 1, 6, C_DIM);
        float s2 = lenSteps(val);
        int bw = s2 < 0 ? w : (int)fminf(w, s2 * w / 4.f);
        rrect(x, cy - 6, bw < 4 ? 4 : bw, 12, 5, c);
        if (s2 < 0) text(x + w - 16, cy - 8, ">", C_BG);
        return;
    }
    if (!strcmp(n, "PROB") || (!strcmp(n, "DENS") && d.kind == K_PCT)) {   // Torte
        int r = h / 2 - 2;
        disc(cx, cy, r, C_OFF2);
        float a1 = fr * 6.2831853f;
        for (float a = 0; a < a1; a += 0.04f) line(cx, cy, cx + (int)(r * sinf(a)), cy - (int)(r * cosf(a)), c);
        return;
    }
    if (d.kind == K_RTRG) { int nn = RTRG_DIV[val % 7]; if (!nn) nn = 1; for (int i = 0; i < nn; i++) { int hh = h - i * h / (nn + 2); fill(x + i * w / nn, y + h - hh, 2, hh, val ? c : C_DIM); } return; }
    if (d.kind == K_MTIM) {
        fill(x, cy, w, 1, C_DIM); fill(cx, y + 2, 1, h - 4, C_TEXT);
        int mx = cx + (val - 23) * (w / 2) / 23;
        rrect(mx - 4, cy - 8, 9, 16, 3, c);
        return;
    }
    if (d.kind == K_COND) {   // Punkte: A von B
        if (val >= CND_AB) {
            int a, b; bool ng; condDecode(val, a, b, ng);
            int r = w / (b * 2 + 1); if (r > 6) r = 6;
            for (int i = 0; i < b; i++) { bool on = (i == a - 1) != ng; int px = x + (i * 2 + 1) * w / (b * 2); if (on) disc(px, cy, r, c); else ring(px, cy, r, 1, C_DIM); }
        } else if (val) { char b2[8]; fmtParam(d, val, b2, sizeof b2); text(cx - textW(b2) / 2, cy - 8, b2, c); }
        else for (int i = 0; i < 4; i++) disc(x + (i * 2 + 1) * w / 8, cy, 4, c);
        return;
    }
    if (nameIs(n, DEC)) { float k = 1.f + (1.f - fr) * 12.f; symCurve(x, y + 2, w, h - 4, c, [&](float t) { return expf(-t * k); }); return; }
    if (nameIs(n, ATK)) { float a = 0.05f + fr * 0.9f; symCurve(x, y + 2, w, h - 4, c, [&](float t) { return t < a ? t / a : 1.f; }); return; }
    if (nameIs(n, SUS)) { symCurve(x, y + 2, w, h - 4, c, [&](float t) { return t < 0.15f ? t / 0.15f : (t < 0.35f ? 1.f - (1.f - fr) * (t - 0.15f) / 0.2f : fr); }); return; }
    if (nameIs(n, PITCH)) { float cyc = 1.f + fr * 5.f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + 0.5f * sinf(t * cyc * 6.2831853f); }, false); return; }
    if (nameIs(n, LEVEL)) {   // Lautsprecher mit Schallwellen
        fill(x + 4, cy - 5, 7, 10, c);
        for (int i = 0; i < 5; i++) line(x + 11, cy - 5 - i, x + 11, cy + 5 + i, c), fill(x + 11 + i, cy - 5 - i * 2, 2, 10 + i * 4, c);
        int waves = (int)(fr * 4.f + 0.5f);
        for (int k = 0; k < waves; k++) { int r = 10 + k * 7; for (int dy = -r * 2 / 3; dy <= r * 2 / 3; dy++) { int dx = (int)sqrtf((float)(r * r - dy * dy)); fill(x + 14 + dx, cy + dy, 2, 1, k == waves - 1 ? c : cd); } }
        return;
    }
    if (nameIs(n, NOISE)) { Rng rr; rr.s = 1234; for (int i = 0; i < w; i += 2) { int a = (int)(rr.next() * fr * h / 2); fill(x + i, cy - (a > 0 ? a : 0), 2, abs(a) + 1, c); } return; }
    if (nameIs(n, DRIVE)) { float g = 1.f + fr * 8.f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { float sv = sinf(t * 2.f * 6.2831853f) * g; return 0.5f + 0.5f * (sv > 1 ? 1 : (sv < -1 ? -1 : sv)); }, false); return; }
    if (!strcmp(n, "CRSH")) { int st = 2 + (int)((1.f - fr) * 14.f); symCurve(x, y + 4, w, h - 8, c, [&](float t) { float sv = 0.5f + 0.5f * sinf(t * 6.2831853f); return floorf(sv * st) / st; }, false); return; }
    if (!strcmp(n, "SRR")) { int st = 32 - (int)(fr * 28.f); symCurve(x, y + 4, w, h - 8, c, [&](float t) { float q = floorf(t * st) / st; return 0.5f + 0.5f * sinf(q * 6.2831853f); }, false); return; }
    if (nameIs(n, GLIDE)) { float sl = 0.05f + fr * 0.9f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { return t < sl ? 0.15f + 0.7f * t / sl : 0.85f; }, false); return; }
    if (nameIs(n, BRIGHT)) {   // Sonne: Strahlen = Helligkeit
        disc(cx, cy, h / 6, c);
        int rays = 3 + (int)(fr * 9.f);
        for (int i = 0; i < rays; i++) { float a = i * 6.2831853f / rays; line(cx + (int)(h / 4 * cosf(a)), cy + (int)(h / 4 * sinf(a)), cx + (int)(h / 2.2f * cosf(a)), cy + (int)(h / 2.2f * sinf(a)), c); }
        return;
    }
    if (nameIs(n, BIPARROW) || d.kind == K_BIP) {   // Pfeil nach oben/unten bzw. Ausschlag um die Mitte
        float v = (val - 64) / 64.f;
        fill(x, cy, w, 1, C_DIM);
        int len = (int)(fabsf(v) * (h / 2 - 2));
        if (len) { fill(cx - 3, v > 0 ? cy - len : cy, 7, len, c); for (int i = 0; i < 6; i++) fill(cx - 6 + i, v > 0 ? cy - len - 6 + i : cy + len + 5 - i, 13 - 2 * i, 1, c); }
        else disc(cx, cy, 3, c);
        return;
    }
    if (!strcmp(n, "PAN")) { fill(x + 10, cy, w - 20, 2, C_DIM); text(x, cy - 8, "L", C_DIM); text(x + w - 8, cy - 8, "R", C_DIM); disc(x + 10 + (int)(fr * (w - 20)), cy, 6, c); return; }
    if (nameIs(n, WIDTH)) { int sp = (int)(fr * (w / 2 - 8)); disc(cx - sp, cy, 6, c); disc(cx + sp, cy, 6, c); fill(cx - sp, cy, 2 * sp, 1, cd); return; }
    if (!strcmp(n, "DLY")) { int nn = 1 + (int)(fr * 5.f); for (int i = 0; i < nn; i++) { int hh = (int)(h * powf(0.7f, (float)i)); fill(x + i * w / 6, y + h - hh, w / 10, hh, i ? cd : c); } return; }
    if (nameIs(n, ROOM)) { int nn = 1 + (int)(fr * 4.f); for (int i = 0; i < nn; i++) ring(cx, cy, 6 + i * (h / 2 - 6) / 4, 1, i == nn - 1 ? c : cd); return; }
    if (nameIs(n, MIXS)) { int r = h / 3; disc(cx - r / 2, cy, (int)(r * (1.f - fr * 0.6f)), cd); disc(cx + r / 2, cy, (int)(r * (0.4f + fr * 0.6f)), c); return; }
    if (!strcmp(n, "PW")) { float pw = 0.5f + (val - 64) / 140.f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { float p = t * 2.f; p -= floorf(p); return p < pw ? 1.f : 0.f; }, false); return; }
    if (!strcmp(n, "SUB")) { symCurve(x, y + 4, w, h - 8, cd, [&](float t) { return 0.5f + 0.4f * sinf(t * 4.f * 6.2831853f); }, false); symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + 0.5f * fr * sinf(t * 2.f * 6.2831853f); }, false); return; }
    if (!strcmp(n, "FREQ")) { float fc = 0.08f + fr * 0.88f; symCurve(x, y + 2, w, h - 4, c, [&](float t) { return t < fc ? 0.8f : 0.8f * expf(-(t - fc) * 12.f); }); return; }
    if (!strcmp(n, "RESO")) { symCurve(x, y + 2, w, h - 4, c, [&](float t) { float pk = expf(-(t - 0.55f) * (t - 0.55f) * 400.f) * fr * 0.45f; return (t < 0.55f ? 0.55f : 0.55f * expf(-(t - 0.55f) * 10.f)) + pk; }); return; }
    if (d.kind == K_FTYPE) { int ty = val % 3; symCurve(x, y + 2, w, h - 4, c, [&](float t) { if (ty == 0) return t < 0.5f ? 0.8f : 0.8f * expf(-(t - 0.5f) * 10.f); if (ty == 2) return t > 0.5f ? 0.8f : 0.8f * expf(-(0.5f - t) * 10.f); return 0.8f * expf(-(t - 0.5f) * (t - 0.5f) * 40.f); }); return; }
    if (!strcmp(n, "BASE") || !strcmp(n, "WDTH")) {   // Durchlassband
        int bs = pv(P_BASE), wd = pv(P_WDTH);
        float lo = bs / 127.f, hi = fminf(1.f, lo + wd / 127.f);
        symCurve(x, y + 2, w, h - 4, c, [&](float t) { return (t < lo ? expf(-(lo - t) * 14.f) : 1.f) * (t > hi ? expf(-(t - hi) * 14.f) : 1.f) * 0.8f; });
        return;
    }
    if (!strcmp(n, "KEYT")) { symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + (t - 0.5f) * fr; }, false); for (int i = 0; i < 7; i++) fill(x + i * w / 7, y + h - 4, w / 7 - 1, 4, C_TEXT); return; }
    if (!strcmp(n, "PHASE")) { int r = h / 2 - 2; ring(cx, cy, r, 1, cd); float a = fr * 6.2831853f; line(cx, cy, cx + (int)(r * sinf(a)), cy - (int)(r * cosf(a)), c, 2); return; }
    if (!strcmp(n, "FADE")) { float v = (val - 64) / 64.f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { return v < 0 ? fminf(1.f, t / (-v + 0.01f)) * 0.9f : (v > 0 ? fmaxf(0.f, 1.f - t / (1.01f - v)) * 0.9f : 0.9f); }); return; }
    if (d.kind == K_LDEST) { ring(x + w - h / 2, cy, h / 3, 2, c); disc(x + w - h / 2, cy, 3, c); line(x, cy, x + w - h / 2 - h / 3 - 2, cy, val ? c : C_DIM, 2); return; }
    if (d.kind == K_LSYNC || d.kind == K_DTIM || !strcmp(n, "D.TIME")) {   // Notensymbol
        disc(x + w / 3, y + h - 8, 6, c); fill(x + w / 3 + 5, y + 4, 2, h - 12, c);
        if (d.kind == K_LSYNC && val == 0) { ring(x + w * 2 / 3, cy, 8, 1, cd); }
        return;
    }
    if (d.kind == K_DIR) {
        static const char* ar[6] = {">>", "<<", "<>", "<=>", "??", "~~"};
        text(cx - textW(ar[val % 6], 2) / 2, cy - 16, ar[val % 6], c, 2);
        return;
    }
    if (!strcmp(n, "PULS") || !strcmp(n, "ROT")) {
        int len = curP().len[track] < 1 ? 1 : curP().len[track], k = pv(P_PULS), rot = pv(P_EROT), r = h / 2 - 3;
        int nn = len > 32 ? 32 : len;
        for (int i = 0; i < nn; i++) { float a = i * 6.2831853f / nn; int px = cx + (int)(r * sinf(a)), py = cy - (int)(r * cosf(a)); bool hit = k > 0 && euclidHitUi(i * len / nn, k, len, rot); if (hit) disc(px, py, 3, c); else fill(px, py, 1, 1, C_DIM); }
        return;
    }
    if (!strcmp(n, "CHNG")) {   // Wuerfel
        rrect(cx - h / 3, cy - h / 3, h * 2 / 3, h * 2 / 3, 5, C_TEXT);
        int pips = 1 + (int)(fr * 5.f), s3 = h / 6;
        static const int px[6][6][2] = {{{0,0}},{{-1,-1},{1,1}},{{-1,-1},{0,0},{1,1}},{{-1,-1},{1,-1},{-1,1},{1,1}},{{-1,-1},{1,-1},{0,0},{-1,1},{1,1}},{{-1,-1},{1,-1},{-1,0},{1,0},{-1,1},{1,1}}};
        for (int i = 0; i < pips; i++) disc(cx + px[pips - 1][i][0] * s3, cy + px[pips - 1][i][1] * s3, 2, C_BG);
        return;
    }
    if (d.kind == K_RANGE) { int st2 = 1 + val / 6; for (int i = 0; i < 8; i++) { int hh = (int)((i * 37 % 8) / 8.f * h * fminf(1.f, st2 / 8.f)) + 2; fill(x + i * w / 8, y + h - hh, w / 8 - 2, hh, val ? c : C_DIM); } return; }
    if (d.kind == K_SMPL) { for (int i = 0; i < w; i += 3) { float a = fabsf(sinf(i * 0.37f + val)) * expf(-i / (float)w * 1.5f); int hh = (int)(a * h / 2); fill(x + i, cy - hh, 2, hh * 2 + 1, c); } return; }
    if (d.kind == K_OFFON) { if (!strcmp(n, "LOOP") || !strcmp(n, "REV")) { text(cx - 16, cy - 16, !strcmp(n, "REV") ? (val ? "<<" : ">>") : (val ? "()" : "->"), val ? c : C_DIM, 2); return; }
        rrect(cx - 18, cy - 9, 36, 18, 9, val ? c : C_OFF2); disc(val ? cx + 9 : cx - 9, cy, 7, C_TEXT); return; }
    if (d.kind == K_ALGO) {   // Mini-Routing
        for (int k = 0; k < 4; k++) { int bx = x + FM_LAYOUT[val & 7][k][0] * w / 4, by = y + FM_LAYOUT[val & 7][k][1] * h / 3; rrect(bx, by + 2, w / 5, h / 4, 2, FM_CARRIERS[val & 7] >> k & 1 ? c : cd); }
        return;
    }
    if (d.kind == K_WTAB || !strcmp(n, "POS")) {
        if (curP().machine[track] == M_WTBL) { const float* f2 = wavetableFrame(pv(P_S0) & 3, pv(P_S1) * 31 / 127); symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + 0.5f * f2[(int)(t * 255)]; }, false); return; }
    }
    if (d.kind == K_SLICE || !strcmp(n, "SLCS")) { int nn = pv(P_S1) + 1; int sl = pv(P_S2); for (int i = 0; i < nn && i < 32; i++) fill(x + i * w / (nn < 32 ? nn : 32), y + 4, 1, h - 8, C_DIM); if (sl) fill(x + ((sl - 1) % nn) * w / nn, y + 4, w / nn > 2 ? w / nn : 2, h - 8, c); return; }
    if (!strcmp(n, "BPM")) {   // Metronom mit Pendel im Takt
        for (int i = 0; i < h; i++) { int hw = i * w / (3 * h); fill(cx - hw, y + i, 2 * hw + 1, 1, C_OFF2); }
        float a = sinf(animTime * 3.14159f * eng.proj.bpm / 60.f) * 0.5f;
        line(cx, y + h - 4, cx + (int)(sinf(a) * h * 0.9f), y + h - 4 - (int)(cosf(a) * h * 0.9f), c, 2);
        return;
    }
    if (d.kind == K_SWNG) { disc(x + w / 4, y + h - 8, 6, c); fill(x + w / 4 + 5, y + 4, 2, h - 12, c); int sx = x + w / 2 + (int)(fr * w / 3); disc(sx, y + h - 8, 6, c); fill(sx + 5, y + 4, 2, h - 12, c); return; }
    if (d.kind == K_ROOT) {   // Tastatur, Grundton leuchtet
        static const int wk[7] = {0, 2, 4, 5, 7, 9, 11}, bk[5] = {1, 3, 6, 8, 10}, bp[5] = {0, 1, 3, 4, 5};
        int kw = w / 7;
        for (int i = 0; i < 7; i++) rrect(x + i * kw, y + 4, kw - 1, h - 6, 2, wk[i] == val ? C_PLAY : C_OFF2);
        for (int i = 0; i < 5; i++) rrect(x + bp[i] * kw + kw - kw / 3, y + 4, kw * 2 / 3, (h - 6) * 3 / 5, 2, bk[i] == val ? C_PLAY : C_BG);
        return;
    }
    if (d.kind == K_SCALE) {
        static const int wk[7] = {0, 2, 4, 5, 7, 9, 11};
        int kw = w / 7;
        for (int i = 0; i < 7; i++) { bool on = val == 0 || inScale(wk[i], val); rrect(x + i * kw, y + 4, kw - 1, h - 6, 2, on ? c : C_OFF2); }
        return;
    }
    if (d.kind == K_THR) { fill(x, y + h - (int)(fr * h), w, 2, c); symCurve(x, y, w, h, cd, [&](float t) { return 0.5f + 0.4f * sinf(t * 18.f) * expf(-t); }, false); return; }
    if (d.kind == K_CRAT) { float r2 = val >= 8 ? 100.f : CMP_RATIOS[val]; symCurve(x, y + 2, w, h - 4, c, [&](float t) { return t < 0.5f ? t : 0.5f + (t - 0.5f) / r2; }, false); return; }
    if (d.kind == K_SCSRC) { if (val) drawIcon(curP().machine[(val - 1) % NTRACKS], cx - h / 2, y, h, c); else text(cx - 24, cy - 8, "SUMME", c); return; }
    if (d.kind == K_GR) { int gh = clampi((int)(grDisp / 24.f * h), 0, h); rrect(cx - 8, y, 16, h, 4, C_OFF); if (gh) rrect(cx - 8, y, 16, gh, 4, C_TRIG); return; }
    if (d.kind == K_FBK) { int nn = 1 + (int)(fr * 4.f); for (int i = 0; i < nn; i++) ring(cx, cy, 4 + i * (h / 2 - 4) / 4, 1, val > 106 && i == nn - 1 ? C_TRIG : c); return; }
    if (d.kind == K_MS) { fill(x, cy, (int)(fr * w / 2), 1, C_DIM); symCurve(x + (int)(fr * w / 2), y + 2, w - (int)(fr * w / 2), h - 4, c, [&](float t) { return expf(-t * 5.f); }); return; }
    // Standard: segmentierter Pegel
    for (int i = 0; i < 8; i++) { int hh = 6 + i * (h - 6) / 8; rrect(x + i * w / 8, y + h - hh, w / 8 - 3, hh, 2, (i + 0.5f) / 8.f <= fr ? c : C_OFF2); }
}

// ---------------------------------------------------------------- Kacheln statt Drehregler

static void tile(int x, int y, int k, const ParamDef& d, int val, int baseVal, bool locked, const char* label, bool sel, Col tc, Uint32 now) {
    char b[32];
    bool justEdited = sel && editAt && now - editAt < 300;
    rrect(x + 4, y + 2, 152, 88, 10, sel ? C_SEL : C_PANEL);
    if (sel) {   // Rahmen in Spurfarbe, pulsierend
        float br = 0.55f + 0.25f * sinf(animTime * 3.f);
        Col fc = dimC(locked ? C_LOCK : tc, br);
        rrect(x + 4, y + 2, 152, 2, 1, fc); rrect(x + 4, y + 88, 152, 2, 1, fc);
        rrect(x + 4, y + 2, 2, 88, 1, fc); rrect(x + 154, y + 2, 2, 88, 1, fc);
    }
    if (d.kind == K_NONE) return;
    char nm[24];
    snprintf(nm, sizeof nm, "%.18s", label[0] ? label : d.name);
    text(x + 14, y + 6, nm, sel ? (locked ? C_LOCK : C_TEXT) : C_DIM);
    knobText(d, val, b, sizeof b, k);
    Col vc = locked ? C_LOCK : (sel ? mix(tc, C_PLAY, justEdited ? 0.5f : 0.f) : C_TEXT);
    float target = knobFrac(d, val);
    knobAnim[k] += (target - knobAnim[k]) * (1.f - expf(-animDt * 14.f));
    // Wert-Symbol links, Zahl rechts
    Col sc2 = locked ? C_LOCK : (sel ? tc : dimC(tc, 0.75f));
    int sw = 78, sh = 50;
    paramSymbol(d, val, knobAnim[k], x + 14, y + 28, sw, sh, mix(sc2, C_PLAY, justEdited ? 0.4f : 0.f));
    int scl = textW(b, 2) <= 52 ? 2 : 1;
    text(x + 150 - textW(b, scl), y + (scl == 2 ? 44 : 52), b, vc, scl);
    if (locked) { text(x + 150 - textW("LOCK"), y + 6, "LOCK", C_LOCK); }
}

// ---------------------------------------------------------------- Grafiken der Engines

// Peaks eines Samples (einmal pro Sample/Breite berechnet)
static const float* samplePeaks(int idx, int W) {
    static int cIdx = -1, cW = 0; static float pk[700];
    const Sample* sm = sampleGet(idx);
    if (!sm) return nullptr;
    if (cIdx != idx || cW != W) {
        cIdx = idx; cW = W;
        for (int i = 0; i < W; i++) {
            int a = (int)((long)sm->len * i / W), b = (int)((long)sm->len * (i + 1) / W);
            float m = 0; int stp = (b - a) / 64 + 1;
            for (int j = a; j < b; j += stp) { float v = fabsf(sm->data[j]); if (v > m) m = v; }
            pk[i] = m;
        }
    }
    return pk;
}

// Drums: grosses Symbol (springt beim Trig), Ausklingkurve, Tonhoehen-Sweep
static void drumGraph(int gx, int gy, int gw, int gh, int m, Col c) {
    graphBox(gx, gy, gw, gh);
    int s = 46 + (int)(flash[track] * 8);
    rrect(gx + 8, gy + 4, 54, 54, 12, mix(C_PANEL, c, flash[track] * 0.25f));
    drawIcon(m, gx + 35 - s / 2, gy + 31 - s / 2, s, mix(c, C_PLAY, flash[track] * 0.5f));
    // Ausklingkurve aus DCAY (Index 1 bei allen Drums, Shaker: Attack+Decay)
    int x0 = gx + 80, W = gw - 200, top = gy + 10, bot = gy + gh - 10;
    float dc = pv(P_S1) / 127.f;
    static int ys[600];
    if (W > 590) W = 590;
    for (int i = 0; i < W; i++) {
        float t = (float)i / W, v;
        if (m == M_SHKR) { float a = 0.02f + pv(P_S2) / 127.f * 0.25f; v = t < a ? t / a : expf(-(t - a) * (9.f - dc * 8.f)); }
        else v = expf(-t * (12.f - dc * 11.f));
        ys[i] = bot - (int)(v * (bot - top));
    }
    curve(x0, ys, W, bot, c, 40);
    if (m == M_KICK || m == M_TOM) {   // Tonhoehen-Sweep gestrichelt
        float sw = pv(P_S2) / 127.f;
        for (int i = 0; i < W; i += 4) { float t = (float)i / W; int yy = bot - (int)((0.3f + 0.7f * sw * expf(-t * 18.f)) * (bot - top) * 0.6f); fill(x0 + i, yy, 2, 2, C_LOCK); }
        text(x0 + W - 70, top - 4, "PITCH", C_LOCK);
    }
    // Pegel der Spur
    int lh = clampi((int)(sqrtf(meterV[track]) * (gh - 12)), 0, gh - 12);
    rrect(gx + gw - 26, gy + 6, 10, gh - 12, 4, C_OFF);
    if (lh) rrect(gx + gw - 26, gy + gh - 6 - lh, 10, lh, 4, c);
    text(gx + gw - 110, gy + gh - 18, MACHINE_LONG[m], C_DIM);
}

const float* wavetableFrame(int t, int f);

// Wavetable: Frames perspektivisch gestapelt, aktuelle Position hervorgehoben
static void wtGraph(int gx, int gy, int gw, int gh, int tab, int pos, Col c) {
    graphBox(gx, gy, gw, gh);
    int W = 300, x0 = gx + 20, base = gy + gh - 4;
    int cur = pos * 31 / 127;
    for (int f = 31; f >= 0; f -= 3) {
        const float* fr = wavetableFrame(tab, f);
        int ox = x0 + f * 7, oy = base - f * 20 / 31 - 10;
        Col cc = dimC(c, 0.18f + 0.25f * (1.f - f / 31.f));
        int px = ox, py = oy - (int)(fr[0] * 9);
        for (int i = 1; i < W; i += 3) { int nx = ox + i, ny = oy - (int)(fr[i * 256 / W] * 9); line(px, py, nx, ny, cc); px = nx; py = ny; }
    }
    const float* fr = wavetableFrame(tab, cur);
    int ox = x0 + cur * 7, oy = base - cur * 20 / 31 - 10, px = ox, py = oy - (int)(fr[0] * 12);
    for (int i = 1; i < W; i++) { int nx = ox + i, ny = oy - (int)(fr[i * 256 / W] * 12); line(px, py, nx, ny, c, 2); px = nx; py = ny; }
    static const char* tn[4] = {"BASIC: SIN > TRI > SAW > SQR", "PWM: 50 % > 5 %", "VOKAL: A > E > I > O > U", "OBERTOENE: 1 > 32"};
    text(gx + gw - 10 - textW(tn[tab & 3]), gy + 4, tn[tab & 3], dimC(c, 0.8f));
    char b[24]; snprintf(b, sizeof b, "FRAME %d/32", cur + 1);
    text(gx + gw - 10 - textW(b), gy + 22, b, c);
}

// Chop: Wellenform mit Scheiben, aktuelle Scheibe hell, Abspielposition
static void chopGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int idx = pv(P_S0);
    const Sample* sm = sampleGet(idx);
    if (!sm) { textC(gx + gw / 2, gy + gh / 2 - 8, "KEINE SAMPLES IM ORDNER samples/", C_DIM); return; }
    int x0 = gx + 10, W = gw - 20, mid = gy + gh / 2, A = (gh - 16) / 2;
    if (W > 690) W = 690;
    const float* pk = samplePeaks(idx, W);
    int n = pv(P_S1) + 1, sl = pv(P_S2) ? (pv(P_S2) - 1) % n : (eng.playing ? eng.playedStep[track] : cursor) % n;
    for (int i = 0; i < W; i++) {
        int h = (int)(sqrtf(pk[i]) * A);
        bool cur = i * n / W == sl;
        fill(x0 + i, mid - h, 1, 2 * h + 1, cur ? c : dimC(c, 0.35f));
    }
    for (int k = 1; k < n; k++) { int xx = x0 + k * W / n; for (int yy = gy + 4; yy < gy + gh - 4; yy += 4) fill(xx, yy, 1, 2, C_LOCK); }
    const Voice& v = eng.v[track];
    if (v.active && v.machine == M_CHOP && v.smp == sm) fill(x0 + (int)(v.spos / sm->len * (W - 1)), gy + 4, 2, gh - 8, C_PLAY);
    char b[48]; snprintf(b, sizeof b, "SCHEIBE %d/%d%s", sl + 1, n, pv(P_S2) ? "" : " (AUTO)");
    text(gx + gw - 10 - textW(b), gy + 4, b, C_TEXT);
}

// Granulator: Wellenform, Positionsfenster, aktive Koerner als Punkte
static void granGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int idx = pv(P_S0);
    const Sample* sm = sampleGet(idx);
    if (!sm) { textC(gx + gw / 2, gy + gh / 2 - 8, "KEINE SAMPLES IM ORDNER samples/", C_DIM); return; }
    int x0 = gx + 10, W = gw - 20, mid = gy + gh / 2, A = (gh - 16) / 2;
    if (W > 690) W = 690;
    const float* pk = samplePeaks(idx, W);
    float pos = pv(P_S1) / 127.f, spr = pv(P_S2) / 127.f * 0.5f;
    int wl = x0 + (int)((pos - spr) * W), wr = x0 + (int)((pos + spr) * W);
    fill(wl < x0 ? x0 : wl, gy + 4, (wr > x0 + W ? x0 + W : wr) - (wl < x0 ? x0 : wl) + 1, gh - 8, c, 40);
    for (int i = 0; i < W; i++) { int h = (int)(sqrtf(pk[i]) * A); fill(x0 + i, mid - h, 1, 2 * h + 1, dimC(c, 0.45f)); }
    fill(x0 + (int)(pos * W), gy + 4, 2, gh - 8, C_PLAY);
    const Voice& v = eng.v[track];
    if (v.active && v.machine == M_GRAN && v.smp == sm)
        for (int g = 0; g < 12; g++) if (v.gr[g].on) {
            int xx = x0 + (int)(v.gr[g].pos / sm->len * W);
            float w = sinf(v.gr[g].age / v.gr[g].len * 3.14159f);
            disc(xx, mid - (int)((g % 5 - 2) * A * 0.35f), 2 + (int)(w * 4), mix(c, C_PLAY, w * 0.6f));
        }
    char b[48];
    snprintf(b, sizeof b, "KORN %.0f ms  %.0f/s", 10.f * exp2f(pv(P_S3) * (5.6f / 127.f)), 2.f * exp2f(pv(P_S4) * (6.f / 127.f)));
    text(gx + gw - 10 - textW(b), gy + 4, b, C_TEXT);
}

// Engine-Waehler als Bildraster (beim Drehen von ENGINE)
static void drawEnginePicker(int cur, Col c) {
    rrect(40, 50, 560, 220, 14, c);
    rrect(42, 52, 556, 216, 13, C_BG, 250);
    for (int i = 0; i < M_COUNT; i++) {
        int m = MACHINE_ORDER[i], col = i % 7, row = i / 7;
        int x = 52 + col * 78, y = 62 + row * 86;
        bool sel = m == cur;
        rrect(x, y, 72, 80, 10, sel ? c : C_PANEL);
        drawIcon(m, x + 18, y + 8, 36, sel ? C_BG : TCOL[track]);
        textC(x + 36, y + 56, MACHINE_NAMES[m], sel ? C_BG : C_TEXT);
    }
    textC(320, 246, MACHINE_LONG[cur], C_TEXT);
}

// Wert-Chip unter einer Grafik
static void paramChip(int k, int x, int y, Col tc) {
    char b[32], label[24];
    ParamDef d; int val, baseVal; bool locked;
    bool ok = knobInfo(k, d, val, locked, baseVal, label, sizeof label);
    bool sel = k == slot;
    rrect(x + 4, y, 152, 26, 7, sel ? C_SEL : C_BG);
    if (!ok) return;
    Col c = locked ? C_LOCK : tc;
    knobAnim[k] += (knobFrac(d, val) - knobAnim[k]) * (1.f - expf(-animDt * 16.f));
    int bw = (int)(140 * knobAnim[k]);
    fill(x + 10, y + 22, 140, 2, C_OFF2);
    if (bw > 0) fill(x + 10, y + 22, bw, 2, sel ? c : dimC(c, 0.7f));
    knobText(d, val, b, sizeof b, k);
    char nm[24];
    int maxc = (140 - textW(b) - 8) / FONT_W;
    snprintf(nm, sizeof nm, "%.*s", maxc < 1 ? 1 : (maxc > 23 ? 23 : maxc), label[0] ? label : d.name);
    text(x + 10, y + 3, nm, sel ? C_TEXT : C_DIM);
    text(x + 150 - textW(b), y + 3, b, locked ? C_LOCK : (sel ? c : C_TEXT));
}

// welche Reihe (0/1) der aktuellen Seite als Grafik erscheint
static int graphKind(int row) {
    if (page == PG_AMP && row == 0) return 1;             // AMP: Huellkurve
    if (page == PG_FLT) return row == 0 ? 2 : 1;          // FLT: Filter + Huellkurve
    if ((page == PG_LFO1 || page == PG_LFO2 || page == PG_LFO3) && row == 0) return 3;   // LFOs
    if (page == PG_SRC && row == 0) {
        int m = curP().machine[track];
        if (m == M_SYNTH) return 4;
        if (m == M_SAMPLE) return 5;
        if (m == M_FM) return 6;                          // FM: Algorithmus-Diagramm
        if (machineIsDrum(m)) return 8;
        if (m == M_WTBL) return 9;
        if (m == M_CHOP) return 10;
        if (m == M_GRAN) return 11;
    }
    if (page == PG_SRC2 && row == 0 && curP().machine[track] == M_FM) return 6;
    if (page == PG_SEQ && row == 0) return 7;
    if (page == PG_FLT2 && row == 0) return 12;
    return 0;
}
static int pv(int idx) {   // Wert (inkl. Lock beim Halten) eines Spurparameters
    const Pattern& P = curP();
    if (aHeld && holdS().lock[idx] != NO_LOCK) return holdS().lock[idx];
    return P.p[track][idx];
}

// Mixer: 8 Kanalzuege mit Fader, Pegel, Panorama, Sends, Mute/Solo
static void drawMixer(int ox) {
    const Pattern& P = curP();
    char b[16];
    for (int t = 0; t < NTRACKS; t++) {
        int x = t * 80 + ox, y = 44;
        bool sel = t == track;
        Col tc = eng.audible(t) ? TCOL[t] : dimC(TCOL[t], 0.35f);
        rrect(x + 2, y + 1, 76, 188, 8, sel ? C_SEL : C_PANEL);
        snprintf(b, sizeof b, "T%d", t + 1);
        text(x + 8, y + 4, b, sel ? TCOL[t] : C_TEXT);
        drawIcon(P.machine[t] % M_COUNT, x + 8, y + 19, 22, mix(TCOL[t], C_PLAY, flash[t] * 0.5f));
        // Mute / Solo
        rrect(x + 50, y + 5, 22, 14, 3, eng.mute[t] ? C_TRIG : C_OFF2); text(x + 57, y + 4, "M", eng.mute[t] ? C_BG : C_DIM);
        rrect(x + 50, y + 21, 22, 14, 3, eng.solo[t] ? C_LOCK : C_OFF2); text(x + 57, y + 20, "S", eng.solo[t] ? C_BG : C_DIM);
        // Fader + Pegel
        int fy = y + 44, fh = 84, vol = P.p[t][P_VOL];
        int hy = fy + fh - vol * fh / 127;
        fill(x + 22, fy, 4, fh, C_OFF2);
        fill(x + 22, hy, 4, fy + fh - hy, dimC(tc, 0.7f));
        bool rs = sel && mixRow == 0;
        rrect(x + 12, hy - 4, 24, 9, 3, rs ? C_PLAY : tc);
        int mh = clampi((int)(sqrtf(meterV[t]) * fh), 0, fh);
        fill(x + 44, fy, 5, fh, C_OFF);
        if (mh) fill(x + 44, fy + fh - mh, 5, mh, meterV[t] > 0.9f ? C_TRIG : C_GREEN);
        snprintf(b, sizeof b, "%d", vol);
        text(x + 54, fy + fh - 14, b, rs ? C_PLAY : C_DIM);
        // Panorama + Sends
        int rowY[3] = {y + 138, y + 154, y + 170};
        for (int r = 1; r < 4; r++) {
            int yy = rowY[r - 1], v = P.p[t][MIX_P[r]];
            bool on = sel && mixRow == r;
            text(x + 8, yy - 5, MIX_NAMES[r], on ? C_PLAY : C_DIM);
            int bx = x + 36, bw = 36;
            fill(bx, yy + 2, bw, 3, C_OFF2);
            if (r == 1) { int px = bx + v * (bw - 1) / 127; fill(bx + bw / 2, yy, 1, 7, C_DIM); rrect(px - 2, yy, 5, 7, 2, on ? C_PLAY : tc); }
            else { int w = v * bw / 127; if (w) fill(bx, yy + 2, w, 3, on ? C_PLAY : tc); }
        }
    }
    // Zeilen-Hinweis fuer den gewaehlten Kanal
    char h[48];
    int v = P.p[track][MIX_P[mixRow]];
    if (mixRow == 1) snprintf(h, sizeof h, "T%d  PAN %+d", track + 1, v - 64);
    else snprintf(h, sizeof h, "T%d  %s %d", track + 1, MIX_NAMES[mixRow], v);
    text(8, 222, h, TCOL[track]);
    text(640 - 8 - textW("A=MUTE  B=SOLO"), 222, "A=MUTE  B=SOLO", C_DIM);
}

// Seitenmenue (SELECT kurz antippen)
static void drawMenu() {
    rrect(70, 60, 500, 300, 14, C_ACC);
    rrect(72, 62, 496, 296, 13, C_BG, 250);
    textC(320, 72, "SEITE WAEHLEN", C_ACC, 2);
    for (int g = 0; g < 3; g++) {
        int y = 128 + g * 72;
        text(92, y - 20, GROUP_NAMES[g], C_DIM);
        int x = 92;
        for (int p = 0; p < NUI_PAGES; p++) {
            if (PG_GROUP[p] != g || !pageAvail(p)) continue;
            bool sel = p == menuSel, cur = p == page;
            int w = textW(PAGE_NAMES[p]) + 22;
            rrect(x, y, w, 30, 8, sel ? C_ACC : (cur ? C_SEL : C_PANEL));
            text(x + 11, y + 7, PAGE_NAMES[p], sel ? C_BG : (cur ? C_ACC : C_TEXT));
            x += w + 8;
        }
    }
    textC(320, 334, PAGE_LONG[menuSel], C_TEXT);
}

static void drawConfirm() {
    int w = textW(confirmText) + 48; if (w < 360) w = 360; if (w > 620) w = 620;
    rrect(320 - w / 2, 170, w, 110, 14, C_ACC);
    rrect(322 - w / 2, 172, w - 4, 106, 13, C_BG, 250);
    textC(320, 190, confirmText, C_TEXT);
    rrect(220, 226, 90, 32, 8, C_GREEN); textC(265, 234, "A = JA", C_BG);
    rrect(330, 226, 90, 32, 8, C_OFF2); textC(375, 234, "B = NEIN", C_TEXT);
}

static void drawKnobs() {
    char b[32], label[24];
    Col tc = TCOL[track];
    Uint32 now = SDL_GetTicks();
    int ox = (int)slideOff;
    if (page == PG_MIX) { drawMixer(ox); goto overlays; }
    for (int row = 0; row < 2; row++) {
        int y = 44 + row * 96;
        int gk = graphKind(row);
        if (gk) {
            int gx = 4 + ox, gy = y + 2, gw = 632, gh = 62;
            int selRel = (slot / 4 == row) ? slot % 4 : -1;
            const Voice& v = eng.v[track];
            switch (gk) {
            case 1: {
                int base = page == PG_AMP ? P_AATK : P_FATK;
                envGraph(gx, gy, gw, gh, pv(base), pv(base + 1), pv(base + 2), pv(base + 3), selRel,
                         v.active ? (page == PG_AMP ? &v.aenv : &v.fenv) : nullptr, tc);
            } break;
            case 2:
                filterGraph(gx, gy, gw, gh, pv(P_FREQ), pv(P_RESO), pv(P_FTYP), pv(P_FENV), selRel,
                            v.active ? v.fBase + v.fEnv * v.fenv.v : -1.f, tc);
                break;
            case 12:
                bwGraph(gx, gy, gw, gh, pv(P_FREQ), pv(P_RESO), pv(P_FTYP), pv(P_BASE), pv(P_WDTH), selRel, tc);
                break;
            case 3: {
                int k = page - PG_LFO1;
                lfoGraph(gx, gy, gw, gh, k, LFO_BASE[k], v.lfPh[k], v.active, tc);
            } break;
            case 4:
                oscGraph(gx, gy, gw, gh, pv(P_S0) & 3, pv(P_S1) & 3, pv(P_S2), pv(P_S3), pv(P_S6), pv(P_S4), selRel, tc);
                break;
            case 6: {
                int rat[4] = {pv(P_S2), pv(P_S3), pv(P_S4), pv(P_S5)}, lvl[4] = {127, pv(P_X0), pv(P_X1), pv(P_X2)};
                int selOp = -1;
                if (page == PG_SRC) selOp = selRel == 0 ? 4 : (selRel == 1 ? 3 : (selRel >= 2 ? selRel - 2 : -1));
                else selOp = selRel >= 0 && selRel < 3 ? selRel + 1 : (selRel == 3 ? 0 : -1);
                fmGraph(gx, gy, gw, gh, pv(P_S0), rat, lvl, selOp, &v, tc);
            } break;
            case 7: seqGraph(gx, gy, gw, gh, selRel, tc); break;
            case 8: drumGraph(gx, gy, gw, gh, curP().machine[track], tc); break;
            case 9: wtGraph(gx, gy, gw, gh, pv(P_S0) & 3, pv(P_S1), tc); break;
            case 10: chopGraph(gx, gy, gw, gh, tc); break;
            case 11: granGraph(gx, gy, gw, gh, tc); break;
            case 5:
                sampleGraph(gx, gy, gw, gh, pv(P_S0), pv(P_S3), pv(P_S4), pv(P_S5) != 0, selRel, tc);
                break;
            }
            for (int k = row * 4; k < row * 4 + 4; k++) paramChip(k, (k % 4) * 160 + ox, y + 66, tc);
            continue;
        }
        for (int k = row * 4; k < row * 4 + 4; k++) {
            int x = (k % 4) * 160 + ox;
            bool sel = k == slot;
            ParamDef d; int val, baseVal; bool locked;
            bool ok = knobInfo(k, d, val, locked, baseVal, label, sizeof label);
            int cx = x + 80, cy = y + 38;
            if (ok && d.kind == K_ACT) {   // Schaltflaeche
                if (sel) rrect(x + 4, y + 2, 152, 88, 10, C_SEL);
                float p = sel ? 0.5f + 0.5f * sinf(animTime * 4.f) : 0.f;
                rrect(cx - 54, cy - 18, 108, 36, 10, sel ? mix(C_ACC, C_PLAY, p * 0.25f) : C_OFF2);
                textC(cx, cy - 8, d.name, sel ? C_BG : C_TEXT);
                textC(cx, y + 70, sel ? "A = START" : "", C_DIM);
                continue;
            }
            Col kc = trackPage(page) ? tc : C_ACC;
            if (ok && d.kind == K_GR) kc = C_TRIG;
            tile(x, y, k, ok ? d : PDEF_NONE, val, baseVal, locked, label, sel, kc, now);
        }
    }
overlays:
    // grosse Werteanzeige beim Drehen bzw. Meldung (die juengste gewinnt)
    bool showEdit = editAt && (now - editAt < 900 || editAt == 1) && editAt >= msgAt;
    bool showMsg = now < msgUntil && msgAt > editAt;
    if (showEdit) {
        ParamDef d; int val, baseVal; bool locked;
        if (knobInfo(editSlot, d, val, locked, baseVal, label, sizeof label)) {
            knobText(d, val, b, sizeof b, editSlot);
            Col c = locked ? C_LOCK : (!trackPage(page) ? C_ACC : tc);
            float t = editAt == 1 ? 1.f : fminf((now - editAt) / 120.f, 1.f);   // kurzes Aufpoppen
            int oy = 204 + (int)((1.f - t) * 8);
            rrect(150, oy, 340, 34, 8, C_BG);
            rrect(150, oy, 4, 34, 2, c);
            text(164, oy + 9, label[0] ? label : d.name, C_DIM);
            int bx = 330, bw = 100;
            if (d.kind == K_SCALE) {   // Mini-Tastatur mit den Skalentoenen
                static const int white[7] = {0, 2, 4, 5, 7, 9, 11}, black[5] = {1, 3, 6, 8, 10};
                static const int bpos[5] = {0, 1, 3, 4, 5};
                int kx = 300, kw = 14;
                for (int i = 0; i < 7; i++) {
                    bool on = val == 0 || inScale(white[i], val), root = val > 0 && (val - 1) % 12 == white[i];
                    rrect(kx + i * kw, oy + 6, kw - 2, 22, 2, root ? C_PLAY : (on ? c : C_OFF2));
                }
                for (int i = 0; i < 5; i++) {
                    bool on = val == 0 || inScale(black[i], val), root = val > 0 && (val - 1) % 12 == black[i];
                    rrect(kx + bpos[i] * kw + kw - 5, oy + 6, 9, 13, 2, root ? C_PLAY : (on ? dimC(c, 0.7f) : C_BG));
                }
            } else {
                rrect(bx, oy + 13, bw, 8, 3, C_OFF2);
                int fw = (int)(bw * knobFrac(d, val));
                if (fw > 0) rrect(bx, oy + 13, fw, 8, 3, c);
            }
            text(478 - textW(b, 1) - 2, oy + 9, b, c);
            if (locked) text(bx, oy + 1, "LOCK", C_LOCK);
            if (d.kind == K_MACH) drawEnginePicker(val, c);
            if (d.kind == K_MTIM) {   // Zeitlineal: Mitte = Raster
                rrect(bx - 8, oy + 6, bw + 16, 22, 6, C_BG);
                fill(bx, oy + 17, bw, 1, C_DIM);
                for (int k2 = 0; k2 <= 4; k2++) fill(bx + k2 * bw / 4, oy + (k2 == 2 ? 9 : 13), 1, k2 == 2 ? 16 : 8, k2 == 2 ? C_TEXT : C_DIM);
                int mx = bx + bw / 2 + (val - 23) * (bw / 2) / 23;
                rrect(mx - 3, oy + 11, 7, 12, 3, c);
            }
            if (d.kind == K_INS) {
                const char* const* names = INS_NAMES;
                int cnt = INS_COUNT;
                rrect(150, oy, 340, 34, 8, C_BG, 250);
                rrect(150, oy, 4, 34, 2, c);
                for (int m = 0; m < cnt; m++) {
                    int mx = 160 + m * 47;
                    if (m == val) rrect(mx - 2, oy + 7, 46, 20, 5, c);
                    textC(mx + 21, oy + 9, names[m], m == val ? C_BG : C_DIM);
                }
            }
        }
    } else if (showMsg) {
        int w = textW(msg) + 28;
        rrect(320 - w / 2, 210, w, 26, 8, C_ACC);
        textC(320, 215, msg, C_BG);
    }
    if (pageAt && now - pageAt < 450) {
        textAlpha = (Uint8)(200 * (1.f - (now - pageAt) / 450.f));
        textC(320 - (int)slideOff, 120, PAGE_NAMES[page], C_PLAY, 4);
        textAlpha = 255;
    }
}

static Col offCol(int s) { return (s / 4) % 2 ? C_OFF2 : C_OFF; }

static void drawGrid() {
    const Pattern& P = curP();
    char b[16];
    int y0 = 244;
    rrect(0, (int)hlY - 2, SW, 18, 4, C_PANEL);   // gleitende Spur-Markierung
    for (int t = 0; t < NTRACKS; t++) {
        int y = y0 + t * 18;
        Col tc = eng.mute[t] ? dimC(TCOL[t], 0.35f) : TCOL[t];
        int chipH = 14 + (int)(flash[t] * 4);
        rrect(4, y + 7 - chipH / 2, 4, chipH, 1, mix(tc, C_PLAY, flash[t] * 0.7f));
        Col lc = eng.mute[t] ? C_DIM : (t == track ? TCOL[t] : C_TEXT);
        snprintf(b, sizeof b, "%d", t + 1);
        text(12, y - 2, b, lc);
        drawIcon(P.machine[t] % M_COUNT, 21, y - 1, 15, eng.mute[t] ? C_DIM : mix(TCOL[t], C_PLAY, flash[t] * 0.6f));
        text(39, y - 2, MACHINE_NAMES[P.machine[t] % M_COUNT], lc);
        if (eng.mute[t]) fill(39, y + 5, 40, 1, C_TRIG);
        if (P.p[t][P_PULS] > 0 || P.p[t][P_DENS] > 0) rrect(32, y - 1, 5, 5, 2, C_LOCK);   // Euklid/Generator aktiv
        int mw = clampi((int)(sqrtf(meterV[t]) * 62), 0, 62);
        fill(12, y + 13, 62, 2, C_OFF);
        if (mw) fill(12, y + 13, mw, 2, tc);
        int len = P.len[t];
        int base = (cursor / 16) * 16;
        for (int s0 = 0; s0 < 16; s0++) {
            int s = base + s0;
            int x = 82 + s0 * 34;
            if (s >= len) continue;
            const Step& st = P.s[t][s];
            bool ph = eng.playing && eng.playedStep[t] == s;
            int pul = P.p[t][P_PULS];
            bool shown = pul > 0 ? euclidHitUi(s, pul, len, P.p[t][P_EROT]) : st.on;
            Col c = shown ? (pul > 0 ? mix(tc, C_PLAY, 0.15f) : tc) : offCol(s);
            int grow = 0;
            if (ph) { c = st.on ? mix(tc, C_PLAY, 0.25f + flash[t] * 0.6f) : mix(c, C_PLAY, 0.18f); if (st.on) grow = (int)(flash[t] * 3); }
            int mt = st.lock[P_MTIM] != NO_LOCK ? st.lock[P_MTIM] : P.p[t][P_MTIM];
            int mo = shown ? (mt - 23) * 10 / 23 : 0;   // Mikrotiming sichtbar verschoben
            rrect(x - grow + mo, y + 1 - grow / 2, 31 + 2 * grow, 12 + grow, 3, c);
            if (mo) fill(x + 15, y + 6, 1, 3, C_BG);
            if (st.on && stepHasLock(st)) fill(x + 25, y + 3, 3, 3, C_BG);
        }
    }
    // gleitender Cursor
    int cx = 82 + (int)(curXg * 34 + 0.5f), cy = y0 + track * 18;
    Col cc = aHeld ? C_LOCK : C_TEXT;
    rrect(cx - 3, cy - 2, 37, 2, 1, cc); rrect(cx - 3, cy + 14, 37, 2, 1, cc);
    rrect(cx - 3, cy - 2, 2, 18, 1, cc); rrect(cx + 32, cy - 2, 2, 18, 1, cc);
}

static void drawSteps() {
    const Pattern& P = curP();
    char b[8];
    Col tc = eng.mute[track] ? dimC(TCOL[track], 0.35f) : TCOL[track];
    int len = P.len[track];
    int sv = curScale();
    if (eng.recording) {
        float pl = 0.5f + 0.5f * sinf(animTime * 6.f);
        rrect(2, 386, 628, 56, 8, mix(dimC(C_TRIG, 0.4f), C_TRIG, pl));
        rrect(4, 388, 624, 52, 7, C_BG);
    }
    // Seitenleiste: 8 Seiten a 16 Steps (Cursor-Seite hell, Wiedergabe in Spurfarbe)
    {
        int npg = (len + 15) / 16, cp = cursor / 16, pp = eng.playing ? eng.playedStep[track] / 16 : -1;
        for (int k = 0; k < 8; k++) {
            int x = 6 + k * 78;
            Col c = k >= npg ? C_DARK : (k == cp ? C_TEXT : C_OFF2);
            rrect(x, 389, 74, 4, 2, c);
            if (k == pp && k != cp) rrect(x, 389, 74, 4, 2, tc);
            if (k == pp && k == cp) rrect(x + 30, 389, 14, 4, 2, tc);
        }
    }
    int base = (cursor / 16) * 16;
    // gleitender Cursor hinter den Tasten
    int ccx = 6 + (int)(curXg * 39 + 0.5f);
    rrect(ccx - 3, 395, 42, 46, 8, aHeld ? C_LOCK : C_TEXT);
    for (int s0 = 0; s0 < 16; s0++) {
        int s = base + s0;
        int x = 6 + s0 * 39, y = 398;
        if (s >= len) { rrect(x, y, 36, 40, 6, C_DARK); continue; }
        const Step& st = P.s[track][s];
        bool ph = eng.playing && eng.playedStep[track] == s;
        int pul = P.p[track][P_PULS];
        bool shown = pul > 0 ? euclidHitUi(s, pul, len, P.p[track][P_EROT]) : st.on;
        Col c = shown ? (pul > 0 ? mix(tc, C_PLAY, 0.15f) : tc) : offCol(s);
        int lift = 0;
        if (ph && shown) { c = mix(tc, C_PLAY, 0.2f + flash[track] * 0.6f); lift = (int)(flash[track] * 3); }
        rrect(x, y - lift, 36, 40, 6, c);
        if (ph) rrect(x + 6, y + 3 - lift, 24, 4, 2, st.on ? C_BG : C_PLAY);
        if (st.on && stepHasLock(st)) rrect(x + 27, y + 9 - lift, 5, 5, 2, C_BG);
        if (st.on && st.lock[P_NOTE] != NO_LOCK) {
            char nb[8];
            fmtParam(COMMON_DEFS[P_NOTE], quantizeNote(st.lock[P_NOTE], sv), nb, sizeof nb);
            textC(x + 18, y + 9 - lift, nb, dimC(C_BG, 0.85f));
        }
        snprintf(b, sizeof b, "%d", s + 1);
        textC(x + 18, y + 22 - lift, b, st.on ? C_BG : C_DIM);
    }
    const char* h;
    if (lockLatch) h = "LOCK-MODUS [R-STICK/X/Y]WERT [</>]STEP [B]LOCKS WEG [A]FERTIG";
    else if (aHeld) h = "[X/Y/R-STICK] LOCK  [B] LOESCHEN  LANGE HALTEN = LOCK-MODUS";
    else if (in.down[BT_SELECT]) h = "[</>]PTN [^]REC [v]TAP [A]MUTE [B]CLR [L1]FILL [L2/R2]LAENGE";
    else if (page == PG_MIX) h = "[^/v]KANAL [L-STICK]ZEILE/KANAL [R-STICK^v]WERT [A]MUTE [B]SOLO";
    else if (page == PG_PRJ) h = "[L-STICK]WAEHLEN [R-STICK^v]PLATZ [A/X]AUSFUEHREN [SEL]MENUE";
    else h = "[A]TRIG [B]HOER [L-STICK]REGLER [R-STICK]WERT [SEL+R-STICK]SEITE";
    hint(6, 452, h, C_ACC, C_DIM);
}

static void drawHelp() {
    rrect(30, 22, 580, 436, 12, C_ACC);
    rrect(32, 24, 576, 432, 11, C_BG, 245);
    static const char* lines[] = {
        "SELECT (FUNC) GEHALTEN +",
        "  LINKS/RECHTS   Pattern wechseln",
        "  HOCH / RUNTER  Aufnahme an/aus / Tap-Tempo",
        "  A / B          Spur stumm / Spur loeschen",
        "  X / Y          Engine wechseln",
        "  L1 / R1        FILL an/aus / Pattern einfuegen",
        "  L2 / R2        Spurlaenge -/+",
        "  START          Speichern (2s halten: Ende)",
        "",
        "OHNE FUNC",
        "  A              Trig setzen/loeschen",
        "  A lang halten  LOCK-MODUS: R-Stick/X/Y lockt, </> Step",
        "                 B Locks loeschen, A antippen = fertig",
        "  B              Spur vorhoeren",
        "  Linker Stick   auf Regler zeigen (oben 1-4, unten 5-8)",
        "  Rechter Stick  hoch/runter: Wert (nur Werte)",
        "  SEL+R-Stick    links/rechts: Seite  hoch/runter: DJ-Filter",
        "  START          Play / Stop",
    };
    for (unsigned i = 0; i < sizeof lines / sizeof *lines; i++)
        text(48, 36 + i * 22, lines[i], i == 0 || i == 9 ? C_ACC : C_TEXT);
}

static void drawSettings();
static void drawAll(bool help) {
    animate();
    setCol(C_BG);
    SDL_RenderClear(ren);
    drawHeader();
    if (page == PG_SYS) drawSettings();
    else { drawKnobs(); drawGrid(); drawSteps(); }
    if (help) drawHelp();
    if (menuOpen) drawMenu();
    if (confirmOpen) drawConfirm();
}

// ---------------------------------------------------------------- Bedienung

static void editValue(int delta) {
    editAt = SDL_GetTicks(); editSlot = slot;
    if (page == PG_MIX) {
        int idx = MIX_P[mixRow];
        curP().p[track][idx] = (uint8_t)clampi(curP().p[track][idx] + delta, 0, 127);
        return;
    }
    if (page == PG_PRJ) {
        if (slot == 0) prjSel = clampi(prjSel + delta, 0, 31);
        else if (slot == 4) presetSel = clampi(presetSel + delta, 0, 31);
        return;
    }
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
        else if (slot == MS_SCALE) {
            uint8_t* m = eng.proj.mst;
            m[MS_SCALE] = (uint8_t)scaleCombine(clampi(scaleType(m[MS_SCALE]) + (delta > 0 ? 1 : -1), 0, NSCALES), m[MS_ROOT]);
        } else if (slot == MS_ROOT) {
            uint8_t* m = eng.proj.mst;
            m[MS_ROOT] = (uint8_t)((m[MS_ROOT] + 12 + (delta > 0 ? 1 : -1)) % 12);
            m[MS_SCALE] = (uint8_t)scaleCombine(scaleType(m[MS_SCALE]), m[MS_ROOT]);
        }
        else if (MST_DEFS[slot].kind != K_NONE) eng.proj.mst[slot] = (uint8_t)clampi(eng.proj.mst[slot] + delta, 0, MST_DEFS[slot].max);
        return;
    }
    Pattern& P = curP();
    int idx = sp() * 8 + slot;
    ParamDef d = paramDef(P.machine[track], idx);
    if (d.kind == K_NONE) return;
    if (d.kind == K_SMPL) d.max = (uint8_t)(sampleCount() > 0 ? sampleCount() - 1 : 0);
    if (page == PAGE_INS && slot >= 1 && slot <= 4) {
        int ty = P.p[track][P_ITYP];
        if (ty <= 0 || ty >= INS_COUNT || (slot <= 3 && INS_PNAMES[ty][slot - 1][0] == '-')) return;
    }
    if (d.kind == K_TLEN) { P.len[track] = (uint8_t)clampi(P.len[track] + delta, 1, NSTEPS); return; }
    if (d.kind == K_TSPD) { P.speed[track] = (uint8_t)clampi(P.speed[track] + (delta > 0 ? 1 : -1), 0, 6); return; }
    if (d.kind == K_LDEST) {   // Ziel: naechster gueltiger Parameter
        int cur = aHeld && holdS().lock[idx] != NO_LOCK ? holdS().lock[idx] : P.p[track][idx];
        int dir = delta > 0 ? 1 : -1;
        for (int i = 0; i < abs(delta); i++) {
            int nv = cur;
            do { nv += dir; } while (nv > 0 && nv <= NPARAMS && (paramDef(P.machine[track], nv - 1).kind == K_NONE ||
                                                               paramDef(P.machine[track], nv - 1).kind == K_MACH ||
                                                               paramDef(P.machine[track], nv - 1).kind == K_TLEN ||
                                                               paramDef(P.machine[track], nv - 1).kind == K_TSPD));
            if (nv < 0 || nv > NPARAMS) break;
            cur = nv;
        }
        if (aHeld) { holdS().lock[idx] = (uint8_t)cur; aEdited = true; } else P.p[track][idx] = (uint8_t)cur;
        return;
    }
    if (d.kind == K_MACH) {   // Engine der Spur (nicht lockbar)
        if (aHeld) return;
        int oi = 0;
        for (int i = 0; i < M_COUNT; i++) if (MACHINE_ORDER[i] == P.machine[track]) oi = i;
        int m = MACHINE_ORDER[clampi(oi + (delta > 0 ? 1 : -1), 0, M_COUNT - 1)];
        if (m != P.machine[track]) { lockA(); Engine::setMachine(P, track, m); unlockA(); lockA(); eng.preview(track, nullptr); unlockA(); }
        return;
    }
    if (aHeld) {
        Step& s = holdS();
        int cur = s.lock[idx] != NO_LOCK ? s.lock[idx] : eng.proj.pat[holdPat].p[holdTrack][idx];
        if (idx == P_NOTE) {   // in Skalenschritten
            int sv = curScale();
            if (sv > 0) { int n = quantizeNote(cur, sv); for (int i = 0; i < abs(delta); i++) n = stepScale(n, delta > 0 ? 1 : -1, sv); s.lock[idx] = (uint8_t)n; }
            else s.lock[idx] = (uint8_t)clampi(cur + delta, 0, d.max);
        } else
        s.lock[idx] = (uint8_t)clampi(cur + delta, 0, d.max);
        aEdited = true;
    } else {
        int sv = curScale();
        if (idx == P_NOTE && sv > 0) {
            int n = quantizeNote(P.p[track][idx], sv);
            for (int i = 0; i < abs(delta); i++) n = stepScale(n, delta > 0 ? 1 : -1, sv);
            P.p[track][idx] = (uint8_t)n;
        } else
        P.p[track][idx] = (uint8_t)clampi(P.p[track][idx] + delta, 0, d.max);
    }
    if (idx == P_NOTE || d.kind == K_SMPL) { lockA(); eng.preview(track, aHeld ? &holdS() : nullptr); unlockA(); }
}

static int curMax() {
    if (page == PAGE_MST) return slot == MS_BPM ? 270 : MST_DEFS[slot].max;
    if (page == PAGE_SYS) return 1;
    if (page == PAGE_CMP) return CMP_DEFS[slot].max;
    if (page == PAGE_FX) return FX_DEFS[slot].max;
    if (!trackPage(page)) return 127;
    return paramDef(curP().machine[track], sp() * 8 + slot).max;
}

// ---------------------------------------------------------------- Projekte, Presets, Abfragen

enum { ACT_LOAD = 1, ACT_SAVE, ACT_NEW, ACT_PSAVE, ACT_INIT, ACT_PASTE, ACT_CLEARPAT, ACT_QUIT };

static void songPath(int i, char* out, int n) { snprintf(out, n, "%sprojects/song%02d.gbx", basePath, i + 1); }
static bool fileExists(const char* p) { struct stat st; return stat(p, &st) == 0; }
static bool songExists(int i) { char p[700]; songPath(i, p, sizeof p); return fileExists(p); }
static void presetPath(int m, int i, char* out, int n) { snprintf(out, n, "%spresets/%s/%02d.gbp", basePath, MACHINE_NAMES[m], i + 1); }
static bool presetExists(int m, int i) { char p[700]; presetPath(m, i, p, sizeof p); return fileExists(p); }

static void askConfirm(int act, int arg, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); vsnprintf(confirmText, sizeof confirmText, fmt, ap); va_end(ap);
    confirmAct = act; confirmArg = arg; confirmOpen = true;
}

static bool savePreset(int t, int i) {
    int m = curP().machine[t];
    char d[700];
    snprintf(d, sizeof d, "%spresets", basePath); mkdir(d, 0755);
    snprintf(d, sizeof d, "%spresets/%s", basePath, MACHINE_NAMES[m]); mkdir(d, 0755);
    char p[700]; presetPath(m, i, p, sizeof p);
    FILE* f = fopen(p, "wb");
    if (!f) return false;
    uint8_t hdr[8] = {'G', 'B', 'P', '1', (uint8_t)m, (uint8_t)NPARAMS, 0, 0};
    bool ok = fwrite(hdr, 8, 1, f) == 1 && fwrite(curP().p[t], NPARAMS, 1, f) == 1;
    fclose(f);
    return ok;
}
static int loadPreset(int t, int m, int i) {   // 1 ok, 0 leer, -1 Fehler
    char p[700]; presetPath(m, i, p, sizeof p);
    FILE* f = fopen(p, "rb");
    if (!f) return 0;
    uint8_t hdr[8], prm[NPARAMS];
    bool ok = fread(hdr, 8, 1, f) == 1 && !memcmp(hdr, "GBP1", 4) && hdr[4] < M_COUNT;
    int np = ok ? (hdr[5] > NPARAMS ? NPARAMS : hdr[5]) : 0;
    ok = ok && np > 0 && fread(prm, np, 1, f) == 1;
    fclose(f);
    if (!ok) return -1;
    lockA();
    Pattern& P = curP();
    Engine::initTrack(P, t, hdr[4]);
    memcpy(P.p[t], prm, np);
    unlockA();
    return 1;
}

static void doLoadSong(int i) {
    char p[700]; songPath(i, p, sizeof p);
    if (!fileExists(p)) { toast("SONG %02d IST LEER", i + 1); return; }
    lockA(); eng.stop(); bool ok = eng.load(p); unlockA();
    if (!ok) { toast("LADEN FEHLGESCHLAGEN"); return; }
    snprintf(projPath, sizeof projPath, "%s", p);
    memcpy(&lastSaved, &eng.proj, sizeof(Project));
    cfg.song = i; saveSettings();
    toast("SONG %02d GELADEN", i + 1);
}
static void doSaveSong(int i) {
    songPath(i, projPath, sizeof projPath);
    cfg.song = i; saveSettings();
    toast(saveProject() ? "IN SONG %02d GESPEICHERT" : "SPEICHERN FEHLGESCHLAGEN", i + 1);
}
static void doNewSong() {
    int j = -1;
    for (int i = 0; i < 32 && j < 0; i++) if (!songExists(i)) j = i;
    if (j < 0) { toast("ALLE 32 PLAETZE BELEGT"); return; }
    lockA(); eng.stop(); eng.defaultProject(false); unlockA();
    prjSel = j;
    doSaveSong(j);
    toast("NEUES PROJEKT: SONG %02d", j + 1);
}
static void runAction(int act, int arg) {
    switch (act) {
    case ACT_LOAD: doLoadSong(arg); break;
    case ACT_SAVE: doSaveSong(arg); break;
    case ACT_NEW: doNewSong(); break;
    case ACT_PSAVE: toast(savePreset(track, arg) ? "PRESET %02d GESPEICHERT" : "PRESET-FEHLER", arg + 1); break;
    case ACT_PASTE: lockA(); curP() = clip; unlockA(); toast("PATTERN EINGEFUEGT"); break;
    case ACT_CLEARPAT:
        lockA();
        for (int t = 0; t < NTRACKS; t++) for (int st = 0; st < NSTEPS; st++) { curP().s[t][st].on = 0; memset(curP().s[t][st].lock, NO_LOCK, NPARAMS); }
        unlockA();
        toast("PATTERN %02d GELEERT", eng.curPat + 1);
        break;
    case ACT_QUIT: quitReq = true; break;
    case ACT_INIT: {
        int m = curP().machine[track];
        lockA(); Engine::initTrack(curP(), track, m); unlockA();
        toast("SPUR %d: KLANG ZURUECKGESETZT", track + 1);
    } break;
    }
}
static void prjTrigger(int k) {   // Aktions-Regler auf der PRJ-Seite ausloesen
    int m = curP().machine[track];
    switch (k) {
    case 1:
        if (!songExists(prjSel)) toast("SONG %02d IST LEER", prjSel + 1);
        else askConfirm(ACT_LOAD, prjSel, "SONG %02d LADEN? UNGESPEICHERTES GEHT VERLOREN", prjSel + 1);
        break;
    case 2:
        if (songExists(prjSel) && cfg.song != prjSel) askConfirm(ACT_SAVE, prjSel, "SONG %02d UEBERSCHREIBEN?", prjSel + 1);
        else doSaveSong(prjSel);
        break;
    case 3: askConfirm(ACT_NEW, 0, "NEUES LEERES PROJEKT ANLEGEN?"); break;
    case 5: {
        int r = loadPreset(track, m, presetSel);
        if (r > 0) toast("%s %02d GELADEN", MACHINE_NAMES[m], presetSel + 1);
        else toast(r == 0 ? "PRESET %02d IST LEER" : "PRESET %02d DEFEKT", presetSel + 1);
    } break;
    case 6:
        if (presetExists(m, presetSel)) askConfirm(ACT_PSAVE, presetSel, "%s-PRESET %02d UEBERSCHREIBEN?", MACHINE_NAMES[m], presetSel + 1);
        else runAction(ACT_PSAVE, presetSel);
        break;
    case 7: askConfirm(ACT_INIT, 0, "KLANG VON SPUR %d ZURUECKSETZEN?", track + 1); break;
    }
}
static bool prjIsAction(int k) { return k == 1 || k == 2 || k == 3 || k >= 5; }

// Stick-"Antippen": ein Schritt pro Auslenkung, beim Halten wiederholt
static int flLX = 0, flLY = 0, flRX = 0; static Uint32 flLXt = 0, flLYt = 0, flRXt = 0;
static int flick(float v, int& state, Uint32& t, Uint32 now) {
    if (fabsf(v) < 0.35f) { state = 0; return 0; }
    if (fabsf(v) < 0.65f) return 0;
    int d = v > 0 ? 1 : -1;
    if (state != d) { state = d; t = now + 420; return d; }
    if (now >= t) { t = now + 220; return d; }
    return 0;
}

// ---------------------------------------------------------------- Einstellungen (Liste, Seite SET)

enum SetItem {
    SI_H_PAT, SI_PATTERN, SI_LEN, SI_LENALL, SI_SPEED, SI_FILL, SI_BPM, SI_SWING, SI_SCALE, SI_ROOT, SI_COPY, SI_PASTE, SI_CLEAR,
    SI_H_AUDIO, SI_OUT, SI_BUF, SI_VOL,
    SI_H_MIDI, SI_MIN, SI_NOUT, SI_CLK, SI_MCH,
    SI_H_USB, SI_USB, SI_IP, SI_WEB,
    SI_H_SYS, SI_LEARN, SI_INFO, SI_QUIT, SI_COUNT
};
static int setSel = SI_PATTERN, prevPage = PG_TRG;
static float setScroll = 0;
static bool setIsHeader(int i) { return i == SI_H_PAT || i == SI_H_AUDIO || i == SI_H_MIDI || i == SI_H_USB || i == SI_H_SYS; }
static bool setIsAction(int i) { return i == SI_COPY || i == SI_PASTE || i == SI_CLEAR || i == SI_LEARN || i == SI_QUIT; }
static bool runLearn();
static char versionStr[32] = "";

static void setLabel(int i, char* l, int ln, char* v, int vn) {
    const Pattern& P = curP();
    v[0] = 0;
    switch (i) {
    case SI_H_PAT: snprintf(l, ln, "PATTERN"); break;
    case SI_PATTERN: snprintf(l, ln, "Pattern"); snprintf(v, vn, "P%02d%s", eng.curPat + 1, eng.nextPat >= 0 ? " >" : ""); break;
    case SI_LEN: snprintf(l, ln, "Laenge Spur %d", track + 1); snprintf(v, vn, "%d Steps", P.len[track]); break;
    case SI_LENALL: snprintf(l, ln, "Laenge alle Spuren"); snprintf(v, vn, "%d Steps", P.len[track]); break;
    case SI_SPEED: snprintf(l, ln, "Tempo-Faktor Spur %d", track + 1); snprintf(v, vn, "%s", SPEED_NAMES[P.speed[track] % 7]); break;
    case SI_FILL: snprintf(l, ln, "FILL-Modus (SELECT+L1)"); snprintf(v, vn, "%s", eng.fill ? "AN" : "AUS"); break;
    case SI_BPM: snprintf(l, ln, "Tempo"); snprintf(v, vn, "%d BPM", eng.proj.bpm); break;
    case SI_SWING: snprintf(l, ln, "Swing"); snprintf(v, vn, "%d%%", 50 + eng.proj.mst[MS_SWNG]); break;
    case SI_SCALE: snprintf(l, ln, "Tonskala");
        if (curScale()) snprintf(v, vn, "%s", SCALE_LONG[scaleType(curScale()) - 1]);
        else snprintf(v, vn, "AUS (chromatisch)");
        break;
    case SI_ROOT: snprintf(l, ln, "Grundton"); snprintf(v, vn, "%s", NOTE_NAMES[eng.proj.mst[MS_ROOT] % 12]); break;
    case SI_COPY: snprintf(l, ln, "Pattern kopieren"); break;
    case SI_PASTE: snprintf(l, ln, "Pattern einfuegen"); snprintf(v, vn, "%s", haveClip ? "" : "(leer)"); break;
    case SI_CLEAR: snprintf(l, ln, "Pattern leeren"); break;
    case SI_H_AUDIO: snprintf(l, ln, "AUDIO"); break;
    case SI_OUT: snprintf(l, ln, "Ausgang"); { int d = devIndex(); snprintf(v, vn, "%.26s", d ? devNames[d - 1] : "Standard (intern)"); } break;
    case SI_BUF: snprintf(l, ln, "Puffer");
        if (bufOverride) snprintf(v, vn, "%d (Startoption)", bufOverride);
        else snprintf(v, vn, "%d  (%.1f ms)", BUF_SIZES[cfg.buf], BUF_SIZES[cfg.buf] * 1000.f / eng.sr);
        break;
    case SI_VOL: snprintf(l, ln, "Lautstaerke"); snprintf(v, vn, "%d", eng.proj.mst[MS_VOL]); break;
    case SI_H_MIDI: snprintf(l, ln, "MIDI"); break;
    case SI_MIN: snprintf(l, ln, "MIDI-Eingang"); snprintf(v, vn, "%s", cfg.midiIn ? "AN" : "AUS"); break;
    case SI_NOUT: snprintf(l, ln, "Noten ausgeben"); snprintf(v, vn, "%s", cfg.notesOut ? "AN (Spur = Kanal)" : "AUS"); break;
    case SI_CLK: snprintf(l, ln, "Clock"); snprintf(v, vn, "%s", cfg.clk == 1 ? "SENDEN" : (cfg.clk == 2 ? "EMPFANGEN" : "AUS")); break;
    case SI_MCH: snprintf(l, ln, "Eingangskanaele"); snprintf(v, vn, "%s", cfg.chm ? "Kanal 1-8 = Spur 1-8" : "AUTO (gewaehlte Spur)"); break;
    case SI_H_USB: snprintf(l, ln, "USB"); break;
    case SI_USB: snprintf(l, ln, "USB-Modus");
        snprintf(v, vn, "%s", usbMode == 0 ? "HOST (Geraete am R36S)" : (usbMode == 1 ? "PC (Netz-MIDI, SSH)" : "PC + AUDIO"));
        break;
    case SI_IP: snprintf(l, ln, "Adresse"); snprintf(v, vn, "%s", usbMode && netIp[0] ? netIp : "-"); break;
    case SI_WEB: snprintf(l, ln, "Samples per Browser");
        if (!webPort()) snprintf(v, vn, "-");
        else if (usbMode && netIp[0]) snprintf(v, vn, webPort() == 80 ? "http://%s" : "http://%s:%d", netIp, webPort());
        else snprintf(v, vn, "erst PC-Modus an");
        break;
    case SI_H_SYS: snprintf(l, ln, "SYSTEM"); break;
    case SI_LEARN: snprintf(l, ln, "Tasten neu lernen"); break;
    case SI_INFO: snprintf(l, ln, "Info");
        snprintf(v, vn, "%s  %d Samples  %s", versionStr[0] ? versionStr : "dev", sampleCount(), midiBackend());
        break;
    case SI_QUIT: snprintf(l, ln, "Groovebox beenden"); break;
    }
}

static void setChange(int i, int d) {
    Pattern& P = curP();
    switch (i) {
    case SI_PATTERN: {
        int to = (eng.curPat + d + NPATTERNS) % NPATTERNS;
        lockA(); if (eng.playing) eng.nextPat = to; else { eng.curPat = to; eng.nextPat = -1; } unlockA();
    } break;
    case SI_LEN: P.len[track] = (uint8_t)clampi(P.len[track] + d, 1, NSTEPS); break;
    case SI_LENALL: { int l = clampi(P.len[track] + d, 1, NSTEPS); for (int t = 0; t < NTRACKS; t++) P.len[t] = (uint8_t)l; } break;
    case SI_SPEED: P.speed[track] = (uint8_t)clampi(P.speed[track] + (d > 0 ? 1 : -1), 0, 6); break;
    case SI_FILL: eng.fill = !eng.fill; break;
    case SI_BPM: eng.proj.bpm = (uint16_t)clampi(eng.proj.bpm + d, 30, 300); break;
    case SI_SWING: eng.proj.mst[MS_SWNG] = (uint8_t)clampi(eng.proj.mst[MS_SWNG] + d, 0, 30); break;
    case SI_SCALE: eng.proj.mst[MS_SCALE] = (uint8_t)scaleCombine(clampi(scaleType(eng.proj.mst[MS_SCALE]) + (d > 0 ? 1 : -1), 0, NSCALES), eng.proj.mst[MS_ROOT]); break;
    case SI_ROOT: {
        uint8_t* m = eng.proj.mst;
        m[MS_ROOT] = (uint8_t)((m[MS_ROOT] + 12 + (d > 0 ? 1 : -1)) % 12);
        m[MS_SCALE] = (uint8_t)scaleCombine(scaleType(m[MS_SCALE]), m[MS_ROOT]);
    } break;
    case SI_VOL: eng.proj.mst[MS_VOL] = (uint8_t)clampi(eng.proj.mst[MS_VOL] + d, 0, 127); break;
    case SI_OUT: {
        refreshDevices();
        int k = clampi(devIndex() + (d > 0 ? 1 : -1), 0, devCount);
        snprintf(cfg.dev, sizeof cfg.dev, "%s", k ? devNames[k - 1] : "");
        audioApplyAt = SDL_GetTicks() + 800; saveSettings();
    } break;
    case SI_BUF: cfg.buf = clampi(cfg.buf + (d > 0 ? 1 : -1), 0, 3); bufOverride = 0; audioApplyAt = SDL_GetTicks() + 800; saveSettings(); break;
    case SI_MIN: cfg.midiIn = !cfg.midiIn; applyMidiSettings(); saveSettings(); break;
    case SI_NOUT: cfg.notesOut = !cfg.notesOut; applyMidiSettings(); saveSettings(); break;
    case SI_CLK: cfg.clk = (cfg.clk + (d > 0 ? 1 : 2)) % 3; applyMidiSettings(); saveSettings(); break;
    case SI_MCH: cfg.chm = !cfg.chm; applyMidiSettings(); saveSettings(); break;
    case SI_USB: usbMode = clampi(usbMode + (d > 0 ? 1 : -1), 0, 2); usbApplyAt = SDL_GetTicks() + 1200; break;
    default: break;
    }
}

static void setAction(int i) {
    switch (i) {
    case SI_COPY: clip = curP(); haveClip = true; toast("PATTERN %02d KOPIERT", eng.curPat + 1); break;
    case SI_PASTE:
        if (haveClip) askConfirm(ACT_PASTE, 0, "PATTERN %02d MIT KOPIE UEBERSCHREIBEN?", eng.curPat + 1);
        else toast("ZWISCHENABLAGE LEER");
        break;
    case SI_CLEAR: askConfirm(ACT_CLEARPAT, 0, "ALLE STEPS VON PATTERN %02d LOESCHEN?", eng.curPat + 1); break;
    case SI_LEARN: runLearn(); break;
    case SI_QUIT: askConfirm(ACT_QUIT, 0, "GROOVEBOX BEENDEN? (PROJEKT WIRD GESPEICHERT)"); break;
    default: setChange(i, 1); break;   // Umschalter: A schaltet weiter
    }
}

static void settingsInput(Input& I, Uint32 now, float dt) {
    int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0) - flick(I.ly, flLY, flLYt, now);
    if (dy) {
        int s2 = setSel;
        do { s2 = (s2 + dy + SI_COUNT) % SI_COUNT; } while (setIsHeader(s2));
        setSel = s2;
    }
    int dx = (I.repeat(BT_RIGHT, now) ? 1 : 0) - (I.repeat(BT_LEFT, now) ? 1 : 0) + (I.repeat(BT_X, now) ? 1 : 0) - (I.repeat(BT_Y, now) ? 1 : 0);
    if (fabsf(I.rx) > fabsf(I.ry)) dx += flick(I.rx, flRX, flRXt, now); else flRX = 0;
    if (dx && !setIsAction(setSel)) setChange(setSel, dx);
    float ry = fabsf(I.ry) > fabsf(I.rx) ? I.ry : 0.f;   // rechter Stick: stufenlos (Tempo etc.)
    if (fabsf(ry) > 0.25f && !setIsAction(setSel) && (setSel == SI_BPM || setSel == SI_VOL || setSel == SI_LEN || setSel == SI_LENALL)) {
        stickAcc += (ry > 0 ? 1.f : -1.f) * (30.f * (fabsf(ry) - 0.25f) + 2.f) * dt;
        int d = (int)stickAcc;
        if (d) { stickAcc -= d; setChange(setSel, d); }
    } else if (fabsf(ry) <= 0.25f) stickAcc = 0;
    if (I.pressed(BT_A)) setAction(setSel);
    if (I.pressed(BT_B)) page = prevPage == PG_SYS ? PG_TRG : prevPage;
    if (I.pressed(BT_START)) { lockA(); if (eng.playing) eng.stop(); else eng.start(); unlockA(); }
}

static void drawSettings() {
    rrect(4, 46, 632, 398, 10, C_PANEL);
    const int rowH = 25, top = 54, visible = 15;
    // Zeile der Auswahl in Sichtweite halten (weich scrollen)
    float target = setSel - visible / 2.f;
    if (target < 0) target = 0;
    if (target > SI_COUNT - visible) target = SI_COUNT - visible;
    setScroll += (target - setScroll) * (1.f - expf(-animDt * 14.f));
    char l[48], v[64];
    for (int i = 0; i < SI_COUNT; i++) {
        float yf = top + (i - setScroll) * rowH;
        if (yf < top - 4 || yf > top + (visible - 1) * rowH + 4) continue;
        int y = (int)yf;
        setLabel(i, l, sizeof l, v, sizeof v);
        if (setIsHeader(i)) {
            text(18, y + 4, l, C_ACC);
            fill(18 + textW(l) + 10, y + 12, 600 - textW(l) - 10, 1, C_OFF2);
            continue;
        }
        bool sel = i == setSel;
        if (sel) { rrect(12, y, 616, rowH - 2, 6, C_SEL); rrect(12, y + 4, 3, rowH - 10, 1, C_ACC); }
        text(32, y + 4, l, sel ? C_TEXT : C_DIM);
        if (setIsAction(i)) {
            int w = textW(v[0] ? v : ">>") + 16;
            if (v[0]) text(612 - textW(v), y + 4, v, C_DIM);
            else { rrect(612 - w + 8, y + 3, w - 8, rowH - 8, 5, sel ? C_ACC : C_OFF2); text(612 - w + 12, y + 4, ">>", sel ? C_BG : C_TEXT); }
        } else {
            text(604 - textW(v), y + 4, v, sel ? C_ACC : C_TEXT);
            if (sel) { text(604 - textW(v) - 16, y + 4, "<", C_DIM); text(608, y + 4, ">", C_DIM); }
        }
    }
    hint(6, 452, "[^/v]WAEHLEN [</>]AENDERN [A]AUSFUEHREN [B]ZURUECK [SEL]MENUE", C_ACC, C_DIM);
}

// Linker Stick zeigt direkt auf einen der 8 Regler (oben 1-4, unten 5-8)
static void stickPointSlot(float lx, float ly) {
    float mag = sqrtf(lx * lx + ly * ly);
    if (mag < 0.6f) return;
    static const float ang[8] = {150, 110, 70, 30, 210, 250, 290, 330};
    float a = atan2f(ly, lx) * 57.29578f; if (a < 0) a += 360.f;
    int best = slot; float bd = 999;
    for (int k = 0; k < 8; k++) {
        if (fabsf(ly) < 0.3f && k / 4 != slot / 4) continue;   // waagerecht: in der Reihe bleiben
        float d = fabsf(a - ang[k]); if (d > 180) d = 360 - d;
        if (d < bd) { bd = d; best = k; }
    }
    slot = best;
}

static void menuInput(Input& I, Uint32 now) {
    // Seitenmenue: 3 Zeilen (Gruppen), Steuerkreuz/Stick waehlt, A oeffnet
    int g = PG_GROUP[menuSel];
    int dx = (I.repeat(BT_RIGHT, now) ? 1 : 0) - (I.repeat(BT_LEFT, now) ? 1 : 0) + flick(I.lx, flLX, flLXt, now);
    int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0) - flick(I.ly, flLY, flLYt, now);
    if (dx) {
        int p = menuSel;
        for (int i = 0; i < NUI_PAGES; i++) { p = (p + dx + NUI_PAGES) % NUI_PAGES; if (PG_GROUP[p] == g && pageAvail(p)) break; }
        menuSel = p;
    }
    if (dy) {
        int ng = (g + dy + 3) % 3, col = 0;
        for (int p = 0; p < menuSel; p++) if (PG_GROUP[p] == g && pageAvail(p)) col++;
        int best = -1, c = 0;
        for (int p = 0; p < NUI_PAGES; p++) if (PG_GROUP[p] == ng && pageAvail(p)) { if (c <= col) best = p; c++; }
        if (best >= 0) menuSel = best;
    }
    if (I.pressed(BT_A) || I.pressed(BT_START)) { page = menuSel; menuOpen = false; }
    if (I.pressed(BT_B) || I.pressed(BT_SELECT)) menuOpen = false;
}

static void handleInput(Uint32 now, float dt) {
    Input& I = in;
    if (confirmOpen) {
        if (I.pressed(BT_A)) { confirmOpen = false; runAction(confirmAct, confirmArg); }
        else if (I.pressed(BT_B) || I.pressed(BT_SELECT)) { confirmOpen = false; toast("ABGEBROCHEN"); }
        return;
    }
    if (menuOpen) { menuInput(I, now); return; }
    {   // rechter Stick: Achse beim Auslenken festlegen, erst in der Mitte wieder frei
        float m = sqrtf(I.rx * I.rx + I.ry * I.ry);
        if (m < 0.22f) rsAxis = 0;
        else if (rsAxis == 0 && m > 0.3f) rsAxis = fabsf(I.ry) >= fabsf(I.rx) ? 1 : 2;
    }
    bool func = I.down[BT_SELECT];
    if (I.pressed(BT_SELECT)) { funcUsed = false; selDownAt = now; }
    if (I.released(BT_SELECT) && !funcUsed && now - selDownAt < 400) { menuOpen = true; menuSel = page; return; }
    static int lastPgSeen = -1;
    if (page != lastPgSeen) { if (page == PG_SYS && lastPgSeen >= 0) prevPage = lastPgSeen; lastPgSeen = page; }
    if (page == PG_SYS && !func) { settingsInput(I, now, dt); return; }

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
            toast("ENGINE: %s", MACHINE_NAMES[m]);
        }
        if (I.pressed(BT_L1)) { funcUsed = true; eng.fill = !eng.fill; toast(eng.fill ? "FILL AN" : "FILL AUS"); }
        if (I.pressed(BT_R1)) {
            funcUsed = true;
            if (haveClip) { lockA(); curP() = clip; unlockA(); toast("PATTERN EINGEFUEGT"); }
            else toast("ZWISCHENABLAGE LEER");
        }
        if (I.repeat(BT_L2, now) || I.repeat(BT_R2, now)) {
            funcUsed = true;
            Pattern& P = curP();
            P.len[track] = (uint8_t)clampi(P.len[track] + (I.down[BT_R2] ? 1 : -1), 1, NSTEPS);
            toast("SPURLAENGE %d", P.len[track]);
        }
        if (I.pressed(BT_START)) {
            funcUsed = true;
            bool ok = saveProject();
            toast(ok ? "GESPEICHERT" : "SPEICHERN FEHLGESCHLAGEN");
        }
        if (I.down[BT_START] && I.heldMs(BT_START, now) > 2000) quitReq = true;
        // SELECT + rechter Stick hoch/runter = DJ-Filter
        // SELECT + rechter Stick links/rechts = Seite wechseln
        if (rsAxis == 2) { int f = flick(I.rx, flRX, flRXt, now); if (f) { funcUsed = true; gotoPage(f); } }
        else flRX = 0;
        if (rsAxis == 1 && fabsf(I.ry) > 0.2f) { funcUsed = true; eng.dj = I.ry > 0 ? (I.ry - 0.2f) / 0.8f : (I.ry + 0.2f) / 0.8f; stickDj = true; }
        else if (stickDj) { eng.dj = 0.f; stickDj = false; }
    } else {
        if (stickDj) { eng.dj = 0.f; stickDj = false; }
        if (lockLatch) {   // Lock-Modus: links/rechts springt zum naechsten Step und bleibt im Modus
            int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            int d = (I.repeat(BT_RIGHT, now, 250, 90) ? 1 : 0) - (I.repeat(BT_LEFT, now, 250, 90) ? 1 : 0);
            if (d) {
                Step& o = holdS();
                if (aJustOn && !aEdited) o.on = 0;
                cursor = ((cursor + d) % cl + cl) % cl;
                holdStep = cursor;
                Step& s = holdS();
                aJustOn = !s.on; aEdited = false;
                if (!s.on) { memset(s.lock, NO_LOCK, NPARAMS); s.on = 1; }
                toast("LOCK STEP %d", cursor + 1);
            }
        }
        if (!aHeld) {
            int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            if (I.repeat(BT_LEFT, now, 250, 70)) cursor = (cursor % cl + cl - 1) % cl;
            if (I.repeat(BT_RIGHT, now, 250, 70)) cursor = (cursor % cl + 1) % cl;
            if (I.repeat(BT_UP, now)) track = (track + NTRACKS - 1) % NTRACKS;
            if (I.repeat(BT_DOWN, now)) track = (track + 1) % NTRACKS;
        }
        bool mixP = page == PG_MIX, prjAct = page == PG_PRJ && prjIsAction(slot);
        if (I.pressed(BT_A) && lockLatch) {   // Lock-Modus beenden
            Step& s = holdS();
            if (aJustOn && !aEdited) { s.on = 0; memset(s.lock, NO_LOCK, NPARAMS); }
            lockLatch = false; aHeld = false;
            toast("LOCK-MODUS AUS");
        } else if (I.pressed(BT_A)) {
            if (mixP) { eng.mute[track] = !eng.mute[track]; toast(eng.mute[track] ? "SPUR %d STUMM" : "SPUR %d AN", track + 1); }
            else if (prjAct) prjTrigger(slot);
            else {
                holdPat = eng.curPat; holdTrack = track; holdStep = cursor;
                Step& s = holdS();
                aJustOn = !s.on;
                if (!s.on) { memset(s.lock, NO_LOCK, NPARAMS); s.on = 1; }
                aHeld = true; aEdited = false; aDownAt = now;
            }
        }
        // A lange halten (ohne zu drehen) = Lock-Modus einrasten: Daumen frei fuer Stick und X/Y
        if (aHeld && !lockLatch && I.down[BT_A] && !aEdited && now - aDownAt > 450) {
            lockLatch = true;
            toast("LOCK-MODUS STEP %d  (A = FERTIG)", holdStep + 1);
        }
        if (I.pressed(BT_B)) {
            if (aHeld) { memset(holdS().lock, NO_LOCK, NPARAMS); aEdited = true; toast("LOCKS GELOESCHT"); }
            else if (mixP) { eng.solo[track] = !eng.solo[track]; toast(eng.solo[track] ? "SPUR %d SOLO" : "SPUR %d SOLO AUS", track + 1); }
            else { lockA(); eng.preview(track, &curP().s[track][cursor]); unlockA(); }
        }
        // Linker Stick: auf Regler zeigen (Mixer: hoch/runter = Zeile, links/rechts = Kanal)
        if (mixP) {
            int f = flick(I.ly, flLY, flLYt, now); if (f) mixRow = clampi(mixRow - f, 0, 3);
            int g = flick(I.lx, flLX, flLXt, now); if (g) track = (track + NTRACKS + g) % NTRACKS;
        } else stickPointSlot(I.lx, I.ly);
        // Rechter Stick: links/rechts = Seite, hoch/runter = Wert
        flRX = 0;   // Seite wechseln nur mit SELECT + rechtem Stick, L2/R2 oder dem Menue
        {   // L1/R1: Mixer-Zeile, auf Spurseiten Step-Seite (16 Steps), sonst Regler
            int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            int dl = (I.repeat(BT_R1, now) ? 1 : 0) - (I.repeat(BT_L1, now) ? 1 : 0);
            if (dl) {
                if (mixP) mixRow = (mixRow + 4 + dl) % 4;
                else if (trackPage(page) && cl > 16) cursor = ((cursor + dl * 16) % cl + cl) % cl;
                else slot = (slot + 8 + dl) % 8;
            }
        }
        if (I.pressed(BT_L2)) gotoPage(-1);
        if (I.pressed(BT_R2)) gotoPage(1);
        if (I.pressed(BT_START)) {
            lockA();
            if (eng.playing) eng.stop(); else eng.start();
            unlockA();
            if (!eng.playing && eng.clockMode == 2) toast("WARTE AUF MIDI-START");
        }
        if (prjAct) {
            if (I.pressed(BT_X)) prjTrigger(slot);
        } else {
            int big = curMax() >= 100;
            int mul = (big && (I.heldMs(BT_X, now) > 1200 || I.heldMs(BT_Y, now) > 1200)) ? 5 : 1;
            if (I.repeat(BT_X, now, 300, 45)) editValue(mul);
            if (I.repeat(BT_Y, now, 300, 45)) editValue(-mul);
            float ry = rsAxis == 1 ? I.ry : 0.f;
            if (fabsf(ry) > 0.25f) {
                float dz = (fabsf(ry) - 0.25f) / 0.75f;
                float rate = (big ? 90.f : 8.f) * dz * dz + 2.f;
                stickAcc += (ry > 0 ? 1.f : -1.f) * rate * dt;
                int d = (int)stickAcc;
                if (d) { stickAcc -= d; editValue(d); }
            } else stickAcc = 0;
        }
    }
    if (I.released(BT_A) && aHeld && !lockLatch) {
        Step& s = holdS();
        if (!aJustOn && !aEdited) { s.on = 0; memset(s.lock, NO_LOCK, NPARAMS); }
        aHeld = false;
    }
    if (aHeld && !lockLatch && !I.down[BT_A]) aHeld = false;
    if (!pageAvail(page)) page = PG_SRC;
    { int cl = curP().len[track] < 1 ? 1 : curP().len[track]; if (cursor >= cl) cursor = cl - 1; }
    // Zustand fuer MIDI-Eingang / Step-Aufnahme
    eng.uiTrack = track; eng.uiPage = trackPage(page) ? sp() : 99; eng.uiCursor = cursor;
    if (eng.stepRecCount != lastStepRec) {
        lastStepRec = eng.stepRecCount;
        int len = curP().len[track] < 1 ? 1 : curP().len[track];
        cursor = (cursor + 1) % len;
    }
    static int lastPg = -1;
    if (page != lastPg) { lastPg = page; if (page == PAGE_SYS) refreshDevices(); }
}

// ---------------------------------------------------------------- Lernmodus

static bool runLearn() {
    SDL_Joystick* j = in.joy;
    if (!j) { printf("Kein Gamepad gefunden - Lernmodus uebersprungen\n"); return false; }
    static const char* prompts[BT_COUNT + 4] = {
        "STEUERKREUZ HOCH", "STEUERKREUZ RUNTER", "STEUERKREUZ LINKS", "STEUERKREUZ RECHTS",
        "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "SELECT", "START",
        "LINKEN STICK NACH OBEN", "RECHTEN STICK NACH OBEN",
        "LINKEN STICK NACH RECHTS", "RECHTEN STICK NACH RECHTS"};
    int nb = SDL_JoystickNumButtons(j), nh = SDL_JoystickNumHats(j), na = SDL_JoystickNumAxes(j);
    Binding res[BT_COUNT];
    int axL = -1, axR = -1, sL = -1, sR = -1, axLx = -1, axRx = -1, sLx = 1, sRx = 1;
    for (int item = 0; item < BT_COUNT + 4; item++) {
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
                    if (stick) {
                        int si = item - BT_COUNT;
                        if (si == 0) { axL = i; sL = sg; } else if (si == 1) { axR = i; sR = sg; }
                        else if (si == 2) { axLx = i; sLx = sg; } else { axRx = i; sRx = sg; }
                    }
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
            snprintf(b, sizeof b, "%d / %d", item + 1, BT_COUNT + 4);
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
    in.axLX = axLx; in.sgnLX = sLx; in.axRX = axRx; in.sgnRX = sRx; in.haveLX = in.haveRX = true;
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
    bool aud = stat("/sys/kernel/config/usb_gadget/groovebox/configs/c.1/uac2.usb0", &st) == 0 ||
               stat("/sys/kernel/config/usb_gadget/groovebox/configs/c.1/uac1.usb0", &st) == 0;
    return aud ? 2 : 1;
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
    if (usbMode && !now) {   // letzte Fehlerzeile aus dem Log anzeigen
        usbMode = 0;
        char lp[700], line[200], err[200] = "PC-MODUS NICHT MOEGLICH (PC-MODUS.LOG)";
        snprintf(lp, sizeof lp, "%spc-modus.log", basePath);
        FILE* lf = fopen(lp, "r");
        if (lf) {
            while (fgets(line, sizeof line, lf))
                if (!strncmp(line, "FEHLER:", 7)) { line[strcspn(line, "\r\n")] = 0; snprintf(err, sizeof err, "%.60s", line); }
            fclose(lf);
        }
        toast("%s", err);
        msgUntil = SDL_GetTicks() + 5000;
    }
    else if (usbMode) {
        char lp[700], line[200], okm[200] = "PC-MODUS AKTIV";
        snprintf(lp, sizeof lp, "%spc-modus.log", basePath);
        FILE* lf = fopen(lp, "r");
        if (lf) {
            while (fgets(line, sizeof line, lf))
                if (!strncmp(line, "OK:", 3)) { line[strcspn(line, "\r\n")] = 0; snprintf(okm, sizeof okm, "%.60s", line + 4); }
            fclose(lf);
        }
        toast("%s", okm);
        msgUntil = SDL_GetTicks() + 4000;
    } else toast("USB: HOST-MODUS");
    usbMode = now;
    if (cfg.usb != usbMode) { cfg.usb = usbMode; saveSettings(); }   // ueber Neustarts merken
    readNetIp();
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

int runDhcpd(const char* ifname);

// Samples nach Upload neu laden: Bank im Hintergrund fuellen, kurz unter Audio-Sperre tauschen,
// Sample-Nummern in allen Patterns ueber den Namen neu zuordnen
static void reloadSamples() {
    static char oldNames[MAX_SAMPLES][48];
    int on = sampleCount();
    for (int i = 0; i < on; i++) snprintf(oldNames[i], 48, "%s", sampleName(i));
    char sdir[600];
    snprintf(sdir, sizeof sdir, "%ssamples", basePath);
    int nn = samplesPrepare(sdir);
    int map[MAX_SAMPLES];
    for (int i = 0; i < on; i++) map[i] = -1;
    lockA();
    eng.dropSamples();
    samplesSwap();
    for (int i = 0; i < on; i++)
        for (int j = 0; j < nn; j++) if (!strcmp(oldNames[i], sampleName(j))) { map[i] = j; break; }
    auto fix = [&](uint8_t& v) { if (v < on) v = (uint8_t)(map[v] >= 0 ? map[v] : 0); };
    auto fixPat = [&](Pattern& P) {
        for (int t = 0; t < NTRACKS; t++) {
            if (!machineUsesSample(P.machine[t])) continue;
            fix(P.p[t][P_S0]);
            for (int s = 0; s < NSTEPS; s++) if (P.s[t][s].lock[P_S0] != NO_LOCK) fix(P.s[t][s].lock[P_S0]);
        }
    };
    for (int p = 0; p < NPATTERNS; p++) fixPat(eng.proj.pat[p]);
    if (haveClip) fixPat(clip);
    unlockA();
    samplesFreeBack();
    printf("Samples neu geladen: %d\n", nn);
}

int main(int argc, char** argv) {
    if (argc >= 3 && !strcmp(argv[1], "--dhcpd")) return runDhcpd(argv[2]);   // Hilfsdienst fuer den PC-Modus
    bool learn = false, windowed = false;
    const char* shot = nullptr; const char* renderOut = nullptr; float renderSecs = 8; int shotPage = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--learn")) learn = true;
        else if (!strcmp(argv[i], "--windowed")) windowed = true;
        else if (!strcmp(argv[i], "--buffer") && i + 1 < argc) bufOverride = clampi(atoi(argv[++i]), 64, 4096);
        else if (!strcmp(argv[i], "--project") && i + 1 < argc) { snprintf(projPath, sizeof projPath, "%s", argv[++i]); projFromArg = true; }
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
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        makeFont();
        eng.start();
        static float tmp[44100];
        for (int k = 0; k < 6; k++) eng.render(tmp, 22050 / 2);
        // 0..12 = Seiten (PG_*), 20 Hilfe, 21 P-Lock, 22 Werte-Overlay, 23 Engine-Leiste, 24 Skala, 25 Menue, 26 Abfrage, 27 FM SRC2
        page = shotPage < NUI_PAGES ? shotPage : PG_TRG; track = 4; cursor = 3; slot = 1;
        refreshDevices();
        if (page == PG_SRC) { track = 7; slot = 0; }
        if (page == PG_SRC || page == PG_SRC2 || shotPage == 27) { }
        if (shotPage == 27 || page == PG_SRC2) { page = PG_SRC2; track = 6; slot = 0; }
        if (page == PG_INS) { track = 5; curP().p[5][P_ITYP] = INS_CHORUS; editAt = 1; editSlot = 0; slot = 0; }
        if (page == PG_MIX) { track = 2; mixRow = 0; eng.solo[1] = true; eng.mute[3] = true; for (int k = 0; k < 3; k++) { eng.render(tmp, 22050 / 2); animate(); } }
        if (page == PG_PRJ) { slot = 2; }
        if (shotPage >= 30 && shotPage <= 33) {   // Engine-Grafiken: 30 KICK, 31 WTBL, 32 CHOP, 33 GRAN
            static const int em[4] = {M_KICK, M_WTBL, M_CHOP, M_GRAN};
            page = PG_SRC; track = shotPage == 30 ? 0 : 5; slot = 1;
            if (shotPage != 30) { Engine::setMachine(curP(), 5, em[shotPage - 30]); if (shotPage >= 32) curP().p[5][P_S0] = 11; curP().p[5][P_S1] = shotPage == 31 ? 70 : (shotPage == 32 ? 7 : 40); }
            for (int t2 = 0; t2 < 8; t2++) eng.mute[t2] = false;
            lockA(); eng.start(); unlockA();
            for (int k2 = 0; k2 < 5; k2++) { eng.render(tmp, 22050 / 2 + 300); animate(); }
        }
        if (shotPage == 34) { page = PG_TRG; track = 4; slot = 6; editSlot = 6; editAt = 1; Engine::setMachine(curP(), 4, M_WTBL); }
        if (shotPage == 35) {
            page = PG_TRG; track = 1; slot = 5; Pattern& Pp = curP(); Pp.s[1][4].lock[P_MTIM] = 23 - 9; Pp.s[1][12].lock[P_MTIM] = 23 + 12;
            holdPat = eng.curPat; holdTrack = 1; holdStep = 12; aHeld = true; cursor = 12; editSlot = 5; editAt = 1;
        }
        if (page == PG_SEQ) {
            track = 4; slot = 3; Pattern& Pp = curP();
            Pp.p[4][P_PULS] = 5; Pp.p[4][P_EROT] = 2; Pp.p[4][P_DENS] = 60; Pp.p[4][P_CHNG] = 10; Pp.p[4][P_RANGE] = 12; Pp.p[4][P_DIR] = 2;
            Pp.p[2][P_DENS] = 40;
            lockA(); eng.start(); unlockA();
            for (int k2 = 0; k2 < 4; k2++) { eng.render(tmp, 22050 / 2); animate(); }
        }
        if (page == PG_LFO1) {
            track = 4; slot = 2;
            Pattern& Pp = curP(); Pp.p[4][P_L1 + LF_DEST] = P_FREQ + 1; Pp.p[4][P_L1 + LF_DEPTH] = 110; Pp.p[4][P_L1 + LF_WAVE] = LW_EXPDN; Pp.p[4][P_L1 + LF_SYNC] = 6; Pp.p[4][P_L1 + LF_MODE] = LM_TRIG;
            for (int k2 = 0; k2 < 3; k2++) { eng.render(tmp, 22050 / 2); animate(); }
        }
        if (shotPage == 28) {   // 64 Steps, Cursor auf Seite 3, COND-Overlay
            page = PG_TRG; track = 4; Pattern& Pp = curP(); Pp.len[4] = 64; cursor = 37;
            Pp.s[4][37].on = 1; Pp.s[4][37].lock[P_COND] = CND_AB + 35 + 5; slot = 7; editSlot = 7; editAt = 1;
            holdPat = eng.curPat; holdTrack = 4; holdStep = 37; aHeld = true; eng.fill = true;
        }
        if (page == PG_SYS) { setSel = SI_OUT; setScroll = SI_OUT - 7.5f; animate(); }
        if (page == PG_CMP) {   // Sidechain-Beispiel
            eng.proj.cmp[CM_SRC] = 1; eng.proj.cmp[CM_THR] = 40; eng.proj.cmp[CM_RAT] = 6; slot = 5;
            for (int k = 0; k < 4; k++) eng.render(tmp, 22050 / 2);
            animate();
        }
        if (shotPage == 21) { holdPat = 0; holdTrack = 4; holdStep = 3; aHeld = true; page = 0; track = 4; slot = 0; }
        if (shotPage == 22) { page = PG_FLT; slot = 0; track = 4; animate(); editAt = 1; editSlot = 0; eng.recording = true; }
        if (shotPage == 23) { page = PG_TRG; slot = 6; track = 4; editSlot = 6; editAt = 1; }
        if (shotPage == 24) { page = PG_MST; slot = MS_SCALE; eng.proj.mst[MS_SCALE] = 1 + 1 * 12 + 9; eng.proj.mst[MS_ROOT] = 9; editSlot = MS_SCALE; editAt = 1; }
        if (shotPage == 25) { page = PG_AMP; menuOpen = true; menuSel = PG_MIX; }
        if (shotPage == 26) { page = PG_PRJ; slot = 1; askConfirm(ACT_LOAD, 2, "SONG 03 LADEN? UNGESPEICHERTES GEHT VERLOREN"); }
        if (page == PG_SRC && curP().machine[track] == M_SAMPLE) { }
        lastPage = page;
        if (shotPage == 20) { page = 0; drawAll(true); } else drawAll(false);
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
    if (!projFromArg) {   // Projektplatz aus den Einstellungen; altes project1.gbx wird zu song01.gbx
        char old[700], s1[700];
        snprintf(old, sizeof old, "%sprojects/project1.gbx", basePath);
        songPath(0, s1, sizeof s1);
        if (fileExists(old) && !fileExists(s1)) rename(old, s1);
        songPath(cfg.song, projPath, sizeof projPath);
    }
    prjSel = cfg.song;
    {
        char vp[700]; snprintf(vp, sizeof vp, "%sVERSION", basePath);
        FILE* vf = fopen(vp, "r");
        if (vf) { if (fgets(versionStr, sizeof versionStr, vf)) versionStr[strcspn(versionStr, "\r\n")] = 0; fclose(vf); }
    }
    usbMode = detectUsbMode();
    readNetIp();
    {   // freigeschalteter OTG-Port startet evtl. im Automatik-/Geraetemodus: fuer S-1 & Co. auf Host stellen
        FILE* f = fopen("/sys/devices/platform/ff2c0000.syscon/ff2c0000.syscon:usb2-phy@100/otg_mode", "r");
        if (f) {
            char m[32] = "";
            if (!fgets(m, sizeof m, f)) m[0] = 0;
            fclose(f);
            if (!usbMode && !cfg.usb && strncmp(m, "host", 4) && strncmp(m, "UNKNOWN", 7)) {
                char cmd[700];
                snprintf(cmd, sizeof cmd, "bash '%spc-modus.sh' aus nosleep", basePath);
                printf("OTG-Port im Modus '%.*s' - stelle auf Host\n", (int)strcspn(m, "\n"), m);
                if (system(cmd) != 0) printf("Host-Umschaltung fehlgeschlagen\n");
            }
        }
    }
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
    webStart(basePath);
    int webSeq = 0;
    if (cfg.usb && !usbMode) { usbMode = cfg.usb; usbApplyAt = SDL_GetTicks() + 1500; }   // PC-Modus nach Neustart wiederherstellen
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
        {   // Browser-Upload: Meldung und Samples neu laden, sobald 1,5 s nichts mehr kommt
            char wm[96];
            int sq = webLastEvent(wm, sizeof wm);
            if (sq != webSeq) { webSeq = sq; toast("%s", wm); }
            if (webTakeChange(now, 1500)) { reloadSamples(); toast("%d SAMPLES GELADEN", sampleCount()); }
        }
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
