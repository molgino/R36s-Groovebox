// SYNTH CITY - Elektron-artige Groovebox fuer den R36S (ArkOS) und Linux-Desktop
#include <SDL.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include "engine.h"
#include "input.h"
#include "font.h"
#include "samples.h"
#include "web.h"
#include "midi.h"
#include "drmout.h"
#include "usbaudio.h"
#include "remote.h"
#include "paramhelp.h"
#include "factory.h"

static const int SW = 640, SH = 480;

struct Col { Uint8 r, g, b; };
// SYNTH CITY: Neo-Noir-Palette (Nachtblau, Neon-Magenta/Cyan, Amber)
static const Col C_BG = {8, 6, 18}, C_PANEL = {20, 16, 38}, C_SEL = {46, 24, 62}, C_DIM = {140, 130, 182},
                 C_TEXT = {214, 238, 255}, C_ACC = {255, 46, 136}, C_TRIG = {255, 64, 96}, C_LOCK = {255, 184, 0},
                 C_OFF = {26, 22, 46}, C_OFF2 = {42, 36, 70}, C_PLAY = {236, 250, 255}, C_DARK = {12, 10, 24},
                 C_GREEN = {0, 255, 170}, C_KNOB = {50, 44, 80}, C_CYAN = {0, 224, 255};

static SDL_Renderer* ren = nullptr;
static SDL_Surface* drmSurf = nullptr;   // Groovebox-OS: Software-Zeichenflaeche fuer die DRM-Ausgabe
static void present() { SDL_RenderPresent(ren); if (drmSurf) drmOutPresent(drmSurf); }
static SDL_Texture* fontTex = nullptr;
static Uint8 textAlpha = 255;
static SDL_AudioDeviceID audioDev = 0;
static Engine eng;
static Input in;

// UI-Zustand
static int track = 0, cursor = 0, page = 0, slot = 0;
static bool aHeld = false, aJustOn = false, aEdited = false;
static bool lockLatch = false;
static int aPrevOn = 0;          // Zustand des gehaltenen Steps vor dem Druck: 0 leer, 1 Trig, 2 trigless   // Lock-Modus eingerastet (A lange gedrueckt)
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
static Step trigClip;               // Trig-Ablage (A halten + L1 / R1)
static bool haveTrigClip = false;
static uint8_t pageClip[8];         // Seiten-Ablage (Seitenmenue X / Y)
static int pageClipStore = -1, pageClipMachine = 0, pageClipTrack = 0, pageClipLen = 16, pageClipSpeed = 4;
static Pattern mutUndo;            // Pattern vor der letzten Mutation
static bool haveMutUndo = false;
static int mutAmt = 1, mutPat = 0;
static int prjSel = 0, presetSel = 0, mixRow = 0;
static const uint8_t MIX_P[4] = {P_VOL, P_PAN, P_DLY, P_REV};
static const char* const MIX_NAMES[4] = {"VOL", "PAN", "DLY", "REV"};
static bool menuOpen = false; static int menuSel = 0;
static bool confirmOpen = false; static char confirmText[80]; static int confirmAct = 0, confirmArg = 0;
static Uint32 selDownAt = 0;
static bool quitReq = false;
static volatile float cpuLoad = 0;
static volatile double audGapMax = 0, audWorkMax = 0;   // Audio-Diagnose (ms) seit der letzten Log-Zeile
static volatile int audLate = 0, audLateTotal = 0;     // verspaetete/zu lange Audio-Aufrufe
static char basePath[512] = "./", cfgPath[600], projPath[600];
static bool selfTesting = false;   // --selftest/--bench: keine Systemaktionen (USB, Lernmodus, Audio-Geraet)
static Project lastSaved;

// System-Einstellungen (settings.cfg)
struct Settings { char dev[128]; int buf, midiIn, notesOut, clk, chm, usb, song, dispOff, usbAudio, hwVol, pcIn, usbVol, intUsb, cores; };
static Settings cfg = {"", 1, 1, 0, 0, 0, 0, 0, 277, 1, 80, 1, 100, 1, 2};
static bool intMuted = false;   // interne Ausgabe stumm, weil USB-Audio laeuft
static volatile float pcInGain = 1.f;   // PC-Ton in den Ausgang (0 = nur fuer Aufnahme)   // dispOff: Bild-Versatz in Bytes (Groovebox-OS, R36S-Panel)
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
static bool driveOn = false;   // Groovebox-OS: Datenpartition gerade als USB-Laufwerk am PC
// Speichern: unter der Audio-Sperre nur kopieren (~1 ms), Schreiben auf die SD-Karte ohne Sperre
// (sonst steht der Audio-Thread waehrend des Schreibens -> Aussetzer beim automatischen Speichern).
// Erst in eine Temp-Datei, dann umbenennen: ein Stromausfall beim Speichern zerstoert das Projekt nicht.
static Project saveSnap;
static bool saveProject() {
    if (driveOn) return false;
    lockA(); memcpy(&saveSnap, &eng.proj, sizeof(Project)); unlockA();
    char tmp[700]; snprintf(tmp, sizeof tmp, "%s.tmp", projPath);
    FILE* f = fopen(tmp, "wb");
    bool ok = f && fwrite(&saveSnap, sizeof(Project), 1, f) == 1;
    if (f) { ok = fflush(f) == 0 && ok; ok = fsync(fileno(f)) == 0 && ok; fclose(f); }
    ok = ok && rename(tmp, projPath) == 0;
    if (ok) memcpy(&lastSaved, &saveSnap, sizeof(Project));
    else remove(tmp);
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
        else if (!strcmp(line, "usb_audio")) cfg.usbAudio = atoi(v) != 0;
        else if (!strcmp(line, "pc_in")) cfg.pcIn = atoi(v) != 0;
        else if (!strcmp(line, "usb_volume")) cfg.usbVol = clampi(atoi(v), 0, 100);
        else if (!strcmp(line, "internal_on_usb")) cfg.intUsb = atoi(v) != 0;
        else if (!strcmp(line, "hw_volume")) cfg.hwVol = clampi(atoi(v), 0, 100);
        else if (!strcmp(line, "audio_cores")) cfg.cores = clampi(atoi(v), 1, 2);
        else if (!strcmp(line, "display_offset")) cfg.dispOff = clampi(atoi(v), -1920, 1920);
    }
    fclose(f);
}
static void saveSettings() {
    FILE* f = fopen(setPath, "w");
    if (!f) return;
    fprintf(f, "# Groovebox Systemeinstellungen\naudio_device %s\nbuffer %d\nmidi_in %d\nmidi_notes_out %d\nmidi_clock %d\nmidi_channels %d\nusb_mode %d\nsong %d\ndisplay_offset %d\nusb_audio %d\nhw_volume %d\npc_in %d\nusb_volume %d\ninternal_on_usb %d\naudio_cores %d\n",
            cfg.dev, cfg.buf, cfg.midiIn, cfg.notesOut, cfg.clk, cfg.chm, cfg.usb, cfg.song, cfg.dispOff, cfg.usbAudio, cfg.hwVol, cfg.pcIn, cfg.usbVol, cfg.intUsb, cfg.cores);
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
        if (nm && (strcasestr(nm, "uac2") || strcasestr(nm, "gadget"))) continue;   // USB zum PC: eigener Schalter "Audio an PC"
        if (nm) snprintf(devNames[devCount++], 128, "%s", nm);
    }
}
static int devIndex() {
    if (!cfg.dev[0]) return 0;
    for (int i = 0; i < devCount; i++) if (!strcmp(devNames[i], cfg.dev)) return i + 1;
    return 0;
}

// ---------------------------------------------------------------- Zeichnen

// Direkter Bildspeicher (Groovebox-OS/DRM): eigene, schnelle Grundfunktionen statt SDL-Software-Renderer
static uint32_t* fbPix = nullptr;   // ARGB8888, SW x SH
static int fbPitch = 0;             // in Pixeln
static Col curCol = {0, 0, 0}; static Uint8 curA = 255;
// Alpha-Mischung mit gepackter Arithmetik: Rot+Blau in einem Wort, Gruen getrennt (2 Multiplikationen pro Pixel)
static inline uint32_t fbMix(uint32_t d, uint32_t srbA, uint32_t sgA, uint32_t ia) {
    uint32_t rb = ((d & 0xFF00FFu) * ia + srbA) >> 8 & 0xFF00FFu;
    uint32_t g = ((d & 0x00FF00u) * ia + sgA) >> 8 & 0x00FF00u;
    return 0xff000000u | rb | g;
}
static inline void fbBlendSpan(uint32_t* p, int n, Col c, Uint8 a) {
    if (a == 255) { uint32_t v = 0xff000000u | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b; for (int i = 0; i < n; i++) p[i] = v; return; }
    if (a == 0) return;
    uint32_t a256 = a + (a >> 7), ia = 256 - a256;
    uint32_t srbA = (((uint32_t)c.r << 16) | c.b) * a256, sgA = ((uint32_t)c.g << 8) * a256;
    for (int i = 0; i < n; i++) p[i] = fbMix(p[i], srbA, sgA, ia);
}
static void fbFill(int x, int y, int w, int h, Col c, Uint8 a) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SW) w = SW - x;
    if (y + h > SH) h = SH - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) fbBlendSpan(fbPix + (size_t)yy * fbPitch + x, w, c, a);
}
static inline void fbPlot(int x, int y) { if ((unsigned)x < (unsigned)SW && (unsigned)y < (unsigned)SH) fbBlendSpan(fbPix + (size_t)y * fbPitch + x, 1, curCol, curA); }
static void fbLine(int x1, int y1, int x2, int y2) {   // Bresenham
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1, dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1, err = dx + dy;
    for (;;) {
        fbPlot(x1, y1);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}
static void setCol(Col c, Uint8 a = 255) { curCol = c; curA = a; if (!fbPix) SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, a); }
static void drawLine(int x1, int y1, int x2, int y2) { if (fbPix) fbLine(x1, y1, x2, y2); else SDL_RenderDrawLine(ren, x1, y1, x2, y2); }
static void clearScreen(Col c) { if (fbPix) fbFill(0, 0, SW, SH, c, 255); else { setCol(c); SDL_RenderClear(ren); } }
static void fill(int x, int y, int w, int h, Col c, Uint8 a = 255) {
    if (fbPix) { fbFill(x, y, w, h, c, a); return; }
    setCol(c, a); SDL_Rect r = {x, y, w, h}; SDL_RenderFillRect(ren, &r);
}
static void makeFont() {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, FONT_N * FONT_W, FONT_H, 32, SDL_PIXELFORMAT_RGBA32);
    Uint8* px = (Uint8*)s->pixels;
    for (int c = 0; c < FONT_N; c++)
        for (int y = 0; y < FONT_H; y++)
            for (int x = 0; x < FONT_W; x++) {
                Uint8* p = px + y * s->pitch + (c * FONT_W + x) * 4;
                p[0] = p[1] = p[2] = 255; p[3] = FONT_DATA[c][y * FONT_W + x];
            }
    fontTex = SDL_CreateTextureFromSurface(ren, s);
    SDL_SetTextureBlendMode(fontTex, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(s);
}
// UTF-8 -> Glyph: ASCII direkt, Umlaute/ß/° als Zusatzglyphen (siehe genfont.py), sonst '?'
static int nextGlyph(const char*& s) {
    unsigned c = (unsigned char)*s++;
    if (c < 0x80) return c >= 32 && c <= 126 ? (int)c - 32 : '?' - 32;
    if (c == 0xC3 || c == 0xC2) {
        unsigned d = (unsigned char)*s;
        if (!d) return '?' - 32;
        s++;
        if (c == 0xC2) return d == 0xB0 ? 102 : '?' - 32;   // °
        switch (d) {
        case 0x84: return 95; case 0x96: return 96; case 0x9C: return 97;    // Ä Ö Ü
        case 0xA4: return 98; case 0xB6: return 99; case 0xBC: return 100;   // ä ö ü
        case 0x9F: return 101;                                                // ß
        }
        return '?' - 32;
    }
    while ((*s & 0xC0) == 0x80) s++;   // andere Mehrbyte-Zeichen ueberspringen
    return '?' - 32;
}
static int textLen(const char* s) { int n = 0; while (*s) { nextGlyph(s); n++; } return n; }
static int textW(const char* s, int sc = 1) { return textLen(s) * FONT_W * sc; }
static void text(int x, int y, const char* s, Col c, int sc = 1) {
    if (fbPix) {   // Glyphen direkt mischen
        uint32_t srb = ((uint32_t)c.r << 16) | c.b, sg = (uint32_t)c.g << 8;
        uint32_t solid = 0xff000000u | srb | sg;
        for (; *s; x += FONT_W * sc) {
            int gi = nextGlyph(s);
            if (x >= SW || x + FONT_W * sc <= 0) continue;
            const uint8_t* g = FONT_DATA[gi];
            if (sc == 1 && x >= 0 && x + FONT_W <= SW && y >= 0 && y + FONT_H <= SH) {   // haeufigster Fall: direkt mischen
                uint32_t* row = fbPix + (size_t)y * fbPitch + x;
                for (int gy = 0; gy < FONT_H; gy++, row += fbPitch, g += FONT_W)
                    for (int gx = 0; gx < FONT_W; gx++) {
                        unsigned av = g[gx];
                        if (!av) continue;
                        if (textAlpha != 255) av = av * textAlpha / 255;
                        if (av >= 250) { row[gx] = solid; continue; }
                        uint32_t a256 = av + (av >> 7);
                        row[gx] = fbMix(row[gx], srb * a256, sg * a256, 256 - a256);
                    }
                continue;
            }
            for (int gy = 0; gy < FONT_H; gy++) {
                int py = y + gy * sc;
                if (py + sc <= 0 || py >= SH) continue;
                for (int gx = 0; gx < FONT_W; gx++) {
                    unsigned av = g[gy * FONT_W + gx];
                    if (!av) continue;
                    av = av * textAlpha / 255;
                    for (int k = 0; k < sc; k++) {
                        int yy = py + k;
                        if ((unsigned)yy >= (unsigned)SH) continue;
                        int xx = x + gx * sc, n = sc;
                        if (xx < 0) { n += xx; xx = 0; }
                        if (xx + n > SW) n = SW - xx;
                        if (n > 0) fbBlendSpan(fbPix + (size_t)yy * fbPitch + xx, n, c, (Uint8)av);
                    }
                }
            }
        }
        return;
    }
    SDL_SetTextureColorMod(fontTex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(fontTex, textAlpha);
    for (; *s; x += FONT_W * sc) {
        int gi = nextGlyph(s);
        SDL_Rect src = {gi * FONT_W, 0, FONT_W, FONT_H}, dst = {x, y, FONT_W * sc, FONT_H * sc};
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
        for (int i = 0; i < segs; i++) drawLine(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y);
    }
}
static void knob(int cx, int cy, int r, float frac, bool bip, Col c) {
    arc(cx, cy, r, 135, 405, C_KNOB, 4);
    float av = 135 + 270 * frac;
    if (bip) arc(cx, cy, r, 270, av, c, 4);
    else arc(cx, cy, r, 135, av, c, 4);
    float a = av * 0.0174533f;
    setCol(c);
    drawLine(cx + (int)((r - 9) * cosf(a)), cy + (int)((r - 9) * sinf(a)),
                       cx + (int)((r - 4) * cosf(a)), cy + (int)((r - 4) * sinf(a)));
}
static void playIcon(int x, int y, bool playing) {
    if (playing) { for (int k = 0; k < 8; k++) fill(x + k, y + k, 1, 16 - 2 * k, C_GREEN); }
    else fill(x, y + 2, 12, 12, C_DIM);
}

// ---------------------------------------------------------------- Optik-Helfer

static const Col TCOL[NTRACKS] = {   // Neon
    {255, 46, 136}, {255, 110, 64}, {255, 196, 40}, {0, 255, 170},
    {0, 224, 255}, {90, 140, 255}, {176, 92, 255}, {255, 100, 220}};

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
    char ch[4] = {0, 0, 0, 0};
    while (*s) {
        if (*s == '[') { k = true; s++; continue; }
        if (*s == ']') { k = false; x += 3; s++; continue; }
        const char* a = s; nextGlyph(s);
        int n = (int)(s - a); if (n > 3) n = 3;
        memcpy(ch, a, n); ch[n] = 0;
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

// Aufnahme vom PC als Sample speichern: samples/pc/pc-NN.wav (16 Bit Stereo), danach Samples neu laden
static void reloadSamples();
static void savePcRecording(const int16_t* d, int frames) {
    char dir[700], path[760];
    snprintf(dir, sizeof dir, "%ssamples/pc", basePath);
    mkdir(dir, 0755);
    int no = 1;
    struct stat st;
    for (; no < 100; no++) { snprintf(path, sizeof path, "%s/pc-%02d.wav", dir, no); if (stat(path, &st) != 0) break; }
    // Stille am Ende abschneiden
    while (frames > 1000 && abs(d[(frames - 1) * 2]) < 200 && abs(d[(frames - 1) * 2 + 1]) < 200) frames--;
    FILE* f = fopen(path, "wb");
    if (f) {
        uint32_t bytes = (uint32_t)frames * 4, v32; uint16_t v16;
        fwrite("RIFF", 1, 4, f); v32 = 36 + bytes; fwrite(&v32, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
        v32 = 16; fwrite(&v32, 4, 1, f); v16 = 1; fwrite(&v16, 2, 1, f); v16 = 2; fwrite(&v16, 2, 1, f);
        v32 = 44100; fwrite(&v32, 4, 1, f); v32 = 44100 * 4; fwrite(&v32, 4, 1, f); v16 = 4; fwrite(&v16, 2, 1, f); v16 = 16; fwrite(&v16, 2, 1, f);
        fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(d, 4, (size_t)frames, f);
        fclose(f);
        sync();
    }
    pcRecDone();
    if (!f) { toast("AUFNAHME: SPEICHERN FEHLGESCHLAGEN"); return; }
    reloadSamples();
    toast("SAMPLE pc/pc-%02d GESPEICHERT (%.1f s)", no, frames / 44100.f);
    printf("PC-Aufnahme gespeichert: %s (%d Frames)\n", path, frames);
}

// Lautstaerke- und Power-Taste (Kernel-Geraete "gpio-keys-vol" und "rk805 pwrkey", im Groovebox-OS nicht ueber SDL sichtbar)
// Power kurz: Display aus/an (Musik laeuft weiter), Power lang (1,5 s): speichern und ausschalten
static int volKeyFd = -2, pwrKeyFd = -1;
static bool screenOff = false;
static Uint32 pwrDownAt = 0;
static void setBacklight(bool on) {
    DIR* d = opendir("/sys/class/backlight");
    struct dirent* e;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[300]; snprintf(p, sizeof p, "/sys/class/backlight/%s/bl_power", e->d_name);
        FILE* f = fopen(p, "w"); if (f) { fputs(on ? "0" : "4", f); fclose(f); }
    }
    if (d) closedir(d);
}
static void drawAll(bool help);
static void powerOff() {
    if (selfTesting) return;
    toast("SPEICHERN UND AUSSCHALTEN ...");
    msgUntil = SDL_GetTicks() + 10000;
    if (screenOff) { setBacklight(true); screenOff = false; }
    drawAll(false); present();
    saveProject();
    saveSettings();
    sync();
    if (system("systemctl poweroff") != 0) printf("Ausschalten fehlgeschlagen\n");
    quitReq = true;
}
static void pollVolumeKeys() {
    if (volKeyFd == -2) {
        volKeyFd = -1;
        struct stat st;
        if (stat("/etc/groovebox-os", &st) != 0) return;   // ArkOS regelt Lautstaerke/Power selbst
        for (int i = 0; i < 16; i++) {
            char p[32]; snprintf(p, sizeof p, "/dev/input/event%d", i);
            int fd = open(p, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) continue;
            char nm[64] = "";
            ioctl(fd, EVIOCGNAME(sizeof nm), nm);
            if (strstr(nm, "vol") && volKeyFd < 0) { volKeyFd = fd; printf("Lautstärketasten: %s (%s)\n", nm, p); }
            else if (strstr(nm, "pwrkey") && pwrKeyFd < 0) { pwrKeyFd = fd; printf("Power-Taste: %s (%s)\n", nm, p); }
            else close(fd);
        }
        if (volKeyFd >= 0) hwVolumeSet(cfg.hwVol);   // gespeicherte Lautstaerke wiederherstellen
    }
    struct input_event ev;
    if (volKeyFd >= 0) {
        int step = 0;
        while (read(volKeyFd, &ev, sizeof ev) == (ssize_t)sizeof ev)
            if (ev.type == EV_KEY && ev.value != 0) {   // 1 = gedrueckt, 2 = gehalten (Wiederholung)
                if (ev.code == KEY_VOLUMEUP) step += 5;
                if (ev.code == KEY_VOLUMEDOWN) step -= 5;
            }
        if (step) {
            cfg.hwVol = clampi(cfg.hwVol + step, 0, 100);
            if (intMuted) toast("INTERN %d%% (STUMM, USB-AUDIO AKTIV)", cfg.hwVol);
            else { hwVolumeSet(cfg.hwVol); toast("LAUTSTÄRKE %d%%", cfg.hwVol); }
            saveSettings();
        }
    }
    if (pwrKeyFd >= 0) {
        Uint32 now = SDL_GetTicks();
        while (read(pwrKeyFd, &ev, sizeof ev) == (ssize_t)sizeof ev) {
            if (ev.type != EV_KEY || ev.code != KEY_POWER) continue;
            if (ev.value == 1) pwrDownAt = now ? now : 1;
            else if (ev.value == 0 && pwrDownAt) {   // losgelassen vor 1,5 s: Display umschalten
                if (now - pwrDownAt < 1500) { screenOff = !screenOff; setBacklight(!screenOff); }
                pwrDownAt = 0;
            }
        }
        if (pwrDownAt && now - pwrDownAt >= 1500) { pwrDownAt = 0; powerOff(); }
    }
}

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
    { static Uint32 jackAt = 0; if (now - jackAt > 500) { jackAt = now; audioJackPoll(); } }
    pollVolumeKeys();
    {   // USB-Audio zum PC an/aus je nach Modus (Geraet erscheint erst nach dem Umschalten in den PC-Modus)
        static Uint32 uaAt = 0;
        if (now - uaAt > 1500) {
            uaAt = now;
            bool want = cfg.usbAudio && usbMode == 2 && !driveOn;
            if (want && !usbAudioActive()) usbAudioStart();
            if (!want && usbAudioActive()) usbAudioStop();
            usbAudioSetGain(cfg.usbVol / 100.f);
            bool mute = cfg.intUsb && usbAudioActive();   // interne Ausgabe stumm, solange USB-Audio laeuft
            if (mute != intMuted) { intMuted = mute; hwVolumeSet(mute ? 0 : cfg.hwVol); }
            bool rec = pcRecState() == 1 || pcRecState() == 2;
            pcInGain = cfg.pcIn ? 1.f : 0.f;
            bool wantIn = (cfg.pcIn || rec) && usbMode == 2 && !driveOn;
            if (wantIn && !pcInActive()) pcInStart();
            if (!wantIn && pcInActive()) pcInStop();
        }
        {   // fertige PC-Aufnahme als Sample speichern
            int16_t* d = nullptr;
            int n = pcRecTake(&d);
            if (n > 0) savePcRecording(d, n);
            else if (pcRecState() == 3) pcRecDone();
        }
    }
}

// ---------------------------------------------------------------- Bildschirm

static Pattern& curP() { return eng.proj.pat[eng.curPat]; }
static Step& holdS() { return eng.proj.pat[holdPat].s[holdTrack][holdStep]; }

// Rueckgaengig: Stapel von Pattern-Staenden; zusammenhaengende Aenderungen (bis 0,8 s Pause) = ein Schritt
enum { UNDO_N = 12 };
static Pattern undoBuf[UNDO_N], undoShadow, redoBuf;
static int undoPat[UNDO_N], undoTop = 0, undoCnt = 0, undoShadowPat = -1, redoPat = -1;
static bool undoDirty = false;
static Uint32 undoLastChange = 0;
static void toast(const char* fmt, ...);
static void lockA();
static void unlockA();
static void undoTrack(Uint32 now) {   // einmal pro Bild nach der Eingabe
    const Pattern& P = eng.proj.pat[eng.curPat];
    if (eng.curPat != undoShadowPat) { undoShadow = P; undoShadowPat = eng.curPat; undoDirty = false; return; }
    if (memcmp(&P, &undoShadow, sizeof(Pattern))) {
        if (!undoDirty) {   // erster Schritt einer Aenderung: Stand davor sichern
            undoBuf[undoTop] = undoShadow; undoPat[undoTop] = undoShadowPat;
            undoTop = (undoTop + 1) % UNDO_N; if (undoCnt < UNDO_N) undoCnt++;
            undoDirty = true; redoPat = -1;
        }
        undoLastChange = now;
        undoShadow = P;
    } else if (undoDirty && now - undoLastChange > 800) undoDirty = false;
}
static void undoStep() {
    if (!undoCnt) { toast("NICHTS RÜCKGÄNGIG ZU MACHEN"); return; }
    undoTop = (undoTop + UNDO_N - 1) % UNDO_N; undoCnt--;
    int pi = undoPat[undoTop];
    redoBuf = eng.proj.pat[pi]; redoPat = pi;
    lockA(); eng.proj.pat[pi] = undoBuf[undoTop]; unlockA();
    if (pi == eng.curPat) undoShadow = undoBuf[undoTop];
    undoDirty = false;
    toast("RÜCKGÄNGIG: PATTERN %02d (%d WEITERE)", pi + 1, undoCnt);
}

// WAV-Aufnahme der Summe (LIVE-Seite, R1): Puffer im Speicher, beim Stoppen in samples/rec/ schreiben (Thread)
enum { REC_MAX_SEC = 300 };
static int16_t* recBuf = nullptr;
static volatile int recPos = 0, recCap = 0, recRate = 44100;
static volatile bool recOn = false, recSaving = false;
static volatile int recDoneNo = 0;   // > 0: gespeichert als rec-NN.wav (Meldung im Hauptthread)
static void recPush(const float* in, int frames) {   // Audio-Thread
    if (!recOn) return;
    int n = frames * 2, room = recCap - recPos;
    if (n > room) { n = room; recOn = false; }
    int16_t* d = recBuf + recPos;
    for (int i = 0; i < n; i++) { float x = in[i] * 32767.f; d[i] = (int16_t)(x > 32767.f ? 32767 : (x < -32768.f ? -32768 : x)); }
    recPos += n;
}
static int recWriter(void* arg) {
    char* path = (char*)arg;
    FILE* f = fopen(path, "wb");
    int n = recPos, bytes = n * 2;
    if (f) {
        auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); }; auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
        fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f); w32(16); w16(1); w16(2);
        w32(recRate); w32(recRate * 4); w16(4); w16(16); fwrite("data", 1, 4, f); w32(bytes);
        fwrite(recBuf, 2, n, f);
        fclose(f);
    }
    free(recBuf); recBuf = nullptr; recCap = 0;
    const char* b = strrchr(path, '-');
    recDoneNo = f && b ? atoi(b + 1) : -1;
    free(path);
    recSaving = false;
    return 0;
}
static void recToggle() {
    if (recSaving) { toast("AUFNAHME WIRD NOCH GESPEICHERT ..."); return; }
    if (!recOn && !recBuf) {
        recRate = eng.sr;
        int cap = recRate * REC_MAX_SEC * 2;
        recBuf = (int16_t*)malloc((size_t)cap * 2);
        if (!recBuf) { toast("ZU WENIG SPEICHER FÜR DIE AUFNAHME"); return; }
        recPos = 0; recCap = cap; recOn = true;
        toast("AUFNAHME LÄUFT (MAX. %d MIN)", REC_MAX_SEC / 60);
        return;
    }
    recOn = false;
    if (recPos < recRate / 2) { free(recBuf); recBuf = nullptr; toast("AUFNAHME ZU KURZ - VERWORFEN"); return; }
    char dir[700]; snprintf(dir, sizeof dir, "%ssamples", basePath); mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%ssamples/rec", basePath); mkdir(dir, 0755);
    char* path = (char*)malloc(800);
    struct stat st;
    for (int i = 1; i < 1000; i++) { snprintf(path, 800, "%s/rec-%03d.wav", dir, i); if (stat(path, &st)) break; }
    recSaving = true;
    SDL_Thread* th = SDL_CreateThread(recWriter, "rec", path);
    if (th) SDL_DetachThread(th); else recWriter(path);
    toast("AUFNAHME WIRD GESPEICHERT ...");
}
static void redoStep() {
    if (redoPat < 0) { toast("NICHTS ZU WIEDERHOLEN"); return; }
    lockA(); eng.proj.pat[redoPat] = redoBuf; unlockA();
    toast("WIEDERHOLT: PATTERN %02d", redoPat + 1);
    redoPat = -1;
}

// Seiten: Spurseiten haben eine Speicherseite (je 8 Parameter), globale nicht
static bool trackPage(int p) { return p < FIRST_GLOBAL_PAGE; }
static int sp() { return PG_STORE[page]; }
static bool pageAvail(int p) {
    if (p == PG_SRC2) return machineHasSrc2(curP().machine[track]);
    if (p == PG_ARP) return !machineIsDrum(curP().machine[track]);   // Arpeggiator nur fuer Klangerzeuger mit Tonhoehe
    return true;
}
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
    // Neon-Linie Magenta -> Cyan, pulsiert im Takt
    for (int x = 0; x < SW; x += 8) {
        Col c = mix(C_ACC, C_CYAN, (float)x / SW);
        fill(x, 40, 8, 1, mix(dimC(c, 0.55f), c, 0.4f + 0.6f * beatPulse));
        fill(x, 41, 8, 1, c, (Uint8)(30 + 110 * beatPulse));
        fill(x, 42, 8, 1, c, (Uint8)(10 + 40 * beatPulse));
    }
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
    if (eng.proj.mst[MS_SWNG]) { snprintf(b, sizeof b, "SW %d%%", 50 + eng.proj.mst[MS_SWNG]); text(84, 21, b, C_DIM); }
    else text(84, 21, eng.playing ? "PLAY" : "STOP", eng.playing ? C_GREEN : C_DIM);
    if (eng.songOn && eng.proj.songLen && !eng.recording && !eng.fill) {
        snprintf(b, sizeof b, "SNG %d/%d", eng.songPos + 1, eng.proj.songLen);
        text(140, 21, b, C_CYAN);
    } else if (curScale() && !eng.recording && !eng.fill) { snprintf(b, sizeof b, "%s%s", NOTE_NAMES[(curScale() - 1) % 12], SCALE_NAMES[scaleType(curScale()) - 1]); text(140, 21, b, dimC(C_ACC, 0.8f)); }
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
    if (trackPage(page)) {   // Signalweg-Oberflaeche: Pfad statt Reiter
        snprintf(b, sizeof b, "SPUR %d > %s", track + 1, PAGE_LONG[page]);
        text(334, 4, b, C_TEXT);
    } else
    {   // Reiter nur der aktuellen Gruppe (Seitenmenue: SELECT kurz antippen)
        int g = PG_GROUP[page], x = 334, right = 612;
        int list[NUI_PAGES], n = 0, cur = 0;
        for (int p = 0; p < NUI_PAGES; p++) if (PG_GROUP[p] == g && pageAvail(p)) { if (p == page) cur = n; list[n++] = p; }
        // zu viele Reiter: so weit nach links schieben, dass die aktuelle Seite sichtbar bleibt
        int first = 0;
        auto widthFrom = [&](int f, int upto) { int w = 0; for (int i = f; i <= upto; i++) w += textW(PAGE_NAMES[list[i]]) + 5; return w; };
        while (first < cur && widthFrom(first, cur) + (first ? 10 : 0) > right - x) first++;
        if (first > 0) { text(x, 4, "<", C_DIM); x += 10; }
        for (int i = first; i < n; i++) {
            int p = list[i], w = textW(PAGE_NAMES[p]) + 4;
            if (x + w > right) { text(right - 4, 4, ">", C_DIM); break; }
            if (p == page) { rrect(x, 3, w, 18, 4, C_ACC); text(x + 2, 4, PAGE_NAMES[p], C_BG); }
            else text(x + 2, 4, PAGE_NAMES[p], C_DIM);
            x += w + 1;
        }
    }
    if (aHeld) {
        snprintf(b, sizeof b, lockLatch ? "LOCK-MODUS STEP %02d" : "TRIG %02d HALTEN: P-LOCK", holdStep + 1);
        text(332, 23, b, C_LOCK);
    } else {
        int hx = 334;
        if (!trackPage(page)) { text(hx, 23, GROUP_NAMES[PG_GROUP[page]], dimC(C_ACC, 0.8f)); hx += textW(GROUP_NAMES[PG_GROUP[page]]) + 10; }
        if (cpuLoad > 0.6f) { snprintf(b, sizeof b, "CPU %d%%", (int)(cpuLoad * 100)); text(hx, 23, b, C_TRIG); }   // nur als Warnung
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
            {"PRESET", K_SLOT, 0, 31 + NFACTORY}, {"P.LADEN", K_ACT, 0, 0}, {"P.SPEICHERN", K_ACT, 0, 0}, {"INIT", K_ACT, 0, 0}};
        d = PRJ_DEFS[k];
        val = k == 0 ? prjSel : (k == 4 ? presetSel : 0);
        if (k == 0) snprintf(label, ln, "SONG %02d %s", prjSel + 1, prjSel == cfg.song ? "(AKTIV)" : (songExists(prjSel) ? "" : "(LEER)"));
        if (k == 4 && presetSel >= 32) snprintf(label, ln, "W:%.16s", FACTORY[presetSel - 32].name);
        else if (k == 4) snprintf(label, ln, "%s %02d%s", MACHINE_NAMES[P.machine[track]], presetSel + 1, presetExists(P.machine[track], presetSel) ? "" : " (LEER)");
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
            else snprintf(label, ln, "%s", usbMode ? "R36S AM PC" : "GERÄTE AM R36S");
            break;
        default: {
            char nm[256]; val = midiPortInfo(nm, sizeof nm);
            snprintf(label, ln, "%.19s", strcmp(midiBackend(), "AUS") ? nm : "MIDI AUS");
        } break;
        }
        if (k == SY_BUF && bufOverride) snprintf(label, ln, "--buffer %d", bufOverride);
    } else if (sp() < 0) {   // globale Seite ohne Spurwerte
        d = PDEF_NONE; val = 0; baseVal = 0;
        return false;
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
static float envShapeK = 1.f;   // Huellkurvenform fuer die Anzeige (Wert^k)
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
        ys[i] = bot - (int)(powf(v, envShapeK) * H);
        hi[i] = seg == sel;
    }
    curve(x0, ys, n, bot, c, 40, hi);
    handle(x0 + wA, top, c, sel == 0);
    handle(x0 + wA + wD, bot - (int)(powf(sv, envShapeK) * H), c, sel == 1 || sel == 2);
    handle(x0 + wA + wD + wS, bot - (int)(powf(sv, envShapeK) * H), c, sel == 2);
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

// Betrag von Filter 1 (Typ FT_*) bei Frequenz f; fc aus FREQ, res 0..127, units = FREQ-Wert (fuer VOWL)
static float filt1Mag(int type, float f, float fc, int res, float units) {
    float r = res / 127.f;
    auto svf = [](float w, float k, float& lo, float& bp, float& hp) {
        float den = sqrtf((1 - w * w) * (1 - w * w) + (w * k) * (w * k));
        lo = 1.f / den; bp = w * k / den; hp = w * w / den;
    };
    float w = f / fc, lo, bp, hp;
    switch (type % FT_COUNT) {
    case FT_LP24: case FT_HP24: {
        float l1, b1, h1; svf(w, 2.f - 1.96f * 0.3f, l1, b1, h1); svf(w, 2.f - 1.96f * r * 0.98f, lo, bp, hp);
        return type == FT_LP24 ? l1 * lo : h1 * hp;
    }
    case FT_LADR: {   // 1 / ((1 + jw)^4 + k)
        float k = r * 4.15f, re = 1, im = 0;
        for (int i = 0; i < 4; i++) { float nr = re - im * w, ni = im + re * w; re = nr; im = ni; }
        re += k;
        return (1.f + k * 0.5f) / sqrtf(re * re + im * im) * 0.6f;
    }
    case FT_NTCH: svf(w, 2.f - 1.96f * r * 0.97f, lo, bp, hp); return fabsf(1 - w * w) * lo;
    case FT_PEAK: svf(w, 2.f - 1.96f * r * 0.97f, lo, bp, hp); return (1 + w * w) * lo * 0.7f;
    case FT_CMBP: case FT_CMBN: {
        float fb = (type == FT_CMBP ? 1.f : -1.f) * (0.2f + r * 0.77f);
        float ph = 6.2831853f * f / fmaxf(fc, 25.f);
        return (1.f - fabsf(fb) * 0.55f) / sqrtf(1.f - 2.f * fb * cosf(ph) + fb * fb);
    }
    case FT_VOWL: {
        float pos = fminf(fmaxf(units / 127.f, 0.f), 1.f) * 4.f;
        int i0 = (int)pos; if (i0 > 3) i0 = 3;
        float fr = pos - i0, k = 2.f - 1.96f * (0.55f + r * 0.42f), a = 0;
        static const float g[3] = {1, 0.6f, 0.4f};
        for (int j = 0; j < 3; j++) { float ff = VOWEL_F[i0][j] + (VOWEL_F[i0 + 1][j] - VOWEL_F[i0][j]) * fr; svf(f / ff, k, lo, bp, hp); a += bp * g[j]; }
        return a * 1.6f;
    }
    default:
        svf(w, 2.f - 1.96f * r * 0.97f, lo, bp, hp);
        return type == FT_BP ? bp : (type == FT_HP12 ? hp : lo);
    }
}

// Filterkurve (SVF LP/BP/HP) mit Live-Cutoff
static void filterGraph(int gx, int gy, int gw, int gh, int freq, int res, int type, int env, int sel, float liveUnits, Col c) {
    graphBox(gx, gy, gw, gh);
    int x0 = gx + 10, W = gw - 20, top = gy + 8, bot = gy + gh - 8, H = bot - top;
    auto mag = [&](float f, float units) {
        float fc = 16.f * exp2f(units * (10.3f / 127.f));
        float m = filt1Mag(type, f, fc, res, units);
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
    static const char* tn[FT_COUNT] = {"LOWPASS 12DB", "BANDPASS", "HIGHPASS 12DB", "LOWPASS 24DB", "LADDER 24DB", "HIGHPASS 24DB",
                                       "NOTCH", "PEAK", "KAMM +", "KAMM -", "VOKAL A-E-I-O-U"};
    const char* tt = tn[type % FT_COUNT];
    text(gx + gw - 10 - textW(tt), gy + 6, tt, sel == 2 ? C_PLAY : dimC(c, 0.8f));
}

// Base-Width-Filter: Filter 1 (grau) und Gesamtkurve mit Band
static void bwGraph(int gx, int gy, int gw, int gh, int freq, int res, int type, int base, int wdth, int sel, Col c) {
    graphBox(gx, gy, gw, gh);
    int x0 = gx + 10, W = gw - 20, top = gy + 8, bot = gy + gh - 8, H = bot - top;
    if (W > 690) W = 690;
    float fc = 16.f * exp2f(freq * (10.3f / 127.f));
    float lo = 20.f * exp2f(base * (10.f / 127.f)), hi = fminf(lo * exp2f(wdth * (10.f / 127.f)), 20000.f);
    bool on = base > 0 || wdth < 127;
    auto f1 = [&](float f) { return filt1Mag(type, f, fc, res, (float)freq); };
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
    if (en < st) text(lo + 4, gy + 4, "<< RÜCKWÄRTS", C_PLAY);
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
// Arpeggiator: Tonfolge ueber die Trig-Laenge (wie die Engine sie spielt)
static void arpGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int mode = pv(P_AMOD), chord = pv(P_ACHD) % 10, oct = pv(P_AOCT) % 4 + 1, base = 0;
    int L[48], n = 0;
    if (chord == 9) { static const int maj[7] = {0, 2, 4, 5, 7, 9, 11}; for (int o = 0; o < oct; o++) for (int k = 0; k < 7 && n < 48; k++) L[n++] = 12 * o + maj[k]; }
    else for (int o = 0; o < oct; o++) for (int k = 0; k < 5 && ARP_IV[chord][k] >= 0 && n < 48; k++) L[n++] = base + 12 * o + ARP_IV[chord][k];
    int seq[64], m = 0;
    if (mode == 2) for (int i = n - 1; i >= 0; i--) seq[m++] = L[i];
    else { for (int i = 0; i < n; i++) seq[m++] = L[i]; if (mode == 3) for (int i = n - 2; i >= 1 && m < 64; i--) seq[m++] = L[i]; }
    float steps = lenSteps(pv(P_LEN)); if (steps < 0 || steps > 16) steps = 16;
    float per = ARP_STEPS[pv(P_ARAT) % 6];
    int cnt = (int)(steps / per + 0.999f); if (cnt < 1) cnt = 1; if (cnt > 64) cnt = 64;
    int top = 12 * oct; if (top < 12) top = 12;
    int x0 = gx + 12, W = gw - 24, y0 = gy + 8, H = gh - 16;
    float bw = (float)W / cnt, gate = pv(P_AGAT) / 100.f;
    uint32_t rs = 12345;
    if (mode == ARP_CHORD) {   // Akkord: alle Toene (max. 4) gleichzeitig ueber die Trig-Laenge
        int nn = n < 4 ? n : 4, w = (int)(W * fminf(steps / 16.f, 1.f)); if (w < 8) w = 8;
        for (int k = 0; k < nn; k++) { int y = y0 + H - 4 - (int)((float)L[k] / top * (H - 8)); rrect(x0, y - 3, w, 6, 2, c, (Uint8)(k ? 170 : 255)); }
        cnt = 0;
    }
    for (int i = 0; i < cnt; i++) {
        int nv;
        if (mode == 4) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; nv = seq[rs % m]; }
        else nv = seq[i % m];
        int y = y0 + H - 4 - (int)((float)nv / top * (H - 8));
        float acc = (i % m == 0) ? 1.f : 1.f - 0.6f * pv(P_AACC) / 100.f;
        int w = (int)(bw * gate); if (w < 2) w = 2;
        rrect(x0 + (int)(i * bw), y - 3, w, 6, 2, mode ? c : C_OFF2, (Uint8)(mode ? 120 + 135 * acc : 255));
    }
    const char* t = mode ? ARP_MODES[mode % 6] : "ARP AUS";
    text(gx + gw - 10 - textW(t), gy + 6, t, dimC(c, 0.8f));
}

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
    static const char* dn[6] = {"VORWÄRTS >>", "<< RÜCKWÄRTS", "PING <->", "PENDEL <=>", "ZUFALL ?", "BETRUNKEN ~"};
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
    for (int k = 0; k < th; k++) { drawLine(x1 + k, y1, x2 + k, y2); drawLine(x1, y1 + k, x2, y2 + k); }
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
    if (d.kind == K_FTYPE) {   // Kurvenbild des Filtertyps (wie die grosse Grafik, feste Werte)
        int ty = val % FT_COUNT;
        symCurve(x, y + 2, w, h - 4, c, [&](float t) {
            float m = filt1Mag(ty, 20.f * powf(1000.f, t), 630.f, ty >= FT_CMBP ? 90 : 70, 40.f);
            float db = 20.f * log10f(m + 1e-6f);
            return fminf(fmaxf((db + 30.f) / 44.f, 0.f), 0.95f);
        });
        return;
    }
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
    static const char* tn[4] = {"BASIC: SIN > TRI > SAW > SQR", "PWM: 50 % > 5 %", "VOKAL: A > E > I > O > U", "OBERTÖNE: 1 > 32"};
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
    if (page == PG_ARP && row == 0) return 13;
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
    rrect(50, 40, 540, 360, 14, C_ACC);
    rrect(52, 42, 536, 356, 13, C_BG);
    textC(320, 50, "SEITE WÄHLEN", C_ACC, 2);
    int y = 104;
    for (int g = 0; g < 3; g++) {
        text(72, y - 20, GROUP_NAMES[g], C_DIM);
        int x = 72;
        for (int p = 0; p < NUI_PAGES; p++) {
            if (PG_GROUP[p] != g || !pageAvail(p)) continue;
            bool sel = p == menuSel, cur = p == page;
            int w = textW(PAGE_NAMES[p]) + 22;
            if (x + w > 568) { x = 72; y += 36; }   // Zeile umbrechen (viele Spurseiten)
            rrect(x, y, w, 30, 8, sel ? C_ACC : (cur ? C_SEL : C_PANEL));
            text(x + 11, y + 7, PAGE_NAMES[p], sel ? C_BG : (cur ? C_ACC : C_TEXT));
            x += w + 8;
        }
        y += 64;
    }
    textC(320, 344, PAGE_LONG[menuSel], C_TEXT);
    if (PG_STORE[menuSel] >= 0) {
        char hb[80];
        int cq = 0; for (int q = 0; q < NUI_PAGES; q++) if (PG_STORE[q] == pageClipStore) { cq = q; break; }
        if (pageClipStore >= 0) snprintf(hb, sizeof hb, "[X] SEITE KOPIEREN  [Y] EINFÜGEN (%s VON T%d)", PAGE_NAMES[cq], pageClipTrack + 1);
        else snprintf(hb, sizeof hb, "[X] SEITE DER SPUR KOPIEREN  [Y] EINFÜGEN");
        hint(320 - textW(hb) / 2, 368, hb, C_ACC, C_DIM);
    }
}

static void drawConfirm() {
    int w = textW(confirmText) + 48; if (w < 360) w = 360; if (w > 620) w = 620;
    rrect(320 - w / 2, 170, w, 110, 14, C_ACC);
    rrect(322 - w / 2, 172, w - 4, 106, 13, C_BG, 250);
    textC(320, 190, confirmText, C_TEXT);
    rrect(220, 226, 90, 32, 8, C_GREEN); textC(265, 234, "A = JA", C_BG);
    rrect(330, 226, 90, 32, 8, C_OFF2); textC(375, 234, "B = NEIN", C_TEXT);
}

static void drawPageGraph(int gk, int gx, int gy, int gw, int gh, int selRel) {
    Col tc = TCOL[track];
    const Voice& v = eng.v[track];
    switch (gk) {
    case 1: {
        int base = page == PG_AMP ? P_AATK : P_FATK;
        envShapeK = exp2f((pv(page == PG_AMP ? P_ACRV : P_FCRV) - 64) / 32.f);
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
    case 13: arpGraph(gx, gy, gw, gh, tc); break;
    case 8: drumGraph(gx, gy, gw, gh, curP().machine[track], tc); break;
    case 9: wtGraph(gx, gy, gw, gh, pv(P_S0) & 3, pv(P_S1), tc); break;
    case 10: chopGraph(gx, gy, gw, gh, tc); break;
    case 11: granGraph(gx, gy, gw, gh, tc); break;
    case 5:
        sampleGraph(gx, gy, gw, gh, pv(P_S0), pv(P_S3), pv(P_S4), pv(P_S5) != 0, selRel, tc);
        break;
    }
}

static void drawOverlays() {
    char b[32], label[24];
    Col tc = TCOL[track];
    Uint32 now = SDL_GetTicks();
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

static void drawKnobs() {
    char label[24];
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
            drawPageGraph(gk, gx, gy, gw, gh, selRel);
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
    drawOverlays();
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
            bool shown = pul > 0 ? euclidHitUi(s, pul, len, P.p[t][P_EROT]) : st.on == 1;
            Col c = shown ? (pul > 0 ? mix(tc, C_PLAY, 0.15f) : tc) : offCol(s);
            int grow = 0;
            if (ph) { c = st.on == 1 ? mix(tc, C_PLAY, 0.25f + flash[t] * 0.6f) : mix(c, C_PLAY, 0.18f); if (st.on == 1) grow = (int)(flash[t] * 3); }
            int mt = st.lock[P_MTIM] != NO_LOCK ? st.lock[P_MTIM] : P.p[t][P_MTIM];
            int mo = shown ? (mt - 23) * 10 / 23 : 0;   // Mikrotiming sichtbar verschoben
            rrect(x - grow + mo, y + 1 - grow / 2, 31 + 2 * grow, 12 + grow, 3, c);
            if (mo) fill(x + 15, y + 6, 1, 3, C_BG);
            if (st.on == 1 && stepHasLock(st)) fill(x + 25, y + 3, 3, 3, C_BG);
            if (st.on == 2 && !shown) {   // trigless: nur Umriss + Lock-Punkt
                rrect(x, y + 1, 31, 12, 3, tc);
                rrect(x + 2, y + 3, 27, 8, 2, ph ? mix(offCol(s), C_PLAY, 0.18f) : offCol(s));
                fill(x + 14, y + 6, 3, 3, tc);
            }
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
        bool shown = pul > 0 ? euclidHitUi(s, pul, len, P.p[track][P_EROT]) : st.on == 1;
        Col c = shown ? (pul > 0 ? mix(tc, C_PLAY, 0.15f) : tc) : offCol(s);
        int lift = 0;
        if (ph && shown) { c = mix(tc, C_PLAY, 0.2f + flash[track] * 0.6f); lift = (int)(flash[track] * 3); }
        rrect(x, y - lift, 36, 40, 6, c);
        if (st.on == 2 && !shown) {   // trigless: Umriss in Spurfarbe + Lock-Punkt
            rrect(x, y, 36, 40, 6, tc);
            rrect(x + 3, y + 3, 30, 34, 4, offCol(s));
            rrect(x + 27, y + 9, 5, 5, 2, tc);
        }
        if (ph) rrect(x + 6, y + 3 - lift, 24, 4, 2, st.on == 1 ? C_BG : C_PLAY);
        if (st.on == 1 && stepHasLock(st)) rrect(x + 27, y + 9 - lift, 5, 5, 2, C_BG);
        if (st.on == 1 && st.lock[P_NOTE] != NO_LOCK) {
            char nb[8];
            fmtParam(COMMON_DEFS[P_NOTE], quantizeNote(st.lock[P_NOTE], sv), nb, sizeof nb);
            textC(x + 18, y + 9 - lift, nb, dimC(C_BG, 0.85f));
        }
        snprintf(b, sizeof b, "%d", s + 1);
        textC(x + 18, y + 22 - lift, b, st.on == 1 ? C_BG : (st.on == 2 ? tc : C_DIM));
    }
    const char* h;
    if (lockLatch) h = "LOCK [R-ST]WERT [</>]STEP [B]WEG [SEL+A]TRIGLESS [L1/R1]KOPIE/EINF [A]OK";
    else if (aHeld) h = "[X/Y/R-STICK]LOCK [B]LÖSCHEN [L1]KOPIE [R1]EINFÜGEN  LANG = LOCK-MODUS";
    else if (in.down[BT_SELECT]) h = "[</>]PTN [^]REC [v]TAP [A]MUTE [B]UNDO [L1/R1]4 STEPS [L2/R2]LÄNGE";
    else if (page == PG_MIX) h = "[^/v]KANAL [L-STICK]ZEILE/KANAL [R-STICK^v]WERT [A]MUTE [B]SOLO";
    else if (page == PG_PRJ) h = "[L-STICK]WÄHLEN [R-STICK^v]PLATZ [A/X]AUSFÜHREN [SEL]MENÜ";
    else h = "[A]TRIG [B]HÖR [L-STICK]REGLER [R-STICK]WERT [SEL+R-STICK]SEITE";
    hint(6, 452, h, C_ACC, C_DIM);
}

static void drawHelp() {
    rrect(30, 22, 580, 436, 12, C_ACC);
    rrect(32, 24, 576, 432, 11, C_BG);
    static const char* lines[] = {
        "SELECT GEHALTEN +",
        "  LINKS/RECHTS   Pattern wechseln",
        "  HOCH / RUNTER  Aufnahme an/aus / Tap-Tempo",
        "  A / B          Spur stumm / RÜCKGÄNGIG",
        "  X / Y          Engine wechseln",
        "  L1 / R1        Step-Cursor 1 Schlag (4 Steps) zurück/vor",
        "  L2 / R2        Spurlänge -/+",
        "  START          Speichern (2s halten: Ende)",
        "",
        "OHNE SELECT",
        "  A / A lang     Trig / LOCK-MODUS (Stick, X/Y lockt)",
        "  A halten+L1/R1 Trig kopieren / einfügen",
        "  B              Spur vorhören",
        "  HOCH / RUNTER  Spur wählen",
        "  L1 / R1        Block im Signalweg (Quelle..Insert)",
        "  L2 / R2        Reihe SEQ: Trig, Seq, Arp, LFOs",
        "  L-Stick/R-St.  Regler wählen / Wert ändern",
        "  SELECT tippen  Menü: LIVE, Mixer, FX, Song, Projekt",
        "  START          Play / Stop",
    };
    for (unsigned i = 0; i < sizeof lines / sizeof *lines; i++)
        text(48, 36 + i * 22, lines[i], i == 0 || i == 9 ? C_ACC : C_TEXT);
}


// ---------------------------------------------------------------- Oberflaeche B: Signalweg
// Spurseiten als Signalweg (Bloecke) + Modulationsreihe, links alle Spuren, unten die Steps der Spur.
static const int SIG_CHAIN[] = {PG_SRC, PG_SRC2, PG_FLT, PG_FLT2, PG_AMP, PG_INS};
static const int SIG_MOD[] = {PG_TRG, PG_SEQ, PG_ARP, PG_LFO1, PG_LFO2, PG_LFO3};
static const int SIG_NCH = 6, SIG_NMOD = 6;
static const char* sigName(int p) {
    switch (p) {
    case PG_SRC: return "QUELLE"; case PG_SRC2: return "QUELLE 2"; case PG_FLT: return "FILTER 1"; case PG_FLT2: return "FILTER 2";
    case PG_AMP: return "AMP"; case PG_INS: return "INSERT"; case PG_TRG: return "TRIG"; case PG_SEQ: return "SEQ";
    case PG_ARP: return "ARP"; case PG_LFO1: return "LFO1"; case PG_LFO2: return "LFO2"; case PG_LFO3: return "LFO3";
    }
    return PAGE_NAMES[p];
}
static void sigSummary(int p, char* b, int n) {
    const Pattern& P = curP();
    const uint8_t* q = P.p[track];
    char v[24];
    switch (p) {
    case PG_SRC: snprintf(b, n, "%s", MACHINE_NAMES[P.machine[track] % M_COUNT]); break;
    case PG_SRC2: snprintf(b, n, "MEHR"); break;
    case PG_FLT: knobText(COMMON_DEFS[P_FTYP], q[P_FTYP], v, sizeof v); snprintf(b, n, "%s %d", v, q[P_FREQ]); break;
    case PG_FLT2: snprintf(b, n, "%s", q[P_BASE] > 0 || q[P_WDTH] < 127 ? "AN" : "AUS"); break;
    case PG_AMP: snprintf(b, n, "VOL %d", q[P_VOL]); break;
    case PG_INS: snprintf(b, n, "%s", q[P_ITYP] < INS_COUNT ? INS_NAMES[q[P_ITYP]] : "?"); break;
    case PG_TRG: fmtParam(COMMON_DEFS[P_NOTE], q[P_NOTE], v, sizeof v); snprintf(b, n, "%s", v); break;
    case PG_SEQ: snprintf(b, n, "%d STEPS", P.len[track]); break;
    case PG_ARP: snprintf(b, n, "%s", ARP_MODES[q[P_AMOD] % 6]); break;
    default: {
        int k = p - PG_LFO1, dst = q[LFO_BASE[k] + 2];
        if (dst > 0 && q[LFO_BASE[k] + 3] != 64) snprintf(b, n, ">%.5s", paramDef(P.machine[track], dst - 1).name);
        else snprintf(b, n, "-");
    }
    }
}
// Seite in einer Reihe (Signalweg oder Modulation) weiterschalten; ausserhalb der Reihe -> erstes Element
static void sigStep(const int* list, int n, int dir) {
    int cur = -1;
    for (int i = 0; i < n; i++) if (list[i] == page) cur = i;
    if (cur < 0) { for (int i = 0; i < n; i++) if (pageAvail(list[i])) { page = list[i]; return; } return; }
    for (int k = 0; k < n; k++) { cur = (cur + dir + n) % n; if (pageAvail(list[cur])) { page = list[cur]; return; } }
}
// kompakte Kachel: Name, Wert, Balken
static void miniTile(int k, int x, int y, int w, int h, Col tc) {
    char b[32], label[24];
    ParamDef d; int val, baseVal; bool locked;
    bool ok = knobInfo(k, d, val, locked, baseVal, label, sizeof label);
    bool sel = k == slot;
    rrect(x, y, w, h, 6, sel ? C_SEL : C_PANEL);
    if (sel) {
        Col fc = dimC(locked ? C_LOCK : tc, 0.6f + 0.25f * sinf(animTime * 3.f));
        rrect(x, y, w, 2, 1, fc); rrect(x, y + h - 2, w, 2, 1, fc); rrect(x, y, 2, h, 1, fc); rrect(x + w - 2, y, 2, h, 1, fc);
    }
    if (!ok || d.kind == K_NONE) return;
    if (d.kind == K_ACT) { textC(x + w / 2, y + h / 2 - 8, d.name, sel ? C_ACC : C_TEXT); return; }
    char nm[24];
    snprintf(nm, sizeof nm, "%.*s", (w - 12) / FONT_W, label[0] ? label : d.name);
    text(x + 7, y + 3, nm, sel ? (locked ? C_LOCK : tc) : C_DIM);
    knobText(d, val, b, sizeof b, k);
    int sc = textW(b, 2) <= w - 14 ? 2 : 1;
    text(x + w - 7 - textW(b, sc), y + (sc == 2 ? 15 : 21), b, locked ? C_LOCK : C_TEXT, sc);
    knobAnim[k] += (knobFrac(d, val) - knobAnim[k]) * (1.f - expf(-animDt * 14.f));
    int bw = (int)((w - 14) * knobAnim[k]);
    fill(x + 7, y + h - 7, w - 14, 3, C_OFF2);
    if (bw > 0) fill(x + 7, y + h - 7, bw, 3, locked ? C_LOCK : (sel ? tc : dimC(tc, 0.7f)));
}
static void drawSigB() {
    const Pattern& P = curP();
    char b[32];
    Col tc = eng.mute[track] ? dimC(TCOL[track], 0.35f) : TCOL[track];
    // --- links: alle Spuren
    int len0 = P.len[track] < 1 ? 1 : P.len[track];
    int base = (cursor / 16) * 16;
    for (int t = 0; t < NTRACKS; t++) {
        int y = 46 + t * 46, x = 2, w = 110, h = 43;
        bool sel = t == track;
        Col c = eng.mute[t] ? dimC(TCOL[t], 0.35f) : TCOL[t];
        rrect(x, y, w, h, 5, sel ? C_SEL : C_PANEL);
        if (sel) { rrect(x, y, w, 2, 1, c); rrect(x, y + h - 2, w, 2, 1, c); rrect(x, y, 2, h, 1, c); rrect(x + w - 2, y, 2, h, 1, c); }
        drawIcon(P.machine[t] % M_COUNT, x + 5, y + 4, 14, mix(c, C_PLAY, flash[t] * 0.6f));
        snprintf(b, sizeof b, "%d %.6s", t + 1, MACHINE_NAMES[P.machine[t] % M_COUNT]);
        text(x + 22, y + 2, b, eng.mute[t] ? C_DIM : (sel ? c : C_TEXT));
        if (eng.mute[t]) text(x + w - 14, y + 2, "M", C_TRIG);
        else if (eng.solo[t]) text(x + w - 14, y + 2, "S", C_LOCK);
        int len = P.len[t];
        for (int s0 = 0; s0 < 16; s0++) {
            int s = base + s0;
            const Step& st = P.s[t][s];
            bool on = s < len && st.on == 1, ph = eng.playing && eng.playedStep[t] == s;
            Col sc = s >= len ? C_DARK : (on ? c : offCol(s));
            if (ph) sc = mix(sc, C_PLAY, 0.5f);
            fill(x + 6 + s0 * 6, y + 23, 5, 9, sc);
        }
        int mw = clampi((int)(sqrtf(meterV[t]) * (w - 12)), 0, w - 12);
        fill(x + 6, y + 36, w - 12, 2, C_OFF);
        if (mw) fill(x + 6, y + 36, mw, 2, meterV[t] > 0.9f ? C_TRIG : C_GREEN);
    }
    // --- Signalweg
    int X0 = 118, W = 518;
    snprintf(b, sizeof b, "SIGNALWEG  SPUR %d", track + 1);
    text(X0, 44, b, C_DIM);
    text(X0 + W - textW("L1/R1"), 44, "L1/R1", dimC(C_ACC, 0.8f));
    // Spalten: zusammengehoerige Seiten untereinander (QUELLE/QUELLE 2, FILTER 1/FILTER 2)
    static const int COLS[4][2] = {{PG_SRC, PG_SRC2}, {PG_FLT, PG_FLT2}, {PG_AMP, -1}, {PG_INS, -1}};
    int gap = 14, bw = (W - gap * 3) / 4;
    for (int ci = 0; ci < 4; ci++) {
        int x = X0 + ci * (bw + gap), y = 62;
        int n = 0, items[2];
        for (int k = 0; k < 2; k++) if (COLS[ci][k] >= 0 && pageAvail(COLS[ci][k])) items[n++] = COLS[ci][k];
        if (ci) fill(x - gap, y + 20, gap, 2, tc);
        int ih = n == 2 ? 20 : 42;
        for (int k = 0; k < n; k++) {
            int p = items[k], yy = y + k * 22;
            bool sel = p == page;
            rrect(x, yy, bw, ih, 5, sel ? C_SEL : C_PANEL);
            if (sel) { rrect(x, yy, bw, 2, 1, tc); rrect(x, yy + ih - 2, bw, 2, 1, tc); rrect(x, yy, 2, ih, 1, tc); rrect(x + bw - 2, yy, 2, ih, 1, tc); }
            sigSummary(p, b, sizeof b);
            char sb[40];
            if (n == 2) {   // eine Zeile: Name + Kurzinfo
                snprintf(sb, sizeof sb, "%s", sigName(p));
                text(x + 6, yy + 2, sb, sel ? tc : C_TEXT);
                char vb[24]; snprintf(vb, sizeof vb, "%.*s", (bw - 16 - textW(sb)) / FONT_W > 0 ? (bw - 16 - textW(sb)) / FONT_W : 0, b);
                text(x + bw - 6 - textW(vb), yy + 2, vb, C_DIM);
            } else {
                text(x + 6, yy + 3, sigName(p), sel ? tc : C_TEXT);
                snprintf(sb, sizeof sb, "%.*s", (bw - 10) / FONT_W, b);
                text(x + 6, yy + 21, sb, C_DIM);
            }
        }
    }
    // --- Modulation / Sequenz
    text(X0, 108, "SEQ", C_DIM);
    text(X0 + W - textW("L2/R2"), 108, "L2/R2", dimC(C_ACC, 0.8f));
    int x = X0 + 34;
    for (int i = 0; i < SIG_NMOD; i++) {
        int p = SIG_MOD[i];
        if (!pageAvail(p)) continue;
        bool sel = p == page;
        sigSummary(p, b, sizeof b);
        char cb[32]; snprintf(cb, sizeof cb, "%s %s", sigName(p), b);
        int cw = textW(cb) + 10;
        if (x + cw > X0 + W - 46) break;
        rrect(x, 106, cw, 20, 4, sel ? tc : C_PANEL);
        text(x + 5, 108, cb, sel ? C_BG : C_TEXT);
        x += cw + 5;
    }
    // --- Bedienfeld der gewaehlten Seite
    int py = 132, ph = 254;
    rrect(X0, py, W, ph, 6, C_PANEL);
    fill(X0, py, W, 2, tc);
    text(X0 + 8, py + 4, PAGE_LONG[page], tc);
    {   // Erklaerung des gewaehlten Reglers
        ParamDef hd; int hv, hb; bool hl; char hlab[24];
        if (knobInfo(slot, hd, hv, hl, hb, hlab, sizeof hlab) && hd.kind != K_NONE) {
            const char* ht = paramHelp(hd.name);
            if (*ht) {
                char hs[96]; snprintf(hs, sizeof hs, "%s: %s", hd.name, ht);
                int maxw = W - 24 - textW(PAGE_LONG[page]);
                while (textW(hs) > maxw && strlen(hs) > 4) hs[strlen(hs) - 1] = 0;
                while (strlen(hs) && ((unsigned char)hs[strlen(hs) - 1] & 0xC0) == 0xC0) hs[strlen(hs) - 1] = 0;   // kein halbes UTF-8-Zeichen
                text(X0 + W - 8 - textW(hs), py + 4, hs, C_TEXT);
            }
        }
    }
    int gk = graphKind(0), grow = 0;
    if (!gk) { gk = graphKind(1); grow = 1; }
    int ty = py + 24;
    if (gk) {
        int selRel = (slot / 4 == grow) ? slot % 4 : -1;
        drawPageGraph(gk, X0 + 6, ty, W - 12, 92, selRel);
        ty += 98;
    }
    int th = gk ? 62 : 100;
    int tw = (W - 12 - 3 * 6) / 4;
    for (int k = 0; k < 8; k++) miniTile(k, X0 + 6 + (k % 4) * (tw + 6), ty + (k / 4) * (th + 6), tw, th, tc);
    // --- Steps der Spur
    int sy = 392;
    int npg = (len0 + 15) / 16;
    snprintf(b, sizeof b, "%d/%d", cursor / 16 + 1, npg);
    for (int s0 = 0; s0 < 16; s0++) {
        int s = base + s0, sx = X0 + s0 * 32;
        if (s >= len0) { rrect(sx, sy, 30, 40, 4, C_DARK); continue; }
        const Step& st = P.s[track][s];
        bool pl = eng.playing && eng.playedStep[track] == s;
        bool shown = st.on == 1;
        Col c = shown ? tc : offCol(s);
        if (pl) c = mix(c, C_PLAY, shown ? 0.3f + flash[track] * 0.5f : 0.2f);
        rrect(sx, sy, 30, 40, 4, c);
        if (st.on == 2) { rrect(sx, sy, 30, 40, 4, tc); rrect(sx + 2, sy + 2, 26, 36, 3, offCol(s)); }
        if (stepHasLock(st)) rrect(sx + 22, sy + 4, 5, 5, 2, st.on == 1 ? C_BG : C_LOCK);
        char nb[6]; snprintf(nb, sizeof nb, "%d", s + 1);
        textC(sx + 15, sy + 22, nb, shown ? C_BG : C_DIM);
        if (s == cursor) {
            Col cc = aHeld ? C_LOCK : C_TEXT;
            rrect(sx - 2, sy - 3, 34, 2, 1, cc); rrect(sx - 2, sy + 41, 34, 2, 1, cc);
        }
    }
    if (npg > 1) text(X0 - 40, sy + 12, b, C_DIM);
    const char* h;
    if (lockLatch) h = "LOCK [R-ST]WERT [</>]STEP [B]WEG [SEL+A]TRIGLESS [A]OK";
    else if (aHeld) h = "[X/Y/R-STICK]LOCK [B]LÖSCHEN [L1]KOPIE [R1]EINFÜGEN";
    else if (in.down[BT_SELECT]) h = "[</>]PTN [^]REC [v]TAP [A]MUTE [B]UNDO [L1/R1]4 STEPS [L2/R2]LÄNGE";
    else h = "[^v]SPUR [L1/R1]BLOCK [L2/R2]SEQ [L-ST]REGLER [R-ST]WERT [SEL]MENÜ";
    hint(6, 452, h, C_ACC, C_DIM);
}

// ---------------------------------------------------------------- LIVE-Seite: 8 Mutes auf den Tasten (Lage wie am Geraet)
static const int LIVE_BTN[NTRACKS] = {BT_UP, BT_RIGHT, BT_DOWN, BT_LEFT, BT_X, BT_A, BT_B, BT_Y};
static const char* const LIVE_KEY[NTRACKS] = {"^", ">", "v", "<", "X", "A", "B", "Y"};
static void drawLive() {
    const Pattern& P = curP();
    char b[48];
    text(10, 48, "LIVE", C_ACC, 2);
    snprintf(b, sizeof b, "PATTERN %02d", eng.curPat + 1);
    text(120, 50, b, C_TEXT);
    if (eng.nextPat >= 0) { snprintf(b, sizeof b, "> %02d", eng.nextPat + 1); text(120 + textW("PATTERN 00 "), 50, b, (SDL_GetTicks() / 250) % 2 ? C_ACC : C_DIM); }
    snprintf(b, sizeof b, "%d BPM", eng.proj.bpm);
    text(630 - textW(b, 2), 48, b, C_TEXT, 2);
    static const int OX[4] = {0, 1, 0, -1}, OY[4] = {-1, 0, 1, 0};
    for (int t = 0; t < NTRACKS; t++) {
        int cx = t < 4 ? 160 : 480, cy = 232, k = t % 4;
        int w = 100, h = 56, x = cx + OX[k] * 105 - w / 2, y = cy + OY[k] * 64 - h / 2;
        bool m = eng.mute[t];
        Col tc = TCOL[t];
        Col bg = m ? C_DARK : mix(dimC(tc, 0.25f), dimC(tc, 0.7f), flash[t]);
        rrect(x, y, w, h, 8, m ? C_OFF2 : tc);
        rrect(x + 2, y + 2, w - 4, h - 4, 7, bg);
        rrect(x + 6, y + 6, 18, 18, 5, m ? C_OFF2 : tc);
        textC(x + 15, y + 7, LIVE_KEY[t], m ? C_DIM : C_BG);
        drawIcon(P.machine[t] % M_COUNT, x + w - 26, y + 6, 18, m ? C_DIM : C_PLAY);
        snprintf(b, sizeof b, "%d %.6s", t + 1, MACHINE_NAMES[P.machine[t] % M_COUNT]);
        text(x + 8, y + 28, b, m ? C_DIM : C_PLAY);
        if (m) text(x + w - 8 - textW("MUTE"), y + 28, "MUTE", C_TRIG);
        int mw = clampi((int)(sqrtf(meterV[t]) * (w - 16)), 0, w - 16);
        fill(x + 8, y + h - 9, w - 16, 3, C_OFF);
        if (mw && !m) fill(x + 8, y + h - 9, mw, 3, C_GREEN);
    }
    textC(160, 226, "STEUERKREUZ", C_DIM);
    textC(480, 226, "TASTEN", C_DIM);
    // Statusleiste
    int y = 340;
    struct Chip { const char* key; const char* name; bool on; Col c; } ch[4] = {
        {"L1", "FILL (HALTEN)", eng.fill, C_LOCK}, {"R1", recOn ? "REC LÄUFT" : (recSaving ? "SPEICHERT" : "WAV-AUFNAHME"), recOn, C_TRIG},
        {"L2/R2", "PATTERN -/+", eng.nextPat >= 0, C_ACC}, {"START", eng.playing ? "STOP" : "PLAY", eng.playing, C_GREEN}};
    for (int i = 0; i < 4; i++) {
        int x = 6 + i * 158;
        bool blink = ch[i].on && (i != 1 || (SDL_GetTicks() / 400) % 2);
        rrect(x, y, 150, 40, 7, blink ? ch[i].c : C_PANEL);
        text(x + 8, y + 3, ch[i].key, blink ? C_BG : ch[i].c);
        text(x + 8, y + 20, ch[i].name, blink ? C_BG : C_TEXT);
    }
    if (recOn) { snprintf(b, sizeof b, "%d:%02d", recPos / 2 / recRate / 60, recPos / 2 / recRate % 60); text(6 + 158 + 150 - 8 - textW(b), y + 3, b, C_BG); }
    // DJ-Filter (rechter Stick)
    int fy = 392;
    text(6, fy, "R-STICK DJ-FILTER", C_DIM);
    fill(170, fy + 6, 300, 4, C_OFF2);
    fill(319, fy + 2, 2, 12, C_DIM);
    int dx = (int)(eng.dj * 150);
    if (dx) fill(dx > 0 ? 320 : 320 + dx, fy + 6, dx > 0 ? dx : -dx, 4, eng.dj > 0 ? C_CYAN : C_ACC);
    text(480, fy, "L-STICK TEMPO", C_DIM);
    hint(6, 452, "[STEUERKREUZ/XABY]MUTE [L1]FILL [R1]REC [L2/R2]PATTERN [R-ST]DJ [SEL]MENÜ", C_ACC, C_DIM);
}

static void drawSettings();
static void drawSong();
static void drawDrive();
static bool isGrooveboxOS();
static void runBench(const char* outPath, bool onDevice);
static void drawAll(bool help) {
    animate();
    clearScreen(C_BG);
    drawHeader();
    if (page == PG_SYS) drawSettings();
    else if (page == PG_SONG) drawSong();
    else if (page == PG_LIVE) { drawLive(); drawOverlays(); }
    else if (trackPage(page)) { drawSigB(); drawOverlays(); }
    else { drawKnobs(); drawGrid(); drawSteps(); }
    if (help) drawHelp();
    if (menuOpen) drawMenu();
    if (confirmOpen) drawConfirm();
    if (driveOn) drawDrive();
    for (int y = 0; y < SH; y += 3) fill(0, y, SW, 1, C_DARK, 26);   // leichte Scanlines (Röhren-Look)
}

// ---------------------------------------------------------------- Splashscreen SYNTH CITY

static uint32_t splashRnd(uint32_t x) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }

// t = Sekunden seit Start: Sonne, Skyline, Gitter, Regen, Neon-Schriftzug, der aufflackert
static void drawSplash(float t) {
    const int HOR = 300;
    // Himmel: Nachtblau -> Magenta-Dunst am Horizont
    for (int y = 0; y < HOR; y += 2) {
        float f = (float)y / HOR;
        fill(0, y, SW, 2, mix(Col{6, 2, 22}, Col{70, 10, 74}, f * f));
    }
    // Synthwave-Sonne mit Streifen, die nach unten wandern
    int cx = SW / 2, cy = HOR - 40, r = 96;
    for (int y = cy - r; y < HOR; y++) {
        float dy = (float)(y - cy);
        if (dy * dy > r * r) continue;
        int hw = (int)sqrtf(r * r - dy * dy);
        float f = (float)(y - (cy - r)) / (2 * r);
        if (y > cy - 20) {   // Streifenluecken
            float band = fmodf((y - (cy - 20)) + t * 14.f, 18.f);
            float gap = 1.f + (y - (cy - 20)) * 0.07f;
            if (band < gap) continue;
        }
        fill(cx - hw, y, 2 * hw, 1, mix(Col{255, 214, 70}, Col{255, 40, 150}, f * 1.4f));
    }
    // Skyline vor der Sonne
    uint32_t sd = 1234567;
    for (int x = 0; x < SW;) {
        sd = splashRnd(sd);
        int w = 18 + sd % 34, h = 30 + (sd >> 8) % 130;
        if (x + w > 214 && x < 426) h = 14 + h / 5;   // in der Mitte niedriger, die Sonne bleibt sichtbar
        Col bc = {14 + (int)(sd >> 20) % 8, 6, 30};
        fill(x, HOR - h, w, h, bc);
        if ((sd >> 3) % 4 == 0) fill(x + w / 2, HOR - h - 14, 2, 14, bc);   // Antenne
        if ((sd >> 3) % 4 == 0 && fmodf(t * 1.3f + x, 2.f) < 1.f) fill(x + w / 2 - 1, HOR - h - 16, 4, 3, C_ACC);
        for (int wy = HOR - h + 6; wy < HOR - 6; wy += 7)   // Fenster
            for (int wx = x + 3; wx < x + w - 4; wx += 6) {
                uint32_t q = splashRnd((uint32_t)(wx * 73 + wy * 151) ^ sd);
                if (q % 5 == 0) {
                    bool flick = (q >> 9) % 23 == 0 && fmodf(t * 3.f + (q % 7), 1.f) < 0.5f;
                    if (!flick) fill(wx, wy, 2, 2, (q >> 4) % 3 ? Col{0, 200, 230} : Col{255, 176, 40}, 200);
                }
            }
        x += w + 2;
    }
    // Boden mit Perspektiv-Gitter, das auf uns zufaehrt
    fill(0, HOR, SW, SH - HOR, Col{8, 2, 20});
    fill(0, HOR, SW, 2, Col{255, 60, 170});
    float sc = fmodf(t * 0.9f, 1.f);
    for (int i = 0; i < 14; i++) {
        float z = (i + 1 - sc);
        int y = HOR + (int)(220.f / z) - 14;
        if (y <= HOR || y >= SH) continue;
        fill(0, y, SW, 1, C_ACC, (Uint8)(40 + 160 * (float)(y - HOR) / (SH - HOR)));
    }
    for (int k = -12; k <= 12; k++) {
        int xb = SW / 2 + k * 70;
        setCol(C_ACC, 140);
        drawLine(SW / 2 + k * 6, HOR, xb, SH);
    }
    // Regen
    for (int i = 0; i < 140; i++) {
        uint32_t q = splashRnd(i * 2654435761u + 7);
        float sp = 380.f + (q % 200);
        int x = (int)((q >> 8) % (SW + 120)) - 60 + (int)(fmodf(t * sp * 0.25f, 1.f));
        int y = (int)fmodf((q >> 3) % SH + t * sp, (float)SH + 40.f) - 20;
        int len = 8 + q % 14;
        setCol((q >> 5) % 4 ? C_CYAN : C_PLAY, (Uint8)(50 + (q >> 12) % 90));
        drawLine(x, y, x - len / 4, y + len);
    }
    // Neon-Schriftzug: Buchstaben zuenden nacheinander mit Flackern
    const char* logo = "SYNTH CITY";
    int n = (int)strlen(logo), scl = 6, lw = n * FONT_W * scl, lx = (SW - lw) / 2, ly = 40;
    for (int i = 0; i < n; i++) {
        float on = t * 6.f - i * 0.9f;   // Zuendzeitpunkt je Buchstabe
        if (on < 0) continue;
        uint32_t q = splashRnd((uint32_t)(i * 977 + (int)(t * 18)));
        bool flick = on < 2.5f && q % 3 == 0;
        if (flick) continue;
        char ch[2] = {logo[i], 0};
        int x = lx + i * FONT_W * scl;
        for (int g = 3; g >= 1; g--) {   // Glow
            Uint8 a = (Uint8)(28 / g);
            text(x - g * 2, ly, ch, C_ACC, scl); text(x + g * 2, ly, ch, C_CYAN, scl);
            text(x, ly - g * 2, ch, C_ACC, scl); text(x, ly + g * 2, ch, C_CYAN, scl);
            (void)a;
        }
        text(x - 3, ly, ch, C_ACC, scl);       // Farbversatz
        text(x + 3, ly, ch, C_CYAN, scl);
        text(x, ly, ch, Col{255, 236, 250}, scl);
    }
    if (t > 1.6f) {
        float a = fminf((t - 1.6f) * 2.f, 1.f);
        textAlpha = (Uint8)(255 * a);
        textC(SW / 2, ly + 104, "R 3 6 S   G R O O V E B O X", C_CYAN);
        textAlpha = 255;
    }
    if (t > 2.2f && fmodf(t, 1.f) < 0.6f) textC(SW / 2, SH - 34, "PRESS ANY BUTTON", C_LOCK);
    for (int y = 0; y < SH; y += 3) fill(0, y, SW, 1, Col{0, 0, 0}, 60);   // Scanlines
}

static bool splashSkip() {
    SDL_Event e;
    bool hit = false;
    while (SDL_PollEvent(&e))
        if (e.type == SDL_KEYDOWN || e.type == SDL_JOYBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONDOWN) hit = true;
    return hit;
}

static void runSplash() {
    Uint32 t0 = SDL_GetTicks();
    for (;;) {
        float t = (SDL_GetTicks() - t0) / 1000.f;
        if (t > 4.5f) break;
        clearScreen(C_BG);
        drawSplash(t);
        present();
        if (splashSkip() && t > 0.4f) break;
        SDL_Delay(16);
    }
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
    if (!trackPage(page) || sp() < 0) return;   // globale Seite ohne eigene Werte (z.B. SONG): nichts aendern
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

enum { ACT_LOAD = 1, ACT_SAVE, ACT_NEW, ACT_PSAVE, ACT_INIT, ACT_PASTE, ACT_CLEARPAT, ACT_QUIT, ACT_CLEARTRK, ACT_DEMO };

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
    uint8_t hdr[8] = {'G', 'B', 'P', '1', (uint8_t)m, (uint8_t)NPARAMS, 2, 0};   // [6]: 1 Filtertypen FT_*, 2 Huellkurvenform
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
    if (hdr[6] == 0 && (hdr[4] == M_SYNTH || hdr[4] == M_WTBL) && P.p[t][P_FTYP] == FT_LP12) P.p[t][P_FTYP] = FT_LP24;   // alte Presets
    if (hdr[6] < 2) { P.p[t][P_FCRV] = 64; P.p[t][P_ACRV] = 64; }
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
    if (j < 0) { toast("ALLE 32 PLÄTZE BELEGT"); return; }
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
    case ACT_DEMO: {
        int j = -1;
        for (int i = 0; i < 32 && j < 0; i++) if (!songExists(i)) j = i;
        if (j < 0) { toast("ALLE 32 PLÄTZE BELEGT"); break; }
        lockA(); eng.stop(); buildDemo(eng); eng.songOn = true; eng.songStart(0); unlockA();
        prjSel = j; doSaveSong(j);
        toast("DEMO 'NEON NIGHTS' ALS SONG %02d - START DRÜCKEN", j + 1);
    } break;
    case ACT_PSAVE: toast(savePreset(track, arg) ? "PRESET %02d GESPEICHERT" : "PRESET-FEHLER", arg + 1); break;
    case ACT_PASTE: lockA(); curP() = clip; unlockA(); toast("PATTERN EINGEFÜGT"); break;
    case ACT_CLEARTRK:
        lockA();
        for (int st = 0; st < NSTEPS; st++) { curP().s[arg][st].on = 0; memset(curP().s[arg][st].lock, NO_LOCK, NPARAMS); }
        unlockA();
        toast("SPUR %d GELEERT (SELECT+B = RÜCKGÄNGIG)", arg + 1);
        break;
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
        toast("SPUR %d: KLANG ZURÜCKGESETZT", track + 1);
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
        if (songExists(prjSel) && cfg.song != prjSel) askConfirm(ACT_SAVE, prjSel, "SONG %02d ÜBERSCHREIBEN?", prjSel + 1);
        else doSaveSong(prjSel);
        break;
    case 3: askConfirm(ACT_NEW, 0, "NEUES LEERES PROJEKT ANLEGEN?"); break;
    case 5: {
        if (presetSel >= 32) {   // Werksklang (setzt auch die Engine)
            lockA(); applyFactory(curP(), track, presetSel - 32); unlockA();
            toast("WERKSKLANG: %s", FACTORY[presetSel - 32].name);
            break;
        }
        int r = loadPreset(track, m, presetSel);
        if (r > 0) toast("%s %02d GELADEN", MACHINE_NAMES[m], presetSel + 1);
        else toast(r == 0 ? "PRESET %02d IST LEER" : "PRESET %02d DEFEKT", presetSel + 1);
    } break;
    case 6:
        if (presetSel >= 32) { toast("WERKSKLÄNGE SIND SCHREIBGESCHÜTZT - PLATZ 1-32 WÄHLEN"); break; }
        if (presetExists(m, presetSel)) askConfirm(ACT_PSAVE, presetSel, "%s-PRESET %02d ÜBERSCHREIBEN?", MACHINE_NAMES[m], presetSel + 1);
        else runAction(ACT_PSAVE, presetSel);
        break;
    case 7: askConfirm(ACT_INIT, 0, "KLANG VON SPUR %d ZURÜCKSETZEN?", track + 1); break;
    }
}
static bool prjIsAction(int k) { return k == 1 || k == 2 || k == 3 || k >= 5; }

// Stick-"Antippen": ein Schritt pro Auslenkung, beim Halten wiederholt
static int flLX = 0, flLY = 0, flRX = 0; static Uint32 flLXt = 0, flLYt = 0, flRXt = 0;
static int flick(float v, int& state, Uint32& t, Uint32 now) {
    if (fabsf(v) < 0.35f) { state = 0; return 0; }
    if (fabsf(v) < 0.65f) return 0;
    int d = v > 0 ? 1 : -1;
    if (state != d) { state = d; t = now + 320; return d; }
    if (now >= t) { t = now + 110; return d; }
    return 0;
}

// ---------------------------------------------------------------- Einstellungen (Liste, Seite SET)

enum SetItem {
    SI_H_PAT, SI_PATTERN, SI_LEN, SI_LENALL, SI_SPEED, SI_FILL, SI_BPM, SI_SWING, SI_SCALE, SI_ROOT, SI_COPY, SI_PASTE, SI_CLEAR, SI_CLRTRK, SI_TMPL, SI_MUTATE, SI_UNDO, SI_REDO, SI_DEMO,
    SI_H_AUDIO, SI_OUT, SI_BUF, SI_VOL, SI_CORES,
    SI_H_MIDI, SI_MIN, SI_NOUT, SI_CLK, SI_MCH,
    SI_H_USB, SI_USB, SI_USBAUDIO, SI_USBVOL, SI_INTUSB, SI_USBTONE, SI_PCIN, SI_PCREC, SI_IP, SI_WEB, SI_DRIVE,
    SI_H_SYS, SI_DISP, SI_LEARN, SI_BENCH, SI_INFO, SI_QUIT, SI_COUNT
};
static int setSel = SI_PATTERN, prevPage = PG_TRG;
// Rhythmus-Vorlagen (Bit i = Step i+1 von 16), werden ueber die ganze Spurlaenge wiederholt
static const char* const RHY_NAMES[] = {"Four on the Floor", "Backbeat (2+4)", "Offbeat", "Achtel", "Sechzehntel", "Breakbeat-Kick",
                                        "Clave 3-2", "Tresillo", "Dembow-Snare", "Leer"};
static const uint16_t RHY_MASK[] = {0x1111, 0x1010, 0x4444, 0x5555, 0xFFFF, 0x0441, 0x1449, 0x4949, 0x4848, 0x0000};
enum { RHY_N = sizeof RHY_MASK / sizeof *RHY_MASK };
static int tmplSel = 0;
static void applyRhythm(int k) {
    lockA();
    Pattern& P = curP();
    int len = P.len[track] < 1 ? 1 : P.len[track];
    for (int st = 0; st < len; st++) {
        bool on = (RHY_MASK[k] >> (st % 16)) & 1;
        P.s[track][st].on = on ? 1 : 0;
        if (!on) memset(P.s[track][st].lock, NO_LOCK, NPARAMS);
    }
    unlockA();
    toast("SPUR %d: %s (SELECT+B = RÜCKGÄNGIG)", track + 1, RHY_NAMES[k]);
}
static float setScroll = 0;
static bool setIsHeader(int i) { return i == SI_H_PAT || i == SI_H_AUDIO || i == SI_H_MIDI || i == SI_H_USB || i == SI_H_SYS; }
static bool setIsAction(int i) { return i == SI_DEMO || i == SI_UNDO || i == SI_REDO || i == SI_CLRTRK || i == SI_BENCH || i == SI_PCREC || i == SI_USBTONE || i == SI_DRIVE || i == SI_COPY || i == SI_PASTE || i == SI_CLEAR || i == SI_LEARN || i == SI_QUIT; }
static bool runLearn();
static char versionStr[32] = "";

static void setLabel(int i, char* l, int ln, char* v, int vn) {
    const Pattern& P = curP();
    v[0] = 0;
    switch (i) {
    case SI_H_PAT: snprintf(l, ln, "PATTERN"); break;
    case SI_PATTERN: snprintf(l, ln, "Pattern"); snprintf(v, vn, "P%02d%s", eng.curPat + 1, eng.nextPat >= 0 ? " >" : ""); break;
    case SI_LEN: snprintf(l, ln, "Länge Spur %d", track + 1); snprintf(v, vn, "%d Steps", P.len[track]); break;
    case SI_LENALL: snprintf(l, ln, "Länge alle Spuren"); snprintf(v, vn, "%d Steps", P.len[track]); break;
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
    case SI_MUTATE: { static const char* const AM[3] = {"LEICHT", "MITTEL", "STARK"};
        snprintf(l, ln, "Pattern mutieren (A = los)"); snprintf(v, vn, "%s", AM[mutAmt % 3]); } break;
    case SI_PASTE: snprintf(l, ln, "Pattern einfügen"); snprintf(v, vn, "%s", haveClip ? "" : "(leer)"); break;
    case SI_CLEAR: snprintf(l, ln, "Pattern leeren"); break;
    case SI_CLRTRK: snprintf(l, ln, "Spur %d leeren", track + 1); break;
    case SI_TMPL: snprintf(l, ln, "Rhythmus-Vorlage für Spur %d", track + 1); snprintf(v, vn, "< %s >", RHY_NAMES[tmplSel]); break;
    case SI_UNDO: snprintf(l, ln, "Rückgängig (SELECT + B)"); snprintf(v, vn, "%d Schritte", undoCnt); break;
    case SI_DEMO: snprintf(l, ln, "Demo-Song 'Neon Nights' laden"); snprintf(v, vn, ">>"); break;
    case SI_REDO: snprintf(l, ln, "Wiederholen"); snprintf(v, vn, "%s", redoPat >= 0 ? ">>" : "(nichts)"); break;
    case SI_H_AUDIO: snprintf(l, ln, "AUDIO"); break;
    case SI_OUT: snprintf(l, ln, "Ausgang"); { int d = devIndex(); snprintf(v, vn, "%.26s", d ? devNames[d - 1] : "Standard (intern)"); } break;
    case SI_BUF: snprintf(l, ln, "Puffer");
        if (bufOverride) snprintf(v, vn, "%d (Startoption)", bufOverride);
        else snprintf(v, vn, "%d  (%.1f ms)", BUF_SIZES[cfg.buf], BUF_SIZES[cfg.buf] * 1000.f / eng.sr);
        break;
    case SI_VOL: snprintf(l, ln, "Lautstärke"); snprintf(v, vn, "%d", eng.proj.mst[MS_VOL]); break;
    case SI_CORES: snprintf(l, ln, "Audio-Rechenkerne"); snprintf(v, vn, "%s", eng.audioThreads > 1 ? "2 (schneller)" : (cfg.cores > 1 ? "1 (nur ein Kern verfügbar)" : "1")); break;
    case SI_H_MIDI: snprintf(l, ln, "MIDI"); break;
    case SI_MIN: snprintf(l, ln, "MIDI-Eingang"); snprintf(v, vn, "%s", cfg.midiIn ? "AN" : "AUS"); break;
    case SI_NOUT: snprintf(l, ln, "Noten ausgeben"); snprintf(v, vn, "%s", cfg.notesOut ? "AN (Spur = Kanal)" : "AUS"); break;
    case SI_CLK: snprintf(l, ln, "Clock"); snprintf(v, vn, "%s", cfg.clk == 1 ? "SENDEN" : (cfg.clk == 2 ? "EMPFANGEN" : "AUS")); break;
    case SI_MCH: snprintf(l, ln, "Eingangskanäle"); snprintf(v, vn, "%s", cfg.chm ? "Kanal 1-8 = Spur 1-8" : "AUTO (gewählte Spur)"); break;
    case SI_H_USB: snprintf(l, ln, "USB"); break;
    case SI_USB: snprintf(l, ln, "USB-Modus");
        snprintf(v, vn, "%s", usbMode == 0 ? "HOST (Geräte am R36S)" : (usbMode == 1 ? "PC (Netz-MIDI, SSH)" : "PC + AUDIO"));
        break;
    case SI_USBAUDIO: snprintf(l, ln, "Audio an PC (USB)");
        if (!cfg.usbAudio) snprintf(v, vn, "AUS");
        else if (usbMode != 2) snprintf(v, vn, "AN (erst PC MIDI+Audio)");
        else snprintf(v, vn, usbAudioStatus() == 2 ? "AN - PC LIEST MIT" : (usbAudioActive() ? "AN - WARTET AUF PC" : "AN - SUCHE GERÄT"));
        break;
    case SI_USBVOL: snprintf(l, ln, "USB-Lautstärke"); snprintf(v, vn, "%d%%", cfg.usbVol); break;
    case SI_INTUSB: snprintf(l, ln, "Intern bei USB-Audio"); snprintf(v, vn, "%s", cfg.intUsb ? (intMuted ? "STUMM (AKTIV)" : "STUMM") : "AN"); break;
    case SI_USBTONE: snprintf(l, ln, "Testton an PC (5 s, 1 kHz)"); if (!usbAudioActive()) snprintf(v, vn, "erst Audio an PC"); break;
    case SI_PCIN: snprintf(l, ln, "PC-Ton über R36S");
        if (!cfg.pcIn) snprintf(v, vn, "AUS");
        else if (usbMode != 2) snprintf(v, vn, "AN (erst PC MIDI+Audio)");
        else snprintf(v, vn, pcInStatus() == 2 ? "AN - PC SPIELT" : "AN - STILL");
        break;
    case SI_PCREC: snprintf(l, ln, "PC-Ton als Sample aufnehmen");
        if (pcRecState() == 1) snprintf(v, vn, "WARTET AUF TON (A = ABBRECHEN)");
        else if (pcRecState() == 2) snprintf(v, vn, "AUFNAHME %.1f s (A = STOP)", pcRecSeconds());
        else if (usbMode != 2) snprintf(v, vn, "erst PC MIDI+Audio");
        break;
    case SI_IP: snprintf(l, ln, "Adresse"); snprintf(v, vn, "%s", usbMode && netIp[0] ? netIp : "-"); break;
    case SI_WEB: snprintf(l, ln, "Samples per Browser");
        if (!webPort()) snprintf(v, vn, "-");
        else if (usbMode && netIp[0]) snprintf(v, vn, webPort() == 80 ? "http://%s" : "http://%s:%d", netIp, webPort());
        else snprintf(v, vn, "erst PC-Modus an");
        break;
    case SI_H_SYS: snprintf(l, ln, "SYSTEM"); break;
    case SI_DISP: snprintf(l, ln, "Bild-Versatz (Lage + Farben)");
        if (drmOutActive()) snprintf(v, vn, "%d Px (%d)", cfg.dispOff / 3, cfg.dispOff);
        else snprintf(v, vn, "nur Groovebox-OS");
        break;
    case SI_LEARN: snprintf(l, ln, "Tasten neu lernen"); break;
    case SI_BENCH: snprintf(l, ln, "Leistungstest (ca. 30 s)"); snprintf(v, vn, "A = START"); break;
    case SI_INFO: snprintf(l, ln, "Info");
        snprintf(v, vn, "%.12s  %d Smp  Aussetzer %d", versionStr[0] ? versionStr : "dev", sampleCount(), audLateTotal + audLate);
        break;
    case SI_QUIT: snprintf(l, ln, "Groovebox beenden"); break;
    case SI_DRIVE: snprintf(l, ln, "USB-Laufwerk (Samples vom PC)"); if (!isGrooveboxOS()) snprintf(v, vn, "nur Groovebox-OS"); break;
    }
}

static void setChange(int i, int d) {
    Pattern& P = curP();
    switch (i) {
    case SI_TMPL: tmplSel = ((tmplSel + d) % RHY_N + RHY_N) % RHY_N; break;
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
    case SI_CORES: cfg.cores = cfg.cores > 1 ? 1 : 2; lockA(); eng.audioThreads = eng.helperReady() && cfg.cores > 1 ? 2 : 1; unlockA(); saveSettings(); break;
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
    case SI_USBAUDIO: cfg.usbAudio = !cfg.usbAudio; saveSettings(); break;
    case SI_USBVOL: cfg.usbVol = clampi(cfg.usbVol + (d > 0 ? 5 : -5), 0, 100); saveSettings(); break;
    case SI_INTUSB: cfg.intUsb = !cfg.intUsb; saveSettings(); break;
    case SI_MUTATE: mutAmt = clampi(mutAmt + (d > 0 ? 1 : -1), 0, 2); break;
    case SI_PCIN: cfg.pcIn = !cfg.pcIn; saveSettings(); break;
    case SI_DISP: cfg.dispOff = clampi(cfg.dispOff + (d > 0 ? 3 : -3), -1920, 1920);   // 1 Pixel je Schritt, Farbanteil bleibt drmOutSetByteOffset(cfg.dispOff); saveSettings(); break;
    default: break;
    }
}

// ---------------------------------------------------------------- Pattern-Mutation
static uint32_t mutRnd = 0x1234567u;
static float mrand() { mutRnd ^= mutRnd << 13; mutRnd ^= mutRnd >> 17; mutRnd ^= mutRnd << 5; return (mutRnd >> 8) * (1.f / 16777216.f); }
// Variation des aktuellen Patterns: Trigs dazu/weg, Velocity, Toene in der Skala, gelegentliche Ratchets/Mikrotiming
static void mutatePattern() {
    static const float AMT[3] = {0.08f, 0.16f, 0.30f};
    float a = AMT[mutAmt % 3];
    mutRnd ^= SDL_GetTicks() * 2654435761u;
    lockA();
    Pattern& P = curP();
    mutUndo = P; haveMutUndo = true; mutPat = eng.curPat;
    int sv = curScale();
    for (int t = 0; t < NTRACKS; t++) {
        int len = P.len[t] < 1 ? 1 : P.len[t];
        bool drum = machineIsDrum(P.machine[t]);
        int trigs = 0; for (int s = 0; s < len; s++) trigs += P.s[t][s].on == 1;
        if (!trigs) continue;   // leere Spuren bleiben leer
        for (int s = 0; s < len; s++) {
            Step& st = P.s[t][s];
            if (st.on == 2) continue;   // trigless Locks bleiben unangetastet
            if (st.on == 1) {
                if (s != 0 && mrand() < a * 0.5f) { st.on = stepHasLock(st) ? 2 : 0; continue; }   // Trig weg (Taktanfang bleibt)
                if (mrand() < a * 2.f) {   // Velocity variieren
                    int vv = st.lock[P_VEL] != NO_LOCK ? st.lock[P_VEL] : P.p[t][P_VEL];
                    st.lock[P_VEL] = (uint8_t)clampi(vv + (int)((mrand() - 0.5f) * 80.f * a * 3.f), 20, 127);
                }
                if (!drum && mrand() < a * 1.5f) {   // Ton in der Skala verschieben
                    int n0 = st.lock[P_NOTE] != NO_LOCK ? st.lock[P_NOTE] : P.p[t][P_NOTE];
                    int steps = 1 + (int)(mrand() * 2.f), dir = mrand() < 0.5f ? -1 : 1, n = n0;
                    for (int k = 0; k < steps; k++) n = sv ? stepScale(n, dir, sv) : n + dir * 2;
                    st.lock[P_NOTE] = (uint8_t)clampi(n, 0, 127);
                }
                if (mrand() < a * 0.25f) st.lock[P_RTRG] = (uint8_t)(1 + (int)(mrand() * 2.f));   // Ratchet
                if (mrand() < a * 0.3f) st.lock[P_MTIM] = (uint8_t)clampi(23 + (int)((mrand() - 0.5f) * 12.f), 0, 46);   // Mikrotiming
            } else if (mrand() < a * 0.35f) {   // neuer Trig, Ton vom naechsten Trig davor
                st.on = 1;
                memset(st.lock, NO_LOCK, NPARAMS);
                st.lock[P_VEL] = (uint8_t)(50 + (int)(mrand() * 60.f));
                if (!drum) for (int k = 1; k < len; k++) {
                    const Step& q = P.s[t][(s - k + len) % len];
                    if (q.on == 1 && q.lock[P_NOTE] != NO_LOCK) { st.lock[P_NOTE] = q.lock[P_NOTE]; break; }
                }
            }
        }
    }
    unlockA();
    static const char* const AM[3] = {"LEICHT", "MITTEL", "STARK"};
    toast("PATTERN %02d MUTIERT (%s)", eng.curPat + 1, AM[mutAmt % 3]);
}

static bool isGrooveboxOS() { struct stat st; return stat("/etc/groovebox-os", &st) == 0; }
static void reloadSamples();
static bool usbScript(const char* args, char* okMsg, int n) {
    if (selfTesting) return false;   // pc-modus.sh ausfuehren, OK:/FEHLER:-Zeile liefern
    char cmd[800], lp[700], line[200];
    snprintf(cmd, sizeof cmd, "bash '%spc-modus.sh' %s", basePath, args);
    int rc = system(cmd);
    snprintf(lp, sizeof lp, "%spc-modus.log", basePath);
    bool ok = false;
    snprintf(okMsg, n, rc == 0 ? "FERTIG" : "FEHLER (PC-MODUS.LOG)");
    FILE* f = fopen(lp, "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "OK:", 3)) { ok = true; snprintf(okMsg, n, "%.60s", line + 4); }
            if (!strncmp(line, "FEHLER:", 7)) { ok = false; snprintf(okMsg, n, "%.60s", line); }
        }
        fclose(f);
    }
    return ok;
}
// Datenpartition als USB-Laufwerk an den PC geben (nur Groovebox-OS)
static void openAudio();
static char driveSavedDev[128] = "";
static bool isUsbAudioDev(const char* n) { return strcasestr(n, "uac") || strcasestr(n, "gadget"); }
static void startDrive() {
    if (selfTesting) return;
    if (!isGrooveboxOS()) { toast("NUR IM GROOVEBOX-OS"); return; }
    saveProject();
    toast("USB-LAUFWERK WIRD VERBUNDEN ...");
    drawAll(false); present();
    char m[200];
    driveSavedDev[0] = 0;
    usbAudioStop();
    if (usbMode) {   // aus dem PC-Modus: erst USB-Audio freigeben und das Geraet sauber abmelden
        if (isUsbAudioDev(cfg.dev)) { snprintf(driveSavedDev, sizeof driveSavedDev, "%s", cfg.dev); cfg.dev[0] = 0; openAudio(); }
        usbScript("aus nosleep", m, sizeof m);
        SDL_Delay(500);
    }
    if (usbScript("laufwerk nosleep", m, sizeof m)) { driveOn = true; usbMode = 1; }
    toast("%s", m);
    msgUntil = SDL_GetTicks() + 3000;
}
static void stopDrive() {
    char m[200];
    toast("LAUFWERK WIRD GETRENNT ...");
    drawAll(false); present();
    usbScript("laufwerk-aus nosleep", m, sizeof m);
    driveOn = false; usbMode = 2;
    if (driveSavedDev[0]) {   // USB-Audio als Ausgang wiederherstellen, sobald das Geraet wieder da ist
        snprintf(cfg.dev, sizeof cfg.dev, "%s", driveSavedDev); driveSavedDev[0] = 0;
        audioApplyAt = SDL_GetTicks() + 2500;
    }
    {   // Update-Datei ueber das Laufwerk kopiert? -> neu starten, groovebox-run installiert
        char ud[700]; snprintf(ud, sizeof ud, "%supdate", basePath);
        DIR* d = opendir(ud); struct dirent* e; bool upd = false;
        while (d && (e = readdir(d))) { size_t l = strlen(e->d_name); if (l > 4 && !strcmp(e->d_name + l - 4, ".tgz")) upd = true; }
        if (d) closedir(d);
        if (upd) { toast("UPDATE GEFUNDEN - NEUSTART ..."); msgUntil = SDL_GetTicks() + 5000; quitReq = true; }
    }
    reloadSamples();
    toast("%d SAMPLES GELADEN", sampleCount());
}
static void drawDrive() {
    rrect(60, 120, 520, 220, 16, C_ACC);
    rrect(62, 122, 516, 216, 15, C_BG, 250);
    textC(320, 142, "USB-LAUFWERK AKTIV", C_ACC);
    textC(320, 182, "Am PC erscheint das Laufwerk GROOVEBOX.", C_TEXT);
    textC(320, 204, "Samples in den Ordner samples/ kopieren,", C_TEXT);
    textC(320, 226, "dann das Laufwerk am PC AUSWERFEN.", C_TEXT);
    rrect(200, 270, 240, 36, 9, C_GREEN); textC(320, 280, "A = FERTIG, ZURÜCK", C_BG);
}

static void setAction(int i) {
    switch (i) {
    case SI_COPY: clip = curP(); haveClip = true; toast("PATTERN %02d KOPIERT", eng.curPat + 1); break;
    case SI_PASTE:
        if (haveClip) askConfirm(ACT_PASTE, 0, "PATTERN %02d MIT KOPIE ÜBERSCHREIBEN?", eng.curPat + 1);
        else toast("ZWISCHENABLAGE LEER");
        break;
    case SI_CLEAR: askConfirm(ACT_CLEARPAT, 0, "ALLE STEPS VON PATTERN %02d LÖSCHEN?", eng.curPat + 1); break;
    case SI_CLRTRK: askConfirm(ACT_CLEARTRK, track, "ALLE STEPS VON SPUR %d LÖSCHEN?", track + 1); break;
    case SI_TMPL: applyRhythm(tmplSel); break;
    case SI_UNDO: undoStep(); break;
    case SI_REDO: redoStep(); break;
    case SI_DEMO: askConfirm(ACT_DEMO, 0, "DEMO-SONG IN EINEM FREIEN PLATZ ANLEGEN?"); break;
    case SI_LEARN: runLearn(); break;
    case SI_BENCH: {   // Messung auf dem Geraet, Bericht in logs/bench.txt (Browser: Logs)
        char bp[700];
        if (isGrooveboxOS()) { mkdir("/data/logs", 0755); snprintf(bp, sizeof bp, "/data/logs/bench.txt"); }
        else snprintf(bp, sizeof bp, "%sbench.txt", basePath);
        clearScreen(C_BG); textC(320, 220, "LEISTUNGSTEST LÄUFT ...", C_ACC, 2); present();
        runBench(bp, true);
        toast("LEISTUNGSTEST FERTIG: LOGS/BENCH.TXT");
    } break;
    case SI_MUTATE: mutatePattern(); break;
    case SI_DRIVE: startDrive(); break;
    case SI_PCREC:
        if (pcRecState() == 1 || pcRecState() == 2) { pcRecStop(); toast(pcRecState() ? "AUFNAHME BEENDET" : "ABGEBROCHEN"); }
        else if (usbMode != 2) toast("ERST USB = PC (MIDI + AUDIO)");
        else if (pcRecStart()) toast("AM PC ABSPIELEN - AUFNAHME STARTET MIT DEM TON");
        break;
    case SI_USBTONE: if (usbAudioActive()) { usbAudioTestTone(5000); toast("TESTTON 5 S AN DEN PC"); } else toast("ERST USB = PC (MIDI + AUDIO)"); break;
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
    if (fabsf(ry) > 0.25f && !setIsAction(setSel) && (setSel == SI_BPM || setSel == SI_VOL || setSel == SI_DISP || setSel == SI_USBVOL || setSel == SI_LEN || setSel == SI_LENALL)) {
        stickAcc += (ry > 0 ? 1.f : -1.f) * (30.f * (fabsf(ry) - 0.25f) + 2.f) * dt;
        int d = (int)stickAcc;
        if (d) { stickAcc -= d; setChange(setSel, d); }
    } else if (fabsf(ry) <= 0.25f) stickAcc = 0;
    if (I.pressed(BT_A)) setAction(setSel);
    if (I.pressed(BT_B)) page = prevPage == PG_SYS ? PG_TRG : prevPage;
    if (I.pressed(BT_START)) { lockA(); if (eng.playing) eng.stop(); else eng.start(); unlockA(); }
}

// ---------------------------------------------------------------- Song-Seite (SNG): Pattern-Kette
static int songSel = 0;
static float songScroll = 0;
static void drawSong() {
    Project& J = eng.proj;
    rrect(4, 46, 632, 398, 10, C_PANEL);
    char b[64];
    text(18, 54, "SONG", C_ACC, 2);
    snprintf(b, sizeof b, "%d EINTRÄGE", J.songLen);
    text(100, 62, b, C_DIM);
    bool on = eng.songOn;
    rrect(440, 54, 186, 28, 8, on ? C_ACC : C_OFF2);
    textC(533, 60, on ? "SONG-MODUS AN" : "SONG-MODUS AUS", on ? C_BG : C_TEXT);
    const int rowH = 30, top = 94, visible = 10;
    if (songSel >= J.songLen) songSel = J.songLen > 0 ? J.songLen - 1 : 0;
    float target = songSel - visible / 2.f; if (target < 0) target = 0;
    if (target > J.songLen - visible) target = J.songLen > visible ? J.songLen - visible : 0;
    songScroll += (target - songScroll) * (1.f - expf(-animDt * 14.f));
    if (J.songLen == 0) { textC(320, 200, "NOCH LEER - A FÜGT DAS AKTUELLE PATTERN EIN", C_DIM); }
    for (int i = 0; i < J.songLen; i++) {
        float yf = top + (i - songScroll) * rowH;
        if (yf < top - 4 || yf > top + (visible - 1) * rowH + 4) continue;
        int y = (int)yf;
        bool sel = i == songSel, play = eng.songOn && eng.playing && eng.songPos == i;
        if (sel) { rrect(12, y, 616, rowH - 4, 6, C_SEL); rrect(12, y + 4, 3, rowH - 12, 1, C_ACC); }
        snprintf(b, sizeof b, "%02d", i + 1); text(26, y + 6, b, C_DIM);
        int pt = J.song[i].pat % NPATTERNS, rep = J.song[i].rep < 1 ? 1 : J.song[i].rep;
        Col pc = TCOL[pt % NTRACKS];
        rrect(70, y + 3, 64, rowH - 10, 5, play ? pc : dimC(pc, 0.35f));
        snprintf(b, sizeof b, "P%02d", pt + 1); textC(102, y + 6, b, play ? C_BG : C_TEXT);
        snprintf(b, sizeof b, "x%d", rep); text(150, y + 6, b, sel ? C_ACC : C_TEXT);
        for (int k = 0; k < rep && k < 16; k++) {   // Wiederholungen als Kaestchen, laufender Durchgang leuchtet
            bool cur = play && k == eng.songRep;
            rrect(200 + k * 24, y + 7, 18, rowH - 18, 3, cur ? C_PLAY : (play && k < eng.songRep ? pc : C_OFF2));
        }
        if (play) text(600 - textW(">>"), y + 6, ">>", C_PLAY);
    }
    hint(6, 452, "[^v]EINTRAG [</>]PATTERN [X/Y]WDH [A]NEU [B]LÖSCHEN [R1]SONG AN/AUS [START]AB HIER", C_ACC, C_DIM);
}

static void songInput(Input& I, Uint32 now) {
    Project& J = eng.proj;
    if (I.pressed(BT_L2)) { gotoPage(-1); return; }   // Seitenwechsel wie ueberall
    if (I.pressed(BT_R2)) { gotoPage(1); return; }
    if (rsAxis == 2) { int f = flick(I.rx, flRX, flRXt, now); if (f) { gotoPage(f); return; } } else flRX = 0;
    int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0) - flick(I.ly, flLY, flLYt, now);
    if (dy && J.songLen) songSel = clampi(songSel + dy, 0, J.songLen - 1);
    int dx = (I.repeat(BT_RIGHT, now) ? 1 : 0) - (I.repeat(BT_LEFT, now) ? 1 : 0) + flick(I.lx, flLX, flLXt, now);
    if (dx && J.songLen) { lockA(); J.song[songSel].pat = (uint8_t)((J.song[songSel].pat + NPATTERNS + dx) % NPATTERNS); unlockA(); }
    int dr = (I.repeat(BT_X, now) ? 1 : 0) - (I.repeat(BT_Y, now) ? 1 : 0);
    if (dr && J.songLen) { lockA(); J.song[songSel].rep = (uint8_t)clampi((J.song[songSel].rep < 1 ? 1 : J.song[songSel].rep) + dr, 1, 16); unlockA(); }
    if (I.pressed(BT_A)) {
        if (J.songLen >= SONG_MAX) toast("SONG VOLL (%d)", SONG_MAX);
        else {
            lockA();
            int at = J.songLen ? songSel + 1 : 0;
            for (int i = J.songLen; i > at; i--) J.song[i] = J.song[i - 1];
            J.song[at].pat = (uint8_t)(J.songLen ? J.song[songSel].pat : eng.curPat);
            J.song[at].rep = 1;
            J.songLen++;
            unlockA();
            songSel = at;
        }
    }
    if (I.pressed(BT_B) && J.songLen) {
        lockA();
        for (int i = songSel; i < J.songLen - 1; i++) J.song[i] = J.song[i + 1];
        J.songLen--;
        if (eng.songPos >= J.songLen) eng.songPos = 0;
        if (!J.songLen) eng.songOn = false;
        unlockA();
        if (songSel >= J.songLen && songSel > 0) songSel--;
    }
    if (I.pressed(BT_R1)) {
        if (!J.songLen) toast("ERST EINTRÄGE ANLEGEN (A)");
        else { lockA(); if (eng.songOn) eng.songOn = false; else eng.songStart(songSel); unlockA(); toast(eng.songOn ? "SONG-MODUS AN" : "SONG-MODUS AUS"); }
    }
    if (I.pressed(BT_START)) {
        lockA();
        if (eng.playing) eng.stop();
        else if (J.songLen) { eng.songStart(songSel); eng.start(); }
        else eng.start();
        unlockA();
    }
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
    hint(6, 452, "[^/v]WÄHLEN [</>]ÄNDERN [A]AUSFÜHREN [B]ZURÜCK [SEL]MENÜ", C_ACC, C_DIM);
}

// (frueher) Linker Stick zeigt direkt auf einen der 8 Regler (oben 1-4, unten 5-8)
[[maybe_unused]] static void stickPointSlot(float lx, float ly) {
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
    if (I.pressed(BT_X) && PG_STORE[menuSel] >= 0) {   // Seite (8 Werte der Spur) kopieren
        const Pattern& P = curP();
        int st = PG_STORE[menuSel];
        for (int i = 0; i < 8; i++) pageClip[i] = P.p[track][st * 8 + i];
        pageClipStore = st; pageClipMachine = P.machine[track]; pageClipTrack = track;
        pageClipLen = P.len[track]; pageClipSpeed = P.speed[track];
        toast("SEITE %s VON T%d KOPIERT", PAGE_NAMES[menuSel], track + 1);
    }
    if (I.pressed(BT_Y) && PG_STORE[menuSel] >= 0) {   // auf der aktuellen Spur einfuegen
        int st = PG_STORE[menuSel];
        bool srcPage = menuSel == PG_SRC || menuSel == PG_SRC2;
        if (pageClipStore < 0) toast("SEITEN-ABLAGE LEER (X = KOPIEREN)");
        else if (pageClipStore != st) toast("ABLAGE IST EINE ANDERE SEITE");
        else if (srcPage && pageClipMachine != curP().machine[track]) toast("ANDERE ENGINE (%s) - NICHT EINGEFÜGT", MACHINE_NAMES[pageClipMachine % M_COUNT]);
        else {
            lockA();
            Pattern& P = curP();
            for (int i = 0; i < 8; i++) P.p[track][st * 8 + i] = pageClip[i];
            if (menuSel == PG_SEQ) { P.len[track] = (uint8_t)pageClipLen; P.speed[track] = (uint8_t)pageClipSpeed; }
            unlockA();
            toast("SEITE %s EINGEFÜGT (T%d -> T%d)", PAGE_NAMES[menuSel], pageClipTrack + 1, track + 1);
        }
    }
    if (I.pressed(BT_A) || I.pressed(BT_START)) { page = menuSel; menuOpen = false; }
    if (I.pressed(BT_B) || I.pressed(BT_SELECT)) menuOpen = false;
}

static void liveInput(Input& I, Uint32 now, float dt) {
    for (int t = 0; t < NTRACKS; t++) if (I.pressed(LIVE_BTN[t])) eng.mute[t] = !eng.mute[t];
    if (I.pressed(BT_L1)) eng.fill = true;
    if (I.released(BT_L1)) eng.fill = false;
    if (I.pressed(BT_R1)) recToggle();
    if (I.pressed(BT_L2) || I.pressed(BT_R2)) {
        int d = I.pressed(BT_R2) ? 1 : -1;
        int from = eng.nextPat >= 0 ? eng.nextPat : eng.curPat, to = (from + d + NPATTERNS) % NPATTERNS;
        lockA();
        if (eng.playing) eng.nextPat = to; else { eng.curPat = to; eng.nextPat = -1; }
        unlockA();
    }
    if (I.pressed(BT_START)) { lockA(); if (eng.playing) eng.stop(); else eng.start(); unlockA(); }
    if (fabsf(I.ry) > 0.2f) { eng.dj = I.ry > 0 ? (I.ry - 0.2f) / 0.8f : (I.ry + 0.2f) / 0.8f; stickDj = true; }
    else if (stickDj) { eng.dj = 0.f; stickDj = false; }
    int f = flick(I.ly, flLY, flLYt, now);
    if (f) eng.proj.bpm = (uint16_t)clampi(eng.proj.bpm + f, 30, 300);
}

static void handleInput(Uint32 now, float dt) {
    Input& I = in;
    if (confirmOpen) {
        if (I.pressed(BT_A)) { confirmOpen = false; runAction(confirmAct, confirmArg); }
        else if (I.pressed(BT_B) || I.pressed(BT_SELECT)) { confirmOpen = false; toast("ABGEBROCHEN"); }
        return;
    }
    if (menuOpen) { menuInput(I, now); return; }
    if (driveOn) { if (I.pressed(BT_A) || I.pressed(BT_START)) stopDrive(); return; }
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
    if (page == PG_SONG && !func) { songInput(I, now); return; }
    if (page == PG_LIVE && !func) { liveInput(I, now, dt); return; }

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
        if (I.pressed(BT_A) && lockLatch) {   // im Lock-Modus: Trig <-> trigless (nur Locks) umschalten
            funcUsed = true;
            Step& hs = holdS();
            hs.on = hs.on == 2 ? 1 : 2; aEdited = true;
            toast(hs.on == 2 ? "STEP %d: TRIGLESS (NUR LOCKS)" : "STEP %d: TRIG", holdStep + 1);
        } else if (I.pressed(BT_A)) { funcUsed = true; eng.mute[track] = !eng.mute[track]; toast(eng.mute[track] ? "SPUR %d STUMM" : "SPUR %d AN", track + 1); }
        if (I.pressed(BT_B)) { funcUsed = true; undoStep(); }
        if (I.pressed(BT_X) || I.pressed(BT_Y)) {
            funcUsed = true;
            int m = (curP().machine[track] + (I.pressed(BT_X) ? 1 : M_COUNT - 1)) % M_COUNT;
            lockA(); Engine::setMachine(curP(), track, m); unlockA();
            toast("ENGINE: %s", MACHINE_NAMES[m]);
        }
        if (I.repeat(BT_L1, now) || I.repeat(BT_R1, now)) {   // Step-Cursor um einen Schlag (4 Steps)
            funcUsed = true;
            int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            cursor = ((cursor + (I.down[BT_R1] ? 4 : -4)) % cl + cl) % cl;
        }
        if (I.repeat(BT_L2, now) || I.repeat(BT_R2, now)) {
            funcUsed = true;
            Pattern& P = curP();
            P.len[track] = (uint8_t)clampi(P.len[track] + (I.down[BT_R2] ? 1 : -1), 1, NSTEPS);
            toast("SPURLÄNGE %d", P.len[track]);
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
                if (aPrevOn == 0 && !aEdited) o.on = 0;
                cursor = ((cursor + d) % cl + cl) % cl;
                holdStep = cursor;
                Step& s = holdS();
                aPrevOn = s.on; aJustOn = s.on == 0; aEdited = false;
                if (s.on == 0) { memset(s.lock, NO_LOCK, NPARAMS); s.on = 1; }
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
            if (aPrevOn == 0 && !aEdited) { s.on = 0; memset(s.lock, NO_LOCK, NPARAMS); }
            lockLatch = false; aHeld = false;
            toast("LOCK-MODUS AUS");
        } else if (I.pressed(BT_A)) {
            if (mixP) { eng.mute[track] = !eng.mute[track]; toast(eng.mute[track] ? "SPUR %d STUMM" : "SPUR %d AN", track + 1); }
            else if (prjAct) prjTrigger(slot);
            else {
                holdPat = eng.curPat; holdTrack = track; holdStep = cursor;
                Step& s = holdS();
                aPrevOn = s.on; aJustOn = s.on == 0;
                if (s.on == 0) { memset(s.lock, NO_LOCK, NPARAMS); s.on = 1; }   // trigless bleibt beim Halten trigless
                aHeld = true; aEdited = false; aDownAt = now;
            }
        }
        // A lange halten (ohne zu drehen) = Lock-Modus einrasten: Daumen frei fuer Stick und X/Y
        if (aHeld && !lockLatch && I.down[BT_A] && !aEdited && now - aDownAt > 450) {
            lockLatch = true;
            toast("LOCK-MODUS STEP %d  (A = FERTIG)", holdStep + 1);
        }
        if (I.pressed(BT_B)) {
            if (aHeld) {
                Step& hs = holdS();
                memset(hs.lock, NO_LOCK, NPARAMS); aEdited = true;
                if (hs.on == 2) { hs.on = 0; toast("TRIGLESS LOCKS GELÖSCHT"); } else toast("LOCKS GELÖSCHT");
            }
            else if (mixP) { eng.solo[track] = !eng.solo[track]; toast(eng.solo[track] ? "SPUR %d SOLO" : "SPUR %d SOLO AUS", track + 1); }
            else { lockA(); eng.preview(track, &curP().s[track][cursor]); unlockA(); }
        }
        // Linker Stick: auf Regler zeigen (Mixer: hoch/runter = Zeile, links/rechts = Kanal)
        if (mixP) {
            int f = flick(I.ly, flLY, flLYt, now); if (f) mixRow = clampi(mixRow - f, 0, 3);
            int g = flick(I.lx, flLX, flLXt, now); if (g) track = (track + NTRACKS + g) % NTRACKS;
        } else {   // Linker Stick: pro Antippen genau eine Kachel weiter (halten = wiederholen)
            int fx = flick(I.lx, flLX, flLXt, now), fy = flick(I.ly, flLY, flLYt, now);
            if (fx) slot = (slot / 4) * 4 + clampi(slot % 4 + fx, 0, 3);
            if (fy) slot = clampi(slot - fy * 4, slot % 4, 4 + slot % 4);
        }
        // Rechter Stick: links/rechts = Seite, hoch/runter = Wert
        flRX = 0;   // Seite wechseln nur mit SELECT + rechtem Stick, L2/R2 oder dem Menue
        if (aHeld && I.pressed(BT_L1)) {   // Trig kopieren (Zustand vor dem Druck, mit allen Locks)
            Step& hs = holdS();
            if (aPrevOn == 0) { hs.on = 0; memset(hs.lock, NO_LOCK, NPARAMS); }   // leerer Step: nicht anlegen
            else hs.on = (uint8_t)aPrevOn;
            trigClip = hs; haveTrigClip = true; aEdited = true;
            toast(aPrevOn == 0 ? "LEEREN STEP KOPIERT" : (aPrevOn == 2 ? "TRIGLESS STEP KOPIERT" : "TRIG KOPIERT (MIT LOCKS)"));
        }
        if (aHeld && I.pressed(BT_R1)) {   // Trig einfuegen in den gehaltenen Step
            if (!haveTrigClip) toast("TRIG-ABLAGE LEER (A HALTEN + L1)");
            else { lockA(); holdS() = trigClip; unlockA(); aEdited = true; toast("TRIG EINGEFÜGT IN STEP %d", holdStep + 1); }
        }
        if (!aHeld) {   // L1/R1: Mixer-Zeile, auf Spurseiten Step-Seite (16 Steps), sonst Regler
            int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            int dl = (I.repeat(BT_R1, now) ? 1 : 0) - (I.repeat(BT_L1, now) ? 1 : 0);
            if (dl && trackPage(page)) sigStep(SIG_CHAIN, SIG_NCH, dl);   // Signalweg: Block
            else if (dl) {
                if (mixP) mixRow = (mixRow + 4 + dl) % 4;
                else if (trackPage(page) && cl > 16) cursor = ((cursor + dl * 16) % cl + cl) % cl;
                else slot = (slot + 8 + dl) % 8;
            }
        }
        if (trackPage(page)) {   // Signalweg: Modulationsreihe
            if (I.pressed(BT_L2)) sigStep(SIG_MOD, SIG_NMOD, -1);
            if (I.pressed(BT_R2)) sigStep(SIG_MOD, SIG_NMOD, 1);
        } else {
            if (I.pressed(BT_L2)) gotoPage(-1);
            if (I.pressed(BT_R2)) gotoPage(1);
        }
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
        if (!aEdited) {
            if (aPrevOn == 1) {   // Trig antippen = Trig weg; Locks bleiben als trigless Step
                if (stepHasLock(s)) { s.on = 2; toast("TRIG WEG - LOCKS BLEIBEN (TRIGLESS)"); }
                else { s.on = 0; memset(s.lock, NO_LOCK, NPARAMS); }
            } else if (aPrevOn == 2) { s.on = 1; toast("TRIGLESS -> TRIG"); }
        }
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
    if (selfTesting) return false;
    SDL_Joystick* j = in.joy;
    if (!j) { printf("Kein Gamepad gefunden - Lernmodus übersprungen\n"); return false; }
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

            clearScreen(C_BG);
            textC(SW / 2, 40, "TASTEN LERNEN", C_ACC, 2);
            textC(SW / 2, 150, "DRÜCKE:", C_DIM);
            textC(SW / 2, 190, prompts[item], C_TEXT, 2);
            char b[64];
            snprintf(b, sizeof b, "%d / %d", item + 1, BT_COUNT + 4);
            textC(SW / 2, 260, b, C_DIM);
            if (stick) { snprintf(b, sizeof b, "Kein Stick? Ueberspringen in %ds", left); textC(SW / 2, 300, b, C_DIM); }
            present();
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
    static bool rtTried = false;
    if (!rtTried) {   // Audio-Thread (und damit der Helfer-Kern) mit Echtzeit-Prioritaet, wenn erlaubt (Groovebox-OS: root)
        rtTried = true;
        struct sched_param sp; sp.sched_priority = 70;
        int pol; struct sched_param cur;
        if (pthread_getschedparam(pthread_self(), &pol, &cur) == 0 && pol == SCHED_OTHER)
            printf("Audio-Thread Echtzeit-Prioritaet: %s\n", pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) == 0 ? "ja" : "nein");
    }
    Uint64 t0 = SDL_GetPerformanceCounter();
    int frames = len / (int)(sizeof(float) * 2);
    {   // Diagnose: Abstand der Aufrufe (zu spaet = Aussetzer, z. B. durch eine Sperre der Oberflaeche)
        static Uint64 lastCb = 0;
        double per = (double)frames / eng.sr * 1000.0;
        if (lastCb) {
            double gap = (double)(t0 - lastCb) * 1000.0 / SDL_GetPerformanceFrequency();
            if (gap > audGapMax) audGapMax = gap;
            if (gap > per * 2.5) audLate++;
        }
        lastCb = t0;
    }
    cbTime = midiNow();
    cbFrames = frames;
    MidiMsg mm;
    while (midiInPop(mm)) eng.midiIn(mm.st, mm.d1, mm.d2);
    eng.render((float*)stream, frames);
    usbAudioPush((const float*)stream, frames);   // parallel an den PC (falls aktiv)
    pcInMix((float*)stream, frames, pcInGain);    // PC-Ton dazumischen (nach dem Senden: kein Echo zum PC)
    recPush((const float*)stream, frames);
    double el = (double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency();
    double avail = (double)frames / eng.sr;
    if (el * 1000.0 > audWorkMax) audWorkMax = el * 1000.0;
    if (el > avail * 0.9) audLate++;
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
    if (selfTesting) return;
    if (audioDev) { SDL_CloseAudioDevice(audioDev); audioDev = 0; }
    if (isUsbAudioDev(cfg.dev)) { cfg.dev[0] = 0; saveSettings(); }   // USB zum PC laeuft parallel, nie als Takt-Ausgang
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100; want.format = AUDIO_F32SYS; want.channels = 2;
    want.samples = (Uint16)(bufOverride ? bufOverride : BUF_SIZES[cfg.buf]);
    want.callback = audioCb;
    const char* name = cfg.dev[0] ? cfg.dev : nullptr;
    audioDev = SDL_OpenAudioDevice(name, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!audioDev && name) {
        printf("Audio: '%s' nicht verfügbar (%s) - nehme Standardausgang\n", name, SDL_GetError());
        toast("AUSGANG NICHT VERFÜGBAR");
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
    if (selfTesting) return;
    usbAudioStop();   // Gadget wird gleich neu aufgebaut
    char cmd[800];
    static const char* args[3] = {"aus", "an", "an audio"};
    snprintf(cmd, sizeof cmd, "bash '%spc-modus.sh' %s nosleep", basePath, args[usbMode]);
    toast(usbMode ? "PC-MODUS WIRD AKTIVIERT ..." : "PC-MODUS AUS ...");
    drawAll(false); present();
    int rc = system(cmd);
    int now = detectUsbMode();
    printf("USB-Modus %d: rc=%d, erkannt=%d\n", usbMode, rc, now);
    if (usbMode && !now) {   // letzte Fehlerzeile aus dem Log anzeigen
        usbMode = 0;
        char lp[700], line[200], err[200] = "PC-MODUS NICHT MÖGLICH (PC-MODUS.LOG)";
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

#include "selftest.inc"

int main(int argc, char** argv) {
    if (argc >= 3 && !strcmp(argv[1], "--dhcpd")) return runDhcpd(argv[2]);   // Hilfsdienst fuer den PC-Modus
    bool learn = false, windowed = false;
    const char* shot = nullptr; const char* renderOut = nullptr; float renderSecs = 8; int shotPage = 0;
    bool benchMode = false, selftestMode = false; const char* benchFile = nullptr; double stressSecs = 30;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--learn")) learn = true;
        else if (!strcmp(argv[i], "--windowed")) windowed = true;
        else if (!strcmp(argv[i], "--buffer") && i + 1 < argc) bufOverride = clampi(atoi(argv[++i]), 64, 4096);
        else if (!strcmp(argv[i], "--project") && i + 1 < argc) { snprintf(projPath, sizeof projPath, "%s", argv[++i]); projFromArg = true; }
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) { shot = argv[++i]; if (i + 1 < argc && argv[i + 1][0] != '-') shotPage = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--render") && i + 1 < argc) { renderOut = argv[++i]; if (i + 1 < argc && argv[i + 1][0] != '-') renderSecs = (float)atof(argv[++i]); }
        else if (!strcmp(argv[i], "--bench")) { benchMode = true; if (i + 1 < argc && argv[i + 1][0] != '-') benchFile = argv[++i]; }
        else if (!strcmp(argv[i], "--selftest")) { selftestMode = true; if (i + 1 < argc && argv[i + 1][0] != '-') stressSecs = atof(argv[++i]); }
        else { printf("Optionen: --learn --windowed --buffer FRAMES --project DATEI --render WAV [sek] --shot BMP [seite] --bench [datei] --selftest [sek]\n"); return 0; }
    }
    setupPaths();

    if (renderOut) {   // Offline-Bounce ohne Grafik/Audiogeraet
        eng.init(44100);
        if (getenv("GB_DEMO")) { buildDemo(eng); eng.songOn = true; eng.songStart(0); }   // Demo-Song rendern
        else if (eng.load(projPath)) printf("Projekt geladen: %s\n", projPath);
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

    if (benchMode || selftestMode) {   // Leistungstest / Selbsttest ohne Anzeige (Zeichnen in den Speicher wie auf dem Geraet)
        eng.init(44100);
        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, SW, SH, 32, SDL_PIXELFORMAT_ARGB8888);
        ren = SDL_CreateSoftwareRenderer(surf);
        if (!getenv("GB_SDLDRAW")) { fbPix = (uint32_t*)surf->pixels; fbPitch = surf->pitch / 4; }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        makeFont();
        if (benchMode) { selfTesting = true; runBench(benchFile, false); return 0; }
        return runSelftest(stressSecs);
    }

    if (shot) {        // Screenshot (Software-Renderer) zum Testen
        eng.init(44100);
        eng.load(projPath);
        bool fbTest = getenv("GB_FB") != nullptr;   // Test der eigenen Zeichenfunktionen
        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, SW, SH, 32, fbTest ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGBA32);
        ren = SDL_CreateSoftwareRenderer(surf);
        if (fbTest) { fbPix = (uint32_t*)surf->pixels; fbPitch = surf->pitch / 4; }
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
        if (page == PG_TRG && shotPage == 0) { Step& tq = curP().s[4][5]; tq.on = 2; memset(tq.lock, NO_LOCK, NPARAMS); tq.lock[P_FREQ] = 20; }
        if (page == PG_ARP) { Pattern& Pa = curP(); Pa.p[track][P_AMOD] = 3; Pa.p[track][P_AOCT] = 1; Pa.p[track][P_ARAT] = 0; Pa.p[track][P_LEN] = 80; Pa.p[track][P_AACC] = 60; }
        if (page == PG_SONG) { eng.proj.songLen = 5; uint8_t sp[5][2] = {{0, 4}, {1, 2}, {0, 2}, {2, 1}, {3, 4}}; for (int k = 0; k < 5; k++) { eng.proj.song[k].pat = sp[k][0]; eng.proj.song[k].rep = sp[k][1]; }
                               eng.songOn = true; eng.songPos = 1; eng.songRep = 1; eng.playing = true; songSel = 2; }
        if (shotPage == 21) { holdPat = 0; holdTrack = 4; holdStep = 3; aHeld = true; page = 0; track = 4; slot = 0; }
        if (shotPage == 22) { page = PG_FLT; slot = 0; track = 4; animate(); editAt = 1; editSlot = 0; eng.recording = true; }
        if (shotPage == 23) { page = PG_TRG; slot = 6; track = 4; editSlot = 6; editAt = 1; }
        if (shotPage == 24) { page = PG_MST; slot = MS_SCALE; eng.proj.mst[MS_SCALE] = 1 + 1 * 12 + 9; eng.proj.mst[MS_ROOT] = 9; editSlot = MS_SCALE; editAt = 1; }
        if (shotPage == 25) { page = PG_AMP; menuOpen = true; menuSel = PG_FLT; pageClipStore = 2; pageClipTrack = 1; }
        if (getenv("GB_SHOT_FT")) { curP().p[4][P_FTYP] = (uint8_t)atoi(getenv("GB_SHOT_FT")); curP().p[4][P_RESO] = 90; slot = 2; }   // Filtertyp fuer Screenshots
        if (shotPage == 26) { page = PG_PRJ; slot = 1; askConfirm(ACT_LOAD, 2, "SONG 03 LADEN? UNGESPEICHERTES GEHT VERLOREN"); }
        if (page == PG_SRC && curP().machine[track] == M_SAMPLE) { }
        lastPage = page;
        if (getenv("GB_BENCH")) {   // Zeichenzeit messen: GB_BENCH=frames ./groovebox --shot x.bmp <seite>
            int n = atoi(getenv("GB_BENCH")); Uint64 t0 = SDL_GetPerformanceCounter();
            for (int i = 0; i < n; i++) { animTime += 0.016f; animate(); drawAll(false); }
            double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency() / n;
            printf("Seite %d: %.2f ms pro Bild\n", page, ms);
        }
        if (shotPage == 40) { clearScreen(C_BG); drawSplash(getenv("GB_SPLASH_T") ? (float)atof(getenv("GB_SPLASH_T")) : 3.f); }
        else if (shotPage == 20) { page = 0; drawAll(true); } else drawAll(false);
        SDL_RenderPresent(ren);
        SDL_SaveBMP(surf, shot);
        return 0;
    }

    // Groovebox-OS: Bild direkt per DRM ausgeben (GB_DISPLAY=drm), SDL dann ohne eigene Anzeige
    const char* gbd = getenv("GB_DISPLAY");
    bool useDrm = gbd && !strcmp(gbd, "drm") && !windowed && drmOutOpen();
    if (useDrm) setenv("SDL_VIDEODRIVER", "offscreen", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0 && useDrm) {
        printf("SDL offscreen: %s, versuche dummy\n", SDL_GetError());
        setenv("SDL_VIDEODRIVER", "dummy", 1);
    }
    if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    char dbPath[600];
    snprintf(dbPath, sizeof dbPath, "%sgamecontrollerdb.txt", basePath);
    SDL_GameControllerAddMappingsFromFile(dbPath);
    if (in.loadMap(cfgPath)) printf("Tastenbelegung geladen: %s\n", cfgPath);
    in.open();
    SDL_ShowCursor(SDL_DISABLE);

    printf("SDL-Videotreiber: %s\n", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "-");
    if (useDrm || getenv("GB_HEADLESS")) {   // GB_HEADLESS: ohne Anzeige in den Speicher zeichnen (Tests der Web-Spiegelung)
        drmOutSetByteOffset(cfg.dispOff);
        drmSurf = SDL_CreateRGBSurfaceWithFormat(0, SW, SH, 32, SDL_PIXELFORMAT_ARGB8888);
        ren = drmSurf ? SDL_CreateSoftwareRenderer(drmSurf) : nullptr;
        if (drmSurf && !getenv("GB_SDLDRAW")) { fbPix = (uint32_t*)drmSurf->pixels; fbPitch = drmSurf->pitch / 4; }   // schnelle eigene Zeichenfunktionen
        if (!ren) { printf("Software-Renderer: %s\n", SDL_GetError()); return 1; }
    } else {
        SDL_Window* win = SDL_CreateWindow("Synth City", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SW, SH,
                                           windowed ? SDL_WINDOW_RESIZABLE : SDL_WINDOW_FULLSCREEN_DESKTOP);
        if (!win) { printf("Fenster: %s\n", SDL_GetError()); return 1; }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
        if (!ren) { printf("Renderer: %s\n", SDL_GetError()); return 1; }
    }
    { SDL_RendererInfo ri; if (SDL_GetRendererInfo(ren, &ri) == 0) printf("Renderer: %s\n", ri.name); }
    SDL_RenderSetLogicalSize(ren, SW, SH);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    makeFont();

    if (!learn && !getenv("GB_NOSPLASH")) runSplash();
    if (learn || (!in.haveMap && in.joy && !in.gc)) runLearn();

    loadSettings();
    drmOutSetByteOffset(cfg.dispOff);
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
    if (isGrooveboxOS() && mlockall(MCL_CURRENT | MCL_FUTURE) == 0) printf("Speicher im RAM gesperrt (keine Seitenfehler im Audio-Thread)\n");
    eng.init(44100);
    eng.audioThreads = eng.helperReady() && cfg.cores > 1 ? 2 : 1;
    printf("Audio-Rechenkerne: %d\n", eng.audioThreads);
    bool firstRun = true;
    for (int i = 0; i < 32; i++) if (songExists(i)) firstRun = false;
    if (eng.load(projPath)) printf("Projekt geladen: %s\n", projPath);
    else if (firstRun) { buildDemo(eng); eng.songOn = true; eng.songStart(0); printf("Erster Start: Demo-Song angelegt\n"); }   // Einsteiger: sofort etwas zum Hoeren
    memcpy(&lastSaved, &eng.proj, sizeof(Project));
    applyMidiSettings();
    eng.midiOut = midiOutHook;
    midiStart();
    openAudio();
    toast(firstRun ? "WILLKOMMEN! START = DEMO ABSPIELEN, SELECT = MENÜ" : "%d SAMPLES GELADEN", sampleCount());
    if (firstRun) msgUntil = SDL_GetTicks() + 6000;
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
        undoTrack(now);
        if (recDoneNo) { if (recDoneNo > 0) toast("GESPEICHERT: SAMPLES/REC/REC-%03d.WAV", recDoneNo); else toast("AUFNAHME: SPEICHERN FEHLGESCHLAGEN"); recDoneNo = 0; }
        if (audioApplyAt && now >= audioApplyAt) { audioApplyAt = 0; openAudio(); }
        if (usbApplyAt && now >= usbApplyAt) { usbApplyAt = 0; applyUsbMode(); }
        {   // Browser-Upload: Meldung und Samples neu laden, sobald 1,5 s nichts mehr kommt
            char wm[96];
            int sq = webLastEvent(wm, sizeof wm);
            if (sq != webSeq) { webSeq = sq; toast("%s", wm); }
            if (webTakeChange(now, 1500)) { reloadSamples(); toast("%d SAMPLES GELADEN", sampleCount()); }
            static Uint32 updAt = 0;
            if (webUpdateReady() && !updAt) { updAt = now; toast("UPDATE EMPFANGEN - NEUSTART ..."); msgUntil = now + 5000; saveProject(); }
            if (updAt && now - updAt > 1500) quitReq = true;   // Dienst startet neu, groovebox-run installiert
        }
        if (now - lastAuto > 120000) {   // Autosave alle 2 Minuten bei Aenderungen
            lastAuto = now;
            if (memcmp(&lastSaved, &eng.proj, sizeof(Project))) saveProject();
        }
        bool help = in.down[BT_SELECT] && !funcUsed && in.heldMs(BT_SELECT, now) > 700;
        Uint64 tq0 = SDL_GetPerformanceCounter();
        bool draw = !screenOff || remoteWanted();   // Display aus: nicht zeichnen (Akku), ausser jemand schaut im Browser zu
        if (draw) drawAll(help);
        Uint64 tq1 = SDL_GetPerformanceCounter();
        if (draw) present();
        Uint64 tq2 = SDL_GetPerformanceCounter();
        {   // Bildschirm-Spiegelung fuers Webinterface (nur wenn jemand zuschaut, max. 30 Bilder/s)
            static Uint32 snapAt = 0;
            if (fbPix && remoteWanted() && now - snapAt >= 33) { snapAt = now; remoteSnapshot(fbPix, fbPitch, SW, SH); }
        }
        {   // Bildrate messen, alle 10 s ins Log
            static double sDraw = 0, sPres = 0, sMax = 0; static int nFr = 0; static Uint32 statAt = 0;
            double f = 1000.0 / SDL_GetPerformanceFrequency(), dd = (tq1 - tq0) * f, dp = (tq2 - tq1) * f;
            sDraw += dd; sPres += dp; if (dd + dp > sMax) sMax = dd + dp; nFr++;
            if (!statAt) statAt = now;
            if (now - statAt >= 10000) {
                double cpy = 0, flp = 0; drmOutStats(&cpy, &flp);
                printf("Bild: %.1f fps, zeichnen %.1f ms, ausgeben %.1f ms (kopieren %.1f, warten/flip %.1f%s) max %.1f ms, CPU Audio %d%%\n",
                       nFr * 1000.0 / (now - statAt), sDraw / nFr, sPres / nFr, cpy, flp, drmOutSingleBuffer() ? ", 1 Puffer" : "",
                       sMax, (int)(cpuLoad * 100));
                if (audLate || audGapMax > 0) {
                    audLateTotal += audLate;
                    printf("Audio: %d verspaetete Aufrufe (gesamt %d), groesster Abstand %.1f ms, laengste Rechnung %.1f ms, Kerne %d\n",
                           audLate, audLateTotal, audGapMax, audWorkMax, eng.audioThreads);
                    audLate = 0; audGapMax = 0; audWorkMax = 0;
                }
                fflush(stdout);
                sDraw = sPres = sMax = 0; nFr = 0; statAt = now;
            }
        }
        // Ruhe: ohne Wiedergabe und 2 s ohne Eingabe nur 30 Bilder/s (spart CPU und Akku)
        static Uint32 lastActive = 0;
        bool active = eng.playing || fabsf(in.lx) + fabsf(in.ly) + fabsf(in.rx) + fabsf(in.ry) > 0.2f;
        for (int b = 0; b < BT_COUNT && !active; b++) if (in.down[b]) active = true;
        if (active || now - msgAt < 1500) lastActive = now;
        Uint32 frameMs = now - lastActive > 2000 ? 33 : 16;
        Uint32 spent = SDL_GetTicks() - now;
        if (spent < frameMs) SDL_Delay(frameMs - spent);
    }

    lockA(); eng.stop(); unlockA();
    if (saveProject()) printf("Projekt gespeichert: %s\n", projPath);
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    midiStop();
    drmOutClose();
    SDL_Quit();
    return 0;
}
