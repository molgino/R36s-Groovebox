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
#include <ctype.h>
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
#include "pcm.h"
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
static int mixRow = 0;
enum { MIX_ROWS = 5 };
static const uint8_t MIX_P[MIX_ROWS] = {P_VOL, P_PAN, P_DLY, P_REV, P_SNDC};
static const char* const MIX_NAMES[MIX_ROWS] = {"VOL", "PAN", "S.A", "S.B", "S.C"};   // Sends an Effekt-Bus A/B/C
static bool menuOpen = false; static int menuSel = 0;
static bool confirmOpen = false; static char confirmText[80]; static int confirmAct = 0, confirmArg = 0;
static Uint32 selDownAt = 0;
static bool quitReq = false;
static volatile float cpuLoad = 0;
static volatile double audGapMax = 0, audWorkMax = 0;   // Audio-Diagnose (ms) seit der letzten Log-Zeile
static volatile int audLate = 0, audLateTotal = 0;     // verspaetete/zu lange Audio-Aufrufe
static char basePath[512] = "./", cfgPath[600], projPath[600];
int g_lang = 0;                    // Sprache (settings.cfg language: 0 de, 1 en)
static bool selfTesting = false;   // --selftest/--bench: keine Systemaktionen (USB, Lernmodus, Audio-Geraet)
static Project lastSaved;

// System-Einstellungen (settings.cfg)
struct Settings { char dev[128]; int buf, midiIn, notesOut, clk, chm, usb, song, dispOff, usbAudio, hwVol, pcIn, usbVol, intUsb, cores, lang, usbNet, uac, uacSendOnly; char project[64]; };   // usbNet 0 NCM / 1 RNDIS, uac 2 / 1
static Settings cfg = {"", 1, 1, 0, 0, 0, 0, 0, 277, 1, 80, 1, 100, 1, 2, 0, 0, 2, 0, ""};
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
static void writePoolTrailer(FILE* f, const Project& J);   // Sample-Namen des Projekts (nach den Projektdaten)
static bool poolSetSlot(int slot, const char* name);
static int poolAssign(const char* name);
static void poolLoadForProject(const char* path, int force = -1);
static int poolLegacy = -1;   // altes Projekt ohne Namensliste: 0 = Liste 1 Ordner-Ebene (bis 128), 1 = alle Ebenen (bis 250); -1 = Namen
static void poolDefaults();
static Project saveSnap;
static bool saveProject() {
    if (driveOn) return false;
    lockA(); memcpy(&saveSnap, &eng.proj, sizeof(Project)); unlockA();
    char tmp[700]; snprintf(tmp, sizeof tmp, "%s.tmp", projPath);
    FILE* f = fopen(tmp, "wb");
    bool ok = f && fwrite(&saveSnap, sizeof(Project), 1, f) == 1;
    if (ok) writePoolTrailer(f, saveSnap);
    if (f) { ok = fflush(f) == 0 && ok; ok = fsync(fileno(f)) == 0 && ok; fclose(f); }
    ok = ok && rename(tmp, projPath) == 0;
    if (ok) { memcpy(&lastSaved, &saveSnap, sizeof(Project)); poolLegacy = -1; }   // jetzt mit Sample-Namen gespeichert
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
        else if (!strcmp(line, "song")) { cfg.song = clampi(atoi(v), 0, 31); if (!cfg.project[0]) snprintf(cfg.project, sizeof cfg.project, "song%02d", cfg.song + 1); }   // alte Einstellung (Platz)
        else if (!strcmp(line, "project")) snprintf(cfg.project, sizeof cfg.project, "%s", v);
        else if (!strcmp(line, "language")) { cfg.lang = clampi(atoi(v), 0, 1); g_lang = cfg.lang; }
        else if (!strcmp(line, "usb_audio")) cfg.usbAudio = atoi(v) != 0;
        else if (!strcmp(line, "pc_in")) cfg.pcIn = atoi(v) != 0;
        else if (!strcmp(line, "usb_volume")) cfg.usbVol = clampi(atoi(v), 0, 100);
        else if (!strcmp(line, "internal_on_usb")) cfg.intUsb = atoi(v) != 0;
        else if (!strcmp(line, "hw_volume")) cfg.hwVol = clampi(atoi(v), 0, 100);
        else if (!strcmp(line, "audio_cores")) cfg.cores = clampi(atoi(v), 1, 2);
        else if (!strcmp(line, "usb_net")) cfg.usbNet = clampi(atoi(v), 0, 1);
        else if (!strcmp(line, "usb_audio_class")) cfg.uac = atoi(v) == 1 ? 1 : 2;
        else if (!strcmp(line, "usb_audio_send_only")) cfg.uacSendOnly = atoi(v) != 0;
        else if (!strcmp(line, "display_offset")) cfg.dispOff = clampi(atoi(v), -1920, 1920);
    }
    fclose(f);
}
static void saveSettings() {
    FILE* f = fopen(setPath, "w");
    if (!f) return;
    fprintf(f, "# Groovebox Systemeinstellungen\naudio_device %s\nbuffer %d\nmidi_in %d\nmidi_notes_out %d\nmidi_clock %d\nmidi_channels %d\nusb_mode %d\nsong %d\ndisplay_offset %d\nusb_audio %d\nhw_volume %d\npc_in %d\nusb_volume %d\ninternal_on_usb %d\naudio_cores %d\nlanguage %d\nusb_net %d\nusb_audio_class %d\nusb_audio_send_only %d\nproject %s\n",
            cfg.dev, cfg.buf, cfg.midiIn, cfg.notesOut, cfg.clk, cfg.chm, cfg.usb, cfg.song, cfg.dispOff, cfg.usbAudio, cfg.hwVol, cfg.pcIn, cfg.usbVol, cfg.intUsb, cfg.cores, cfg.lang, cfg.usbNet, cfg.uac, cfg.uacSendOnly, cfg.project);
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
        if (nm && (strcasestr(nm, "uac2") || strcasestr(nm, "gadget"))) continue;   // USB zum PC: eigener Schalter TR("Audio an PC", "Audio to PC")
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
static void (*textHook)(const char*) = nullptr;   // Selbsttest: angezeigte Texte mitschneiden
static void text(int x, int y, const char* s, Col c, int sc = 1) {
    if (textHook) textHook(s);
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
static float oscDrift = 0;
static int batPct = -1; static bool batChg = false, batFull = false; static Uint32 batAt = 0;
static int batDrv = -1, batMv = 0, batMa = 0, batSrcV = 0; static bool batPlug = false; static float batEst = -1.f; static char batStatus[24] = "";
static char batPath[300] = "";

// Aufnahme vom PC als Sample speichern: samples/pc/pc-NN.wav (24 Bit Stereo, Engine-Rate), danach Samples neu laden
static void reloadSamples();
static void savePcRecording(const float* d, int frames) {
    char dir[700], path[760];
    snprintf(dir, sizeof dir, "%ssamples/pc", basePath);
    mkdir(dir, 0755);
    int no = 1;
    struct stat st;
    for (; no < 100; no++) { snprintf(path, sizeof path, "%s/pc-%02d.wav", dir, no); if (stat(path, &st) != 0) break; }
    // Stille am Ende abschneiden
    while (frames > 1000 && fabsf(d[(frames - 1) * 2]) < 0.006f && fabsf(d[(frames - 1) * 2 + 1]) < 0.006f) frames--;
    const int sr = pcRecRate();
    FILE* f = fopen(path, "wb");
    if (f) {
        uint8_t hdr[44]; wavHeader(hdr, sr, 2, 24, (uint32_t)frames); fwrite(hdr, 1, 44, f);
        static uint8_t ob[1024 * 6]; Dither dt;
        for (int i = 0; i < frames; i += 1024) {
            int n = frames - i < 1024 ? frames - i : 1024;
            pcmTo24(ob, d + (size_t)i * 2, n * 2, dt); fwrite(ob, 6, (size_t)n, f);
        }
        fclose(f);
        sync();
    }
    pcRecDone();
    if (!f) { toast(TR("AUFNAHME: SPEICHERN FEHLGESCHLAGEN", "RECORDING: SAVING FAILED")); return; }
    reloadSamples();
    toast(TR("SAMPLE pc/pc-%02d GESPEICHERT (%.1f s)", "SAMPLE pc/pc-%02d SAVED (%.1f s)"), no, frames / (float)sr);
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
    toast(TR("SPEICHERN UND AUSSCHALTEN ...", "SAVING AND POWERING OFF ..."));
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
            if (intMuted) toast(TR("INTERN %d%% (STUMM, USB-AUDIO AKTIV)", "INTERNAL %d%% (MUTED, USB AUDIO ACTIVE)"), cfg.hwVol);
            else { hwVolumeSet(cfg.hwVol); toast(TR("LAUTSTÄRKE %d%%", "VOLUME %d%%"), cfg.hwVol); }
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

// Akku: Prozent aus der Spannung (Ruhespannung ~ Spannung + Strom x Innenwiderstand, Li-Ion-Kennlinie), geglaettet;
// der Wert des Treibers (Coulomb-Zaehler) ist beim R36S oft unkalibriert (springt, haengt bei 100 %) - er steht nur zur Info in SET.
// Laden: Status "Charging/Full" ODER ein Netzteil/USB-Eingang meldet online.
static char chgPath[8][300]; static int chgN = -1;
static bool readSysInt(const char* dir, const char* name, long& v) {
    char p[400], b[40] = ""; snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE* f = fopen(p, "r"); if (!f) return false;
    bool ok = fgets(b, sizeof b, f) != nullptr; fclose(f);
    if (!ok) return false;
    v = atol(b); return true;
}
static float batOcvPct(float v) {   // Ruhespannung (V) -> Ladezustand (%), typische LiPo-Zelle
    static const float T[][2] = {{3.30f, 0}, {3.61f, 5}, {3.69f, 10}, {3.71f, 15}, {3.73f, 20}, {3.75f, 25}, {3.77f, 30}, {3.79f, 35}, {3.80f, 40},
                                 {3.82f, 45}, {3.84f, 50}, {3.85f, 55}, {3.87f, 60}, {3.91f, 65}, {3.95f, 70}, {3.98f, 75}, {4.02f, 80},
                                 {4.08f, 85}, {4.11f, 90}, {4.15f, 95}, {4.19f, 100}};
    const int N = sizeof T / sizeof *T;
    if (v <= T[0][0]) return 0.f;
    if (v >= T[N - 1][0]) return 100.f;
    for (int i = 1; i < N; i++) if (v < T[i][0]) return T[i - 1][1] + (v - T[i - 1][0]) / (T[i][0] - T[i - 1][0]) * (T[i][1] - T[i - 1][1]);
    return 100.f;
}
// Ladebuchse (separater DC-Port): Pin dc-det = GPIO0_B3 (Bank gpio0, Leitung 11). Der Akku-Treiber wertet ihn nicht aus,
// darum hier direkt lesen. Polaritaet unbekannt -> lernen: bei deutlichem Entladestrom ist der Pegel "nicht eingesteckt".
#include <linux/gpio.h>
#include <errno.h>
#include <sys/ioctl.h>
static int dcFd = -2, dcRaw = -1, dcIdle = -1;   // dcIdle: Pegel ohne Netzteil (-1 = noch unbekannt, Annahme aktiv-low)
static int readDcDet() {
    if (dcFd == -2) {
        dcFd = -1;
        for (int c = 0; c < 8 && dcFd < 0; c++) {
            char p[32]; snprintf(p, sizeof p, "/dev/gpiochip%d", c);
            int fd = open(p, O_RDONLY | O_CLOEXEC);
            if (fd < 0) continue;
            struct gpiochip_info ci; memset(&ci, 0, sizeof ci);
            bool bank0 = ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &ci) == 0 && (!strcmp(ci.label, "gpio0") || strstr(ci.label, "ff040000"));
            if (bank0) {
                struct gpiohandle_request rq; memset(&rq, 0, sizeof rq);
                rq.lineoffsets[0] = 11; rq.lines = 1; rq.flags = GPIOHANDLE_REQUEST_INPUT;
                snprintf(rq.consumer_label, sizeof rq.consumer_label, "groovebox-dcdet");
                if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &rq) == 0) dcFd = rq.fd;
                else printf("Akku: dc-det (GPIO0_B3) nicht lesbar (%s)\n", strerror(errno));
            }
            close(fd);
        }
    }
    if (dcFd < 0) return -1;
    struct gpiohandle_data hd; memset(&hd, 0, sizeof hd);
    if (ioctl(dcFd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &hd) != 0) return -1;
    return hd.values[0];
}

static void readBattery() {
    if (chgN < 0) {   // einmal: Akku und Lade-Eingaenge suchen
        chgN = 0;
        const char* root = getenv("GB_POWER_SUPPLY") ? getenv("GB_POWER_SUPPLY") : "/sys/class/power_supply";   // Tests: nachgebautes Verzeichnis
        DIR* d = opendir(root);
        if (d) {
            struct dirent* e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                char p[300], ty[32] = "";
                snprintf(p, sizeof p, "%s/%s/type", root, e->d_name);
                FILE* f = fopen(p, "r");
                if (!f) continue;
                if (!fgets(ty, sizeof ty, f)) ty[0] = 0;
                fclose(f);
                if (!strncmp(ty, "Battery", 7) && !batPath[0]) snprintf(batPath, sizeof batPath, "%s/%s", root, e->d_name);
                else if (chgN < 8 && (!strncmp(ty, "Mains", 5) || !strncmp(ty, "USB", 3))) snprintf(chgPath[chgN++], sizeof chgPath[0], "%s/%s", root, e->d_name);
            }
            closedir(d);
        }
    }
    if (!batPath[0]) { batPct = -2; return; }
    long v;
    batDrv = readSysInt(batPath, "capacity", v) ? (int)v : -1;
    batMv = readSysInt(batPath, "voltage_now", v) ? (int)(v / 1000) : 0;
    if (batMv <= 0 && readSysInt(batPath, "voltage_avg", v)) batMv = (int)(v / 1000);
    batMa = readSysInt(batPath, "current_now", v) ? (int)(v / 1000) : 0;
    {   char p[340]; snprintf(p, sizeof p, "%s/status", batPath);
        FILE* f = fopen(p, "r"); batStatus[0] = 0;
        if (f) { if (fgets(batStatus, sizeof batStatus, f)) batStatus[strcspn(batStatus, "\r\n")] = 0; fclose(f); } }
    bool online = false;
    for (int i = 0; i < chgN; i++) if (readSysInt(chgPath[i], "online", v) && v > 0) online = true;
    batFull = !strncmp(batStatus, "Full", 4);
    batChg = !strncmp(batStatus, "Charging", 8) || batFull;
    dcRaw = getenv("GB_POWER_SUPPLY") ? -1 : readDcDet();
    if (dcRaw >= 0) {   // Polaritaet lernen: deutlicher Entlade- bzw. Ladestrom zeigt den Pegel
        if (batMa < -150) dcIdle = dcRaw;
        else if (batMa > 150) dcIdle = !dcRaw;
    }
    bool dcIn = dcRaw >= 0 && (dcIdle >= 0 ? dcRaw != dcIdle : dcRaw == 0);
    // eingesteckt: Treiber meldet Laden/voll, ein Lade-Eingang ist online, der dc-det-Pin, oder es fliesst Strom in den Akku
    // ("Not charging" = Treiber laedt nicht selbst, aber es kommt kein Entladestrom -> externes Laden ueber die DC-Buchse)
    batPlug = batChg || online || dcIn || batMa > 50 || !strncmp(batStatus, "Not charging", 12);
    if (batMv > 2500 && batMv < 4600) {   // Spannungs-Schaetzung
        float ia = fabsf((float)batMa) / 1000.f;
        if (ia > 3.f) ia = 0.f;   // unplausibel (Einheit/Treiber) -> ohne Ausgleich
        const float R = 0.15f;    // Innenwiderstand + Leitungen (Ohm)
        float ocv = batMv / 1000.f + (batPlug ? -ia * R : ia * R);
        float p = batOcvPct(ocv);
        if (batEst < 0.f) batEst = p;
        else {
            float k = batPlug ? 0.05f : 0.1f;   // ~1-2 min Zeitkonstante bei 5-s-Takt
            float n = batEst + (p - batEst) * k;
            if (!batPlug && n > batEst) n = batEst + (n - batEst) * 0.1f;   // beim Entladen kaum steigen (Spannung erholt sich nach Last)
            if (batPlug && n < batEst) n = batEst;                          // beim Laden nicht fallen
            batEst = n;
        }
        if (batFull) batEst = 100.f;
        batSrcV = 1;
        batPct = clampi((int)(batEst + 0.5f), 0, 100);
    } else { batSrcV = 0; batPct = batDrv; }
    static Uint32 logAt = 0;
    Uint32 now = SDL_GetTicks();
    if (!logAt || now - logAt > 60000) {   // Diagnose im Log (zum Vergleichen mit dem Treiberwert)
        logAt = now;
        printf("Akku: %d %% (Treiber %d %%), %d mV, %d mA, Status %s, Netz %s, dc-det %d (ohne Netzteil %d), Lade-Eingang %s\n", batPct, batDrv, batMv, batMa,
               batStatus[0] ? batStatus : "?", batPlug ? "an" : "aus", dcRaw, dcIdle, online ? "online" : "-");
    }
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
    if (now - batAt > 5000 || batAt == 0) { batAt = now; readBattery(); }
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
            float* d = nullptr;
            int n = pcRecTake(&d);
            if (n > 0) savePcRecording(d, n);
            else if (pcRecState() == 3) pcRecDone();
        }
    }
}

// ---------------------------------------------------------------- Bildschirm

static int curPi() { return eng.patOf(track); }   // Pattern der gewaehlten Spur (im Song: aus der laufenden Zeile)
static Pattern& curP() { return eng.proj.pat[curPi()]; }
static void toast(const char* fmt, ...);
static void lockA();
static void unlockA();
// Pattern wechseln (am Pattern-Ende, falls es laeuft). Im Song-Modus: Song aus, dann wechseln
static void switchPattern(int to) {
    to = (to % NPATTERNS + NPATTERNS) % NPATTERNS;
    lockA();
    bool wasSong = eng.songActive();
    if (wasSong) { eng.songOn = false; eng.curPat = eng.trkPat[0]; }
    if (eng.playing) eng.nextPat = to; else { eng.curPat = to; eng.nextPat = -1; }
    unlockA();
    if (wasSong) toast(TR("SONG AUS - PATTERN %02d", "SONG OFF - PATTERN %02d"), to + 1);
}
static Step& holdS() { return eng.proj.pat[holdPat].s[holdTrack][holdStep]; }

// Rueckgaengig: Stapel von Pattern-Staenden; zusammenhaengende Aenderungen (bis 0,8 s Pause) = ein Schritt
enum { UNDO_N = 12 };
static Pattern undoBuf[UNDO_N], undoShadow, redoBuf;
static int undoPat[UNDO_N], undoTop = 0, undoCnt = 0, undoShadowPat = -1, redoPat = -1;
static bool undoDirty = false;
static Uint32 undoLastChange = 0;
static void undoTrack(Uint32 now) {   // einmal pro Bild nach der Eingabe
    const Pattern& P = curP();
    if (curPi() != undoShadowPat) { undoShadow = P; undoShadowPat = curPi(); undoDirty = false; return; }
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
    if (!undoCnt) { toast(TR("NICHTS RÜCKGÄNGIG ZU MACHEN", "NOTHING TO UNDO")); return; }
    undoTop = (undoTop + UNDO_N - 1) % UNDO_N; undoCnt--;
    int pi = undoPat[undoTop];
    redoBuf = eng.proj.pat[pi]; redoPat = pi;
    lockA(); eng.proj.pat[pi] = undoBuf[undoTop]; unlockA();
    if (pi == curPi()) undoShadow = undoBuf[undoTop];
    undoDirty = false;
    toast(TR("RÜCKGÄNGIG: PATTERN %02d (%d WEITERE)", "UNDO: PATTERN %02d (%d MORE)"), pi + 1, undoCnt);
}

// WAV-Aufnahme der Summe (LIVE-Seite, R1): Puffer im Speicher, beim Stoppen in samples/rec/ schreiben (Thread)
enum { REC_MAX_SEC = 300 };
static uint8_t* recBuf = nullptr;   // 24 Bit (3 Byte je Sample), Stereo verschraenkt
static volatile int recPos = 0, recCap = 0, recRate = 44100;   // Positionen in Samples (nicht Bytes)
static Dither recDith;
static volatile bool recOn = false, recSaving = false;
static volatile int recDoneNo = 0;   // > 0: gespeichert als rec-NN.wav (Meldung im Hauptthread)
static void recPush(const float* in, int frames) {   // Audio-Thread
    if (!recOn) return;
    int n = frames * 2, room = recCap - recPos;
    if (n > room) { n = room; recOn = false; }
    pcmTo24(recBuf + (size_t)recPos * 3, in, n, recDith);
    recPos += n;
}
static int recWriter(void* arg) {
    char* path = (char*)arg;
    FILE* f = fopen(path, "wb");
    int n = recPos / 2;   // Frames
    if (f) {
        uint8_t hdr[44]; wavHeader(hdr, recRate, 2, 24, (uint32_t)n); fwrite(hdr, 1, 44, f);
        fwrite(recBuf, 6, (size_t)n, f);
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
    if (recSaving) { toast(TR("AUFNAHME WIRD NOCH GESPEICHERT ...", "STILL SAVING RECORDING ...")); return; }
    if (!recOn && !recBuf) {
        recRate = eng.sr;
        int cap = recRate * REC_MAX_SEC * 2;
        recBuf = (uint8_t*)malloc((size_t)cap * 3);
        if (!recBuf) { toast(TR("ZU WENIG SPEICHER FÜR DIE AUFNAHME", "NOT ENOUGH MEMORY FOR RECORDING")); return; }
        recPos = 0; recCap = cap; recOn = true;
        toast(TR("AUFNAHME LÄUFT (MAX. %d MIN)", "RECORDING (MAX. %d MIN)"), REC_MAX_SEC / 60);
        return;
    }
    recOn = false;
    if (recPos < recRate / 2) { free(recBuf); recBuf = nullptr; toast(TR("AUFNAHME ZU KURZ - VERWORFEN", "RECORDING TOO SHORT - DISCARDED")); return; }
    char dir[700]; snprintf(dir, sizeof dir, "%ssamples", basePath); mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%ssamples/rec", basePath); mkdir(dir, 0755);
    char* path = (char*)malloc(800);
    struct stat st;
    for (int i = 1; i < 1000; i++) { snprintf(path, 800, "%s/rec-%03d.wav", dir, i); if (stat(path, &st)) break; }
    recSaving = true;
    SDL_Thread* th = SDL_CreateThread(recWriter, "rec", path);
    if (th) SDL_DetachThread(th); else recWriter(path);
    toast(TR("AUFNAHME WIRD GESPEICHERT ...", "SAVING RECORDING ..."));
}
static void redoStep() {
    if (redoPat < 0) { toast(TR("NICHTS ZU WIEDERHOLEN", "NOTHING TO REDO")); return; }
    lockA(); eng.proj.pat[redoPat] = redoBuf; unlockA();
    toast(TR("WIEDERHOLT: PATTERN %02d", "REDONE: PATTERN %02d"), redoPat + 1);
    redoPat = -1;
}

// Seiten: Spurseiten haben eine Speicherseite (je 8 Parameter), globale nicht
static bool trackPage(int p) { return p >= 0 && p < NUI_PAGES && PG_STORE[p] >= 0; }
static int sp() { return PG_STORE[page]; }
static bool pageAvail(int p) {
    if (p == PG_SRC2) return machineHasSrc2(curP().machine[track]);
    if (p == PG_SRC3 || p == PG_SRC4 || p == PG_SRC5) return machineHasSrc35(curP().machine[track]);
    if (p == PG_ARP) return !machineIsDrum(curP().machine[track]);   // Arpeggiator nur fuer Klangerzeuger mit Tonhoehe
    return true;
}
// Seitenringe fuer L2/R2: innerhalb der Gruppe (Spur / Live, Mix & FX / Projekt), Reihenfolge wie der Signalfluss
static const int RING_TRK[] = {PG_TRG, PG_SEQ, PG_ARP, PG_SRC, PG_SRC2, PG_SRC3, PG_SRC4, PG_SRC5, PG_FLT, PG_FLT2, PG_AMP, PG_INS, PG_SEND, PG_LFO1, PG_LFO2, PG_LFO3};
enum { RING_TRK_N = sizeof RING_TRK / sizeof *RING_TRK };
static const int RING_MIX[] = {PG_LIVE, PG_MIX, PG_FX, PG_FXB, PG_FXC, PG_CMP};
static const int RING_PRJ[] = {PG_SONG, PG_PRJ, PG_MST, PG_SYS};
static void pageRing(int p, const int*& r, int& n) {
    for (int i = 0; i < 6; i++) if (RING_MIX[i] == p) { r = RING_MIX; n = 6; return; }
    for (int i = 0; i < 4; i++) if (RING_PRJ[i] == p) { r = RING_PRJ; n = 4; return; }
    r = RING_TRK; n = RING_TRK_N;
}
// Seitentitel je Engine (FM: Operatoren, SYNTH/WTBL: Oszillatoren)
static const char* pageLong(int p) {
    int m = curP().machine[track];
    if (m == M_FM) {
        if (p == PG_SRC) return TR("FM: ALGORITHMUS", "FM: ALGORITHM");
        if (p == PG_SRC2) return "OPERATOR 1";
        if (p == PG_SRC3) return "OPERATOR 2";
        if (p == PG_SRC4) return "OPERATOR 3";
        if (p == PG_SRC5) return "OPERATOR 4";
    }
    if (p == PG_SRC2 && m == M_SYNTH) return TR("OSZILLATOREN", "OSCILLATORS");
    if (p == PG_SRC2 && m == M_WTBL) return TR("OSZILLATOR + EIGENE WAVETABLE", "OSCILLATOR + USER WAVETABLE");
    return PAGE_LONG[p];
}
static void gotoPage(int dir) {
    const int* r; int n; pageRing(page, r, n);
    int cur = 0; for (int i = 0; i < n; i++) if (r[i] == page) cur = i;
    for (int k = 0; k < n; k++) { cur = (cur + dir + n) % n; if (pageAvail(r[cur])) { page = r[cur]; return; } }
}
// START: gestoppt -> sofort Play; beim Spielen kurz = Stop (beim Loslassen), halten = FILL solange gedrueckt
static bool startHeldPlaying = false, startFill = false;
static bool startTap(Input& I, Uint32 now) {
    if (I.pressed(BT_START)) {
        if (!eng.playing) { startHeldPlaying = false; return true; }
        startHeldPlaying = true; startFill = false;
    }
    if (startHeldPlaying && I.down[BT_START] && !startFill && I.heldMs(BT_START, now) > 250) { startFill = true; eng.fill = true; }
    if (startHeldPlaying && !I.down[BT_START]) {
        startHeldPlaying = false;
        if (startFill) { startFill = false; eng.fill = false; return false; }
        return true;
    }
    return false;
}
static void startStop() {
    lockA();
    if (eng.playing) eng.stop(); else eng.start();
    unlockA();
    if (!eng.playing && eng.clockMode == 2) toast(TR("WARTE AUF MIDI-START", "WAITING FOR MIDI START"));
}
static int curScale() { return eng.proj.mst[MS_SCALE]; }

static bool stepHasLock(const Step& s) {
    for (int i = 0; i < NPARAMS; i++) if (s.lock[i] != NO_LOCK) return true;
    return false;
}

static void battery(int x, int y) {
    if (batPct < 0) return;
    Col c = batPct <= 15 ? C_TRIG : (batPct <= 30 ? C_LOCK : C_DIM);
    if (batPlug) c = C_GREEN;
    if (batPct <= 10 && !batPlug && (SDL_GetTicks() / 500) % 2) c = dimC(C_TRIG, 0.4f);
    rrect(x, y, 22, 11, 2, c);
    rrect(x + 1, y + 1, 20, 9, 2, C_PANEL);
    fill(x + 22, y + 3, 2, 5, c);
    int w = clampi(batPct * 18 / 100, 0, 18);
    if (batPlug && !batFull) {   // Laden: Fuellung laeuft vom Ladestand bis voll
        float f = fmodf(animTime * 0.6f, 1.f);
        int w2 = w + (int)((18 - w) * f);
        if (w2 > w) rrect(x + 2, y + 2, w2, 7, 1, dimC(C_GREEN, 0.35f + 0.3f * (1.f - f)));
    }
    if (w) rrect(x + 2, y + 2, w, 7, 1, c);
    if (batPlug) {   // Ladeblitz links neben dem Symbol, pulsiert
        float pl = 0.6f + 0.4f * sinf(animTime * 4.f);
        Col bc = mix(C_LOCK, C_PLAY, batFull ? 0.f : 0.3f * pl);
        int bx = x - 10, by = y - 3;
        for (int i = 0; i < 7; i++) fill(bx + 5 - i / 2, by + i, 3, 1, bc);        // obere Haelfte (schraeg)
        fill(bx + 1, by + 7, 7, 2, bc);                                            // Querbalken
        for (int i = 0; i < 6; i++) fill(bx + 5 - i / 2, by + 9 + i, 2, 1, bc);    // untere Spitze
    }
    char b[8]; snprintf(b, sizeof b, "%d%%", batPct);
    text(x + 27, y - 3, b, c);
}

static void drawIcon(int m, int x, int y, int s, Col c);
// ---------------------------------------------------------------- Touch-Flaechen (Fernbedienung im Browser)
// Jede Zeichenfunktion meldet ihre bedienbaren Flaechen; Touch prueft gegen das zuletzt gezeichnete Bild (oberste zuerst).
enum HitKind { HK_TRACK, HK_PAGE, HK_SLOT, HK_STEP, HK_MUTE, HK_SOLO, HK_MIXCH, HK_MENU, HK_YES, HK_NO, HK_LIVE, HK_BTN, HK_SET, HK_SONG, HK_PRJ, HK_BLOCK };
struct Hit { short x, y, w, h; uint8_t k; short a, b; };
enum { MAX_HITS = 320 };
static Hit hits[MAX_HITS];
static int nHits = 0, hitModalAt = 0;
static void hitAdd(int x, int y, int w, int h, int k, int a = 0, int b = -1) {
    if (nHits < MAX_HITS) hits[nHits++] = Hit{(short)x, (short)y, (short)w, (short)h, (uint8_t)k, (short)a, (short)b};
}
static void hitModal() { hitModalAt = nHits; hitAdd(0, 0, SW, SH, HK_BLOCK); }   // Fenster darueber: darunter nichts mehr treffen

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
    snprintf(b, sizeof b, "P%02d", curPi() + 1);
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
        snprintf(b, sizeof b, "SNG%d/%d", eng.songPos + 1, eng.proj.songLen);
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
        Col hc = eng.muted(track) ? C_DIM : mix(tc, C_PLAY, flash[track] * 0.35f);
        int is = 30 + (int)(flash[track] * 4);
        drawIcon(m, 206 - is / 2, 20 - is / 2, is, hc);
        text(226, 4, MACHINE_NAMES[m], hc, 2);
        snprintf(b, sizeof b, "T%d", track + 1);
        text(310, 2, b, C_DIM);
    }
    if (trackPage(page)) {   // Signalweg-Oberflaeche: Pfad statt Reiter
        snprintf(b, sizeof b, TR("SPUR %d > %s", "TRACK %d > %s"), track + 1, pageLong(page));
        text(334, 4, b, C_TEXT);
    } else
    {   // Reiter nur der aktuellen Gruppe (Seitenmenue: SELECT kurz antippen)
        int g = PG_GROUP[page], x = 334, right = 612;
        int list[NUI_PAGES], n = 0, cur = 0;
        const int* ring; int rn; pageRing(page, ring, rn); (void)g;   // gleiche Reihenfolge wie L2/R2 und die Seitenkarte
        for (int k = 0; k < rn; k++) { int p = ring[k]; if (pageAvail(p)) { if (p == page) cur = n; list[n++] = p; } }
        // zu viele Reiter: so weit nach links schieben, dass die aktuelle Seite sichtbar bleibt
        int first = 0;
        auto widthFrom = [&](int f, int upto) { int w = 0; for (int i = f; i <= upto; i++) w += textW(PAGE_NAMES[list[i]]) + 5; return w; };
        while (first < cur && widthFrom(first, cur) + (first ? 10 : 0) > right - x) first++;
        if (first > 0) { text(x, 4, "<", C_DIM); x += 10; }
        for (int i = first; i < n; i++) {
            int p = list[i], w = textW(PAGE_NAMES[p]) + 4;
            if (x + w > right) { text(right - 4, 4, ">", C_DIM); break; }
            hitAdd(x - 2, 0, w + 4, 22, HK_PAGE, p);
            if (p == page) { rrect(x, 3, w, 18, 4, C_ACC); text(x + 2, 4, PAGE_NAMES[p], C_BG); }
            else text(x + 2, 4, PAGE_NAMES[p], C_DIM);
            x += w + 1;
        }
    }
    if (aHeld) {
        snprintf(b, sizeof b, lockLatch ? TR("LOCK-MODUS STEP %02d", "LOCK MODE STEP %02d") : TR("TRIG %02d HALTEN: P-LOCK", "HOLD TRIG %02d: P-LOCK"), holdStep + 1);
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


// Wert + Anzeige eines Reglers der aktuellen Seite ermitteln
static bool knobInfo(int k, ParamDef& d, int& val, bool& locked, int& baseVal, char* label, int ln) {
    const Pattern& P = curP();
    locked = false; label[0] = 0;
    if (page == PAGE_MST) {
        d = MST_DEFS[k];
        val = k == MS_BPM ? eng.proj.bpm : eng.proj.mst[k];
    } else if (page == PG_PRJ) {
        static const ParamDef PRJ_DE[8] = {
            {TR("LADEN...", "LOAD..."), K_ACT, 0, 0}, {TR("SPEICHERN", "SAVE"), K_ACT, 0, 0}, {TR("SPEICH. ALS", "SAVE AS..."), K_ACT, 0, 0}, {TR("NEU...", "NEW..."), K_ACT, 0, 0},
            {TR("SOUND LADEN", "LOAD SOUND"), K_ACT, 0, 0}, {TR("SOUND SPEICH.", "SAVE SOUND"), K_ACT, 0, 0}, {"INIT SOUND", K_ACT, 0, 0}, {"DEMO-SONG", K_ACT, 0, 0}};
        static const ParamDef PRJ_EN[8] = {
            {"LOAD...", K_ACT, 0, 0}, {"SAVE", K_ACT, 0, 0}, {"SAVE AS...", K_ACT, 0, 0}, {"NEW...", K_ACT, 0, 0},
            {"LOAD SOUND", K_ACT, 0, 0}, {"SAVE SOUND", K_ACT, 0, 0}, {"INIT SOUND", K_ACT, 0, 0}, {"DEMO SONG", K_ACT, 0, 0}};
        d = g_lang ? PRJ_EN[k] : PRJ_DE[k];
        val = 0;
        if (k == 1) snprintf(label, ln, "%.18s", cfg.project);                      // aktuelles Projekt unter SPEICHERN
        if (k == 5 || k == 6) snprintf(label, ln, "%s %d", MACHINE_NAMES[P.machine[track] % M_COUNT], track + 1);
    } else if (page == PG_MIX) {
        d = PDEF_NONE;
        val = 0;
    } else if (busOfPage(page) >= 0) {   // Effekt-Bus: Regler je nach Effekt-Typ
        const uint8_t* bp = eng.proj.bus[busOfPage(page)];
        d = busDef(busOfPage(page), bp[BX_TYPE], k);
        val = bp[k];
        if (k == BX_FWD && d.kind != K_NONE) snprintf(label, ln, TR("> BUS %c", "> BUS %c"), 'A' + busOfPage(page) + 1);
    } else if (page == PAGE_CMP) {
        d = CMP_DEFS[k];
        val = k == CM_GR ? 0 : eng.proj.cmp[k];
        if (k == CM_SRC && val) snprintf(label, ln, "SC: T%d %s", val, MACHINE_NAMES[P.machine[val - 1]]);
        if (k == CM_THR && val >= 127 && eng.proj.cmp[CM_GAIN] == 0) snprintf(label, ln, TR("THR (AUS)", "THR (OFF)"));
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
            else snprintf(label, ln, "%s", usbMode ? "R36S AM PC" : TR("GERÄTE AM R36S", "DEVICES ON R36S"));
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
        if (d.kind == K_LDEST) snprintf(label, ln, "%s", val > 0 ? TR("ZIEL", "TARGET") : "DEST");
        if (d.kind == K_SCALE && val > 0) snprintf(label, ln, "%s", SCALE_LONG[((val - 1) / 12) % NSCALES]);
        if (d.kind == K_ROOT) snprintf(label, ln, TR("GRUNDTON", "ROOT"));
        if (d.kind == K_SMPL) {
            d.max = (uint8_t)(sampleCount() > 0 ? sampleCount() - 1 : 0);
            const char* sn = sampleGet(val) ? sampleName(val) : nullptr;
            const char* sl = sn ? strrchr(sn, '/') : nullptr;   // nur der Dateiname, Ordner zeigt der Explorer
            if (sn) snprintf(label, ln, "%.19s", sl ? sl + 1 : sn);
            else snprintf(label, ln, "%s", libCount() ? TR("(leer)", "(empty)") : TR("KEINE SAMPLES", "NO SAMPLES"));
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
    const char* tn[FT_COUNT] = {"LOWPASS 12DB", "BANDPASS", "HIGHPASS 12DB", "LOWPASS 24DB", "LADDER 24DB", "HIGHPASS 24DB",
                                       "NOTCH", "PEAK", TR("KAMM +", "COMB +"), TR("KAMM -", "COMB -"), TR("VOKAL A-E-I-O-U", "VOWEL A-E-I-O-U")};
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
    const char* t = on ? "BASE-WIDTH" : TR("BASE-WIDTH AUS", "BASE-WIDTH OFF");
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
    else snprintf(b, sizeof b, TR("KEIN ZIEL", "NO TARGET"));
    text(gx + 10, gy + 4, b, dest > 0 && dep != 64 ? c : C_DIM);
}

// Oszillator: eine gemischte Wellenform (OSC1 + OSC2 + SUB) wie sie klingt; Verstimmung = Bewegung
static void oscGraph(int gx, int gy, int gw, int gh, int w1, int w2, int det, int mixv, int pwv, int subv, int sel, Col c) {
    graphBox(gx, gy, gw, gh);
    int x0 = gx + 10, W = gw - 20, mid = gy + gh / 2, A = (gh - 18) / 2;
    float pw = 0.5f + (pwv - 64) / 140.f, m = mixv / 127.f, sub = subv / 127.f * 0.8f;
    float ratio = exp2f((det - 64) / 64.f * 0.5f / 12.f);
    float drift = oscDrift * (ratio - 1.f) * 40.f;   // sichtbares TR("Wandern", "Wander") von OSC2
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
        float lv = lvl[k] / 127.f;
        Col bc = car ? mix(dimC(c, 0.35f + 0.65f * lv), C_PLAY, glow * 0.4f) : mix(C_SEL, c, glow * 0.6f);
        if (k == selOp) { float p = 0.5f + 0.5f * sinf(animTime * 7.f); rrect(bx[k] - 3, by[k] - 3, bw + 6, bh + 6, 6, c, (Uint8)(60 + 60 * p)); }
        rrect(bx[k], by[k], bw, bh, 4, bc);
        if (!car) { rrect(bx[k] + 1, by[k] + 1, bw - 2, bh - 2, 3, C_PANEL); fill(bx[k] + 2, by[k] + bh - 3, (int)((bw - 4) * lv), 2, c); }
        char b[24];
        snprintf(b, sizeof b, "OP%d x%.2g", k + 1, FM_RATIOS[rat[k] % FM_NRAT]);
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
    if (!sm) { textC(gx + gw / 2, gy + gh / 2 - 8, TR("KEINE SAMPLES IM ORDNER samples/", "NO SAMPLES IN FOLDER samples/"), C_DIM); return; }
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
    if (en < st) text(lo + 4, gy + 4, TR("<< RÜCKWÄRTS", "<< BACKWARD"), C_PLAY);
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
    const char* t = mode ? ARP_MODES[mode % 6] : TR("ARP AUS", "ARP OFF");
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
    const char* dn[6] = {TR("VORWÄRTS >>", "FORWARD >>"), TR("<< RÜCKWÄRTS", "<< BACKWARD"), "PING <->", TR("PENDEL <=>", "PENDULUM <=>"), TR("ZUFALL ?", "RANDOM ?"), TR("BETRUNKEN ~", "DRUNK ~")};
    char b[48];
    text(gx + 84, gy + 6, dn[dir], sel == 0 ? C_PLAY : c);
    if (puls > 0) snprintf(b, sizeof b, "EUKLID E(%d,%d) ROT %d", puls > len ? len : puls, len, rot % len);
    else snprintf(b, sizeof b, "%d STEPS  %s", len, SPEED_NAMES[P.speed[track] % 7]);
    text(gx + 84, gy + 24, b, puls > 0 ? c : C_DIM);
    if (dens > 0 || range > 0) snprintf(b, sizeof b, "GEN %d%%  MUT %d%%  MEL +%d", dens, chng, range);
    else snprintf(b, sizeof b, TR("GENERATOR AUS", "GENERATOR OFF"));
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
static inline float fastTanhUi(float x) { return x < -3.f ? -1.f : (x > 3.f ? 1.f : x * (27.f + x * x) / (27.f + 9.f * x * x)); }
static float symScroll() { return fmodf(animTime * 0.35f, 1.f); }   // Laufschrift fuer periodische Wert-Symbole
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
    if (w > 8) {   // Leuchtpunkt laeuft die Kurve entlang (je Kachel versetzt), mit kurzem Schweif
        int dx = (int)(fmodf(animTime * 0.45f + x * 0.0137f + y * 0.0071f, 1.f) * w);
        for (int k = 10; k >= 0; k--) {
            int i = dx - k; if (i < 0) continue;
            float v = f((float)i / w); v = v < 0 ? 0 : (v > 1 ? 1 : v);
            int yy = y + h - (int)(v * h);
            if (k) fill(x + i, yy - 1, 1, 3, mix(c, C_PLAY, (10 - k) / 14.f));
            else { disc(x + i, yy, 3, mix(c, C_PLAY, 0.7f)); fill(x + i, yy, 1, 1, C_PLAY); }
        }
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
    static const char* DEC[] = {"DCAY", "DEC", "REL", "O.DEC", "O.REL", "N.DEC", "M.DEC", "F.DEC", "F.REL", "DEC1", "DEC2", "DEC3", "DEC4", "R.DAMP", 0};
    static const char* ATK[] = {"ATK", "F.ATK", "O.ATK", 0};
    static const char* SUS[] = {"SUS", "F.SUS", "O.SUS", 0};
    static const char* PITCH[] = {"TUNE", "PTCH", "FINE", "DETN", "RAT1", "RAT2", "RAT3", "RAT4", "RATE", "SPD", 0};
    static const char* LEVEL[] = {"VOL", "GAIN", "LVL", "LVL1", "LVL2", "LVL3", "LVL4", "O.LVL", 0};
    static const char* NOISE[] = {"NOIS", "GRIT", "SNAP", "CLCK", "METL", "SPRY", "P.SPR", 0};
    static const char* DRIVE[] = {"DRIV", "DRIVE", "BODY", 0};
    static const char* GLIDE[] = {"GLID", "SWEP", "S.TIM", 0};
    static const char* BRIGHT[] = {"TONE", "D.TONE", 0};
    static const char* BIPARROW[] = {"ENV", "P.ENV", "VELF", "DEPTH", "WARP", 0};
    static const char* WIDTH[] = {"D.PING", "R.WIDE", "WIDTH", 0};
    static const char* ROOM[] = {"REV", "R.SIZE", 0};
    static const char* MIXS[] = {"MIX", 0};

    if (d.kind == K_MACH) { int bob = (int)(sinf(animTime * 3.f) * 2.f); drawIcon(val, cx - h / 2, y + bob, h, mix(c, C_PLAY, 0.35f * (0.5f + 0.5f * sinf(animTime * 3.f)))); return; }
    if (d.kind == K_WAVE || (d.kind == K_LWAV)) {
        if (d.kind == K_WAVE) symCurve(x, y + 4, w, h - 8, c, [&](float t) { return symWave(val, t * 2.f + symScroll()); }, false);
        else symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + 0.5f * lfoWaveUi(val, t * 2.f + symScroll()); }, false);
        return;
    }
    if (!strcmp(n, "NOTE")) {   // Klaviatur, Taste der Note hervorgehoben
        static const int wk[7] = {0, 2, 4, 5, 7, 9, 11}, bk[5] = {1, 3, 6, 8, 10}, bp[5] = {0, 1, 3, 4, 5};
        int kw = w / 7, pc = val % 12;
        Col kc = mix(c, C_PLAY, 0.6f * (0.5f + 0.5f * sinf(animTime * 3.f)));
        for (int i = 0; i < 7; i++) rrect(x + i * kw, y + 4, kw - 1, h - 6, 2, wk[i] == pc ? kc : C_TEXT);
        for (int i = 0; i < 5; i++) rrect(x + bp[i] * kw + kw * 2 / 3, y + 4, kw * 2 / 3, (h - 6) * 3 / 5, 2, bk[i] == pc ? kc : C_BG);
        int kx = -1; for (int i = 0; i < 7; i++) if (wk[i] == pc) kx = x + i * kw + kw / 2; for (int i = 0; i < 5; i++) if (bk[i] == pc) kx = x + bp[i] * kw + kw;
        if (kx >= 0) { float f2 = fmodf(animTime * 1.2f, 1.f); int r2 = 2 + (int)(f2 * kw); ring(kx, y + h / 2, r2, 1, dimC(C_PLAY, 1.f - f2)); }
        return;
    }
    if (d.kind == K_OCT || d.kind == K_SUBO) {   // Oktave bipolar: Mitte = 0, Balken nach oben (+) bzw. unten (-); ein Punkt huepft von 0 zur Oktave
        int lo = d.kind == K_OCT ? -3 : -2, hi = d.kind == K_OCT ? 3 : 0;
        int cur = d.kind == K_OCT ? val - 3 : -(val + 1);
        int nst = hi - lo + 1, sw2 = w / nst, unit = (h / 2 - 3) / (hi > -lo ? hi : -lo);
        fill(x, cy, nst * sw2, 1, C_DIM);
        for (int i = lo; i <= hi; i++) {
            int bx = x + (i - lo) * sw2, hh = abs(i) * unit;
            bool on = cur != 0 && (cur > 0 ? (i > 0 && i <= cur) : (i < 0 && i >= cur));
            Col bc = i == cur ? c : (on ? cd : C_OFF2);
            if (i == 0) rrect(bx, cy - 2, sw2 - 2, 5, 2, cur == 0 ? c : C_DIM);
            else rrect(bx, i > 0 ? cy - hh : cy + 1, sw2 - 2, hh, 2, bc);
        }
        int steps = abs(cur) + 1, hop = (int)(animTime * 3.f) % steps, hi2 = cur < 0 ? -hop : hop;
        float fy = fmodf(animTime * 3.f, 1.f);
        int ty = hi2 > 0 ? cy - hi2 * unit - 4 : (hi2 < 0 ? cy + (-hi2) * unit + 4 : cy - 5);
        disc(x + (hi2 - lo) * sw2 + sw2 / 2 - 1, ty - (int)(sinf(fy * 3.14159f) * 4.f) * (hi2 < 0 ? -1 : 1), 2, C_PLAY);
        return;
    }
    if (!strcmp(n, "OCT") && d.kind == K_CNT) {   // Arp-Oktavbereich: Treppe aufwaerts
        int nst = d.max + 1, cur = val; if (nst < 2) nst = 2;
        int sw2 = w / nst;
        for (int i = 0; i < nst; i++) { int hh = 4 + i * (h - 8) / (nst - 1); rrect(x + i * sw2, y + h - hh, sw2 - 2, hh, 2, i == cur ? c : (i < cur ? cd : C_OFF2)); }
        int hop = (int)(animTime * 3.f) % (cur + 1); float fy = fmodf(animTime * 3.f, 1.f);
        int hh = 4 + hop * (h - 8) / (nst - 1);
        disc(x + hop * sw2 + sw2 / 2 - 1, y + h - hh - 4 - (int)(sinf(fy * 3.14159f) * 5.f), 2, C_PLAY);
        return;
    }
    if (d.kind == K_RATIO) { float r = FM_RATIOS[val % FM_NRAT]; float cyc = r < 0.5f ? 0.5f : (r > 8.f ? 8.f : r); symCurve(x, y + 2, w, h - 4, c, [&](float t) { return 0.5f + 0.45f * sinf((t * cyc + symScroll()) * 6.2831853f); }, false); return; }
    if (!strcmp(n, "XMOD")) { symCurve(x, y + 2, w, h - 4, c, [&](float t) { float p = (t * 2.f + symScroll()) * 6.2831853f; return 0.5f + 0.45f * sinf(p + fr * 3.f * sinf(p * 2.f)); }, false); return; }
    if (!strcmp(n, "RING")) { symCurve(x, y + 2, w, h - 4, c, [&](float t) { float p = (t + symScroll()) * 6.2831853f; return 0.5f + 0.45f * sinf(p * 6.f) * (1.f - fr + fr * sinf(p)); }, false); return; }
    if (!strcmp(n, "GATE")) { symCurve(x, y + 4, w, h - 8, c, [&](float t) { float p = t * 4.f + symScroll() * 2.f; p -= floorf(p); return p < fr ? 0.9f : 0.05f; }); return; }
    if (!strcmp(n, "ACCT")) { for (int i = 0; i < 8; i++) { bool ac = ((i * 5 + 3) % 8) < fr * 8.f; int hh = ac ? h - 2 : h / 2; bool hot = i == (int)(animTime * 6.f) % 8; rrect(x + i * w / 8, y + h - hh, w / 8 - 3, hh, 2, hot ? C_PLAY : (ac ? c : cd)); } return; }
    if (!strcmp(n, "F.KRV") || !strcmp(n, "A.KRV")) { float k = exp2f((val - 64) / 32.f); symCurve(x, y + 2, w, h - 4, c, [&](float t) { return powf(1.f - t, k); }); return; }
    if (d.kind == K_ARPM) {   // Tonfolge des Arp-Modus, Licht laeuft mit
        static const int pat[6][8] = {{0,0,0,0,0,0,0,0}, {0,1,2,3,0,1,2,3}, {3,2,1,0,3,2,1,0}, {0,1,2,3,2,1,0,1}, {2,0,3,1,3,0,2,1}, {3,3,3,3,3,3,3,3}};
        int m = val % 6, hot = (int)(animTime * 6.f) % 8;
        for (int i = 0; i < 8; i++) {
            int lv = pat[m][i], bx = x + i * w / 8;
            if (m == 5) { for (int j = 0; j < 3; j++) fill(bx, y + h - 4 - j * (h - 6) / 3, w / 8 - 3, 3, i == hot ? C_PLAY : c); continue; }
            fill(bx, y + h - 4 - lv * (h - 6) / 3, w / 8 - 3, 4, m == 0 ? C_OFF2 : (i == hot ? C_PLAY : c));
        }
        return;
    }
    if (d.kind == K_ACHD) {   // Akkord: Notenpunkte uebereinander, Arp-Licht laeuft durch
        int nn = 2 + val % 3, hot = (int)(animTime * 6.f) % nn;
        for (int i = 0; i < 5; i++) fill(x + 4, y + 3 + i * (h - 6) / 4, w - 8, 1, C_OFF2);
        for (int i = 0; i < nn; i++) { int py = y + h - 5 - i * (h - 10) / (nn - 1 > 0 ? nn - 1 : 1), px = cx + (i - nn / 2) * 12; fill(px + 4, py - 12, 1, 12, c); disc(px, py, 4, i == hot ? C_PLAY : c); }
        return;
    }
    if (n[0] == 'P' && n[1] >= '1' && n[1] <= '3' && !n[2]) {   // Insert-Regler P1-P3: Bild je Effekt-Typ und Regler
        int ty = pv(P_ITYP) % INS_COUNT, k = n[1] - '1';
        float v = val / 127.f, t0 = animTime;
        auto band = [&](float pos, float q, float amt) {   // Frequenzgang mit Kerbe/Spitze bei pos
            return [=](float t) { float d2 = (t - pos) * q; return 0.55f + amt * expf(-d2 * d2); };
        };
        switch (ty) {
        case INS_CHORUS:
            if (k == 0) {   // RATE: zwei Stimmen laufen gegeneinander, Tempo = Wert
                float ph = sinf(t0 * (0.5f + v * 6.f)) * 0.25f;
                symCurve(x, y + 3, w, h - 6, cd, [&](float t) { return 0.5f + 0.42f * sinf((t * 2.f + symScroll()) * 6.2831853f); }, false);
                symCurve(x, y + 3, w, h - 6, c, [&](float t) { return 0.5f + 0.42f * sinf((t * 2.f + symScroll() + ph) * 6.2831853f); }, false);
            } else if (k == 1) {   // DEPTH: Kopie pendelt um so weiter, je tiefer
                for (int j = 2; j >= 0; j--) { float ph = sinf(t0 * 2.f + j * 2.1f) * v * 0.3f * (j ? 1.f : 0.f);
                    symCurve(x, y + 3, w, h - 6, j ? dimC(c, 0.45f) : c, [&](float t) { return 0.5f + 0.42f * sinf((t * 2.f + symScroll() + ph) * 6.2831853f); }, false); }
            } else {   // WIDTH: zwei Stimmen links/rechts, Abstand = Breite
                int sp = (int)(v * (w / 2 - 10) * (0.85f + 0.15f * sinf(t0 * 2.f)));
                text(x, cy - 8, "L", C_DIM); text(x + w - 8, cy - 8, "R", C_DIM);
                for (int j = -1; j <= 1; j += 2) { int px = cx + j * sp; disc(px, cy + (int)(sinf(t0 * 3.f + j) * 4.f), 5, c); ring(px, cy, 8 + (int)(fmodf(t0, 1.f) * 6), 1, dimC(c, 1.f - fmodf(t0, 1.f))); }
            }
            return;
        case INS_PHASER: case INS_FLANGER: {
            bool fl = ty == INS_FLANGER;
            float rate = k == 0 ? v : pv(P_IP1) / 127.f, dep = k == 1 ? v : pv(P_IP2) / 127.f, fb = k == 2 ? v : pv(P_IP3) / 127.f;
            float sw = 0.5f + 0.5f * sinf(t0 * (0.4f + rate * 5.f));
            float lo = 0.5f - dep * 0.4f, hi = 0.5f + dep * 0.4f, pos = lo + (hi - lo) * sw;
            if (k == 1) fill(x + (int)(lo * w), y, (int)((hi - lo) * w) + 1, h, dimC(c, 0.15f));   // DEPTH: Bereich der Bewegung
            if (fl) symCurve(x, y + 2, w, h - 4, c, [&](float t) { float cm = cosf((t / (0.06f + pos * 0.12f)) * 6.2831853f); return 0.5f + 0.4f * cm * (0.4f + 0.6f * fb); }, false);
            else {
                float amt = -(0.3f + 0.25f * fb);
                auto n1 = band(pos * 0.6f, 9.f + fb * 20.f, amt), n2 = band(pos * 0.6f + 0.35f, 9.f + fb * 20.f, amt);
                symCurve(x, y + 2, w, h - 4, c, [&](float t) { return fminf(n1(t), n2(t)) + (k == 2 ? fb * 0.25f * expf(-((t - pos * 0.6f - 0.17f) * 14.f) * ((t - pos * 0.6f - 0.17f) * 14.f)) : 0.f); });
            }
            if (k == 0) disc(x + (int)(pos * w), y + 2, 2, C_PLAY);
            return;
        }
        case INS_DRIVE:
            if (k == 0) { float g = 1.f + v * 9.f; symCurve(x, y + 3, w, h - 6, c, [&](float t) { float sv = sinf((t * 2.f + symScroll()) * 6.2831853f) * g; return 0.5f + 0.45f * fastTanhUi(sv); }, false); }
            else if (k == 1) { float tl = (v - 0.5f) * 0.8f; symCurve(x, y + 2, w, h - 4, c, [&](float t) { return 0.5f + tl * (t - 0.5f) + 0.04f * sinf(t * 20.f + t0 * 3.f); }); }
            else { int lv = 1 + (int)(v * 7.f); int hot = (int)(t0 * 8.f) % (lv + 1); for (int i = 0; i < 8; i++) { int hh = 4 + i * (h - 6) / 8; rrect(x + i * w / 8, y + h - hh, w / 8 - 3, hh, 2, i < lv ? (i == hot ? C_PLAY : c) : C_OFF2); } }
            return;
        case INS_RING: {   // FREQ/FINE: Traeger * Signal, Schwebung
            float cf = 2.f + (pv(P_IP1) / 127.f) * 14.f, fi = (pv(P_IP2) - 64) / 64.f;
            symCurve(x, y + 3, w, h - 6, cd, [&](float t) { return 0.5f + 0.42f * sinf((t * 1.5f + symScroll()) * 6.2831853f); }, false);
            symCurve(x, y + 3, w, h - 6, c, [&](float t) { float p = (t + symScroll() * 0.5f) * 6.2831853f; return 0.5f + 0.42f * sinf(p * 1.5f) * sinf(p * (cf + fi * (k == 1 ? 2.f : 0.5f)) + t0 * 2.f); }, false);
            return;
        }
        case INS_APAN: {   // RATE/DEPTH: Punkt wandert zwischen L und R
            float rate = k == 0 ? v : pv(P_IP1) / 127.f, dep = k == 1 ? v : pv(P_IP2) / 127.f;
            text(x, cy - 8, "L", C_DIM); text(x + w - 8, cy - 8, "R", C_DIM);
            fill(x + 12, cy, w - 24, 1, C_OFF2);
            int rx = (int)(dep * (w / 2 - 14));
            fill(cx - rx, cy - 1, 2 * rx + 1, 3, dimC(c, 0.3f));
            float ph = sinf(t0 * (0.5f + rate * 8.f));
            int px = cx + (int)(ph * rx);
            for (int j = 1; j < 5; j++) { float pp = sinf((t0 - j * 0.04f) * (0.5f + rate * 8.f)); disc(cx + (int)(pp * rx), cy, 4 - j / 2, dimC(c, 1.f - j * 0.2f)); }
            disc(px, cy, 6, c);
            return;
        }
        default: break;
        }
    }
    if (d.kind == K_LMODE) {   // LFO-Modus: Kurve laeuft frei / startet mit dem Trig neu / haelt / einmal / halbe Welle
        int m = val % 5; float sc = symScroll() * 2.f;
        symCurve(x, y + 3, w, h - 6, c, [&](float t) {
            float p = t * 2.f + (m == LM_FREE ? sc : 0.f);
            if (m == LM_TRIG) p = fmodf(t * 2.f, 1.f) * 1.4f;
            if (m == LM_HOLD) p = floorf(t * 4.f) * 0.37f;
            if (m == LM_ONE && t > 0.5f) p = 1.f;
            if (m == LM_HALF && t > 0.25f) p = 0.5f;
            return 0.5f + 0.42f * sinf(p * 6.2831853f);
        }, false);
        if (m == LM_TRIG) for (int i = 0; i < 2; i++) fill(x + i * w / 2, y, 1, h, C_PLAY);
        return;
    }
    if (d.kind == K_INS) {   // Insert-Typ als laufende Mini-Welle
        int ty = val % INS_COUNT; float sc = symScroll();
        if (ty == INS_OFF) { fill(x, cy, w, 1, C_DIM); disc(x + (int)(sc * w), cy, 2, C_DIM); return; }
        if (ty == INS_CHORUS || ty == INS_FLANGER) symCurve(x, y + 3, w, h - 6, cd, [&](float t) { return 0.5f + 0.4f * sinf((t * 3.f + sc + 0.1f * sinf(animTime * 2.f)) * 6.2831853f); }, false);
        symCurve(x, y + 3, w, h - 6, c, [&](float t) {
            float p = (t * 3.f + sc) * 6.2831853f, v = sinf(p);
            switch (ty) {
            case INS_PHASER: v = sinf(p + 1.5f * sinf(animTime * 1.5f + t * 3.f)); break;
            case INS_DRIVE: v = fmaxf(-0.6f, fminf(0.6f, v * 2.5f)) / 0.6f; break;
            case INS_RING: v = sinf(p * 3.f) * sinf(p * 0.5f + animTime); break;
            case INS_APAN: v = v * (0.6f + 0.4f * sinf(animTime * 3.f)); break;
            default: break;
            }
            return 0.5f + 0.42f * v;
        }, false);
        return;
    }
    if ((!strcmp(n, "STRT") || !strcmp(n, "END")) && curP().machine[track] == M_SAMPLE) {   // Ausschnitt im Sample, Kopf laeuft im Bereich
        float a = pv(P_S3) / 127.f, b2 = pv(P_S4) / 127.f, lo = fminf(a, b2), hi = fmaxf(a, b2);
        for (int i = 0; i < w; i += 2) { float t = i / (float)w; float am = fabsf(sinf(i * 0.37f + 3.f)) * expf(-t * 1.5f); int hh = (int)(am * h / 2); fill(x + i, cy - hh, 1, hh * 2 + 1, t >= lo && t <= hi ? c : C_OFF2); }
        int ax = x + (int)(a * w), bx = x + (int)(b2 * w);
        fill(ax, y, 1, h, !strcmp(n, "STRT") ? C_PLAY : C_DIM); fill(bx, y, 1, h, !strcmp(n, "END") ? C_PLAY : C_DIM);
        float f2 = fmodf(animTime * 0.5f, 1.f); fill(ax + (int)((bx - ax) * f2), y + 2, 2, h - 4, C_TEXT);
        return;
    }
    if (curP().machine[track] == M_GRAN && (!strcmp(n, "SPRY") || !strcmp(n, "SIZE") || !strcmp(n, "DENS") || !strcmp(n, "POS"))) {   // Koerner um die Position
        float pos = pv(P_S1) / 127.f, spr = pv(P_S2) / 127.f, sz = pv(P_S3) / 127.f, dn = pv(P_S4) / 127.f;
        int ng = 3 + (int)(dn * 12.f), gl = 3 + (int)(sz * w / 6);
        fill(x, cy, w, 1, C_OFF2);
        fill(x + (int)(pos * w), y, 1, h, C_DIM);
        for (int i = 0; i < ng; i++) {
            float life = fmodf(animTime * (0.7f + dn) + i / (float)ng, 1.f);
            uint32_t hsh = (uint32_t)(i * 2654435761u + (uint32_t)((animTime * (0.7f + dn) + i / (float)ng)) * 40503u);
            float off = ((hsh >> 8) % 1000 / 1000.f - 0.5f) * (0.05f + spr * 0.9f);
            int gx = x + (int)((pos + off) * w) - gl / 2, gy = y + 2 + (int)((hsh >> 20) % 1000 / 1000.f * (h - 6));
            float br = sinf(life * 3.14159f);
            if (gx >= x && gx + gl <= x + w) rrect(gx, gy, gl, 3, 1, dimC(mix(c, C_PLAY, br * 0.4f), 0.3f + 0.7f * br));
        }
        return;
    }
    if (!strcmp(n, "VEL")) { for (int i = 0; i < w; i += 4) { int hh = (int)((float)i / w * h); fill(x + i, y + h - hh, 3, hh, i < fr * w ? (fabsf(i / (float)w - fmodf(animTime * 0.7f, 1.f)) < 0.06f ? C_PLAY : c) : C_OFF2); } return; }
    if (d.kind == K_LEN) {   // Notenlaenge als Balken ueber einem Step-Raster
        for (int i = 0; i < 4; i++) fill(x + i * w / 4, y + h - 6, 1, 6, C_DIM);
        float s2 = lenSteps(val);
        int bw = s2 < 0 ? w : (int)fminf(w, s2 * w / 4.f);
        rrect(x, cy - 6, bw < 4 ? 4 : bw, 12, 5, c);
        if (s2 < 0) text(x + w - 16, cy - 8, ">", C_BG);
        { int px = x + (int)(fmodf(animTime * 0.5f, 1.f) * w); fill(px, cy - 9, 2, 18, px < x + bw ? C_PLAY : C_DIM); }
        return;
    }
    if (!strcmp(n, "PROB") || (!strcmp(n, "DENS") && d.kind == K_PCT)) {   // Torte
        int r = h / 2 - 2;
        disc(cx, cy, r, C_OFF2);
        float a1 = fr * 6.2831853f;
        for (float a = 0; a < a1; a += 0.04f) line(cx, cy, cx + (int)(r * sinf(a)), cy - (int)(r * cosf(a)), c);
        { float a = fmodf(animTime * 2.f, 6.2831853f); bool in = a < a1; disc(cx + (int)((r + 3) * sinf(a)), cy - (int)((r + 3) * cosf(a)), 2, in ? C_PLAY : C_DIM); }
        return;
    }
    if (d.kind == K_RTRG) { int nn = RTRG_DIV[val % 7]; if (!nn) { nn = 1; float f2 = fmodf(animTime * 0.8f, 1.f); disc(x + (int)(f2 * w), cy, 2, dimC(c, 1.f - f2)); } for (int i = 0; i < nn; i++) { int hh = h - i * h / (nn + 2); fill(x + i * w / nn, y + h - hh, 2, hh, val ? (i == (int)(animTime * 8.f) % nn ? C_PLAY : c) : C_DIM); } return; }
    if (d.kind == K_MTIM) {
        fill(x, cy, w, 1, C_DIM); fill(cx, y + 2, 1, h - 4, C_TEXT);
        int mx = cx + (val - 23) * (w / 2) / 23;
        float f2 = fmodf(animTime * 0.9f, 1.f); int gx = cx + (int)((mx - cx) * f2) + (val == 23 ? (int)(sinf(animTime * 4.f) * 6.f) : 0);
        rrect(gx - 3, cy - 5, 7, 10, 3, dimC(C_PLAY, 0.7f));
        rrect(mx - 4, cy - 8, 9, 16, 3, c);
        return;
    }
    if (d.kind == K_COND) {   // Punkte: A von B
        if (val >= CND_AB) {
            int a, b; bool ng; condDecode(val, a, b, ng);
            int r = w / (b * 2 + 1); if (r > 6) r = 6;
            int cur2 = (int)(animTime * 3.f) % b; for (int i = 0; i < b; i++) { bool on = (i == a - 1) != ng; int px = x + (i * 2 + 1) * w / (b * 2); if (on) disc(px, cy, r, i == cur2 ? C_PLAY : c); else ring(px, cy, r, 1, i == cur2 ? C_TEXT : C_DIM); }
        } else if (val) { char b2[8]; fmtParam(d, val, b2, sizeof b2); text(cx - textW(b2) / 2, cy - 8, b2, c); }
        else for (int i = 0; i < 4; i++) disc(x + (i * 2 + 1) * w / 8, cy, 4, i == (int)(animTime * 4.f) % 4 ? C_PLAY : c);
        return;
    }
    if (nameIs(n, DEC)) { float k = 1.f + (1.f - fr) * 12.f; symCurve(x, y + 2, w, h - 4, c, [&](float t) { return expf(-t * k); }); return; }
    if (nameIs(n, ATK)) { float a = 0.05f + fr * 0.9f; symCurve(x, y + 2, w, h - 4, c, [&](float t) { return t < a ? t / a : 1.f; }); return; }
    if (nameIs(n, SUS)) { symCurve(x, y + 2, w, h - 4, c, [&](float t) { return t < 0.15f ? t / 0.15f : (t < 0.35f ? 1.f - (1.f - fr) * (t - 0.15f) / 0.2f : fr); }); return; }
    if (nameIs(n, PITCH)) { float cyc = 1.f + fr * 5.f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + 0.5f * sinf((t * cyc + symScroll()) * 6.2831853f); }, false); return; }
    if (nameIs(n, LEVEL)) {   // Lautsprecher mit Schallwellen
        int sp = h / 5;
        fill(x + 4, cy - sp, 7, 2 * sp, c);
        for (int i = 0; i < 5; i++) fill(x + 11 + i, cy - sp - i * sp / 3, 2, 2 * sp + i * sp * 2 / 3, c);
        int waves = 1 + (int)(fr * 3.f + 0.5f);
        float mv = fmodf(animTime * 1.5f, 1.f);
        for (int k = 0; k < waves; k++) { int r = sp + 6 + (int)((k + mv) * (sp + 2)), ry = r * 2 / 3 < h / 2 - 1 ? r * 2 / 3 : h / 2 - 1;
            for (int dy = -ry; dy <= ry; dy++) { int dx = (int)sqrtf((float)(r * r - dy * dy)); fill(x + 14 + dx, cy + dy, 2, 1, dimC(c, 1.f - (k + mv) / 4.f)); } }
        return;
    }
    if (nameIs(n, NOISE)) { Rng rr; rr.s = 1234 + (int)(animTime * 14.f); for (int i = 0; i < w; i += 2) { int a = (int)(rr.next() * (0.12f + 0.88f * fr) * h / 2); fill(x + i, cy - (a > 0 ? a : 0), 2, abs(a) + 1, c); } return; }
    if (nameIs(n, DRIVE)) { float g = 1.f + fr * 8.f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { float sv = sinf((t * 2.f + symScroll()) * 6.2831853f) * g; return 0.5f + 0.5f * (sv > 1 ? 1 : (sv < -1 ? -1 : sv)); }, false); return; }
    if (!strcmp(n, "CRSH")) {   // Bitcrush: links die Bits als Zellen (je weniger, desto groeber), rechts die Welle als Treppe
        int bits = 8 - (int)(fr * 7.f + 0.5f), st = 1 << (bits > 4 ? 4 : bits);
        int cw2 = 10, ch = (h - 4) / 8;
        for (int i = 0; i < 8; i++) {
            bool on = i < bits; bool lsb = on && i == bits - 1;
            rrect(x, y + h - 2 - (i + 1) * ch, cw2, ch - 1, 1, on ? (lsb ? mix(c, C_PLAY, 0.5f + 0.5f * sinf(animTime * 8.f)) : c) : C_OFF2);
        }
        int sx = x + cw2 + 6, sw = w - cw2 - 6;
        for (int i = 0; i <= st && st <= 8; i++) fill(sx, y + 4 + i * (h - 8) / st, sw, 1, C_OFF2);
        symCurve(sx, y + 4, sw, h - 8, c, [&](float t) { float sv = 0.5f + 0.5f * sinf((t * 1.5f + symScroll()) * 6.2831853f); return floorf(sv * st + 0.5f) / st; }, false);
        return;
    }
    if (!strcmp(n, "SRR")) {   // Abtastrate: senkrechte Abtastpunkte (Stiel + Punkt), gehalten bis zum naechsten
        int st = 22 - (int)(fr * 18.f); float sc = symScroll();
        symCurve(x, y + 4, w, h - 8, cd, [&](float t) { return 0.5f + 0.5f * sinf((t + sc) * 6.2831853f); }, false);
        int by = y + h - 4, hh = h - 8, py = -1;
        for (int i = 0; i <= st; i++) {
            float t = (i - fmodf(sc * st, 1.f)) / st; if (t < 0) continue;
            float q = floorf((t + sc) * st + 0.5f) / st;
            int sx = x + (int)(t * w), sy = by - (int)((0.5f + 0.5f * sinf(q * 6.2831853f)) * hh);
            if (sx > x + w) break;
            if (py >= 0) fill(sx - (w / st), py, w / st, 1, c);   // Halten
            fill(sx, sy < by ? sy : by, 1, abs(by - sy) / 3 + 1, dimC(c, 0.5f));
            disc(sx, sy, st > 20 ? 1 : 2, i == (int)(animTime * 6.f) % (st + 1) ? C_PLAY : c);
            py = sy;
        }
        return;
    }
    if (nameIs(n, GLIDE)) { float sl = 0.05f + fr * 0.9f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { return t < sl ? 0.15f + 0.7f * t / sl : 0.85f; }, false); return; }
    if (nameIs(n, BRIGHT)) {   // Sonne: Strahlen = Helligkeit
        disc(cx, cy, h / 6, c);
        int rays = 3 + (int)(fr * 9.f);
        for (int i = 0; i < rays; i++) { float a = i * 6.2831853f / rays + animTime * 0.6f; line(cx + (int)(h / 4 * cosf(a)), cy + (int)(h / 4 * sinf(a)), cx + (int)(h / 2.2f * cosf(a)), cy + (int)(h / 2.2f * sinf(a)), c); }
        return;
    }
    if (nameIs(n, BIPARROW) || d.kind == K_BIP) {   // Pfeil nach oben/unten bzw. Ausschlag um die Mitte
        float v = (val - 64) / 64.f;
        fill(x, cy, w, 1, C_DIM);
        int len = (int)(fabsf(v) * (h / 2 - 2) * (0.85f + 0.15f * sinf(animTime * 3.f)));
        if (len) { fill(cx - 3, v > 0 ? cy - len : cy, 7, len, c); for (int i = 0; i < 6; i++) fill(cx - 6 + i, v > 0 ? cy - len - 6 + i : cy + len + 5 - i, 13 - 2 * i, 1, c);
                   float f2 = fmodf(animTime * 1.3f, 1.f); int py = cy + (int)((v > 0 ? -1 : 1) * f2 * len); fill(cx - 2, py - 1, 5, 3, mix(c, C_PLAY, 1.f - f2)); }
        else { float f2 = fmodf(animTime, 1.f); ring(cx, cy, 3 + (int)(f2 * 8), 1, dimC(c, 1.f - f2)); disc(cx, cy, 3, c); }
        return;
    }
    if (!strcmp(n, "PAN")) { fill(x + 10, cy, w - 20, 2, C_DIM); text(x, cy - 8, "L", C_DIM); text(x + w - 8, cy - 8, "R", C_DIM); { int px = x + 10 + (int)(fr * (w - 20)); float mv = fmodf(animTime, 1.f); ring(px, cy, 6 + (int)(mv * 8), 1, dimC(c, 1.f - mv)); disc(px, cy, 5, c); } return; }
    if (nameIs(n, WIDTH)) { int sp = (int)(fr * (w / 2 - 8) * (0.85f + 0.15f * sinf(animTime * 2.5f))); disc(cx - sp, cy, 6, c); disc(cx + sp, cy, 6, c); fill(cx - sp, cy, 2 * sp, 1, cd); return; }
    if (!strcmp(n, "DLY")) { int nn = 1 + (int)(fr * 5.f); for (int i = 0; i < nn; i++) { int hh = (int)(h * powf(0.7f, (float)i)); fill(x + i * w / 6, y + h - hh, w / 10, hh, i == (int)fmodf(animTime * 3.f, (float)nn) ? C_PLAY : (i ? cd : c)); } return; }
    if (nameIs(n, ROOM)) { int nn = 1 + (int)(fr * 4.f); float mv = fmodf(animTime * 0.6f, 1.f); for (int i = 0; i < nn; i++) { float rr2 = (i + mv) / nn; ring(cx, cy, 3 + (int)(rr2 * (h / 2 - 3) * (0.4f + 0.6f * fr)), 1, dimC(c, 1.f - rr2 * 0.8f)); } disc(cx, cy, 2, c); return; }
    if (nameIs(n, MIXS)) { int r = h / 3; float br = 1.f + 0.08f * sinf(animTime * 3.f); disc(cx - r / 2, cy, (int)(r * (1.f - fr * 0.6f) * br), cd); disc(cx + r / 2, cy, (int)(r * (0.4f + fr * 0.6f) * (2.f - br)), c); return; }
    if (!strcmp(n, "PW")) { float pw = 0.5f + (val - 64) / 140.f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { float p = t * 2.f + symScroll(); p -= floorf(p); return p < pw ? 1.f : 0.f; }, false); return; }
    if (!strcmp(n, "SUB")) { symCurve(x, y + 4, w, h - 8, cd, [&](float t) { return 0.5f + 0.4f * sinf((t * 4.f + symScroll() * 2.f) * 6.2831853f); }, false); symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + 0.5f * fr * sinf((t * 2.f + symScroll()) * 6.2831853f); }, false); return; }
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
    if (!strcmp(n, "PHASE")) { int r = h / 2 - 2; ring(cx, cy, r, 1, cd); float a = fr * 6.2831853f; line(cx, cy, cx + (int)(r * sinf(a)), cy - (int)(r * cosf(a)), cd, 1); a += animTime * 2.f; line(cx, cy, cx + (int)(r * sinf(a)), cy - (int)(r * cosf(a)), c, 2); return; }
    if (!strcmp(n, "FADE")) { float v = (val - 64) / 64.f; symCurve(x, y + 4, w, h - 8, c, [&](float t) { return v < 0 ? fminf(1.f, t / (-v + 0.01f)) * 0.9f : (v > 0 ? fmaxf(0.f, 1.f - t / (1.01f - v)) * 0.9f : 0.9f); }); return; }
    if (d.kind == K_LDEST) { { float f2 = fmodf(animTime, 1.f); ring(x + w - h / 2, cy, h / 3 + (int)(f2 * 5), 1, dimC(c, 1.f - f2)); } ring(x + w - h / 2, cy, h / 3, 2, c); disc(x + w - h / 2, cy, 3, c); line(x, cy, x + w - h / 2 - h / 3 - 2, cy, val ? c : C_DIM, 2); if (val) for (int i = 0; i < 3; i++) disc(x + (int)(fmodf(animTime * 0.8f + i / 3.f, 1.f) * (w - h / 2 - h / 3 - 2)), cy, 2, C_PLAY); return; }
    if (d.kind == K_LSYNC || d.kind == K_DTIM || !strcmp(n, "D.TIME")) {   // Notensymbol
        float bt = fmodf(animTime * eng.proj.bpm / 60.f, 1.f); int jy = -(int)(sinf(bt * 3.14159f) * 4.f);
        disc(x + w / 3, y + h - 8 + jy, 6, c); fill(x + w / 3 + 5, y + 4 + jy, 2, h - 12, c);
        if (d.kind == K_LSYNC && val == 0) { float f2 = fmodf(animTime, 1.f); ring(x + w * 2 / 3, cy, 4 + (int)(f2 * 8), 1, dimC(cd, 1.f - f2 * 0.7f)); }
        return;
    }
    if (d.kind == K_DIR) {
        static const char* ar[6] = {">>", "<<", "<>", "<=>", "??", "~~"};
        { int dx = (int)(sinf(animTime * 3.f) * 5.f) * (val % 6 == 1 ? -1 : 1); text(cx - textW(ar[val % 6], 2) / 2 + dx, cy - 16, ar[val % 6], mix(c, C_PLAY, 0.3f * (0.5f + 0.5f * sinf(animTime * 3.f))), 2); }
        return;
    }
    if (!strcmp(n, "PULS") || !strcmp(n, "ROT")) {
        int len = curP().len[track] < 1 ? 1 : curP().len[track], k = pv(P_PULS), rot = pv(P_EROT), r = h / 2 - 3;
        int nn = len > 32 ? 32 : len;
        for (int i = 0; i < nn; i++) { float a = i * 6.2831853f / nn; int px = cx + (int)(r * sinf(a)), py = cy - (int)(r * cosf(a)); bool hit = k > 0 && euclidHitUi(i * len / nn, k, len, rot); if (hit) disc(px, py, 3, c); else fill(px, py, 1, 1, C_DIM); }
        { int i = eng.playing && eng.playedStep[track] >= 0 ? eng.playedStep[track] % len * nn / len : (int)(animTime * 4.f) % nn; float a = i * 6.2831853f / nn; ring(cx + (int)(r * sinf(a)), cy - (int)(r * cosf(a)), 5, 1, C_PLAY); }
        return;
    }
    if (!strcmp(n, "CHNG")) {   // Wuerfel
        cx += (int)(sinf(animTime * 5.f) * (1.5f + 3.f * fr)); cy += (int)(fabsf(sinf(animTime * 2.5f)) * -3.f);
        rrect(cx - h / 3, cy - h / 3, h * 2 / 3, h * 2 / 3, 5, C_TEXT);
        int pips = 1 + (int)(fr * 5.f), s3 = h / 6;
        static const int px[6][6][2] = {{{0,0}},{{-1,-1},{1,1}},{{-1,-1},{0,0},{1,1}},{{-1,-1},{1,-1},{-1,1},{1,1}},{{-1,-1},{1,-1},{0,0},{-1,1},{1,1}},{{-1,-1},{1,-1},{-1,0},{1,0},{-1,1},{1,1}}};
        for (int i = 0; i < pips; i++) disc(cx + px[pips - 1][i][0] * s3, cy + px[pips - 1][i][1] * s3, 2, C_BG);
        return;
    }
    if (d.kind == K_RANGE) { int st2 = 1 + val / 6; for (int i = 0; i < 8; i++) { int sd = (int)(animTime * 3.f); int hh = (int)(((i * 37 + sd * 11 + (i * sd) % 5) % 8) / 8.f * h * fminf(1.f, st2 / 8.f)) + 2; fill(x + i * w / 8, y + h - hh, w / 8 - 2, hh, val ? (i == sd % 8 ? C_PLAY : c) : C_DIM); } return; }
    if (d.kind == K_SMPL) { for (int i = 0; i < w; i += 3) { float a = fabsf(sinf(i * 0.37f + val)) * expf(-i / (float)w * 1.5f); int hh = (int)(a * h / 2); fill(x + i, cy - hh, 2, hh * 2 + 1, c); } fill(x + (int)(fmodf(animTime * 0.4f, 1.f) * w), y, 1, h, C_PLAY); return; }
    if (d.kind == K_OFFON) { if (!strcmp(n, "LOOP") || !strcmp(n, "REV")) { int dx = (int)(fmodf(animTime * 1.5f, 1.f) * 10.f) - 5; bool rv = !strcmp(n, "REV") && val; text(cx - 16 + (rv ? -dx : dx), cy - 16, !strcmp(n, "REV") ? (val ? "<<" : ">>") : (val ? "()" : "->"), val ? mix(c, C_PLAY, 0.3f * (0.5f + 0.5f * sinf(animTime * 3.f))) : C_DIM, 2); return; }
        rrect(cx - 18, cy - 9, 36, 18, 9, val ? mix(c, C_PLAY, 0.15f + 0.15f * sinf(animTime * 3.f)) : C_OFF2); disc(val ? cx + 9 : cx - 9, cy, 7, C_TEXT); disc(val ? cx + 9 : cx - 9, cy, 2 + (int)(2.f * (0.5f + 0.5f * sinf(animTime * 3.f))), val ? c : C_DIM); return; }
    if (d.kind == K_ALGO) {   // Mini-Routing
        for (int k = 0; k < 4; k++) { int bx = x + FM_LAYOUT[val & 7][k][0] * w / 4, by = y + FM_LAYOUT[val & 7][k][1] * h / 3; rrect(bx, by + 2, w / 5, h / 4, 2, FM_CARRIERS[val & 7] >> k & 1 ? mix(c, C_PLAY, 0.3f + 0.3f * sinf(animTime * 4.f + k)) : cd); }
        return;
    }
    if (d.kind == K_WTAB || !strcmp(n, "POS")) {
        if (curP().machine[track] == M_WTBL) { const float* f2 = wavetableFrame(pv(P_S0) & 3, pv(P_S1) * 31 / 127); symCurve(x, y + 4, w, h - 8, c, [&](float t) { return 0.5f + 0.5f * f2[(int)(t * 255)]; }, false); return; }
    }
    if (d.kind == K_SLICE || !strcmp(n, "SLCS")) { int nn = pv(P_S1) + 1; int sl = pv(P_S2); for (int i = 0; i < nn && i < 32; i++) fill(x + i * w / (nn < 32 ? nn : 32), y + 4, 1, h - 8, C_DIM); int cs = sl ? (sl - 1) % nn : (int)(animTime * 4.f) % nn; fill(x + cs * w / nn, y + 4, w / nn > 2 ? w / nn : 2, h - 8, sl ? c : dimC(C_PLAY, 0.6f)); return; }
    if (!strcmp(n, "BPM")) {   // Metronom mit Pendel im Takt
        for (int i = 0; i < h; i++) { int hw = i * w / (3 * h); fill(cx - hw, y + i, 2 * hw + 1, 1, C_OFF2); }
        float a = sinf(animTime * 3.14159f * eng.proj.bpm / 60.f) * 0.5f;
        line(cx, y + h - 4, cx + (int)(sinf(a) * h * 0.9f), y + h - 4 - (int)(cosf(a) * h * 0.9f), c, 2);
        return;
    }
    if (d.kind == K_SWNG) { float bt = fmodf(animTime * eng.proj.bpm / 120.f, 1.f); int sx = x + w / 2 + (int)(fr * w / 3); int j1 = -(int)(sinf(bt * 3.14159f) * 4.f), j2 = -(int)(sinf(fmodf(bt + 0.5f + fr * 0.3f, 1.f) * 3.14159f) * 4.f); disc(x + w / 4, y + h - 8 + j1, 6, c); fill(x + w / 4 + 5, y + 4 + j1, 2, h - 12, c); disc(sx, y + h - 8 + j2, 6, c); fill(sx + 5, y + 4 + j2, 2, h - 12, c); return; }
    if (d.kind == K_ROOT) {   // Tastatur, Grundton leuchtet
        static const int wk[7] = {0, 2, 4, 5, 7, 9, 11}, bk[5] = {1, 3, 6, 8, 10}, bp[5] = {0, 1, 3, 4, 5};
        int kw = w / 7;
        Col rc = mix(C_ACC, C_PLAY, (0.5f + 0.5f * sinf(animTime * 3.f))); for (int i = 0; i < 7; i++) rrect(x + i * kw, y + 4, kw - 1, h - 6, 2, wk[i] == val ? rc : C_OFF2);
        for (int i = 0; i < 5; i++) rrect(x + bp[i] * kw + kw - kw / 3, y + 4, kw * 2 / 3, (h - 6) * 3 / 5, 2, bk[i] == val ? rc : C_BG);
        return;
    }
    if (d.kind == K_SCALE) {
        static const int wk[7] = {0, 2, 4, 5, 7, 9, 11};
        int kw = w / 7;
        int hot = (int)(animTime * 5.f) % 7; for (int i = 0; i < 7; i++) { bool on = val == 0 || inScale(wk[i], val); rrect(x + i * kw, y + 4, kw - 1, h - 6, 2, on ? (i == hot ? C_PLAY : c) : C_OFF2); }
        return;
    }
    if (d.kind == K_THR) { fill(x, y + h - (int)(fr * h), w, 2, c); symCurve(x, y, w, h, cd, [&](float t) { return 0.5f + 0.4f * sinf(t * 18.f) * expf(-t); }, false); return; }
    if (d.kind == K_CRAT) { float r2 = val >= 8 ? 100.f : CMP_RATIOS[val]; symCurve(x, y + 2, w, h - 4, c, [&](float t) { return t < 0.5f ? t : 0.5f + (t - 0.5f) / r2; }, false); return; }
    if (d.kind == K_SCSRC) { if (val) { float fl = flash[(val - 1) % NTRACKS]; ring(cx, cy, h / 2 - 2 + (int)((1.f - fl) * 4), 1, dimC(c, fl)); drawIcon(curP().machine[(val - 1) % NTRACKS], cx - h / 2, y, h, mix(c, C_PLAY, fl * 0.6f)); } else text(cx - 24, cy - 8, TR("SUMME", "MASTER"), c); return; }
    if (d.kind == K_GR) { int gh = clampi((int)(grDisp / 24.f * h), 0, h); rrect(cx - 8, y, 16, h, 4, C_OFF); if (gh) rrect(cx - 8, y, 16, gh, 4, C_TRIG); fill(cx - 8, y + (int)(fmodf(animTime * 0.8f, 1.f) * (h - 2)), 16, 2, dimC(C_TRIG, 0.5f)); return; }
    if (d.kind == K_FBK) { int nn = 1 + (int)(fr * 4.f); float mv = fmodf(animTime * 0.8f, 1.f); for (int i = 0; i < nn; i++) { float rr2 = (i + mv) / 4.f; ring(cx, cy, 3 + (int)(rr2 * (h / 2 - 3)), 1, val > 106 && i == nn - 1 ? C_TRIG : dimC(c, 1.f - rr2 * 0.7f)); } return; }
    if (d.kind == K_MS) { fill(x, cy, (int)(fr * w / 2), 1, C_DIM); symCurve(x + (int)(fr * w / 2), y + 2, w - (int)(fr * w / 2), h - 4, c, [&](float t) { return expf(-t * 5.f); }); return; }
    // Standard: segmentierter Pegel
    int lit = (int)(fr * 8.f + 0.5f);
    int pk = lit > 0 ? lit - 1 - (int)((0.5f + 0.5f * sinf(animTime * 4.f + x * 0.05f)) * (lit > 2 ? 1.6f : 0.f)) : -1;
    int run = (int)(fmodf(animTime * 1.6f + x * 0.01f, 1.f) * 9.f) - 1;   // Lichtlauf von unten bis zum Wert
    for (int i = 0; i < 8; i++) { int hh = 6 + i * (h - 6) / 8; rrect(x + i * w / 8, y + h - hh, w / 8 - 3, hh, 2, i <= pk ? (i == run ? C_PLAY : c) : (i < lit ? cd : (i == run && lit == 0 ? cd : C_OFF2))); }
}

// ---------------------------------------------------------------- Kacheln statt Drehregler

static void filler(int x, int y, int w, int h, Col c, int seed);
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
    if (d.kind == K_NONE) { filler(x + 6, y + 6, 148, 80, tc, k + page); return; }   // leeres Feld: Animation statt Leere
    knobText(d, val, b, sizeof b, k);
    Col vc = locked ? C_LOCK : (sel ? mix(tc, C_PLAY, justEdited ? 0.5f : 0.f) : C_TEXT);
    float target = knobFrac(d, val);
    knobAnim[k] += (target - knobAnim[k]) * (1.f - expf(-animDt * 14.f));
    // Kopfzeile: Name links, Zahl (klein) rechts; darunter das animierte Wert-Symbol ueber die ganze Breite
    int vw = textW(b); if (vw > 100) { b[100 / FONT_W] = 0; vw = textW(b); }
    char nm[24];
    int room = (136 - vw - 8) / FONT_W; if (room < 3) room = 3;
    snprintf(nm, sizeof nm, "%.*s", room > 18 ? 18 : room, label[0] ? label : d.name);
    text(x + 14, y + 6, nm, sel ? (locked ? C_LOCK : C_TEXT) : C_DIM);
    rrect(x + 146 - vw - 6, y + 6, vw + 10, 18, 5, sel ? dimC(locked ? C_LOCK : tc, 0.25f) : C_BG);
    text(x + 146 - vw - 1, y + 7, b, vc);
    Col sc2 = locked ? C_LOCK : (sel ? tc : dimC(tc, 0.75f));
    paramSymbol(d, val, knobAnim[k], x + 16, y + 30, 128, 50, mix(sc2, C_PLAY, justEdited ? 0.4f : 0.f));
    if (locked) text(x + 14, y + 70, "LOCK", C_LOCK);
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
    const char* tn[4] = {"BASIC: SIN > TRI > SAW > SQR", "PWM: 50 % > 5 %", TR("VOKAL: A > E > I > O > U", "VOWEL: A > E > I > O > U"), TR("OBERTÖNE: 1 > 32", "HARMONICS: 1 > 32")};
    text(gx + gw - 10 - textW(tn[tab & 3]), gy + 4, tn[tab & 3], dimC(c, 0.8f));
    char b[24]; snprintf(b, sizeof b, "FRAME %d/32", cur + 1);
    text(gx + gw - 10 - textW(b), gy + 22, b, c);
}

// Chop: Wellenform mit Scheiben, aktuelle Scheibe hell, Abspielposition
static void chopGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int idx = pv(P_S0);
    const Sample* sm = sampleGet(idx);
    if (!sm) { textC(gx + gw / 2, gy + gh / 2 - 8, TR("KEINE SAMPLES IM ORDNER samples/", "NO SAMPLES IN FOLDER samples/"), C_DIM); return; }
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
    char b[48]; snprintf(b, sizeof b, TR("SCHEIBE %d/%d%s", "SLICE %d/%d%s"), sl + 1, n, pv(P_S2) ? "" : " (AUTO)");
    text(gx + gw - 10 - textW(b), gy + 4, b, C_TEXT);
}

// Granulator: Wellenform, Positionsfenster, aktive Koerner als Punkte
static void granGraph(int gx, int gy, int gw, int gh, Col c) {
    graphBox(gx, gy, gw, gh);
    int idx = pv(P_S0);
    const Sample* sm = sampleGet(idx);
    if (!sm) { textC(gx + gw / 2, gy + gh / 2 - 8, TR("KEINE SAMPLES IM ORDNER samples/", "NO SAMPLES IN FOLDER samples/"), C_DIM); return; }
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
    snprintf(b, sizeof b, TR("KORN %.0f ms  %.0f/s", "GRAIN %.0f ms  %.0f/s"), 10.f * exp2f(pv(P_S3) * (5.6f / 127.f)), 2.f * exp2f(pv(P_S4) * (6.f / 127.f)));
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
static int fmOpPage(int p) { return p == PG_SRC2 ? 0 : (p == PG_SRC3 ? 1 : (p == PG_SRC4 ? 2 : (p == PG_SRC5 ? 3 : -1))); }
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
    if (curP().machine[track] == M_FM && fmOpPage(page) >= 0) return row == 1 ? 14 : 0;   // FM-Operator: eigene Huellkurve
    if (page == PG_SEQ && row == 0) return 7;
    if (page == PG_ARP && row == 0) return 13;
    if (page == PG_FLT2 && row == 0) return 12;
    if (busOfPage(page) >= 0) return row == 0 ? 15 : 21;   // Effekt-Bus: Signalfluss + Erklaerung, darunter Animation des Effekts
    if (page == PG_TRG && row == 0) return 16;            // TRIG: Piano-Roll der Spur
    if (page == PG_SRC2 && row == 0 && curP().machine[track] == M_SYNTH) return 17;   // Oszillatoren
    if (page == PG_SRC2 && row == 0 && curP().machine[track] == M_WTBL) return 18;    // Wavetable-Stapel
    if (page == PG_INS && row == 0) return 19;            // Insert-Effekt
    if (page == PG_SEND && row == 0) return 20;           // Sends zu den Bussen
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
        hitAdd(x + 2, y + 1, 76, 188, HK_MIXCH, t);
        hitAdd(x + 48, y + 3, 26, 17, HK_MUTE, t); hitAdd(x + 48, y + 20, 26, 17, HK_SOLO, t);
        snprintf(b, sizeof b, "T%d", t + 1);
        text(x + 8, y + 4, b, sel ? TCOL[t] : C_TEXT);
        drawIcon(P.machine[t] % M_COUNT, x + 8, y + 19, 22, mix(TCOL[t], C_PLAY, flash[t] * 0.5f));
        // Mute / Solo
        rrect(x + 50, y + 5, 22, 14, 3, eng.muted(t) ? C_TRIG : C_OFF2); text(x + 57, y + 4, "M", eng.muted(t) ? C_BG : C_DIM);
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
        int rowY[MIX_ROWS - 1] = {y + 134, y + 148, y + 162, y + 176};
        for (int r = 1; r < MIX_ROWS; r++) {
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

// Seitenkarte (SELECT kurz antippen): alle Seiten als Signalweg + Gruppen, Steuerkreuz waehlt, A oeffnet
static const char* sigName(int p);
static void sigSummary(int p, char* b, int n);
static const int MAP_ROWS[5][6] = {
    {PG_TRG, PG_SEQ, PG_ARP, PG_LFO1, PG_LFO2, PG_LFO3},
    {PG_SRC, PG_SRC2, PG_SRC3, PG_SRC4, PG_SRC5, -1},
    {PG_FLT, PG_FLT2, PG_AMP, PG_INS, PG_SEND, -1},
    {PG_LIVE, PG_MIX, PG_FX, PG_FXB, PG_FXC, PG_CMP},
    {PG_SONG, PG_PRJ, PG_MST, PG_SYS, -1, -1}};
static const char* const PAGE_DESC_DE[NUI_PAGES] = {
    "Werte des Steps: Note, Anschlag, Länge, Wahrscheinlichkeit, Bedingung, Engine",
    "Laufrichtung, Länge, Tempo-Faktor, Euklid und Zufalls-Generator",
    "Arpeggiator und Akkorde: Modus, Akkord, Tempo, Oktaven",
    "Klangerzeuger der Spur (je nach Engine)",
    "Weitere Klang-Regler der Engine",
    "Filter 1: Typ, Grenzfrequenz, Resonanz, Hüllkurve",
    "Filter 2 (Hoch-/Tiefpass), Key-Tracking, Hüllkurvenform",
    "Lautstärke-Hüllkurve, Pegel, Panorama, Effekt-Sends",
    "LFO 1: moduliert einen Regler der Spur",
    "LFO 2: moduliert einen Regler der Spur",
    "LFO 3: moduliert einen Regler der Spur",
    "Insert-Effekt der Spur: Verzerrer, Bitcrusher, Chorus ...",
    "Mixer: Lautstärke, Panorama, Sends, Mute und Solo aller Spuren",
    "Effekt-Bus A: Effekt frei wählbar (Delay, Tape, Reverb, Spring, Chorus)",
    "Kompressor auf der Summe mit Sidechain",
    "Projekte laden/speichern, Sounds der Spur laden/speichern",
    "Song als Clip-Matrix: Pattern je Spur, Mutes, Export als WAV",
    "Tempo, Swing, Tonskala, Lautstärke und Summen-Klang",
    "Einstellungen: Pattern, Audio, MIDI, USB/PC, Samples, System",
    "Performance: Mutes auf den Tasten, FILL, WAV-Aufnahme, DJ-Filter",
    "Weitere Klang-Regler der Engine (FM: Operator 2)",
    "Weitere Klang-Regler der Engine (FM: Operator 3)",
    "Weitere Klang-Regler der Engine (FM: Operator 4)",
    "Anteil der Spur an den Effekt-Bussen A, B und C",
    "Effekt-Bus B: Effekt frei wählbar (Delay, Tape, Reverb, Spring, Chorus)",
    "Effekt-Bus C: Effekt frei wählbar (Delay, Tape, Reverb, Spring, Chorus)"};
static const char* const PAGE_DESC_EN[NUI_PAGES] = {
    "Step values: note, velocity, length, probability, condition, engine",
    "Direction, length, tempo factor, Euclid and random generator",
    "Arpeggiator and chords: mode, chord, rate, octaves",
    "Sound generator of the track (depends on the engine)",
    "More sound controls of the engine",
    "Filter 1: type, cutoff, resonance, envelope",
    "Filter 2 (high/low cut), key tracking, envelope shape",
    "Amp envelope, level, panorama, effect sends",
    "LFO 1: modulates a control of the track",
    "LFO 2: modulates a control of the track",
    "LFO 3: modulates a control of the track",
    "Insert effect of the track: drive, bitcrusher, chorus ...",
    "Mixer: level, pan, sends, mute and solo of all tracks",
    "Effect bus A: any effect (delay, tape, reverb, spring, chorus)",
    "Compressor on the master with sidechain",
    "Load/save projects, load/save sounds of the track",
    "Song as clip matrix: pattern per track, mutes, WAV export",
    "Tempo, swing, scale, volume and master sound",
    "Settings: pattern, audio, MIDI, USB/PC, samples, system",
    "Performance: mutes on the buttons, FILL, WAV recording, DJ filter",
    "More sound controls of the engine (FM: operator 2)",
    "More sound controls of the engine (FM: operator 3)",
    "More sound controls of the engine (FM: operator 4)",
    "Share of the track sent to effect buses A, B and C",
    "Effect bus B: any effect (delay, tape, reverb, spring, chorus)",
    "Effect bus C: any effect (delay, tape, reverb, spring, chorus)"};
#define PAGE_DESC (g_lang ? PAGE_DESC_EN : PAGE_DESC_DE)
static void mapPos(int p, int& r, int& c) { r = c = 0; for (int i = 0; i < 5; i++) for (int k = 0; k < 6; k++) if (MAP_ROWS[i][k] == p) { r = i; c = k; } }
static int mapCols(int r) { int n = 0; while (n < 6 && MAP_ROWS[r][n] >= 0) n++; return n; }
static void mapBox(int r, int c, int& x, int& y, int& w, int& h) {   // Raster mit 6 Spalten
    static const int RY[5] = {82, 148, 214, 290, 356};
    h = 42; y = RY[r];
    w = 88; x = 26 + c * (w + 12);
}
static const char* mapName(int p) {   // kurze Namen der globalen Seiten (88 px)
    switch (p) {
    case PG_LIVE: return "LIVE"; case PG_MIX: return "MIXER"; case PG_FX: return "FX A"; case PG_FXB: return "FX B"; case PG_FXC: return "FX C";
    case PG_CMP: return TR("KOMPR.", "COMP"); case PG_SONG: return "SONG"; case PG_PRJ: return TR("PROJEKT", "PROJECT"); case PG_MST: return "MASTER";
    case PG_SYS: return TR("SETUP", "SETUP");
    }
    return PAGE_NAMES[p];
}
static void mapSummary(int p, char* b, int n) {
    if (trackPage(p)) { sigSummary(p, b, n); return; }
    switch (p) {
    case PG_LIVE: snprintf(b, n, TR("MUTES  FILL  REC", "MUTES  FILL  REC")); break;
    case PG_MIX: snprintf(b, n, TR("8 KANÄLE", "8 CHANNELS")); break;
    case PG_FX: case PG_FXB: case PG_FXC: snprintf(b, n, "%s", BUS_TYPE_NAMES[eng.proj.bus[busOfPage(p)][BX_TYPE] % BF_COUNT]); break;
    case PG_CMP: snprintf(b, n, "SIDECHAIN"); break;
    case PG_SONG: snprintf(b, n, TR("%d ZEILEN%s", "%d ROWS%s"), eng.proj.songLen, eng.songOn ? TR("  AN", "  ON") : ""); break;
    case PG_PRJ: snprintf(b, n, "%.15s", cfg.project); break;
    case PG_MST: snprintf(b, n, "%d BPM", eng.proj.bpm); break;
    case PG_SYS: snprintf(b, n, TR("AUDIO, USB", "AUDIO, USB")); break;
    default: b[0] = 0;
    }
}
static void drawExportModal();
static void drawWtEditorIfOpen();
static void drawMenu() {
    hitModal();
    rrect(10, 30, 620, 420, 14, C_ACC);
    rrect(12, 32, 616, 416, 13, C_BG);
    char b[64];
    textC(320, 40, TR("SEITEN", "PAGES"), C_ACC, 2);
    Col tc = TCOL[track];
    snprintf(b, sizeof b, TR("SPUR %d - %s - P%02d", "TRACK %d - %s - P%02d"), track + 1, MACHINE_NAMES[curP().machine[track] % M_COUNT], curPi() + 1);
    text(26, 46, b, tc);
    static const char* const RL_DE[5] = {"SEQUENZ + MODULATION", "QUELLE", "KLANGWEG", "LIVE, MIX & EFFEKT-BUSSE", "PROJEKT"};
    static const char* const RL_EN[5] = {"SEQUENCE + MODULATION", "SOURCE", "SOUND PATH", "LIVE, MIX & EFFECT BUSES", "PROJECT"};
    for (int r = 0; r < 5; r++) {
        int x, y, w, h; mapBox(r, 0, x, y, w, h);
        text(26, y - 15, g_lang ? RL_EN[r] : RL_DE[r], r <= 2 ? dimC(tc, 0.85f) : C_DIM);
        if (r == 3) fill(26, y - 22, 588, 1, C_OFF2);
        for (int c = 0; c < mapCols(r); c++) {
            int p = MAP_ROWS[r][c];
            mapBox(r, c, x, y, w, h);
            bool av = pageAvail(p), sel = p == menuSel, cur = p == page;
            if (!av) continue;   // Seite gibt es bei dieser Engine nicht
            if ((r == 1 || r == 2) && c > 0) {   // Pfeil im Signalweg
                Col ac = av ? tc : C_OFF2;
                fill(x - 11, y + h / 2, 9, 2, ac); fill(x - 5, y + h / 2 - 2, 2, 6, ac); fill(x - 4, y + h / 2 - 1, 2, 4, ac);
            }
            Col bg = sel ? (r <= 2 ? tc : C_ACC) : (cur ? C_SEL : C_PANEL);
            hitAdd(x, y, w, h, HK_MENU, p);
            rrect(x, y, w, h, 7, av ? bg : C_BG);
            if (cur && !sel) { Col fc = r <= 2 ? tc : C_ACC; rrect(x, y, w, 2, 1, fc); rrect(x, y + h - 2, w, 2, 1, fc); }
            if (!av) { rrect(x, y, w, h, 7, C_DARK); text(x + 8, y + 5, r <= 2 ? sigName(p) : mapName(p), C_OFF2); continue; }
            const char* nm = r <= 2 ? sigName(p) : mapName(p);
            text(x + 8, y + 5, nm, sel ? C_BG : (cur ? (r <= 2 ? tc : C_ACC) : C_TEXT));
            mapSummary(p, b, sizeof b);
            char sb[40]; snprintf(sb, sizeof sb, "%.*s", (w - 14) / FONT_W, b);
            text(x + 8, y + 23, sb, sel ? C_BG : C_DIM);
        }
    }
    textC(320, 408, PAGE_DESC[menuSel], C_TEXT);
    char hb[96];
    if (PG_STORE[menuSel] >= 0) {
        int cq = 0; for (int q = 0; q < NUI_PAGES; q++) if (PG_STORE[q] == pageClipStore) { cq = q; break; }
        if (pageClipStore >= 0) snprintf(hb, sizeof hb, TR("[A]ÖFFNEN [X]SEITE KOPIEREN [Y]EINFÜGEN (%s VON T%d) [B]ZURÜCK", "[A]OPEN [X]COPY PAGE [Y]PASTE (%s OF T%d) [B]BACK"), PAGE_NAMES[cq], pageClipTrack + 1);
        else snprintf(hb, sizeof hb, TR("[A]ÖFFNEN [X]SEITE DER SPUR KOPIEREN [Y]EINFÜGEN [B]ZURÜCK", "[A]OPEN [X]COPY TRACK PAGE [Y]PASTE [B]BACK"));
    } else snprintf(hb, sizeof hb, TR("[STEUERKREUZ]WÄHLEN [A]ÖFFNEN [B]ZURÜCK", "[D-PAD]SELECT [A]OPEN [B]BACK"));
    hint(320 - textW(hb) / 2, 428, hb, C_ACC, C_DIM);
}

static void drawConfirm() {
    int w = textW(confirmText) + 48; if (w < 360) w = 360; if (w > 620) w = 620;
    rrect(320 - w / 2, 170, w, 110, 14, C_ACC);
    rrect(322 - w / 2, 172, w - 4, 106, 13, C_BG, 250);
    textC(320, 190, confirmText, C_TEXT);
    hitModal(); hitAdd(220, 222, 90, 40, HK_YES); hitAdd(330, 222, 90, 40, HK_NO);
    rrect(220, 226, 90, 32, 8, C_GREEN); textC(265, 234, "A = JA", C_BG);
    rrect(330, 226, 90, 32, 8, C_OFF2); textC(375, 234, TR("B = NEIN", "B = NO"), C_TEXT);
}

#include "anim.inc"

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
        int rat[4], lvl[4];
        for (int k = 0; k < 4; k++) { rat[k] = pv(FM_OPB[k] + OP_RAT); lvl[k] = pv(FM_OPB[k] + OP_LVL); }
        int op = fmOpPage(page);
        fmGraph(gx, gy, gw, gh, pv(P_S0), rat, lvl, op >= 0 ? op : (selRel == 0 ? 4 : -1), &v, tc);
    } break;
    case 16: trigGraph(gx, gy, gw, gh, tc); break;
    case 17: oscPageGraph(gx, gy, gw, gh, tc); break;
    case 18: wtStackGraph(gx, gy, gw, gh, tc); break;
    case 19: insGraph(gx, gy, gw, gh, tc); break;
    case 20: sendGraph(gx, gy, gw, gh, tc); break;
    case 21: busAnimGraph(gx, gy, gw, gh, busOfPage(page) < 0 ? 0 : busOfPage(page)); break;
    case 15: {   // Effekt-Bus: Spuren -> Bus -> (naechster Bus) -> Summe, Pegel, Erklaerung des Reglers
        int b = busOfPage(page);
        const uint8_t* bp = eng.proj.bus[b];
        graphBox(gx, gy, gw, gh);
        Col bc = C_ACC;
        int ns = 0; for (int t = 0; t < NTRACKS; t++) if (curP().p[t][b == 0 ? P_DLY : (b == 1 ? P_REV : P_SNDC)] > 0) ns++;
        char ib[96];
        snprintf(ib, sizeof ib, TR("%d SPUREN", "%d TRACKS"), ns);
        int y = gy + 10;
        rrect(gx + 10, y, 92, 22, 5, C_PANEL); textC(gx + 56, y + 3, ib, ns ? C_TEXT : C_DIM);
        fill(gx + 104, y + 10, 24, 2, bc);
        snprintf(ib, sizeof ib, "BUS %c: %s", 'A' + b, BUS_TYPE_NAMES[bp[BX_TYPE] % BF_COUNT]);
        int bw = textW(ib) + 20;
        rrect(gx + 130, y - 2, bw, 26, 6, bp[BX_TYPE] ? bc : C_OFF2); text(gx + 140, y + 3, ib, bp[BX_TYPE] ? C_BG : C_TEXT);
        int xo = gx + 130 + bw;
        fill(xo, y + 10, 24, 2, bc);
        rrect(xo + 26, y, 70, 22, 5, C_PANEL); textC(xo + 61, y + 3, TR("SUMME", "MIX"), C_TEXT);
        if (b < NBUS - 1 && bp[BX_FWD] > 0) {   // Weiterleitung
            fill(xo + 8, y + 12, 2, 18, bc);
            snprintf(ib, sizeof ib, "> BUS %c (%d)", 'B' + b, bp[BX_FWD]); text(xo + 14, y + 22, ib, bc);
        }
        float pk = eng.bus[b].peak; int mw = clampi((int)(sqrtf(pk) * 120), 0, 120);
        fill(gx + gw - 136, y + 4, 120, 6, C_OFF2); if (mw) fill(gx + gw - 136, y + 4, mw, 6, C_GREEN);
        ParamDef hd; int hv, hb; bool hl; char hlab[24];
        if (knobInfo(slot, hd, hv, hl, hb, hlab, sizeof hlab) && hd.kind != K_NONE) {
            const char* ht = busHelp(bp[BX_TYPE] % BF_COUNT, hd.name);
            if (*ht) { snprintf(ib, sizeof ib, "%s: %s", hd.name, ht); text(gx + 10, gy + gh - 20, ib, C_TEXT); }
        }
    } break;
    case 14: {   // FM-Operator: Huellkurve + Kurzinfo
        int op = fmOpPage(page), b = FM_OPB[op < 0 ? 0 : op];
        envShapeK = 1.f;
        envGraph(gx, gy, gw, gh, pv(b + OP_ATK), pv(b + OP_DEC), pv(b + OP_SUS), pv(b + OP_REL), selRel, nullptr, tc);
        int algo = pv(P_S0) & 7;
        bool car = FM_CARRIERS[algo] >> op & 1;
        char ib[64];
        float r = FM_RATIOS[pv(b + OP_RAT) % FM_NRAT] + (pv(b + OP_FINE) - 64) / 64.f * 0.5f;
        int cents = (int)lrintf((pv(b + OP_DETN) - 64) / 64.f * 50.f);
        int tgt = 0; for (int k = 0; k < 4; k++) if (FM_MODS[algo][k] >> op & 1) tgt |= 1 << k;
        if (car) snprintf(ib, sizeof ib, TR("OP%d  x%.3g  %+d CENT  TRÄGER (HÖRBAR)", "OP%d  x%.3g  %+d CENT  CARRIER (AUDIBLE)"), op + 1, r, cents);
        else snprintf(ib, sizeof ib, TR("OP%d  x%.3g  %+d CENT  MODULIERT OP%d", "OP%d  x%.3g  %+d CENT  MODULATES OP%d"), op + 1, r, cents, tgt ? __builtin_ctz(tgt) + 1 : 0);
        text(gx + gw - 8 - textW(ib), gy + gh - 18, ib, car ? tc : C_TEXT);
        if (v.active && v.machine == M_FM && op >= 0) { int lh = (int)((gh - 8) * v.fmEnv[op]); fill(gx + gw - 8, gy + gh - 4 - lh, 4, lh, tc); }
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
            for (int k = row * 4; k < row * 4 + 4; k++) { paramChip(k, (k % 4) * 160 + ox, y + 66, tc); hitAdd((k % 4) * 160 + ox + 4, y + 64, 152, 30, HK_SLOT, k); }
            continue;
        }
        for (int k = row * 4; k < row * 4 + 4; k++) {
            int x = (k % 4) * 160 + ox;
            bool sel = k == slot;
            ParamDef d; int val, baseVal; bool locked;
            bool ok = knobInfo(k, d, val, locked, baseVal, label, sizeof label);
            int cx = x + 80, cy = y + 38;
            if (ok && d.kind == K_ACT) {   // Schaltflaeche
                hitAdd(x + 4, y + 2, 152, 88, HK_PRJ, k);
                if (sel) rrect(x + 4, y + 2, 152, 88, 10, C_SEL);
                float p = sel ? 0.5f + 0.5f * sinf(animTime * 4.f) : 0.f;
                prjIcon(k, x + 14, y + 12, 44, sel ? C_ACC : dimC(C_ACC, 0.6f));
                rrect(x + 64, cy - 18, 86, 36, 10, sel ? mix(C_ACC, C_PLAY, p * 0.25f) : C_OFF2);
                char an[16]; snprintf(an, sizeof an, "%.10s", d.name);
                textC(x + 107, cy - 8, an, sel ? C_BG : C_TEXT);
                textC(cx, y + 70, sel ? TR("A = START", "A = GO") : label, sel ? C_DIM : C_TEXT);
                continue;
            }
            Col kc = trackPage(page) ? tc : C_ACC;
            if (ok && d.kind == K_GR) kc = C_TRIG;
            tile(x, y, k, ok ? d : PDEF_NONE, val, baseVal, locked, label, sel, kc, now);
            if (ok && d.kind != K_NONE) hitAdd(x + 4, y + 2, 152, 88, HK_SLOT, k);
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
        hitAdd(0, y - 2, 80, 18, HK_TRACK, t);
        Col tc = eng.muted(t) ? dimC(TCOL[t], 0.35f) : TCOL[t];
        int chipH = 14 + (int)(flash[t] * 4);
        rrect(4, y + 7 - chipH / 2, 4, chipH, 1, mix(tc, C_PLAY, flash[t] * 0.7f));
        Col lc = eng.muted(t) ? C_DIM : (t == track ? TCOL[t] : C_TEXT);
        snprintf(b, sizeof b, "%d", t + 1);
        text(12, y - 2, b, lc);
        drawIcon(P.machine[t] % M_COUNT, 21, y - 1, 15, eng.muted(t) ? C_DIM : mix(TCOL[t], C_PLAY, flash[t] * 0.6f));
        text(39, y - 2, MACHINE_NAMES[P.machine[t] % M_COUNT], lc);
        if (eng.muted(t)) fill(39, y + 5, 40, 1, C_TRIG);
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
            hitAdd(x - 1, y - 2, 34, 18, HK_STEP, s, t);
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
    Col tc = eng.muted(track) ? dimC(TCOL[track], 0.35f) : TCOL[track];
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
        hitAdd(x - 1, y - 4, 39, 46, HK_STEP, s, track);
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
    if (lockLatch) h = TR("LOCK [X/Y]WERT [L1/R1]REGLER [L2/R2]SEITE [</>]STEP [B]WEG [SEL+L1/R1]KOPIE [A]OK", "LOCK [X/Y]VALUE [L1/R1]CONTROL [L2/R2]PAGE [</>]STEP [B]CLEAR [SEL+L1/R1]COPY [A]OK");
    else if (aHeld) h = TR("[X/Y]LOCK [L1/R1]REGLER [L2/R2]SEITE [B]LÖSCHEN [SEL+L1/R1]KOPIE  LANG = LOCK-MODUS", "[X/Y]LOCK [L1/R1]CONTROL [L2/R2]PAGE [B]CLEAR [SEL+L1/R1]COPY  LONG = LOCK MODE");
    else if (in.down[BT_SELECT]) h = TR("[</>]PATTERN [^]REC [v]TAP [A]MUTE [B]UNDO [X/Y]ENGINE [L1/R1]4 STEPS [L2/R2]16 STEPS", "[</>]PATTERN [^]REC [v]TAP [A]MUTE [B]UNDO [X/Y]ENGINE [L1/R1]4 STEPS [L2/R2]16 STEPS");
    else if (page == PG_MIX) h = TR("[</>]KANAL [^v]ZEILE [X/Y]WERT [A]MUTE [B]SOLO [L2/R2]SEITE [SEL]KARTE", "[</>]CHANNEL [^v]ROW [X/Y]VALUE [A]MUTE [B]SOLO [L2/R2]PAGE [SEL]MAP");
    else if (page == PG_PRJ) h = TR("[</>]FELD [^v]SPUR [A]AUSFÜHREN [X/Y]WERT [L2/R2]SEITE [SEL]KARTE", "[</>]FIELD [^v]TRACK [A]RUN [X/Y]VALUE [L2/R2]PAGE [SEL]MAP");
    else if (!trackPage(page)) h = TR("[STEUERKREUZ/L1/R1]REGLER [X/Y]WERT [L2/R2]SEITE [SEL]KARTE", "[D-PAD/L1/R1]CONTROL [X/Y]VALUE [L2/R2]PAGE [SEL]MAP");
    else h = TR("[^v]SPUR [</>]STEP [L1/R1]REGLER [X/Y]WERT [L2/R2]SEITE [SEL]KARTE", "[^v]TRACK [</>]STEP [L1/R1]CONTROL [X/Y]VALUE [L2/R2]PAGE [SEL]MAP");
    hint(6, 452, h, C_ACC, C_DIM);
}

static void drawHelp() {
    rrect(30, 22, 580, 436, 12, C_ACC);
    rrect(32, 24, 576, 432, 11, C_BG);
    const char* lines[] = {
        TR("ÜBERALL", "EVERYWHERE"),
        TR("  SELECT tippen  Karte aller Seiten (Signalweg, Mix, Projekt)", "  tap SELECT     Map of all pages (signal path, mix, project)"),
        TR("  L2 / R2        Seite zurück / vor", "  L2 / R2        Page back / forward"),
        TR("  START          Play / Stop  (beim Spielen halten = FILL)", "  START          Play / stop  (hold while playing = FILL)"),
        TR("SPURSEITEN", "TRACK PAGES"),
        TR("  HOCH/RUNTER    Spur      LINKS/RECHTS  Step", "  UP/DOWN        Track     LEFT/RIGHT    Step"),
        TR("  L1 / R1        Regler wählen     X / Y  Wert -/+", "  L1 / R1        Select control    X / Y  Value -/+"),
        TR("  A / A lang     Trig / LOCK-MODUS (L1/R1/L2/R2 gehen)", "  A / A long     Trig / LOCK MODE (L1/R1/L2/R2 work)"),
        TR("  B              Spur vorhören", "  B              Preview track"),
        TR("  Sticks         optional: links Regler, rechts Wert", "  Sticks         optional: left control, right value"),
        TR("SELECT GEHALTEN +", "HOLD SELECT +"),
        TR("  LINKS/RECHTS   Pattern     HOCH/RUNTER  Aufnahme / Tap", "  LEFT/RIGHT     Pattern     UP/DOWN      Record / tap"),
        TR("  A / B          Spur stumm / RÜCKGÄNGIG", "  A / B          Mute track / UNDO"),
        TR("  X / Y          Engine wechseln", "  X / Y          Change engine"),
        TR("  L1/R1  L2/R2   4 Steps / 16 Steps springen", "  L1/R1  L2/R2   Jump 4 steps / 16 steps"),
        TR("  L1 / R1        im Lock-Modus: Trig kopieren / einfügen", "  L1 / R1        in lock mode: copy / paste trig"),
        TR("  START          Speichern (2 s halten: Ende)", "  START          Save (hold 2 s: quit)"),
    };
    for (unsigned i = 0; i < sizeof lines / sizeof *lines; i++)
        text(48, 36 + i * 22, lines[i], i == 0 || i == 4 || i == 10 ? C_ACC : C_TEXT);
}


// ---------------------------------------------------------------- Oberflaeche B: Signalweg
// Spurseiten als Signalweg (Bloecke) + Modulationsreihe, links alle Spuren, unten die Steps der Spur.
static const int SIG_CHAIN[] = {PG_SRC, PG_SRC2, PG_FLT, PG_FLT2, PG_AMP, PG_INS};
static const int SIG_MOD[] = {PG_TRG, PG_SEQ, PG_ARP, PG_LFO1, PG_LFO2, PG_LFO3};
static const int SIG_NCH = 6, SIG_NMOD = 6;
static const char* sigName(int p) {
    int m = curP().machine[track];
    if (m == M_FM) switch (p) { case PG_SRC: return "FM"; case PG_SRC2: return "OP 1"; case PG_SRC3: return "OP 2"; case PG_SRC4: return "OP 3"; case PG_SRC5: return "OP 4"; }
    if (p == PG_SRC2 && (m == M_SYNTH || m == M_WTBL)) return "OSC";
    switch (p) {
    case PG_SEND: return "SENDS";
    case PG_SRC: return TR("QUELLE", "SOURCE"); case PG_SRC2: return TR("QUELLE 2", "SOURCE 2"); case PG_FLT: return "FILTER 1"; case PG_FLT2: return "FILTER 2";
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
    case PG_SRC2: case PG_SRC3: case PG_SRC4: case PG_SRC5: {
        int m = P.machine[track] % M_COUNT;
        if (m == M_FM) { int op = p == PG_SRC2 ? 0 : p - PG_SRC3 + 1; snprintf(b, n, "x%.3g", FM_RATIOS[q[FM_OPB[op] + OP_RAT] % FM_NRAT]); }
        else if (m == M_SYNTH) snprintf(b, n, "%+d/%+d", q[P_X0] - 3, q[P_X1] - 3);
        else if (m == M_WTBL) snprintf(b, n, "%s", q[P_S0] % 5 == 4 ? "USER" : "OCT");
        else snprintf(b, n, TR("MEHR", "MORE"));
    } break;
    case PG_SEND: snprintf(b, n, "%d/%d/%d", q[P_DLY], q[P_REV], q[P_SNDC]); break;
    case PG_FLT: knobText(COMMON_DEFS[P_FTYP], q[P_FTYP], v, sizeof v); snprintf(b, n, "%s %d", v, q[P_FREQ]); break;
    case PG_FLT2: snprintf(b, n, "%s", q[P_BASE] > 0 || q[P_WDTH] < 127 ? TR("AN", "ON") : TR("AUS", "OFF")); break;
    case PG_AMP: snprintf(b, n, "VOL %d", q[P_VOL]); break;
    case PG_INS: snprintf(b, n, "%s", q[P_ITYP] < INS_COUNT ? INS_NAMES[q[P_ITYP]] : "?"); break;
    case PG_TRG: fmtParam(COMMON_DEFS[P_NOTE], q[P_NOTE], v, sizeof v); snprintf(b, n, "%s", v); break;
    case PG_SEQ: snprintf(b, n, "%d STEPS", P.len[track]); break;
    case PG_ARP: snprintf(b, n, "%s", ARP_MODES[q[P_AMOD] % 6]); break;
    case PG_LFO1: case PG_LFO2: case PG_LFO3: {
        int k = p - PG_LFO1, dst = q[LFO_BASE[k] + 2];
        if (dst > 0 && q[LFO_BASE[k] + 3] != 64) snprintf(b, n, ">%.5s", paramDef(P.machine[track], dst - 1).name);
        else snprintf(b, n, "-");
    } break;
    default: b[0] = 0;
    }
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
    if (!ok || d.kind == K_NONE) { filler(x + 2, y + 2, w - 4, h - 4, dimC(tc, 0.8f), k + page); return; }
    if (d.kind == K_ACT) { textC(x + w / 2, y + h / 2 - 8, d.name, sel ? C_ACC : C_TEXT); return; }
    // Kopfzeile: Name links, Zahl (klein) rechts; darunter animiertes Wert-Symbol, unten duenner Balken
    knobText(d, val, b, sizeof b, k);
    int vw = textW(b); if (vw > w - 40) { b[(w - 40) / FONT_W] = 0; vw = textW(b); }
    char nm[24];
    int room = (w - 14 - vw - 6) / FONT_W; if (room < 2) room = 2;
    snprintf(nm, sizeof nm, "%.*s", room > 20 ? 20 : room, label[0] ? label : d.name);
    text(x + 7, y + 3, nm, sel ? (locked ? C_LOCK : tc) : C_DIM);
    text(x + w - 7 - vw, y + 3, b, locked ? C_LOCK : (sel ? C_PLAY : C_TEXT));
    knobAnim[k] += (knobFrac(d, val) - knobAnim[k]) * (1.f - expf(-animDt * 14.f));
    int sy = y + 21, sh = h - 33;
    if (sh >= 14) {
        Col sc2 = locked ? C_LOCK : (sel ? tc : dimC(tc, 0.75f));
        int sw = w - 18; if (sw > sh * 4) sw = sh * 4;
        paramSymbol(d, val, knobAnim[k], x + (w - sw) / 2, sy, sw, sh, sc2);
    }
    int bw = (int)((w - 14) * knobAnim[k]);
    Col bc = locked ? C_LOCK : (sel ? tc : dimC(tc, 0.7f));
    fill(x + 7, y + h - 7, w - 14, 2, C_OFF2);
    if (d.kind == K_BIP || d.kind == K_OCT) {   // bipolar: Balken ab der Mitte
        int mid = x + 7 + (w - 14) / 2, e = x + 7 + bw;
        fill(mid, y + h - 9, 1, 6, C_DIM);
        if (e != mid) fill(e < mid ? e : mid, y + h - 7, abs(e - mid), 2, bc);
    } else if (bw > 0) fill(x + 7, y + h - 7, bw, 2, bc);
}
// FILTER 2: BASE und WDTH teilen sich eine breite Kachel (ein Durchlassband); Spektrum der Summe, innen hell
static void bwTile(int x, int y, int w, int h, Col tc) {
    bool sel = slot == 0 || slot == 1;
    rrect(x, y, w, h, 6, sel ? C_SEL : C_PANEL);
    if (sel) {
        Col fc = dimC(tc, 0.6f + 0.25f * sinf(animTime * 3.f));
        rrect(x, y, w, 2, 1, fc); rrect(x, y + h - 2, w, 2, 1, fc); rrect(x, y, 2, h, 1, fc); rrect(x + w - 2, y, 2, h, 1, fc);
    }
    int bs = pv(P_BASE), wd = pv(P_WDTH);
    bool lB = aHeld && holdS().lock[P_BASE] != NO_LOCK, lW = aHeld && holdS().lock[P_WDTH] != NO_LOCK;
    char b[16];
    text(x + 7, y + 3, "BASE", slot == 0 ? (lB ? C_LOCK : tc) : C_DIM);
    snprintf(b, sizeof b, "%d", bs); text(x + 7 + textW("BASE") + 8, y + 3, b, slot == 0 ? C_PLAY : C_TEXT);
    snprintf(b, sizeof b, "%d", wd); int vx = x + w - 7 - textW(b);
    text(vx, y + 3, b, slot == 1 ? C_PLAY : C_TEXT);
    text(vx - 8 - textW("WDTH"), y + 3, "WDTH", slot == 1 ? (lW ? C_LOCK : tc) : C_DIM);
    // Band
    int gx = x + 8, gw = w - 16, gy = y + 21, gh = h - 30;
    float lo = bs / 127.f, hi = fminf(1.f, lo + wd / 127.f);
    int lx = gx + (int)(lo * gw), hx = gx + (int)(hi * gw);
    if (hx > lx) fill(lx, gy, hx - lx, gh, dimC(tc, 0.18f));
    specUpdate();
    int nb = SPEC_N, bw2 = gw / nb;
    for (int i = 0; i < nb; i++) {   // Spektrum der Summe (bei Stille: ruhige Welle), ausserhalb des Bandes gedaempft
        float t = (i + 0.5f) / nb;
        float v = fmaxf(specVal[i], 0.12f + 0.1f * sinf(animTime * 2.f + i * 0.5f));
        float g = (t < lo ? expf(-(lo - t) * 14.f) : 1.f) * (t > hi ? expf(-(t - hi) * 14.f) : 1.f);
        int bh = (int)(v * g * (gh - 2)); if (bh < 1) bh = 1;
        fill(gx + i * bw2, gy + gh - bh, bw2 - 1, bh, g > 0.7f ? mix(tc, C_PLAY, v * 0.3f) : dimC(tc, 0.25f + 0.5f * g));
    }
    symCurve(gx, gy, gw, gh, dimC(tc, 0.9f), [&](float t) { return (t < lo ? expf(-(lo - t) * 14.f) : 1.f) * (t > hi ? expf(-(t - hi) * 14.f) : 1.f) * 0.92f; }, false);
    float pu = 0.5f + 0.5f * sinf(animTime * 4.f);
    fill(lx, gy, 2, gh, slot == 0 ? mix(tc, C_PLAY, pu) : dimC(tc, 0.7f));
    fill(hx - 1, gy, 2, gh, slot == 1 ? mix(tc, C_PLAY, pu) : dimC(tc, 0.7f));
    // unten: Bereich als Balken
    fill(x + 7, y + h - 7, w - 14, 2, C_OFF2);
    int bl = x + 7 + (int)(lo * (w - 14)), bh2 = x + 7 + (int)(hi * (w - 14));
    if (bh2 > bl) fill(bl, y + h - 7, bh2 - bl, 2, sel ? tc : dimC(tc, 0.7f));
}
static void drawSigB() {
    const Pattern& P = curP();
    char b[32];
    Col tc = eng.muted(track) ? dimC(TCOL[track], 0.35f) : TCOL[track];
    // --- links: alle Spuren
    int len0 = P.len[track] < 1 ? 1 : P.len[track];
    int base = (cursor / 16) * 16;
    for (int t = 0; t < NTRACKS; t++) {
        int y = 46 + t * 46, x = 2, w = 110, h = 43;
        hitAdd(x, y, w, h, HK_TRACK, t);
        bool sel = t == track;
        Col c = eng.muted(t) ? dimC(TCOL[t], 0.35f) : TCOL[t];
        rrect(x, y, w, h, 5, sel ? C_SEL : C_PANEL);
        if (sel) { rrect(x, y, w, 2, 1, c); rrect(x, y + h - 2, w, 2, 1, c); rrect(x, y, 2, h, 1, c); rrect(x + w - 2, y, 2, h, 1, c); }
        drawIcon(P.machine[t] % M_COUNT, x + 5, y + 4, 14, mix(c, C_PLAY, flash[t] * 0.6f));
        snprintf(b, sizeof b, "%d %.6s", t + 1, MACHINE_NAMES[P.machine[t] % M_COUNT]);
        text(x + 22, y + 2, b, eng.muted(t) ? C_DIM : (sel ? c : C_TEXT));
        if (eng.muted(t)) text(x + w - 14, y + 2, "M", C_TRIG);
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
    // --- Signalweg: Spalten SEQUENZ | QUELLE -> FILTER -> AMP -> INSERT | MOD (LFO 1-3 untereinander)
    int X0 = 118, W = 518;
    snprintf(b, sizeof b, TR("SIGNALWEG  SPUR %d", "SIGNAL PATH  TRACK %d"), track + 1);
    text(X0, 44, b, C_DIM);
    { const char* lh = TR("L2/R2 = SEITE  SELECT = KARTE", "L2/R2 = PAGE  SELECT = MAP"); text(X0 + W - textW(lh), 44, lh, dimC(C_ACC, 0.8f)); }
    {
        int m = P.machine[track] % M_COUNT;
        // QUELLE-Spalte: Quelle + 2. Seite; FM: Operator der gerade gewaehlten bzw. zuletzt besuchten Seite
        static int lastOp = PG_SRC2;
        if (page == PG_SRC2 || page == PG_SRC3 || page == PG_SRC4 || page == PG_SRC5) lastOp = page;
        int opPage = m == M_FM ? lastOp : PG_SRC2;
        const int COLS[6][3] = {{PG_TRG, PG_SEQ, PG_ARP}, {PG_SRC, opPage, -1}, {PG_FLT, PG_FLT2, -1}, {PG_AMP, PG_SEND, -1}, {PG_INS, -1, -1}, {PG_LFO1, PG_LFO2, PG_LFO3}};
        int gap = 8, cw = (W - gap * 5) / 6, rowH = 21, y0 = 60;
        for (int c = 0; c < 6; c++) {
            int x = X0 + c * (cw + gap);
            if (c >= 2 && c <= 4) { fill(x - gap, y0 + 9, gap, 2, tc); fill(x - 3, y0 + 7, 1, 6, tc); }   // Pfeile im Klangweg
            bool side = c == 0 || c == 5;
            for (int r = 0; r < 3; r++) {
                int p = COLS[c][r];
                if (p < 0 || !pageAvail(p)) continue;
                int y = y0 + r * (rowH + 1);
                bool sel = p == page || (c == 1 && r == 1 && m == M_FM && (page == PG_SRC2 || page == PG_SRC3 || page == PG_SRC4 || page == PG_SRC5));
                hitAdd(x, y, cw, rowH, HK_PAGE, p);
                rrect(x, y, cw, rowH - 1, 4, sel ? tc : (side ? C_BG : C_PANEL));
                char nm[24], sm[24], cb[40];
                snprintf(nm, sizeof nm, "%s", sigName(sel && c == 1 && r == 1 ? page : p));
                sigSummary(sel && c == 1 && r == 1 ? page : p, sm, sizeof sm);
                int maxc = (cw - 8) / FONT_W;
                snprintf(cb, sizeof cb, "%s", nm);
                int room = maxc - (int)strlen(cb) - 1;
                if (room >= 2 && sm[0] && strcmp(sm, nm)) { size_t l = strlen(cb); snprintf(cb + l, sizeof cb - l, " %.*s", room, sm); }
                cb[maxc > 0 ? maxc : 0] = 0;
                text(x + 4, y + 1, cb, sel ? C_BG : (side ? C_DIM : C_TEXT));
            }
        }
        if (m == M_FM) text(X0 + 1 * (cw + gap) + 4, y0 + 2 * (rowH + 1) + 1, "OP 1-4", page == PG_SRC2 || page == PG_SRC3 || page == PG_SRC4 || page == PG_SRC5 ? tc : C_OFF2);
    }
    // --- Bedienfeld der gewaehlten Seite
    int py = 132, ph = 254;
    rrect(X0, py, W, ph, 6, C_PANEL);
    fill(X0, py, W, 2, tc);
    text(X0 + 8, py + 4, pageLong(page), tc);
    {   // Erklaerung des gewaehlten Reglers
        ParamDef hd; int hv, hb; bool hl; char hlab[24];
        if (knobInfo(slot, hd, hv, hl, hb, hlab, sizeof hlab) && hd.kind != K_NONE) {
            const char* ht = paramHelp(hd.name);
            if (curP().machine[track] == M_FM && !strcmp(hd.name, "FINE")) ht = TR("Ratio fein: +-0,5 (unharmonisch, glockig)", "Fine ratio: +-0.5 (inharmonic, bell-like)");
            if (curP().machine[track] == M_FM && !strcmp(hd.name, "DETN")) ht = TR("Verstimmung +-50 Cent (Schwebung)", "Detune +-50 cents (beating)");
            if (*ht) {
                char hs[96]; snprintf(hs, sizeof hs, "%s: %s", hd.name, ht);
                int maxw = W - 24 - textW(pageLong(page));
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
    for (int k = 0; k < 8; k++) {
        if (page == PG_FLT2 && k == 1) continue;   // BASE + WDTH = eine breite Kachel
        if (page == PG_FLT2 && k == 0) { bwTile(X0 + 6, ty, tw * 2 + 6, th, tc); hitAdd(X0 + 6, ty, tw + 3, th, HK_SLOT, 0); hitAdd(X0 + 9 + tw, ty, tw + 3, th, HK_SLOT, 1); continue; }
        miniTile(k, X0 + 6 + (k % 4) * (tw + 6), ty + (k / 4) * (th + 6), tw, th, tc);
        hitAdd(X0 + 6 + (k % 4) * (tw + 6), ty + (k / 4) * (th + 6), tw, th, HK_SLOT, k);
    }
    // --- Steps der Spur
    int sy = 392;
    int npg = (len0 + 15) / 16;
    snprintf(b, sizeof b, "%d/%d", cursor / 16 + 1, npg);
    for (int s0 = 0; s0 < 16; s0++) {
        int s = base + s0, sx = X0 + s0 * 32;
        if (s >= len0) { rrect(sx, sy, 30, 40, 4, C_DARK); continue; }
        hitAdd(sx, sy - 2, 31, 46, HK_STEP, s, track);
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
    if (lockLatch) h = TR("LOCK [X/Y]WERT [L1/R1]REGLER [L2/R2]SEITE [</>]STEP [B]WEG [SEL+L1/R1]KOPIE [A]OK", "LOCK [X/Y]VALUE [L1/R1]CONTROL [L2/R2]PAGE [</>]STEP [B]CLEAR [SEL+L1/R1]COPY [A]OK");
    else if (aHeld) h = TR("[X/Y]LOCK [L1/R1]REGLER [L2/R2]SEITE [B]LÖSCHEN [SEL+L1/R1]KOPIE  LANG = LOCK-MODUS", "[X/Y]LOCK [L1/R1]CONTROL [L2/R2]PAGE [B]CLEAR [SEL+L1/R1]COPY  LONG = LOCK MODE");
    else if (in.down[BT_SELECT]) h = TR("[</>]PATTERN [^]REC [v]TAP [A]MUTE [B]UNDO [X/Y]ENGINE [L1/R1]4 STEPS [L2/R2]16 STEPS", "[</>]PATTERN [^]REC [v]TAP [A]MUTE [B]UNDO [X/Y]ENGINE [L1/R1]4 STEPS [L2/R2]16 STEPS");
    else h = TR("[^v]SPUR [</>]STEP [L1/R1]REGLER [X/Y]WERT [L2/R2]SEITE [SEL]KARTE", "[^v]TRACK [</>]STEP [L1/R1]CONTROL [X/Y]VALUE [L2/R2]PAGE [SEL]MAP");
    hint(6, 452, h, C_ACC, C_DIM);
}

// ---------------------------------------------------------------- LIVE-Seite: 8 Mutes auf den Tasten (Lage wie am Geraet)
static const int LIVE_BTN[NTRACKS] = {BT_UP, BT_RIGHT, BT_DOWN, BT_LEFT, BT_X, BT_A, BT_B, BT_Y};
static const char* const LIVE_KEY[NTRACKS] = {"^", ">", "v", "<", "X", "A", "B", "Y"};
static void drawLive() {
    const Pattern& P = curP();
    char b[48];
    liveArt();   // Spektrum oben, Takt-Ringe in der Mitte
    text(10, 48, "LIVE", C_ACC, 2);
    snprintf(b, sizeof b, "PATTERN %02d", curPi() + 1);
    text(120, 50, b, C_TEXT);
    if (eng.nextPat >= 0) { snprintf(b, sizeof b, "> %02d", eng.nextPat + 1); text(120 + textW("PATTERN 00 "), 50, b, (SDL_GetTicks() / 250) % 2 ? C_ACC : C_DIM); }
    snprintf(b, sizeof b, "%d BPM", eng.proj.bpm);
    text(630 - textW(b, 2), 48, b, C_TEXT, 2);
    static const int OX[4] = {0, 1, 0, -1}, OY[4] = {-1, 0, 1, 0};
    for (int t = 0; t < NTRACKS; t++) {
        int cx = t < 4 ? 160 : 480, cy = 232, k = t % 4;
        int w = 100, h = 56, x = cx + OX[k] * 105 - w / 2, y = cy + OY[k] * 64 - h / 2;
        bool m = eng.muted(t);
        hitAdd(x, y, w, h, HK_LIVE, t);
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
    textC(160, 226, TR("STEUERKREUZ", "D-PAD"), C_DIM);
    textC(480, 226, TR("TASTEN", "BUTTONS"), C_DIM);
    // Statusleiste
    int y = 340;
    struct Chip { const char* key; const char* name; bool on; Col c; } ch[4] = {
        {"L1", TR("FILL (HALTEN)", "FILL (HOLD)"), eng.fill, C_LOCK}, {"R1", recOn ? TR("REC LÄUFT", "RECORDING") : (recSaving ? TR("SPEICHERT", "SAVING") : TR("WAV-AUFNAHME", "WAV RECORDING")), recOn, C_TRIG},
        {"SEL + </>", "PATTERN -/+", eng.nextPat >= 0, C_ACC}, {"START", eng.playing ? "STOP" : "PLAY", eng.playing, C_GREEN}};
    for (int i = 0; i < 4; i++) {
        int x = 6 + i * 158;
        { static const int LB[4] = {BT_L1, BT_R1, -1, BT_START}; if (LB[i] >= 0) hitAdd(x, y, 150, 40, HK_BTN, LB[i]); }
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
    hint(6, 452, TR("[STEUERKREUZ/XABY]MUTE [L1]FILL [R1]REC [L2/R2]SEITE [R-ST]DJ [SEL]KARTE", "[D-PAD/XABY]MUTE [L1]FILL [R1]REC [L2/R2]PAGE [R-ST]DJ [SEL]MAP"), C_ACC, C_DIM);
}

static bool fbOpen, kbOpen;
static void drawBrowser();
static void drawKeyboard();
static void drawSettings();
static void drawSong();
static void drawDrive();
static bool isGrooveboxOS();
static void runBench(const char* outPath, bool onDevice);
static bool touchBlocked();
static void drawAll(bool help) {
    nHits = 0; hitModalAt = 0;
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
    if (touchBlocked()) hitModal();   // Explorer/Tastatur/Editor/Export: per Steuerkreuz bedienen
    drawWtEditorIfOpen();
    drawExportModal();
    if (fbOpen) drawBrowser();
    if (kbOpen) drawKeyboard();
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
        Col bc = {(Uint8)(14 + (int)(sd >> 20) % 8), 6, 30};
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
    if (page == PG_PRJ) return;   // nur Schaltflaechen
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
    if (busOfPage(page) >= 0) {
        int b = busOfPage(page);
        uint8_t* bp = eng.proj.bus[b];
        const ParamDef& bd = busDef(b, bp[BX_TYPE], slot);
        if (bd.kind == K_NONE) return;
        lockA();
        if (slot == BX_TYPE) {   // Typ wechseln: Regler auf dessen Grundwerte, Sends/Weiterleitung bleiben
            uint8_t fwd = bp[BX_FWD], lvl = bp[BX_LVL];
            Engine::setBusType(eng.proj, b, clampi(bp[BX_TYPE] + (delta > 0 ? 1 : -1), 0, BF_COUNT - 1));
            bp[BX_FWD] = fwd; bp[BX_LVL] = lvl;
        } else bp[slot] = (uint8_t)clampi(bp[slot] + delta, 0, bd.max);
        unlockA();
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
    if (busOfPage(page) >= 0) return busDef(busOfPage(page), eng.proj.bus[busOfPage(page)][BX_TYPE], slot).max;
    if (!trackPage(page)) return 127;
    return paramDef(curP().machine[track], sp() * 8 + slot).max;
}

// ---------------------------------------------------------------- Projekte, Presets, Abfragen

enum { ACT_LOAD = 1, ACT_SAVE, ACT_NEW, ACT_PSAVE, ACT_INIT, ACT_PASTE, ACT_CLEARPAT, ACT_QUIT, ACT_CLEARTRK, ACT_DEMO,
       ACT_SAVEAS, ACT_NEWNAME, ACT_PSAVENAME, ACT_LOADNAME };

static bool fileExists(const char* p) { struct stat st; return stat(p, &st) == 0; }

static void askConfirm(int act, int arg, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); vsnprintf(confirmText, sizeof confirmText, fmt, ap); va_end(ap);
    confirmAct = act; confirmArg = arg; confirmOpen = true;
}

static int loadPresetPath(int t, const char* p) {   // 1 ok, 0 fehlt, -1 Fehler
    FILE* f = fopen(p, "rb");
    if (!f) return 0;
    uint8_t hdr[8], prm[NPARAMS];
    bool ok = fread(hdr, 8, 1, f) == 1 && !memcmp(hdr, "GBP1", 4) && hdr[4] < M_COUNT;
    int np = ok ? (hdr[5] > NPARAMS ? NPARAMS : hdr[5]) : 0;
    ok = ok && np > 0 && fread(prm, np, 1, f) == 1;
    if (ok && np < hdr[5]) fseek(f, hdr[5] - np, SEEK_CUR);
    char sname[96] = "";   // Sample-Name (Trailer wie im Projekt: "SPL1", Anzahl, Platz, Laenge, Name)
    bool hasName = false;
    if (ok) {
        char mg[4]; uint16_t n = 0; uint8_t eh[2];
        if (fread(mg, 1, 4, f) == 4 && !memcmp(mg, "SPL1", 4) && fread(&n, 2, 1, f) == 1) {
            hasName = true;
            if (n > 0 && fread(eh, 1, 2, f) == 2 && eh[1] < sizeof sname && fread(sname, 1, eh[1], f) == eh[1]) sname[eh[1]] = 0;
            else sname[0] = 0;
        }
    }
    fclose(f);
    if (!ok) return -1;
    int slot = -1, si = machineSampleParam(hdr[4]);
    if (si == P_X3 && hdr[6] < 3) si = -1;   // WTBL-Presets vor v13 hatten keine eigene Wavetable
    if (si >= 0 && np > si) {
        if (!hasName) {   // alter Sound: Nummer = Position in der frueheren Sample-Liste (neuere 250er-Liste, wenn sie reicht)
            static char legacy[250][96];
            int n = libLegacyList(legacy, 250, SAMPLE_DEPTH, 500);
            if (prm[P_S0] >= n) n = libLegacyList(legacy, 128, 1, 256);
            if (prm[P_S0] < n) snprintf(sname, sizeof sname, "%s", legacy[prm[P_S0]]);
        }
        if (sname[0]) slot = poolAssign(sname);
        if (slot < 0) slot = si == P_S0 ? curP().p[t][P_S0] : -1;   // Sample fehlt: bisheriges behalten
    }
    lockA();
    Pattern& P = curP();
    Engine::initTrack(P, t, hdr[4]);
    if (hdr[6] < 3) {   // bis Format 2: 96 Parameter im alten Layout -> v13 (Sends, FM-Operatoren, SYNTH/WTBL-Seite 2)
        uint8_t o[OLD_NPARAMS];
        for (int i = 0; i < OLD_NPARAMS; i++) o[i] = i < np ? prm[i] : P.p[t][i];
        Engine::migrateTrack13(hdr[4], o, P.p[t], false);
    } else memcpy(P.p[t], prm, np);
    if (hdr[6] == 0 && (hdr[4] == M_SYNTH || hdr[4] == M_WTBL) && P.p[t][P_FTYP] == FT_LP12) P.p[t][P_FTYP] = FT_LP24;   // alte Presets
    if (hdr[6] < 2) { P.p[t][P_FCRV] = 64; P.p[t][P_ACRV] = 64; }
    if (slot >= 0 && si >= 0) P.p[t][si] = (uint8_t)slot;
    else if (si == P_X3 && P.p[t][P_S0] % 5 == 4) P.p[t][P_S0] = 0;   // eigene Wavetable fehlt: BASIC
    unlockA();
    return 1;
}

#include "filedlg.inc"
#include "wtedit.inc"
static void drawWtEditorIfOpen() { if (wteOpen) drawWtEditor(); }

static void runAction(int act, int arg) {
    switch (act) {
    case ACT_SAVEAS: doSaveProjectAs(pendingName); break;
    case ACT_NEWNAME: doNewProject(pendingName); break;
    case ACT_LOADNAME: doLoadProject(pendingName); break;
    case ACT_PSAVENAME: {
        char p[700]; presetFile(curP().machine[arg], pendingName, p, sizeof p);
        toast(savePresetFile(arg, p) ? TR("SOUND GESPEICHERT: %s", "SOUND SAVED: %s") : TR("SPEICHERN FEHLGESCHLAGEN", "SAVING FAILED"), pendingName);
    } break;
    case ACT_DEMO: {
        char nm[64]; freeProjectName("Neon Nights", nm, sizeof nm);
        lockA(); eng.stop(); buildDemo(eng); eng.songOn = true; eng.songStart(0); unlockA();
        poolDefaults();
        doSaveProjectAs(nm);
        toast(TR("DEMO '%s' GELADEN - START DRÜCKEN", "DEMO '%s' LOADED - PRESS START"), nm);
    } break;
    case ACT_PASTE: lockA(); curP() = clip; unlockA(); toast(TR("PATTERN EINGEFÜGT", "PATTERN PASTED")); break;
    case ACT_CLEARTRK:
        lockA();
        for (int st = 0; st < NSTEPS; st++) { curP().s[arg][st].on = 0; memset(curP().s[arg][st].lock, NO_LOCK, NPARAMS); }
        unlockA();
        toast(TR("SPUR %d GELEERT (SELECT+B = RÜCKGÄNGIG)", "TRACK %d CLEARED (SELECT+B = UNDO)"), arg + 1);
        break;
    case ACT_CLEARPAT:
        lockA();
        for (int t = 0; t < NTRACKS; t++) for (int st = 0; st < NSTEPS; st++) { curP().s[t][st].on = 0; memset(curP().s[t][st].lock, NO_LOCK, NPARAMS); }
        unlockA();
        toast(TR("PATTERN %02d GELEERT", "PATTERN %02d CLEARED"), curPi() + 1);
        break;
    case ACT_QUIT: quitReq = true; break;
    case ACT_INIT: {
        int m = curP().machine[track];
        lockA(); Engine::initTrack(curP(), track, m); unlockA();
        toast(TR("SPUR %d: KLANG ZURÜCKGESETZT", "TRACK %d: SOUND RESET"), track + 1);
    } break;
    }
}
static void prjTrigger(int k) {   // Schaltflaechen der PRJ-Seite
    switch (k) {
    case 0: openBrowser(FB_PROJECT); break;
    case 1: toast(saveProject() ? TR("GESPEICHERT: %s", "SAVED: %s") : TR("SPEICHERN FEHLGESCHLAGEN", "SAVING FAILED"), cfg.project); break;
    case 2: openKeyboard(KB_SAVEAS, cfg.project); break;
    case 3: { char nm[64]; freeProjectName(TR("Projekt", "Project"), nm, sizeof nm); openKeyboard(KB_NEW, nm); } break;
    case 4: openBrowser(FB_PRESET); break;
    case 5: { char nm[48]; snprintf(nm, sizeof nm, "%s %d", MACHINE_NAMES[curP().machine[track] % M_COUNT], track + 1); openKeyboard(KB_PSAVE, nm); } break;
    case 6: askConfirm(ACT_INIT, 0, TR("KLANG VON SPUR %d ZURÜCKSETZEN?", "RESET SOUND OF TRACK %d?"), track + 1); break;
    case 7: askConfirm(ACT_DEMO, 0, TR("DEMO-SONG ALS NEUES PROJEKT ANLEGEN?", "CREATE DEMO SONG AS NEW PROJECT?")); break;
    }
}
static bool prjIsAction(int k) { return k >= 0 && k < 8; }

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
    SI_H_PAT, SI_PATTERN, SI_BPM, SI_SWING, SI_SCALE, SI_ROOT, SI_LENALL, SI_FILL, SI_COPY, SI_PASTE, SI_CLEAR, SI_MUTATE, SI_UNDO, SI_REDO, SI_DEMO,
    SI_H_TRK, SI_LEN, SI_SPEED, SI_TMPL, SI_CLRTRK,
    SI_H_AUDIO, SI_OUT, SI_BUF, SI_VOL, SI_CORES,
    SI_H_MIDI, SI_MIN, SI_NOUT, SI_CLK, SI_MCH,
    SI_H_USB, SI_USB, SI_USBAUDIO, SI_USBVOL, SI_INTUSB, SI_USBTONE, SI_PCIN, SI_PCREC, SI_IP, SI_USBNET, SI_UACVER, SI_UACDIR, SI_MIDIINFO,
    SI_H_SMP, SI_SAMPLES, SI_WEB, SI_DRIVE, SI_OLDSMP,
    SI_H_SYS, SI_LANG, SI_DISP, SI_LEARN, SI_BENCH, SI_INFO, SI_BAT, SI_QUIT, SI_COUNT
};
static int setSel = SI_PATTERN, prevPage = PG_TRG;
static bool setIn = false;   // SET: false = Bereiche links waehlen, true = in den Eintraegen des Bereichs (A rein, B raus)
static int poolMissing = 0;   // beim letzten Projekt-Laden nicht gefundene Samples
// Rhythmus-Vorlagen (Bit i = Step i+1 von 16), werden ueber die ganze Spurlaenge wiederholt
static const char* const RHY_NAMES_DE[] = {"Four on the Floor", "Backbeat (2+4)", "Offbeat", "Achtel", "Sechzehntel", "Breakbeat-Kick",
                                           "Clave 3-2", "Tresillo", "Dembow-Snare", "Leer"};
static const char* const RHY_NAMES_EN[] = {"Four on the floor", "Backbeat (2+4)", "Offbeat", "Eighths", "Sixteenths", "Breakbeat kick",
                                           "Clave 3-2", "Tresillo", "Dembow snare", "Empty"};
#define RHY_NAMES (g_lang ? RHY_NAMES_EN : RHY_NAMES_DE)
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
    toast(TR("SPUR %d: %s (SELECT+B = RÜCKGÄNGIG)", "TRACK %d: %s (SELECT+B = UNDO)"), track + 1, RHY_NAMES[k]);
}
static float setScroll = 0;
static bool setIsHeader(int i) { return i == SI_H_PAT || i == SI_H_TRK || i == SI_H_AUDIO || i == SI_H_MIDI || i == SI_H_USB || i == SI_H_SMP || i == SI_H_SYS; }
static int setCatOf(int i) { while (i > 0 && !setIsHeader(i)) i--; return i; }   // Bereich = vorangehende Ueberschrift
static int setCatEnd(int h) { int e = h + 1; while (e < SI_COUNT && !setIsHeader(e)) e++; return e; }
// kurze Erklaerung je Eintrag (unten in den Einstellungen)
static const char* setHelp(int i) {
    switch (i) {
    case SI_PATTERN: return TR("Welches Pattern spielt (beim Abspielen am Pattern-Ende). Kurz: SELECT + links/rechts", "Which pattern plays (switches at the pattern end). Shortcut: SELECT + left/right");
    case SI_BPM: return TR("Tempo. Tap-Tempo: SELECT + runter mehrmals im Takt", "Tempo. Tap tempo: SELECT + down several times in time");
    case SI_SWING: return TR("Jeder zweite Sechzehntel kommt später", "Every second sixteenth comes later");
    case SI_SCALE: return TR("Töne aller Spuren rasten in diese Tonleiter ein", "Notes of all tracks snap to this scale");
    case SI_ROOT: return TR("Grundton der Tonleiter", "Root note of the scale");
    case SI_LENALL: return TR("Setzt alle 8 Spuren auf dieselbe Länge", "Sets all 8 tracks to the same length");
    case SI_FILL: return TR("FILL dauerhaft an. Kurz: START beim Spielen halten, LIVE: L1 halten", "FILL permanently on. Shortcut: hold START while playing, LIVE: hold L1");
    case SI_COPY: return TR("Kopiert das ganze Pattern in die Ablage", "Copies the whole pattern to the clipboard");
    case SI_PASTE: return TR("Überschreibt das Pattern mit der Ablage", "Overwrites the pattern with the clipboard");
    case SI_CLEAR: return TR("Löscht alle Steps aller Spuren (Klänge bleiben)", "Clears all steps of all tracks (sounds stay)");
    case SI_MUTATE: return TR("Variiert Trigs, Töne und Anschlag. Stärke mit links/rechts, A = los", "Varies trigs, notes and velocity. Amount with left/right, A = go");
    case SI_UNDO: return TR("Letzte Änderung zurücknehmen. Kurz: SELECT + B", "Undo the last change. Shortcut: SELECT + B");
    case SI_REDO: return TR("Zurückgenommene Änderung wiederherstellen", "Restores the last undone change");
    case SI_DEMO: return TR("Legt den Demo-Song als eigenes Projekt an", "Creates the demo song as a separate project");
    case SI_LEN: return TR("Steps der gewählten Spur (1-128). Unterschiedliche Längen = Polymetrie", "Steps of the selected track (1-128). Different lengths = polymeter");
    case SI_SPEED: return TR("Tempo der Spur relativ zum Song (1/8x bis 2x)", "Tempo of the track relative to the song (1/8x to 2x)");
    case SI_TMPL: return TR("Fertiger Rhythmus für die Spur. Links/rechts wählen, A setzt ihn", "Ready-made rhythm for the track. Choose with left/right, A applies it");
    case SI_CLRTRK: return TR("Löscht alle Steps der gewählten Spur", "Clears all steps of the selected track");
    case SI_OUT: return TR("Audio-Ausgang (intern, USB-Interface ...)", "Audio output (internal, USB interface ...)");
    case SI_BUF: return TR("Kleiner = weniger Verzögerung, größer = sicherer gegen Aussetzer", "Smaller = less latency, larger = safer against dropouts");
    case SI_VOL: return TR("Gesamtlautstärke der Groovebox", "Overall volume of the groovebox");
    case SI_CORES: return TR("2 Kerne verteilen die Stimmen (weniger Aussetzer)", "2 cores share the voices (fewer dropouts)");
    case SI_MIN: return TR("Noten und Regler von einem MIDI-Gerät empfangen", "Receive notes and controls from a MIDI device");
    case SI_NOUT: return TR("Spuren senden ihre Noten als MIDI (Spur = Kanal)", "Tracks send their notes as MIDI (track = channel)");
    case SI_CLK: return TR("MIDI-Clock senden oder als Taktgeber folgen", "Send MIDI clock or follow it");
    case SI_MCH: return TR("Welche Spur MIDI-Noten spielt", "Which track plays incoming MIDI notes");
    case SI_USB: return TR("HOST: Geräte am R36S. PC: Netzwerk, MIDI und Audio zum Computer", "HOST: devices on the R36S. PC: network, MIDI and audio to the computer");
    case SI_USBAUDIO: return TR("Summe als USB-Audio an den PC", "Master as USB audio to the PC");
    case SI_USBVOL: return TR("Lautstärke des USB-Audios zum PC", "Volume of the USB audio to the PC");
    case SI_INTUSB: return TR("Lautsprecher/Kopfhörer stumm, solange USB-Audio läuft", "Mute speaker/headphones while USB audio runs");
    case SI_USBTONE: return TR("Prüft, ob der PC Ton von der Groovebox bekommt", "Checks whether the PC receives sound from the groovebox");
    case SI_PCIN: return TR("Ton vom PC über den R36S hören", "Hear PC audio through the R36S");
    case SI_PCREC: return TR("Ton vom PC als Sample aufnehmen (samples/pc)", "Record PC audio as a sample (samples/pc)");
    case SI_IP: return TR("Adresse des R36S im PC-Modus", "Address of the R36S in PC mode");
    case SI_UACDIR: return TR("NUR SENDEN: Handy/PC sieht die Groovebox nur als Eingang und behält Lautsprecher/Bluetooth (z. B. iPhone + Ton im Browser). Dann kein PC-Ton zum R36S", "SEND ONLY: phone/PC sees the groovebox only as input and keeps its speaker/Bluetooth (e.g. iPhone + audio in browser). No PC audio to the R36S then");
    case SI_USBNET: return TR("NCM: ohne Treiber an Windows 11, Mac, iPad, Linux. RNDIS: nur für ältere Windows 10", "NCM: driverless on Windows 11, Mac, iPad, Linux. RNDIS: only for older Windows 10");
    case SI_UACVER: return TR("UAC2: 44,1/48 kHz, Standard. UAC1: für ältere Geräte ohne UAC2 (Windows 7/8, Konsolen)", "UAC2: 44.1/48 kHz, default. UAC1: for older devices without UAC2 (Windows 7/8, consoles)");
    case SI_MIDIINFO: return TR("Verbundene MIDI-Geräte und empfangene/gesendete Nachrichten", "Connected MIDI devices and received/sent messages");
    case SI_SAMPLES: return TR("WAVs in der Bibliothek / im Projekt geladen / fehlend", "WAVs in the library / loaded in the project / missing");
    case SI_WEB: return TR("Im Browser öffnen: Samples hochladen, Projekte sichern, Bildschirm", "Open in a browser: upload samples, back up projects, screen");
    case SI_DRIVE: return TR("Speicher als USB-Laufwerk am PC (Samples kopieren)", "Storage as a USB drive on the PC (copy samples)");
    case SI_OLDSMP: return TR("Nur alte Projekte: andere Zuordnung der Sample-Nummern probieren", "Old projects only: try another mapping of the sample numbers");
    case SI_LANG: return TR("Sprache der Oberfläche", "Language of the interface");
    case SI_DISP: return TR("Bild verschoben oder Farben falsch? Hier ausgleichen", "Picture shifted or colours wrong? Adjust here");
    case SI_LEARN: return TR("Tasten und Sticks neu zuordnen", "Map buttons and sticks again");
    case SI_BENCH: return TR("Misst Audio- und Bildleistung (Bericht: Logs/bench.txt)", "Measures audio and display performance (report: logs/bench.txt)");
    case SI_INFO: return TR("Version, Samples, Aussetzer seit dem Start", "Version, samples, dropouts since start");
    case SI_BAT: return TR("Ladestand aus der Akkuspannung (geglättet), dahinter Spannung, Strom und der Wert des Akku-Treibers zum Vergleich", "Charge from the battery voltage (smoothed), then voltage, current and the battery driver's value for comparison");
    case SI_QUIT: return TR("Speichert das Projekt und beendet die Groovebox", "Saves the project and quits the groovebox");
    }
    return "";
}
static bool setIsAction(int i) { return i == SI_DEMO || i == SI_UNDO || i == SI_REDO || i == SI_CLRTRK || i == SI_BENCH || i == SI_PCREC || i == SI_USBTONE || i == SI_DRIVE || i == SI_COPY || i == SI_PASTE || i == SI_CLEAR || i == SI_LEARN || i == SI_QUIT; }
static bool runLearn();
static char versionStr[32] = "";

static void setLabel(int i, char* l, int ln, char* v, int vn) {
    const Pattern& P = curP();
    v[0] = 0;
    switch (i) {
    case SI_H_PAT: snprintf(l, ln, "PATTERN"); break;
    case SI_H_TRK: snprintf(l, ln, TR("SPUR %d", "TRACK %d"), track + 1); break;
    case SI_H_SMP: snprintf(l, ln, "SAMPLES"); break;
    case SI_PATTERN: snprintf(l, ln, "Pattern"); snprintf(v, vn, "P%02d%s", curPi() + 1, eng.nextPat >= 0 ? " >" : ""); break;
    case SI_LEN: snprintf(l, ln, TR("Länge", "Length")); snprintf(v, vn, "%d Steps", P.len[track]); break;
    case SI_LENALL: snprintf(l, ln, TR("Länge alle Spuren", "Length all tracks")); snprintf(v, vn, "%d Steps", P.len[track]); break;
    case SI_SPEED: snprintf(l, ln, TR("Tempo-Faktor", "Tempo factor")); snprintf(v, vn, "%s", SPEED_NAMES[P.speed[track] % 7]); break;
    case SI_FILL: snprintf(l, ln, TR("FILL dauerhaft", "FILL latched")); snprintf(v, vn, "%s", eng.fill ? TR("AN", "ON") : TR("AUS", "OFF")); break;
    case SI_BPM: snprintf(l, ln, "Tempo"); snprintf(v, vn, "%d BPM", eng.proj.bpm); break;
    case SI_SWING: snprintf(l, ln, "Swing"); snprintf(v, vn, "%d%%", 50 + eng.proj.mst[MS_SWNG]); break;
    case SI_SCALE: snprintf(l, ln, TR("Tonskala", "Scale"));
        if (curScale()) snprintf(v, vn, "%s", SCALE_LONG[scaleType(curScale()) - 1]);
        else snprintf(v, vn, TR("AUS (chromatisch)", "OFF (chromatic)"));
        break;
    case SI_ROOT: snprintf(l, ln, TR("Grundton", "Root note")); snprintf(v, vn, "%s", NOTE_NAMES[eng.proj.mst[MS_ROOT] % 12]); break;
    case SI_COPY: snprintf(l, ln, TR("Pattern kopieren", "Copy pattern")); break;
    case SI_MUTATE: { const char* const AM[3] = {TR("LEICHT", "LIGHT"), TR("MITTEL", "MEDIUM"), TR("STARK", "STRONG")};
        snprintf(l, ln, TR("Pattern mutieren (A = los)", "Mutate pattern (A = go)")); snprintf(v, vn, "%s", AM[mutAmt % 3]); } break;
    case SI_PASTE: snprintf(l, ln, TR("Pattern einfügen", "Paste pattern")); snprintf(v, vn, "%s", haveClip ? "" : TR("(leer)", "(empty)")); break;
    case SI_CLEAR: snprintf(l, ln, TR("Pattern leeren", "Clear pattern")); break;
    case SI_CLRTRK: snprintf(l, ln, TR("Spur leeren", "Clear track")); break;
    case SI_TMPL: snprintf(l, ln, TR("Rhythmus-Vorlage", "Rhythm template")); snprintf(v, vn, "< %s >", RHY_NAMES[tmplSel]); break;
    case SI_UNDO: snprintf(l, ln, TR("Rückgängig (SELECT + B)", "Undo (SELECT + B)")); snprintf(v, vn, TR("%d Schritte", "%d steps"), undoCnt); break;
    case SI_DEMO: snprintf(l, ln, TR("Demo-Song 'Neon Nights' laden", "Load demo song 'Neon Nights'")); snprintf(v, vn, ">>"); break;
    case SI_REDO: snprintf(l, ln, TR("Wiederholen", "Redo")); snprintf(v, vn, "%s", redoPat >= 0 ? ">>" : TR("(nichts)", "(nothing)")); break;
    case SI_H_AUDIO: snprintf(l, ln, "AUDIO"); break;
    case SI_OUT: snprintf(l, ln, TR("Ausgang", "Output")); { int d = devIndex(); snprintf(v, vn, "%.26s", d ? devNames[d - 1] : TR("Standard (intern)", "Default (internal)")); } break;
    case SI_BUF: snprintf(l, ln, TR("Puffer", "Buffer"));
        if (bufOverride) snprintf(v, vn, TR("%d (Startoption)", "%d (start option)"), bufOverride);
        else snprintf(v, vn, "%d  (%.1f ms)", BUF_SIZES[cfg.buf], BUF_SIZES[cfg.buf] * 1000.f / eng.sr);
        break;
    case SI_VOL: snprintf(l, ln, TR("Lautstärke", "Volume")); snprintf(v, vn, "%d", eng.proj.mst[MS_VOL]); break;
    case SI_CORES: snprintf(l, ln, TR("Audio-Rechenkerne", "Audio CPU cores")); snprintf(v, vn, "%s", eng.audioThreads > 1 ? TR("2 (schneller)", "2 (faster)") : (cfg.cores > 1 ? TR("1 (nur ein Kern verfügbar)", "1 (only one core available)") : "1")); break;
    case SI_H_MIDI: snprintf(l, ln, "MIDI"); break;
    case SI_MIN: snprintf(l, ln, TR("MIDI-Eingang", "MIDI input")); snprintf(v, vn, "%s", cfg.midiIn ? TR("AN", "ON") : TR("AUS", "OFF")); break;
    case SI_NOUT: snprintf(l, ln, TR("Noten ausgeben", "Send notes")); snprintf(v, vn, "%s", cfg.notesOut ? TR("AN (Spur = Kanal)", "ON (track = channel)") : TR("AUS", "OFF")); break;
    case SI_CLK: snprintf(l, ln, "Clock"); snprintf(v, vn, "%s", cfg.clk == 1 ? TR("SENDEN", "SEND") : (cfg.clk == 2 ? TR("EMPFANGEN", "RECEIVE") : TR("AUS", "OFF"))); break;
    case SI_MCH: snprintf(l, ln, TR("Eingangskanäle", "Input channels")); snprintf(v, vn, "%s", cfg.chm ? TR("Kanal 1-8 = Spur 1-8", "Channel 1-8 = track 1-8") : TR("AUTO (gewählte Spur)", "AUTO (selected track)")); break;
    case SI_H_USB: snprintf(l, ln, "USB & PC"); break;
    case SI_USB: snprintf(l, ln, TR("USB-Modus", "USB mode"));
        snprintf(v, vn, "%s", usbMode == 0 ? TR("HOST (Geräte am R36S)", "HOST (devices on R36S)") : (usbMode == 1 ? TR("PC (Netz-MIDI, SSH)", "PC (network MIDI, SSH)") : "PC + AUDIO"));
        break;
    case SI_USBAUDIO: snprintf(l, ln, TR("Audio an PC (USB)", "Audio to PC (USB)"));
        if (!cfg.usbAudio) snprintf(v, vn, TR("AUS", "OFF"));
        else if (usbMode != 2) snprintf(v, vn, TR("AN (erst PC MIDI+Audio)", "ON (needs PC MIDI+audio)"));
        else snprintf(v, vn, usbAudioStatus() == 2 ? TR("AN - PC LIEST MIT", "ON - PC IS LISTENING") : (usbAudioActive() ? TR("AN - WARTET AUF PC", "ON - WAITING FOR PC") : TR("AN - SUCHE GERÄT", "ON - SEARCHING DEVICE")));
        break;
    case SI_USBVOL: snprintf(l, ln, TR("USB-Lautstärke", "USB volume")); snprintf(v, vn, "%d%%", cfg.usbVol); break;
    case SI_INTUSB: snprintf(l, ln, TR("Intern bei USB-Audio", "Internal with USB audio")); snprintf(v, vn, "%s", cfg.intUsb ? (intMuted ? TR("STUMM (AKTIV)", "MUTED (ACTIVE)") : TR("STUMM", "MUTED")) : "AN"); break;
    case SI_USBTONE: snprintf(l, ln, TR("Testton an PC (5 s, 1 kHz)", "Test tone to PC (5 s, 1 kHz)")); if (!usbAudioActive()) snprintf(v, vn, TR("erst Audio an PC", "needs audio to PC")); break;
    case SI_PCIN: snprintf(l, ln, TR("PC-Ton über R36S", "PC audio through R36S"));
        if (!cfg.pcIn) snprintf(v, vn, TR("AUS", "OFF"));
        else if (usbMode != 2) snprintf(v, vn, TR("AN (erst PC MIDI+Audio)", "ON (needs PC MIDI+audio)"));
        else snprintf(v, vn, pcInStatus() == 2 ? TR("AN - PC SPIELT", "ON - PC PLAYING") : TR("AN - STILL", "ON - SILENT"));
        break;
    case SI_PCREC: snprintf(l, ln, TR("PC-Ton als Sample aufnehmen", "Record PC audio as sample"));
        if (pcRecState() == 1) snprintf(v, vn, TR("WARTET AUF TON (A = ABBRECHEN)", "WAITING FOR SOUND (A = CANCEL)"));
        else if (pcRecState() == 2) snprintf(v, vn, TR("AUFNAHME %.1f s (A = STOP)", "RECORDING %.1f s (A = STOP)"), pcRecSeconds());
        else if (usbMode != 2) snprintf(v, vn, TR("erst PC MIDI+Audio", "needs PC MIDI+audio"));
        break;
    case SI_UACDIR: snprintf(l, ln, TR("USB-Audio-Richtung", "USB audio direction"));
        snprintf(v, vn, "%s", cfg.uacSendOnly ? TR("NUR SENDEN (Gerät behält Lautsprecher)", "SEND ONLY (device keeps speaker)") : TR("BEIDE (Standard)", "BOTH (default)")); break;
    case SI_USBNET: snprintf(l, ln, TR("USB-Netzwerk", "USB network")); snprintf(v, vn, "%s", cfg.usbNet ? TR("RNDIS (altes Windows)", "RNDIS (old Windows)") : TR("NCM (Standard)", "NCM (standard)")); break;
    case SI_UACVER: snprintf(l, ln, TR("USB-Audio-Klasse", "USB audio class"));
        snprintf(v, vn, "%s", cfg.uac == 1 ? TR("UAC1 (ältere Geräte)", "UAC1 (older devices)") : "UAC2 (44,1/48 kHz)");
        if (usbMode == 2 && usbAudioRate() > 0) { size_t k = strlen(v); snprintf(v + k, vn - k, " - PC: %d Hz", usbAudioRate()); }
        break;
    case SI_MIDIINFO: { snprintf(l, ln, TR("MIDI-Geräte", "MIDI devices"));
        char nm[96]; midiPortInfo(nm, sizeof nm);
        if (!strncmp(nm, "kein", 4)) snprintf(nm, sizeof nm, "%s", TR("keine", "none"));   // midi.cpp meldet ohne Sprache
        unsigned ni = 0, no = 0; midiCounts(&ni, &no);
        snprintf(v, vn, TR("%.40s  ein %u / aus %u", "%.40s  in %u / out %u"), nm, ni, no);
    } break;
    case SI_IP: snprintf(l, ln, TR("Adresse", "Address")); snprintf(v, vn, "%s", usbMode && netIp[0] ? netIp : "-"); break;
    case SI_WEB: snprintf(l, ln, TR("Samples per Browser", "Samples via browser"));
        if (!webPort()) snprintf(v, vn, "-");
        else if (usbMode && netIp[0]) snprintf(v, vn, webPort() == 80 ? "http://%s" : "http://%s:%d", netIp, webPort());
        else snprintf(v, vn, TR("erst PC-Modus an", "needs PC mode"));
        break;
    case SI_H_SYS: snprintf(l, ln, "SYSTEM"); break;
    case SI_DISP: snprintf(l, ln, TR("Bild-Versatz (Lage + Farben)", "Display offset (position + colours)"));
        if (drmOutActive()) snprintf(v, vn, "%d Px (%d)", cfg.dispOff / 3, cfg.dispOff);
        else snprintf(v, vn, TR("nur Groovebox-OS", "Groovebox OS only"));
        break;
    case SI_LEARN: snprintf(l, ln, TR("Tasten neu lernen", "Learn buttons again")); break;
    case SI_LANG: snprintf(l, ln, "Sprache / Language"); snprintf(v, vn, "%s", g_lang ? "English" : "Deutsch"); break;
    case SI_BENCH: snprintf(l, ln, TR("Leistungstest (ca. 30 s)", "Benchmark (approx. 30 s)")); snprintf(v, vn, TR("A = START", "A = GO")); break;
    case SI_INFO: snprintf(l, ln, "Info");
        snprintf(v, vn, TR("%.12s  %d Smp  Aussetzer %d", "%.12s  %d smp  dropouts %d"), versionStr[0] ? versionStr : "dev", sampleCount(), audLateTotal + audLate);
        break;
    case SI_BAT: snprintf(l, ln, TR("Akku", "Battery"));
        if (batPct < 0) snprintf(v, vn, TR("kein Akku gefunden", "no battery found"));
        else snprintf(v, vn, "%d%%%s %.2fV %dmA DC%d %s%d%%", batPct, batPlug ? (batFull ? TR(" VOLL", " FULL") : TR(" LÄDT", " CHG")) : "", batMv / 1000.f, batMa, dcRaw, TR("Chip ", "chip "), batDrv);
        break;
    case SI_QUIT: snprintf(l, ln, TR("Groovebox beenden", "Quit groovebox")); break;
    case SI_DRIVE: snprintf(l, ln, TR("USB-Laufwerk (Samples vom PC)", "USB drive (samples from PC)")); if (!isGrooveboxOS()) snprintf(v, vn, TR("nur Groovebox-OS", "Groovebox OS only")); break;
    case SI_OLDSMP: snprintf(l, ln, TR("Alte Sample-Nummern", "Old sample numbers"));
        if (poolLegacy < 0) snprintf(v, vn, TR("- (Projekt hat Namen)", "- (project has names)"));
        else snprintf(v, vn, "%s", poolLegacy ? TR("ALLE EBENEN (BIS 250)", "ALL LEVELS (UP TO 250)") : TR("1 ORDNER-EBENE (BIS 128)", "1 FOLDER LEVEL (UP TO 128)"));
        break;
    case SI_SAMPLES: {   // Bibliothek / im Projekt geladen / fehlend / nicht lesbare Formate
        int inPool = 0; for (int k = 0; k < PREVIEW_SLOT; k++) if (sampleGet(k)) inPool++;
        snprintf(l, ln, "Samples");
        int o = snprintf(v, vn, TR("%d in Bibliothek, %d im Projekt", "%d in library, %d in project"), libCount(), inPool);
        if (poolMissing && o < vn) o += snprintf(v + o, vn - o, TR(", %d fehlen", ", %d missing"), poolMissing);
        if (libSkipped() && o < vn) snprintf(v + o, vn - o, TR(", %d kein WAV", ", %d not WAV"), libSkipped());
    } break;
    }
}

static void setChange(int i, int d) {
    Pattern& P = curP();
    switch (i) {
    case SI_LANG: g_lang = cfg.lang = cfg.lang ? 0 : 1; saveSettings(); break;
    case SI_OLDSMP:   // altes Projekt: andere fruehere Reihenfolge ausprobieren (bis zum naechsten Speichern)
        if (poolLegacy >= 0) {
            poolLoadForProject(projPath, 1 - poolLegacy);
            if (!poolMissing) toast(TR("SAMPLES NEU ZUGEORDNET - ANHÖREN, DANN SPEICHERN", "SAMPLES REMAPPED - LISTEN, THEN SAVE"));
        }
        break;
    case SI_TMPL: tmplSel = ((tmplSel + d) % RHY_N + RHY_N) % RHY_N; break;
    case SI_PATTERN: {
        switchPattern(curPi() + d);
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
    case SI_UACDIR: cfg.uacSendOnly = !cfg.uacSendOnly; saveSettings(); if (usbMode == 2) usbApplyAt = SDL_GetTicks() + 1200; break;   // Gadget neu aufbauen
    case SI_USBNET: cfg.usbNet = !cfg.usbNet; saveSettings(); if (usbMode) usbApplyAt = SDL_GetTicks() + 1200; break;   // Gadget neu aufbauen
    case SI_UACVER: cfg.uac = cfg.uac == 1 ? 2 : 1; saveSettings(); if (usbMode == 2) usbApplyAt = SDL_GetTicks() + 1200; break;
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
    mutUndo = P; haveMutUndo = true; mutPat = curPi();
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
    const char* const AM[3] = {TR("LEICHT", "LIGHT"), TR("MITTEL", "MEDIUM"), TR("STARK", "STRONG")};
    toast(TR("PATTERN %02d MUTIERT (%s)", "PATTERN %02d MUTATED (%s)"), curPi() + 1, AM[mutAmt % 3]);
}

static bool isGrooveboxOS() { struct stat st; return stat("/etc/groovebox-os", &st) == 0; }
static void reloadSamples();
// Umgebung fuer pc-modus.sh: Netzwerk- und Audio-Klasse aus SET
static const char* usbEnv() {
    static char e[96];
    snprintf(e, sizeof e, "GB_USB_NET=%s GB_UAC=%d GB_UAC_IN=%d ", cfg.usbNet ? "rndis" : "ncm", cfg.uac == 1 ? 1 : 2, cfg.uacSendOnly ? 0 : 1);
    return e;
}
static bool usbScript(const char* args, char* okMsg, int n) {
    if (selfTesting) return false;   // pc-modus.sh ausfuehren, OK:/FEHLER:-Zeile liefern
    char cmd[800], lp[700], line[200];
    snprintf(cmd, sizeof cmd, "%sbash '%spc-modus.sh' %s", usbEnv(), basePath, args);
    int rc = system(cmd);
    snprintf(lp, sizeof lp, "%spc-modus.log", basePath);
    bool ok = false;
    snprintf(okMsg, n, rc == 0 ? TR("FERTIG", "DONE") : TR("FEHLER (PC-MODUS.LOG)", "ERROR (PC-MODUS.LOG)"));
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
    if (!isGrooveboxOS()) { toast(TR("NUR IM GROOVEBOX-OS", "ONLY ON GROOVEBOX OS")); return; }
    saveProject();
    toast(TR("USB-LAUFWERK WIRD VERBUNDEN ...", "CONNECTING USB DRIVE ..."));
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
    toast(TR("LAUFWERK WIRD GETRENNT ...", "DISCONNECTING DRIVE ..."));
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
        if (upd) { toast(TR("UPDATE GEFUNDEN - NEUSTART ...", "UPDATE FOUND - RESTARTING ...")); msgUntil = SDL_GetTicks() + 5000; quitReq = true; }
    }
    reloadSamples();
    toast(TR("%d SAMPLES GELADEN", "%d SAMPLES LOADED"), sampleCount());
}
static void drawDrive() {
    rrect(60, 120, 520, 220, 16, C_ACC);
    rrect(62, 122, 516, 216, 15, C_BG, 250);
    textC(320, 142, TR("USB-LAUFWERK AKTIV", "USB DRIVE ACTIVE"), C_ACC);
    textC(320, 182, TR("Am PC erscheint das Laufwerk GROOVEBOX.", "The drive GROOVEBOX appears on the PC."), C_TEXT);
    textC(320, 204, TR("Samples in den Ordner samples/ kopieren,", "Copy samples into the folder samples/,"), C_TEXT);
    textC(320, 226, TR("dann das Laufwerk am PC AUSWERFEN.", "then EJECT the drive on the PC."), C_TEXT);
    rrect(200, 270, 240, 36, 9, C_GREEN); textC(320, 280, TR("A = FERTIG, ZURÜCK", "A = DONE, BACK"), C_BG);
}

static void setAction(int i) {
    switch (i) {
    case SI_COPY: clip = curP(); haveClip = true; toast(TR("PATTERN %02d KOPIERT", "PATTERN %02d COPIED"), curPi() + 1); break;
    case SI_PASTE:
        if (haveClip) askConfirm(ACT_PASTE, 0, TR("PATTERN %02d MIT KOPIE ÜBERSCHREIBEN?", "OVERWRITE PATTERN %02d WITH COPY?"), curPi() + 1);
        else toast(TR("ZWISCHENABLAGE LEER", "CLIPBOARD EMPTY"));
        break;
    case SI_CLEAR: askConfirm(ACT_CLEARPAT, 0, TR("ALLE STEPS VON PATTERN %02d LÖSCHEN?", "CLEAR ALL STEPS OF PATTERN %02d?"), curPi() + 1); break;
    case SI_CLRTRK: askConfirm(ACT_CLEARTRK, track, TR("ALLE STEPS VON SPUR %d LÖSCHEN?", "CLEAR ALL STEPS OF TRACK %d?"), track + 1); break;
    case SI_TMPL: applyRhythm(tmplSel); break;
    case SI_UNDO: undoStep(); break;
    case SI_REDO: redoStep(); break;
    case SI_DEMO: askConfirm(ACT_DEMO, 0, TR("DEMO-SONG IN EINEM FREIEN PLATZ ANLEGEN?", "CREATE DEMO SONG IN A FREE SLOT?")); break;
    case SI_LEARN: runLearn(); break;
    case SI_BENCH: {   // Messung auf dem Geraet, Bericht in logs/bench.txt (Browser: Logs)
        char bp[700];
        if (isGrooveboxOS()) { mkdir("/data/logs", 0755); snprintf(bp, sizeof bp, "/data/logs/bench.txt"); }
        else snprintf(bp, sizeof bp, "%sbench.txt", basePath);
        clearScreen(C_BG); textC(320, 220, TR("LEISTUNGSTEST LÄUFT ...", "BENCHMARK RUNNING ..."), C_ACC, 2); present();
        runBench(bp, true);
        toast(TR("LEISTUNGSTEST FERTIG: LOGS/BENCH.TXT", "BENCHMARK DONE: LOGS/BENCH.TXT"));
    } break;
    case SI_MUTATE: mutatePattern(); break;
    case SI_DRIVE: startDrive(); break;
    case SI_PCREC:
        if (pcRecState() == 1 || pcRecState() == 2) { pcRecStop(); toast(pcRecState() ? TR("AUFNAHME BEENDET", "RECORDING FINISHED") : TR("ABGEBROCHEN", "CANCELLED")); }
        else if (usbMode != 2) toast(TR("ERST USB = PC (MIDI + AUDIO)", "SET USB = PC (MIDI + AUDIO) FIRST"));
        else if (pcRecStart()) toast(TR("AM PC ABSPIELEN - AUFNAHME STARTET MIT DEM TON", "PLAY ON THE PC - RECORDING STARTS WITH THE SOUND"));
        break;
    case SI_USBTONE: if (usbAudioActive()) { usbAudioTestTone(5000); toast(TR("TESTTON 5 S AN DEN PC", "TEST TONE 5 S TO PC")); } else toast(TR("ERST USB = PC (MIDI + AUDIO)", "SET USB = PC (MIDI + AUDIO) FIRST")); break;
    case SI_QUIT: askConfirm(ACT_QUIT, 0, TR("GROOVEBOX BEENDEN? (PROJEKT WIRD GESPEICHERT)", "QUIT GROOVEBOX? (PROJECT WILL BE SAVED)")); break;
    default: setChange(i, 1); break;   // Umschalter: A schaltet weiter
    }
}

static void settingsInput(Input& I, Uint32 now, float dt) {
    int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0) - flick(I.ly, flLY, flLYt, now);
    if (setIsHeader(setSel)) setSel++;
    auto nextCat = [&](int dc) {   // Bereich wechseln (im Kreis), erster Eintrag
        int h = setCatOf(setSel);
        if (dc > 0) { h = setCatEnd(h); if (h >= SI_COUNT) h = 0; }
        else { h = h > 0 ? setCatOf(h - 1) : setCatOf(SI_COUNT - 1); }
        setSel = h + 1; setScroll = 0;
    };
    int dc = (I.pressed(BT_R1) ? 1 : 0) - (I.pressed(BT_L1) ? 1 : 0);
    if (dc) nextCat(dc);   // L1/R1: Bereich wechseln (auf beiden Ebenen)
    if (!setIn) {   // Ebene 1: Bereiche links. hoch/runter = Bereich, A/rechts = hinein, B = SET verlassen
        if (dy) nextCat(dy);
        if (I.pressed(BT_A) || I.pressed(BT_RIGHT)) setIn = true;
        else if (I.pressed(BT_B)) page = prevPage == PG_SYS ? PG_TRG : prevPage;
        if (I.pressed(BT_L2)) gotoPage(-1);
        if (I.pressed(BT_R2)) gotoPage(1);
        if (startTap(I, now)) startStop();
        return;
    }
    if (I.pressed(BT_B)) { setIn = false; return; }   // Ebene 2 -> zurueck zu den Bereichen
    if (dy) {   // innerhalb des Bereichs (im Kreis)
        int h = setCatOf(setSel), e = setCatEnd(h), n = e - h - 1;
        setSel = h + 1 + ((setSel - h - 1 + dy) % n + n) % n;
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
    if (I.pressed(BT_L2)) gotoPage(-1);
    if (I.pressed(BT_R2)) gotoPage(1);
    if (startTap(I, now)) startStop();
}

// ---------------------------------------------------------------- Song-Export (WAV): Zeilen von..bis offline in einer zweiten Engine rendern
static Engine renderEng;                      // eigene Instanz, laeuft im Export-Thread (die Wiedergabe-Engine bleibt unberuehrt)
static volatile int expState = 0;             // 0 aus, 1 laeuft, 2 fertig, 3 Fehler, 4 abgebrochen
static volatile float expProg = 0;
static volatile bool expCancel = false;
static int expFrom = 0, expTo = -1;           // Zeilen (0-basiert), expTo -1 = bis zum Ende
static long expFrames = 0;
static char expPath[800], expName[96];
// Laenge der Zeilen from..to in Sequencer-Ticks (12 pro 16tel), wie doTick: laengste belegte Spur x Wiederholungen
static long songTicks(const Project& J, int from, int to) {
    long tot = 0;
    for (int i = from; i <= to && i < J.songLen; i++) {
        const Project::SongRow& r = J.row[i];
        long m = 0;
        for (int t = 0; t < NTRACKS; t++) {
            if (r.pat[t] == SONG_EMPTY) continue;
            const Pattern& P = J.pat[r.pat[t] % NPATTERNS];
            long l = (long)(P.len[t] < 1 ? 1 : P.len[t]) * SPEED_TPS[P.speed[t] % 7];
            if (l > m) m = l;
        }
        if (m <= 0) m = 16 * 12;
        tot += m * (r.rep < 1 ? 1 : r.rep);
    }
    return tot;
}
static void expRange(int& a, int& b) {
    int n = eng.proj.songLen;
    a = clampi(expFrom, 0, n > 0 ? n - 1 : 0);
    b = expTo < 0 ? n - 1 : clampi(expTo, 0, n - 1);
    if (b < a) { int t = a; a = b; b = t; }
}
static int exportThread(void*) {
    Engine& R = renderEng;
    char part[820]; snprintf(part, sizeof part, "%s.part", expPath);
    FILE* f = fopen(part, "wb");
    if (!f) { expState = 3; return 0; }
    uint8_t hdr[44] = {0};
    fwrite(hdr, 1, 44, f);
    static float buf[2 * 1024];
    static uint8_t ob[2 * 1024 * 3];
    Dither dt;
    long done = 0, tailMax = (long)R.sr * 8, quiet = 0, tail = 0;
    long total = expFrames;
    bool ok = true, inTail = false;
    for (;;) {
        if (expCancel) { ok = false; break; }
        int n = 1024;
        if (!inTail && total - done < n) n = (int)(total - done);
        if (!inTail && n <= 0) { R.stop(); inTail = true; continue; }   // Song zu Ende: Toene ausklingen lassen (Hall/Delay)
        R.render(buf, n);
        float pk = 0;
        for (int i = 0; i < n * 2; i++) { float a = fabsf(buf[i]); if (a > pk) pk = a; }
        pcmTo24(ob, buf, n * 2, dt);
        if (fwrite(ob, 6, (size_t)n, f) != (size_t)n) { ok = false; break; }
        if (!inTail) done += n;
        else {
            tail += n;
            quiet = pk < 0.0005f ? quiet + n : 0;
            if (quiet > R.sr / 2 || tail >= tailMax) break;
        }
        expProg = (float)(done + tail) / (float)(total + R.sr * 2);
    }
    long frames = done + tail;
    wavHeader(hdr, R.sr, 2, 24, (uint32_t)frames);
    if (ok) { fseek(f, 0, SEEK_SET); ok = fwrite(hdr, 1, 44, f) == 44; }
    if (fclose(f) != 0) ok = false;
    if (ok) { sync(); ok = rename(part, expPath) == 0; }
    if (!ok) unlink(part);
    expProg = 1.f;
    expState = ok ? 2 : (expCancel ? 4 : 3);
    return 0;
}
static void startExport() {
    Project& J = eng.proj;
    if (!J.songLen) { toast(TR("SONG IST LEER (L1 = NEUE ZEILE)", "SONG IS EMPTY (L1 = NEW ROW)")); return; }
    if (expState == 1) return;
    int a, b; expRange(a, b);
    char dir[700]; snprintf(dir, sizeof dir, "%ssamples", basePath); mkdir(dir, 0755);
    snprintf(dir, sizeof dir, "%ssamples/rec", basePath); mkdir(dir, 0755);
    char base[64]; cleanName(cfg.project[0] ? cfg.project : "song", base, sizeof base);
    struct stat st;
    for (int i = 1; i < 1000; i++) {
        snprintf(expName, sizeof expName, "%s-song-%02d.wav", base, i);
        snprintf(expPath, sizeof expPath, "%s/%s", dir, expName);
        if (stat(expPath, &st)) break;
    }
    lockA();
    if (eng.playing) eng.stop();
    renderEng.init(eng.sr);
    renderEng.audioThreads = eng.audioThreads;
    memcpy(&renderEng.proj, &J, sizeof(Project));
    unlockA();
    renderEng.fill = false; renderEng.dj = 0; renderEng.recording = false; renderEng.notesOut = false; renderEng.clockMode = 0;
    for (int t = 0; t < NTRACKS; t++) renderEng.solo[t] = false;
    renderEng.songOn = true; renderEng.songStart(a); renderEng.start();
    long ticks = songTicks(J, a, b);
    expFrames = (long)((double)ticks * renderEng.sr * 60.0 / (J.bpm * 48.0) + 0.5);
    expProg = 0; expCancel = false; expState = 1;
    SDL_Thread* th = SDL_CreateThread(exportThread, "export", nullptr);
    if (th) SDL_DetachThread(th); else { expState = 3; }
}
static void drawExport();
static void drawExportModal() { if (expState == 1) drawExport(); }
static void drawExport() {   // Fortschritt (modal)
    rrect(100, 170, 440, 130, 14, C_ACC);
    rrect(102, 172, 436, 126, 13, C_BG, 250);
    textC(320, 186, TR("SONG WIRD EXPORTIERT ...", "EXPORTING SONG ..."), C_ACC);
    textC(320, 210, expName, C_TEXT);
    fill(130, 238, 380, 10, C_OFF2);
    fill(130, 238, (int)(380 * fminf(fmaxf(expProg, 0.f), 1.f)), 10, C_ACC);
    textC(320, 266, TR("B = ABBRECHEN", "B = CANCEL"), C_DIM);
}

// ---------------------------------------------------------------- Song-Seite (SNG): Clip-Matrix
// Zeilen = Szenen (Song-Eintraege), Spalten: ALLE, Spur 1-8 (Pattern je Spur + Mute), WDH (Wiederholungen)
static int songSel = 0, songCol = 0;
static float songScroll = 0;
static bool songExp = false;   // Cursor in der Export-Leiste unter der Matrix
static int expField = 2;       // 0 VON, 1 BIS, 2 EXPORT
enum { SC_ALL = 0, SC_T1 = 1, SC_REP = SC_T1 + NTRACKS, SC_N };
static int rowAllPat(const Project::SongRow& r) {   // gemeinsames Pattern aller Spuren, sonst -1
    int p = -1;
    for (int t = 0; t < NTRACKS; t++) { if (r.pat[t] == SONG_EMPTY) return -1; if (p < 0) p = r.pat[t]; else if (p != r.pat[t]) return -1; }
    return p;
}
static void drawSong() {
    Project& J = eng.proj;
    rrect(4, 46, 632, 398, 10, C_PANEL);
    char b[64];
    text(18, 54, "SONG", C_ACC, 2);
    snprintf(b, sizeof b, TR("%d ZEILEN", "%d ROWS"), J.songLen);
    text(100, 62, b, C_DIM);
    bool on = eng.songOn;
    rrect(440, 54, 186, 28, 8, on ? C_ACC : C_OFF2);
    textC(533, 60, on ? TR("SONG-MODUS AN", "SONG MODE ON") : TR("SONG-MODUS AUS", "SONG MODE OFF"), on ? C_BG : C_TEXT);
    const int rowH = 30, top = 116, visible = 9;
    int ea = 0, eb = -1; if (J.songLen) expRange(ea, eb);
    const int xAll = 50, wAll = 58, xT = 112, wT = 54, xRep = xT + NTRACKS * wT + 4, wRep = 628 - xRep - 6;
    // Spaltenkoepfe
    bool hs = songCol == SC_ALL;
    textC(xAll + wAll / 2, 94, TR("ALLE", "ALL"), hs ? C_ACC : C_DIM);
    for (int t = 0; t < NTRACKS; t++) {
        int cx = xT + t * wT + wT / 2;
        bool hc = songCol == SC_T1 + t;
        drawIcon(J.pat[curPi()].machine[t] % M_COUNT, cx - 15, 92, 13, hc ? C_ACC : dimC(TCOL[t], 0.8f));
        snprintf(b, sizeof b, "%d", t + 1); text(cx + 2, 94, b, hc ? C_ACC : C_DIM);
    }
    textC(xRep + wRep / 2, 94, TR("WDH", "REP"), songCol == SC_REP ? C_ACC : C_DIM);
    if (songSel >= J.songLen) songSel = J.songLen > 0 ? J.songLen - 1 : 0;
    float target = songSel - visible / 2.f; if (target < 0) target = 0;
    if (target > J.songLen - visible) target = J.songLen > visible ? J.songLen - visible : 0;
    songScroll += (target - songScroll) * (1.f - expf(-animDt * 14.f));
    if (J.songLen == 0) {
        textC(320, 136, TR("NOCH LEER - L1 LEGT EINE ZEILE MIT DEM", "STILL EMPTY - L1 ADDS A ROW WITH THE"), C_DIM);
        textC(320, 156, TR("AKTUELLEN PATTERN UND SEINEN MUTES AN", "CURRENT PATTERN AND ITS MUTES"), C_DIM);
    }
    for (int i = 0; i < J.songLen; i++) {
        float yf = top + (i - songScroll) * rowH;
        if (yf < top - 4 || yf > top + (visible - 1) * rowH + 4) continue;
        int y = (int)yf;
        const Project::SongRow& r = J.row[i];
        bool sel = i == songSel, play = eng.songOn && eng.playing && eng.songPos == i;
        hitAdd(xAll, y, wAll, rowH - 4, HK_SONG, i, SC_ALL);
        for (int t2 = 0; t2 < NTRACKS; t2++) hitAdd(xT + t2 * wT, y, wT, rowH - 4, HK_SONG, i, SC_T1 + t2);
        hitAdd(xRep, y, wRep, rowH - 4, HK_SONG, i, SC_REP);
        if (sel && !songExp) { rrect(10, y - 1, 620, rowH - 2, 6, C_SEL); rrect(10, y + 4, 3, rowH - 12, 1, C_ACC); }
        if (i >= ea && i <= eb) fill(6, y - 1, 2, rowH, dimC(C_CYAN, 0.6f));   // Export-Bereich
        snprintf(b, sizeof b, "%02d", i + 1); text(20, y + 6, b, play ? C_PLAY : C_DIM);
        // ALLE
        int ap = rowAllPat(r);
        if (ap >= 0) snprintf(b, sizeof b, "P%02d", ap + 1); else snprintf(b, sizeof b, "MIX");
        Col ac = ap >= 0 ? TCOL[ap % NTRACKS] : C_DIM;
        rrect(xAll, y + 2, wAll, rowH - 8, 5, play ? ac : dimC(ac, 0.3f));
        textC(xAll + wAll / 2, y + 6, b, play ? C_BG : C_TEXT);
        if (sel && !songExp && songCol == SC_ALL) { rrect(xAll - 2, y, wAll + 4, 2, 1, C_ACC); rrect(xAll - 2, y + rowH - 6, wAll + 4, 2, 1, C_ACC); }
        // Spuren
        for (int t = 0; t < NTRACKS; t++) {
            int x = xT + t * wT;
            bool empty = r.pat[t] == SONG_EMPTY, mu = (r.mute >> t) & 1;
            Col tc = TCOL[t];
            if (empty) { rrect(x + 2, y + 2, wT - 4, rowH - 8, 5, C_BG); textC(x + wT / 2, y + 6, "--", C_OFF2); }
            else {
                Col bg = mu ? C_BG : (play ? tc : dimC(tc, 0.35f));
                rrect(x + 2, y + 2, wT - 4, rowH - 8, 5, bg);
                if (mu) { rrect(x + 2, y + 2, wT - 4, 2, 1, dimC(tc, 0.5f)); }
                snprintf(b, sizeof b, "%02d", r.pat[t] % NPATTERNS + 1);
                textC(x + wT / 2 - (mu ? 6 : 0), y + 6, b, mu ? C_DIM : (play ? C_BG : C_TEXT));
                if (mu) text(x + wT - 15, y + 6, "M", C_TRIG);
            }
            if (sel && !songExp && songCol == SC_T1 + t) { rrect(x, y, wT, 2, 1, C_ACC); rrect(x, y + rowH - 6, wT, 2, 1, C_ACC); }
        }
        // Wiederholungen: Zahl + laufender Durchgang
        int rep = r.rep < 1 ? 1 : r.rep;
        snprintf(b, sizeof b, play ? "%d/%d" : "x%d", play ? eng.songRep + 1 : rep, rep);
        text(xRep + 6, y + 6, b, sel && songCol == SC_REP ? C_ACC : (play ? C_PLAY : C_TEXT));
        if (sel && !songExp && songCol == SC_REP) { rrect(xRep, y, wRep, 2, 1, C_ACC); rrect(xRep, y + rowH - 6, wRep, 2, 1, C_ACC); }
    }
    {   // freie Flaeche unter der Matrix: Zeitleiste des Songs + Oszilloskop
        int yfree = top + (J.songLen - (int)songScroll) * rowH + 4;
        if (J.songLen < visible && 386 - yfree >= 50) songTimeline(10, yfree, 620, 386 - yfree);
    }
    // Export-Leiste: Zeilen von..bis als WAV (Standard: ganzer Song)
    {
        int y = 392, x = 10;
        rrect(x, y, 620, 44, 8, songExp ? C_SEL : C_BG);
        if (songExp) rrect(x, y + 6, 3, 32, 1, C_ACC);
        text(x + 12, y + 6, TR("EXPORT WAV", "EXPORT WAV"), songExp ? C_ACC : C_DIM);
        if (J.songLen) {
            long tk = songTicks(J, ea, eb);
            int sec = (int)(tk * 60.0 / (J.bpm * 48.0) + 0.5);
            snprintf(b, sizeof b, TR("Länge %d:%02d", "Length %d:%02d"), sec / 60, sec % 60);
        } else snprintf(b, sizeof b, TR("Song leer", "Song empty"));
        text(x + 12, y + 24, b, C_DIM);
        const char* lab[3] = {TR("VON", "FROM"), TR("BIS", "TO"), ""};
        char val[3][24];
        snprintf(val[0], sizeof val[0], "%02d", ea + 1);
        if (expTo < 0) snprintf(val[1], sizeof val[1], TR("ENDE (%02d)", "END (%02d)"), eb + 1); else snprintf(val[1], sizeof val[1], "%02d", eb + 1);
        snprintf(val[2], sizeof val[2], TR("EXPORT STARTEN", "START EXPORT"));
        int fx[3] = {170, 290, 450}, fw[3] = {100, 140, 160};
        for (int k = 0; k < 3; k++) {
            bool fs = songExp && expField == k;
            if (k < 2) {
                text(fx[k], y + 14, lab[k], C_DIM);
                int vx = fx[k] + textW(lab[k]) + 8;
                rrect(vx - 4, y + 9, fw[k] - textW(lab[k]) - 4, 26, 6, fs ? C_ACC : C_PANEL);
                text(vx + 2, y + 14, val[k], fs ? C_BG : C_TEXT);
            } else {
                rrect(fx[k], y + 9, fw[k], 26, 6, fs ? C_ACC : C_PANEL);
                textC(fx[k] + fw[k] / 2, y + 14, val[k], fs ? C_BG : C_TEXT);
            }
        }
    }
    if (songExp) hint(6, 452, TR("[</>]FELD [X/Y]ZEILE [A]EXPORT STARTEN [^]ZUR MATRIX [START]PLAY", "[</>]FIELD [X/Y]ROW [A]START EXPORT [^]BACK TO MATRIX [START]PLAY"), C_ACC, C_DIM);
    else hint(6, 452, TR("[^v<>]ZELLE [X/Y]PATTERN/WDH [A]MUTE [L1]NEU [B]LÖSCHEN [R1]SONG [START]AB HIER",
                    "[^v<>]CELL [X/Y]PATTERN/REP [A]MUTE [L1]NEW [B]DELETE [R1]SONG [START]FROM HERE"), C_ACC, C_DIM);
}

// Zelle aendern: Pattern einer Spur (mit "leer" unterhalb von 01), alle Spuren gemeinsam oder Wiederholungen
static void songEditCell(Project& J, int i, int col, int d) {
    Project::SongRow& r = J.row[i];
    if (col == SC_REP) { r.rep = (uint8_t)clampi((r.rep < 1 ? 1 : r.rep) + d, 1, 16); return; }
    if (col == SC_ALL) {
        int base = rowAllPat(r);
        if (base < 0) {   // MIX: erster Druck vereinheitlicht auf das Pattern der ersten belegten Spur
            base = 0; for (int t = 0; t < NTRACKS; t++) if (r.pat[t] != SONG_EMPTY) { base = r.pat[t]; break; }
            d = 0;
        }
        int np = clampi(base + d, 0, NPATTERNS - 1);
        uint8_t rp = r.rep; Engine::rowFromPattern(J, r, np); r.rep = rp;   // Mutes vom Pattern uebernehmen
    } else {
        int t = col - SC_T1;
        int cur = r.pat[t] == SONG_EMPTY ? -1 : r.pat[t];
        int nv = clampi(cur + d, -1, NPATTERNS - 1);
        r.pat[t] = nv < 0 ? (uint8_t)SONG_EMPTY : (uint8_t)nv;
        if (nv >= 0) r.mute = (uint8_t)((r.mute & ~(1u << t)) | (J.patMute[nv] & (1u << t)));   // Mute vom Pattern uebernehmen
    }
    if (eng.songActive() && eng.songPos == i) { if (eng.playing) eng.rowPending = true; else eng.applyRow(i); }   // laufende Zeile: ab naechstem Durchgang
}

static void songInput(Input& I, Uint32 now) {
    Project& J = eng.proj;
    if (I.pressed(BT_L2)) { gotoPage(-1); return; }   // Seitenwechsel wie ueberall
    if (I.pressed(BT_R2)) { gotoPage(1); return; }
    if (rsAxis == 2) { int f = flick(I.rx, flRX, flRXt, now); if (f) { gotoPage(f); return; } } else flRX = 0;
    int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0) - flick(I.ly, flLY, flLYt, now);
    if (!J.songLen) songExp = false;
    if (songExp) {   // Export-Leiste
        if (dy < 0) { songExp = false; return; }
        int fx = (I.repeat(BT_RIGHT, now) ? 1 : 0) - (I.repeat(BT_LEFT, now) ? 1 : 0) + flick(I.lx, flLX, flLXt, now);
        if (fx) expField = clampi(expField + fx, 0, 2);
        int dv = (I.repeat(BT_X, now) ? 1 : 0) - (I.repeat(BT_Y, now) ? 1 : 0);
        if (dv) {
            int a, b; expRange(a, b);
            if (expField == 0) expFrom = clampi(a + dv, 0, J.songLen - 1);
            else if (expField == 1) { int nb = clampi(b + dv, 0, J.songLen - 1); expTo = nb >= J.songLen - 1 ? -1 : nb; }   // letzte Zeile = "bis Ende"
            if (expField == 2) expField = dv > 0 ? 2 : 1;
        }
        if (I.pressed(BT_A)) startExport();
        if (startTap(I, now)) startStop();
        return;
    }
    if (dy > 0 && J.songLen && songSel >= J.songLen - 1) { songExp = true; expField = 2; return; }
    if (dy && J.songLen) songSel = clampi(songSel + dy, 0, J.songLen - 1);
    int dx = (I.repeat(BT_RIGHT, now) ? 1 : 0) - (I.repeat(BT_LEFT, now) ? 1 : 0) + flick(I.lx, flLX, flLXt, now);
    if (dx) songCol = clampi(songCol + dx, 0, SC_N - 1);
    int dv = (I.repeat(BT_X, now) ? 1 : 0) - (I.repeat(BT_Y, now) ? 1 : 0);
    if (dv && J.songLen) { lockA(); songEditCell(J, songSel, songCol, dv); unlockA(); }
    if (I.pressed(BT_A) && J.songLen) {
        Project::SongRow& r = J.row[songSel];
        lockA();
        if (songCol >= SC_T1 && songCol < SC_REP) {
            int t = songCol - SC_T1;
            if (r.pat[t] != SONG_EMPTY) r.mute ^= (uint8_t)(1u << t);
        } else if (songCol == SC_ALL) {   // Mutes neu von den Patterns uebernehmen
            r.mute = 0;
            for (int t = 0; t < NTRACKS; t++) if (r.pat[t] != SONG_EMPTY) r.mute |= (uint8_t)(J.patMute[r.pat[t] % NPATTERNS] & (1u << t));
        }
        unlockA();
        if (songCol == SC_ALL) toast(TR("MUTES VON DEN PATTERNS ÜBERNOMMEN", "MUTES TAKEN FROM THE PATTERNS"));
    }
    if (I.pressed(BT_L1)) {
        if (J.songLen >= SONG_MAX) toast(TR("SONG VOLL (%d)", "SONG FULL (%d)"), SONG_MAX);
        else {
            lockA();
            int at = J.songLen ? songSel + 1 : 0;
            for (int i = J.songLen; i > at; i--) J.row[i] = J.row[i - 1];
            if (J.songLen) J.row[at] = J.row[songSel];   // Kopie der gewaehlten Zeile
            else Engine::rowFromPattern(J, J.row[at], curPi());
            J.row[at].rep = 1;
            J.songLen++;
            if (eng.songActive() && eng.songPos >= at && J.songLen > 1) eng.songPos = eng.songPos + 1;
            unlockA();
            songSel = at;
        }
    }
    if (I.pressed(BT_B) && J.songLen) {
        lockA();
        for (int i = songSel; i < J.songLen - 1; i++) J.row[i] = J.row[i + 1];
        J.songLen--;
        if (eng.songPos >= J.songLen) eng.songPos = 0;
        if (!J.songLen) eng.songOn = false;
        else if (eng.songOn) eng.rowPending = true;
        unlockA();
        if (songSel >= J.songLen && songSel > 0) songSel--;
    }
    if (I.pressed(BT_R1)) {
        if (!J.songLen) toast(TR("ERST ZEILEN ANLEGEN (L1)", "ADD ROWS FIRST (L1)"));
        else {
            lockA();
            if (eng.songOn) { eng.songOn = false; if (eng.playing) eng.nextPat = eng.curPat; }
            else eng.songStart(songSel);
            unlockA();
            toast(eng.songOn ? TR("SONG-MODUS AN", "SONG MODE ON") : TR("SONG-MODUS AUS", "SONG MODE OFF"));
        }
    }
    if (startTap(I, now)) {
        lockA();
        if (eng.playing) eng.stop();
        else if (J.songLen) { eng.songStart(songSel < J.songLen ? songSel : 0); eng.start(); }
        else eng.start();
        unlockA();
    }
}

static void drawSettings() {
    rrect(4, 46, 632, 398, 10, C_PANEL);
    char l[64], v[64];
    // links: Bereiche
    int cat = setCatOf(setSel), cy = 56;
    for (int i = 0; i < SI_COUNT; i++) {
        if (!setIsHeader(i)) continue;
        setLabel(i, l, sizeof l, v, sizeof v);
        bool sel = i == cat;
        hitAdd(10, cy, 140, 30, HK_SET, i + 1, 1);   // Bereich: erster Eintrag
        if (sel && !setIn) {   // Bereiche-Ebene: gewaehlter Bereich leuchtet und pulsiert
            float pl = 0.5f + 0.5f * sinf(animTime * 4.f);
            rrect(8, cy - 2, 144, 34, 8, mix(C_ACC, C_PLAY, 0.3f * pl));
        }
        rrect(10, cy, 140, 30, 7, sel ? (setIn ? dimC(C_ACC, 0.45f) : C_ACC) : C_BG);
        text(20, cy + 7, l, sel ? (setIn ? C_TEXT : C_BG) : C_TEXT);
        if (sel) text(136, cy + 7, setIn ? ">" : "", C_TEXT);
        cy += 36;
    }
    text(12, cy + 6, setIn ? TR("B = BEREICHE", "B = SECTIONS") : TR("A = ÖFFNEN", "A = OPEN"), C_DIM);
    if (436 - (cy + 30) > 40) { rrect(10, cy + 30, 140, 436 - (cy + 30), 8, C_BG); drawSpectrum(14, cy + 36, 132, 436 - (cy + 30) - 12, C_ACC, 12); }
    // rechts: Eintraege des Bereichs
    const int X0 = 158, W = 470, rowH = 27, top = 56, visible = 13;
    int first = cat + 1, end = setCatEnd(cat), n = end - first;
    float target = (setSel - first) - visible / 2.f;
    if (target > n - visible) target = (float)(n - visible);
    if (target < 0) target = 0;
    static int lastCat = -1;
    if (cat != lastCat || setScroll > n || setScroll < 0) { lastCat = cat; setScroll = target; }
    setScroll += (target - setScroll) * (1.f - expf(-animDt * 14.f));
    for (int k = 0; k < n; k++) {
        int i = first + k;
        float yf = top + (k - setScroll) * rowH;
        if (yf < top - 4 || yf > top + (visible - 1) * rowH + 4) continue;
        int y = (int)yf;
        setLabel(i, l, sizeof l, v, sizeof v);
        bool sel = i == setSel && setIn;
        hitAdd(X0, y, W, rowH - 2, HK_SET, i, 0);
        if (sel) { rrect(X0, y, W, rowH - 3, 6, C_SEL); rrect(X0, y + 4, 3, rowH - 11, 1, C_ACC); }
        text(X0 + 12, y + 5, l, sel ? C_TEXT : C_DIM);
        if (setIsAction(i)) {
            int w = textW(v[0] ? v : ">>") + 16;
            if (v[0]) text(X0 + W - 8 - textW(v), y + 5, v, C_DIM);
            else { rrect(X0 + W - w, y + 3, w - 8, rowH - 9, 5, sel ? C_ACC : C_OFF2); text(X0 + W - w + 4, y + 5, ">>", sel ? C_BG : C_TEXT); }
        } else {
            char vb[64]; snprintf(vb, sizeof vb, "%s", v);
            while (textW(vb) > W - 40 - textW(l) && strlen(vb) > 3) vb[strlen(vb) - 1] = 0;
            text(X0 + W - 18 - textW(vb), y + 5, vb, sel ? C_ACC : C_TEXT);
            if (sel) { text(X0 + W - 18 - textW(vb) - 14, y + 5, "<", C_DIM); text(X0 + W - 12, y + 5, ">", C_DIM); }
        }
    }
    {   // freie Flaeche unter der Liste: Bild zum Bereich
        int used = top + (n - (int)setScroll) * rowH + 6;
        int ci = 0; for (int i = 0; i < cat; i++) if (setIsHeader(i)) ci++;
        if (n <= visible && 404 - used >= 44) settingsArt(ci, X0, used, W, 404 - used);
    }
    // Erklaerung des gewaehlten Eintrags
    fill(X0, 410, W, 1, C_OFF2);
    const char* hs = setHelp(setSel);
    if (*hs) {
        char hb[128]; snprintf(hb, sizeof hb, "%s", hs);
        if (textW(hb) <= W) text(X0, 418, hb, C_TEXT);
        else {   // zweizeilig: letztes Leerzeichen, bis zu dem die erste Zeile passt
            int cut = 0;
            for (int k = 1; hb[k]; k++) if (hb[k] == ' ') { char c = hb[k]; hb[k] = 0; bool fits = textW(hb) <= W; hb[k] = c; if (fits) cut = k; else break; }
            if (cut > 0) { hb[cut] = 0; text(X0, 413, hb, C_TEXT); text(X0, 428, hb + cut + 1, C_TEXT); }
            else text(X0, 418, hb, C_TEXT);
        }
    }
    if (setIn) hint(6, 452, TR("[^v]WÄHLEN [</>]ÄNDERN [A]AUSFÜHREN [B]ZU DEN BEREICHEN [L1/R1]BEREICH [L2/R2]SEITE", "[^v]SELECT [</>]CHANGE [A]RUN [B]TO SECTIONS [L1/R1]SECTION [L2/R2]PAGE"), C_ACC, C_DIM);
    else hint(6, 452, TR("[^v]BEREICH [A]ÖFFNEN [B]ZURÜCK [L2/R2]SEITE [SEL]KARTE", "[^v]SECTION [A]OPEN [B]BACK [L2/R2]PAGE [SEL]MAP"), C_ACC, C_DIM);
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
    // Seitenkarte: Steuerkreuz (oder linker Stick) im Raster, A oeffnet
    int dx = (I.repeat(BT_RIGHT, now) ? 1 : 0) - (I.repeat(BT_LEFT, now) ? 1 : 0) + flick(I.lx, flLX, flLXt, now);
    int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0) - flick(I.ly, flLY, flLYt, now);
    if (I.pressed(BT_R1) || I.pressed(BT_R2)) dx++;
    if (I.pressed(BT_L1) || I.pressed(BT_L2)) dx--;
    int r, c; mapPos(menuSel, r, c);
    if (dx) {
        int n = mapCols(r);
        for (int k = 0; k < n; k++) { c = (c + dx + n) % n; if (pageAvail(MAP_ROWS[r][c])) break; }
        menuSel = MAP_ROWS[r][c];
    }
    if (dy) {
        int x, y, w, h; mapBox(r, c, x, y, w, h);
        int cx = x + w / 2;
        for (int k = 0; k < 5; k++) {
            r = (r + dy + 5) % 5;
            int best = -1, bd = 1 << 30;
            for (int c2 = 0; c2 < mapCols(r); c2++) {
                if (!pageAvail(MAP_ROWS[r][c2])) continue;
                mapBox(r, c2, x, y, w, h);
                int d = abs(x + w / 2 - cx);
                if (d < bd) { bd = d; best = c2; }
            }
            if (best >= 0) { menuSel = MAP_ROWS[r][best]; break; }
        }
    }
    if (I.pressed(BT_X) && PG_STORE[menuSel] >= 0) {   // Seite (8 Werte der Spur) kopieren
        const Pattern& P = curP();
        int st = PG_STORE[menuSel];
        for (int i = 0; i < 8; i++) pageClip[i] = P.p[track][st * 8 + i];
        pageClipStore = st; pageClipMachine = P.machine[track]; pageClipTrack = track;
        pageClipLen = P.len[track]; pageClipSpeed = P.speed[track];
        toast(TR("SEITE %s VON T%d KOPIERT", "PAGE %s OF T%d COPIED"), PAGE_NAMES[menuSel], track + 1);
    }
    if (I.pressed(BT_Y) && PG_STORE[menuSel] >= 0) {   // auf der aktuellen Spur einfuegen
        int st = PG_STORE[menuSel];
        bool srcPage = menuSel == PG_SRC || menuSel == PG_SRC2;
        if (pageClipStore < 0) toast(TR("SEITEN-ABLAGE LEER (X = KOPIEREN)", "PAGE CLIPBOARD EMPTY (X = COPY)"));
        else if (pageClipStore != st) toast(TR("ABLAGE IST EINE ANDERE SEITE", "CLIPBOARD HOLDS ANOTHER PAGE"));
        else if (srcPage && pageClipMachine != curP().machine[track]) toast(TR("ANDERE ENGINE (%s) - NICHT EINGEFÜGT", "OTHER ENGINE (%s) - NOT PASTED"), MACHINE_NAMES[pageClipMachine % M_COUNT]);
        else {
            lockA();
            Pattern& P = curP();
            for (int i = 0; i < 8; i++) P.p[track][st * 8 + i] = pageClip[i];
            if (menuSel == PG_SEQ) { P.len[track] = (uint8_t)pageClipLen; P.speed[track] = (uint8_t)pageClipSpeed; }
            unlockA();
            toast(TR("SEITE %s EINGEFÜGT (T%d -> T%d)", "PAGE %s PASTED (T%d -> T%d)"), PAGE_NAMES[menuSel], pageClipTrack + 1, track + 1);
        }
    }
    if (I.pressed(BT_A) || I.pressed(BT_START)) { page = menuSel; menuOpen = false; }
    if (I.pressed(BT_B) || I.pressed(BT_SELECT)) menuOpen = false;
}

static void liveInput(Input& I, Uint32 now, float dt) {
    for (int t = 0; t < NTRACKS; t++) if (I.pressed(LIVE_BTN[t])) eng.toggleMute(t);
    if (I.pressed(BT_L1)) eng.fill = true;
    if (I.released(BT_L1)) eng.fill = false;
    if (I.pressed(BT_R1)) recToggle();
    if (I.pressed(BT_L2)) gotoPage(-1);   // Seiten wie ueberall; Pattern: SELECT + links/rechts
    if (I.pressed(BT_R2)) gotoPage(1);
    if (startTap(I, now)) startStop();
    if (fabsf(I.ry) > 0.2f) { eng.dj = I.ry > 0 ? (I.ry - 0.2f) / 0.8f : (I.ry + 0.2f) / 0.8f; stickDj = true; }
    else if (stickDj) { eng.dj = 0.f; stickDj = false; }
    int f = flick(I.ly, flLY, flLYt, now);
    if (f) eng.proj.bpm = (uint16_t)clampi(eng.proj.bpm + f, 30, 300);
}

static bool touchBlocked() { return wteOpen || expState == 1 || fbOpen || kbOpen; }
#include "touch.inc"

static void handleInput(Uint32 now, float dt) {
    Input& I = in;
    touchProcess(now);
    if (confirmOpen) {
        if (I.pressed(BT_A)) { confirmOpen = false; runAction(confirmAct, confirmArg); }
        else if (I.pressed(BT_B) || I.pressed(BT_SELECT)) { confirmOpen = false; toast(TR("ABGEBROCHEN", "CANCELLED")); }
        return;
    }
    if (kbOpen) { kbInput(I, now); return; }
    if (fbOpen) { fbInput(I, now); return; }
    if (wteOpen) { wteInput(I, now, dt); return; }
    if (expState == 1) { if (I.pressed(BT_B)) expCancel = true; return; }   // Export laeuft: nur Abbrechen
    if (expState >= 2) {
        if (expState == 2) toast(TR("EXPORT FERTIG: SAMPLES/REC/%s", "EXPORT DONE: SAMPLES/REC/%s"), expName);
        else toast(expState == 4 ? TR("EXPORT ABGEBROCHEN", "EXPORT CANCELLED") : TR("EXPORT FEHLGESCHLAGEN (SPEICHER VOLL?)", "EXPORT FAILED (STORAGE FULL?)"));
        expState = 0;
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
            int to = ((eng.nextPat >= 0 ? eng.nextPat : curPi()) + d + NPATTERNS) % NPATTERNS;
            bool wasSong = eng.songActive();
            switchPattern(to);
            if (!wasSong) toast("PATTERN %02d", to + 1);
        }
        if (I.pressed(BT_UP)) {
            funcUsed = true;
            eng.recording = !eng.recording;
            toast(eng.recording ? TR("AUFNAHME AN", "RECORD ON") : TR("AUFNAHME AUS", "RECORD OFF"));
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
            toast(hs.on == 2 ? TR("STEP %d: TRIGLESS (NUR LOCKS)", "STEP %d: TRIGLESS (LOCKS ONLY)") : "STEP %d: TRIG", holdStep + 1);
        } else if (I.pressed(BT_A)) { funcUsed = true; eng.toggleMute(track); toast(eng.muted(track) ? TR("SPUR %d STUMM", "TRACK %d MUTED") : TR("SPUR %d AN", "TRACK %d ON"), track + 1); }
        if (I.pressed(BT_B)) { funcUsed = true; undoStep(); }
        if (I.pressed(BT_X) || I.pressed(BT_Y)) {
            funcUsed = true;
            int m = (curP().machine[track] + (I.pressed(BT_X) ? 1 : M_COUNT - 1)) % M_COUNT;
            lockA(); Engine::setMachine(curP(), track, m); unlockA();
            toast("ENGINE: %s", MACHINE_NAMES[m]);
        }
        if (aHeld && I.pressed(BT_L1)) {   // Lock-Modus: Trig kopieren (Zustand vor dem Druck, mit allen Locks)
            funcUsed = true;
            Step& hs = holdS();
            if (aPrevOn == 0) { hs.on = 0; memset(hs.lock, NO_LOCK, NPARAMS); }   // leerer Step: nicht anlegen
            else hs.on = (uint8_t)aPrevOn;
            trigClip = hs; haveTrigClip = true; aEdited = true;
            toast(aPrevOn == 0 ? TR("LEEREN STEP KOPIERT", "EMPTY STEP COPIED") : (aPrevOn == 2 ? TR("TRIGLESS STEP KOPIERT", "TRIGLESS STEP COPIED") : TR("TRIG KOPIERT (MIT LOCKS)", "TRIG COPIED (WITH LOCKS)")));
        } else if (aHeld && I.pressed(BT_R1)) {   // Lock-Modus: Trig in den gehaltenen Step einfuegen
            funcUsed = true;
            if (!haveTrigClip) toast(TR("TRIG-ABLAGE LEER (LOCK-MODUS: SELECT + L1)", "TRIG CLIPBOARD EMPTY (LOCK MODE: SELECT + L1)"));
            else { lockA(); holdS() = trigClip; unlockA(); aEdited = true; toast(TR("TRIG EINGEFÜGT IN STEP %d", "TRIG PASTED INTO STEP %d"), holdStep + 1); }
        } else if (!aHeld && (I.repeat(BT_L1, now) || I.repeat(BT_R1, now))) {   // Step-Cursor um einen Schlag (4 Steps)
            funcUsed = true;
            int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            cursor = ((cursor + (I.down[BT_R1] ? 4 : -4)) % cl + cl) % cl;
        }
        if (!aHeld && (I.repeat(BT_L2, now) || I.repeat(BT_R2, now))) {   // Step-Seite (16 Steps)
            funcUsed = true;
            int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            if (cl > 16) { cursor = ((cursor + (I.down[BT_R2] ? 16 : -16)) % cl + cl) % cl; toast(TR("STEPS %d-%d", "STEPS %d-%d"), cursor / 16 * 16 + 1, cursor / 16 * 16 + 16); }
            else toast(TR("SPUR HAT NUR %d STEPS (LÄNGE: SEITE SEQ)", "TRACK HAS ONLY %d STEPS (LENGTH: SEQ PAGE)"), cl);
        }
        if (I.pressed(BT_START)) {
            funcUsed = true;
            bool ok = saveProject();
            toast(ok ? TR("GESPEICHERT", "SAVED") : TR("SPEICHERN FEHLGESCHLAGEN", "SAVING FAILED"));
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
        bool mixP = page == PG_MIX, prjAct = page == PG_PRJ && prjIsAction(slot);
        bool knobPg = busOfPage(page) >= 0 || page == PG_CMP || page == PG_MST;   // globale Reglerseiten: Steuerkreuz waehlt den Regler
        if (!aHeld) {
            int cl = curP().len[track] < 1 ? 1 : curP().len[track];
            int dx = (I.repeat(BT_RIGHT, now, 250, 70) ? 1 : 0) - (I.repeat(BT_LEFT, now, 250, 70) ? 1 : 0);
            int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0);
            if (mixP) {   // Mixer: links/rechts = Kanal, hoch/runter = Zeile
                if (dx) track = (track + NTRACKS + dx) % NTRACKS;
                if (dy) mixRow = clampi(mixRow + dy, 0, MIX_ROWS - 1);
            } else if (knobPg) {
                if (dx) slot = (slot + 8 + dx) % 8;
                if (dy) slot = (slot + 4) % 8;
            } else if (page == PG_PRJ) {   // Projekt: links/rechts = Feld, hoch/runter = Spur (Sounds)
                if (dx) slot = (slot + 8 + dx) % 8;
                if (dy) track = (track + NTRACKS + dy) % NTRACKS;
            } else {
                if (dx) cursor = ((cursor + dx) % cl + cl) % cl;
                if (dy) track = (track + NTRACKS + dy) % NTRACKS;
            }
        }
        if (I.pressed(BT_A) && lockLatch) {   // Lock-Modus beenden
            Step& s = holdS();
            if (aPrevOn == 0 && !aEdited) { s.on = 0; memset(s.lock, NO_LOCK, NPARAMS); }
            lockLatch = false; aHeld = false;
            toast(TR("LOCK-MODUS AUS", "LOCK MODE OFF"));
        } else if (I.pressed(BT_A)) {
            if (mixP) { eng.toggleMute(track); toast(eng.muted(track) ? TR("SPUR %d STUMM", "TRACK %d MUTED") : TR("SPUR %d AN", "TRACK %d ON"), track + 1); }
            else if (prjAct) prjTrigger(slot);
            else {
                holdPat = curPi(); holdTrack = track; holdStep = cursor;
                Step& s = holdS();
                aPrevOn = s.on; aJustOn = s.on == 0;
                if (s.on == 0) { memset(s.lock, NO_LOCK, NPARAMS); s.on = 1; }   // trigless bleibt beim Halten trigless
                aHeld = true; aEdited = false; aDownAt = now;
            }
        }
        // A lange halten (ohne zu drehen) = Lock-Modus einrasten: Daumen frei fuer Stick und X/Y
        if (aHeld && !lockLatch && I.down[BT_A] && !aEdited && now - aDownAt > 450) {
            lockLatch = true;
            toast(TR("LOCK-MODUS STEP %d  (A = FERTIG)", "LOCK MODE STEP %d  (A = DONE)"), holdStep + 1);
        }
        if (I.pressed(BT_B)) {
            if (aHeld) {
                Step& hs = holdS();
                memset(hs.lock, NO_LOCK, NPARAMS); aEdited = true;
                if (hs.on == 2) { hs.on = 0; toast(TR("TRIGLESS LOCKS GELÖSCHT", "TRIGLESS LOCKS CLEARED")); } else toast(TR("LOCKS GELÖSCHT", "LOCKS CLEARED"));
            }
            else if (mixP) { eng.solo[track] = !eng.solo[track]; toast(eng.solo[track] ? TR("SPUR %d SOLO", "TRACK %d SOLO") : TR("SPUR %d SOLO AUS", "TRACK %d SOLO OFF"), track + 1); }
            else { lockA(); eng.preview(track, &curP().s[track][cursor]); unlockA(); }
        }
        // Linker Stick: auf Regler zeigen (Mixer: hoch/runter = Zeile, links/rechts = Kanal)
        if (mixP) {
            int f = flick(I.ly, flLY, flLYt, now); if (f) mixRow = clampi(mixRow - f, 0, MIX_ROWS - 1);
            int g = flick(I.lx, flLX, flLXt, now); if (g) track = (track + NTRACKS + g) % NTRACKS;
        } else {   // Linker Stick: pro Antippen genau eine Kachel weiter (halten = wiederholen)
            int fx = flick(I.lx, flLX, flLXt, now), fy = flick(I.ly, flLY, flLYt, now);
            if (fx) slot = (slot / 4) * 4 + clampi(slot % 4 + fx, 0, 3);
            if (fy) slot = clampi(slot - fy * 4, slot % 4, 4 + slot % 4);
        }
        // Rechter Stick: hoch/runter = Wert (Seite wechseln: L2/R2, SELECT-Karte oder SELECT + rechter Stick)
        flRX = 0;
        {   // L1/R1: Regler waehlen (auch im Lock-Modus), Mixer: Zeile
            int dl = (I.repeat(BT_R1, now, 300, 110) ? 1 : 0) - (I.repeat(BT_L1, now, 300, 110) ? 1 : 0);
            if (dl && mixP) mixRow = (mixRow + MIX_ROWS + dl) % MIX_ROWS;
            else if (dl) {
                int s2 = slot;
                for (int k = 0; k < 8; k++) {   // leere Felder ueberspringen
                    s2 = (s2 + 8 + dl) % 8;
                    ParamDef kd; int kv, kb; bool kl; char kn[24];
                    int o = slot; slot = s2; bool ok = knobInfo(s2, kd, kv, kl, kb, kn, sizeof kn); slot = o;
                    if (ok) break;
                }
                slot = s2;
            }
        }
        if (I.pressed(BT_L2)) gotoPage(-1);   // L2/R2: Seite (auch im Lock-Modus)
        if (I.pressed(BT_R2)) gotoPage(1);
        if (startTap(I, now)) startStop();
        if (prjAct) {
            if (I.pressed(BT_X)) prjTrigger(slot);
        } else {
            int big = curMax() >= 100;
            int mul = (big && (I.heldMs(BT_X, now) > 1200 || I.heldMs(BT_Y, now) > 1200)) ? 5 : 1;
            if (I.pressed(BT_X) && trackPage(page)) {   // Sample-Regler: X oeffnet den Explorer (mit Unterordnern)
                ParamDef sd; int sv, sb; bool sl; char slab[24];
                if (knobInfo(slot, sd, sv, sl, sb, slab, sizeof slab)) {
                    if (sd.kind == K_SMPL) { fbSmpIdx = sp() * 8 + slot; openBrowser(FB_SAMPLE); return; }
                    if (sd.kind == K_WDRAW && !aHeld) { openWtEditor(); return; }
                    if (sd.kind == K_LDEST) { fbParam = sp() * 8 + slot; openBrowser(FB_LDEST); return; }
                    if (sd.kind == K_MACH && !aHeld) { openBrowser(FB_MACH); return; }
                }
            }
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
                if (stepHasLock(s)) { s.on = 2; toast(TR("TRIG WEG - LOCKS BLEIBEN (TRIGLESS)", "TRIG REMOVED - LOCKS KEPT (TRIGLESS)")); }
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
    const char* prompts[BT_COUNT + 4] = {
        TR("STEUERKREUZ HOCH", "D-PAD UP"), TR("STEUERKREUZ RUNTER", "D-PAD DOWN"), TR("STEUERKREUZ LINKS", "D-PAD LEFT"), TR("STEUERKREUZ RECHTS", "D-PAD RIGHT"),
        "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "SELECT", "START",
        TR("LINKEN STICK NACH OBEN", "LEFT STICK UP"), TR("RECHTEN STICK NACH OBEN", "RIGHT STICK UP"),
        TR("LINKEN STICK NACH RECHTS", "LEFT STICK RIGHT"), TR("RECHTEN STICK NACH RECHTS", "RIGHT STICK RIGHT")};
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
            textC(SW / 2, 40, TR("TASTEN LERNEN", "LEARN BUTTONS"), C_ACC, 2);
            textC(SW / 2, 150, TR("DRÜCKE:", "PRESS:"), C_DIM);
            textC(SW / 2, 190, prompts[item], C_TEXT, 2);
            char b[64];
            snprintf(b, sizeof b, "%d / %d", item + 1, BT_COUNT + 4);
            textC(SW / 2, 260, b, C_DIM);
            if (stick) { snprintf(b, sizeof b, TR("Kein Stick? Ueberspringen in %ds", "No stick? Skipping in %ds"), left); textC(SW / 2, 300, b, C_DIM); }
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
    scopePush((const float*)stream, frames);      // fuer Oszilloskop/Spektrum der Oberflaeche
    remoteAudioPush((const float*)stream, frames, eng.sr);   // Ton in den Browser (nur bei Zuhoerern)
    double el = (double)(SDL_GetPerformanceCounter() - t0) / SDL_GetPerformanceFrequency();
    double avail = (double)frames / eng.sr;
    if (el * 1000.0 > audWorkMax) audWorkMax = el * 1000.0;
    if (el > avail * 0.9) audLate++;
    cpuLoad = cpuLoad * 0.9f + 0.1f * (float)(el / avail);
}

static bool writeWav(const char* path, const float* data, int frames, int sr) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    uint8_t hdr[44]; wavHeader(hdr, sr, 2, 24, (uint32_t)frames); fwrite(hdr, 1, 44, f);
    static uint8_t ob[1024 * 6]; Dither dt;
    for (int i = 0; i < frames; i += 1024) {
        int n = frames - i < 1024 ? frames - i : 1024;
        pcmTo24(ob, data + (size_t)i * 2, n * 2, dt); fwrite(ob, 6, (size_t)n, f);
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
        toast(TR("AUSGANG NICHT VERFÜGBAR", "OUTPUT NOT AVAILABLE"));
        audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    }
    if (!audioDev) { printf("Audio: %s\n", SDL_GetError()); return; }
    printf("Audio: %s, %d Hz, Puffer %d Frames (%.1f ms)\n", name ? name : "Standard", have.freq, have.samples,
           have.samples * 1000.0 / have.freq);
    eng.setRate(have.freq);
    usbAudioSetEngineRate(eng.sr);   // USB/PC-Eingang rechnen von/auf die tatsaechliche Engine-Rate um
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
    snprintf(cmd, sizeof cmd, "%sbash '%spc-modus.sh' %s nosleep", usbEnv(), basePath, args[usbMode]);
    toast(usbMode ? TR("PC-MODUS WIRD AKTIVIERT ...", "ACTIVATING PC MODE ...") : TR("PC-MODUS AUS ...", "PC MODE OFF ..."));
    drawAll(false); present();
    int rc = system(cmd);
    int now = detectUsbMode();
    printf("USB-Modus %d: rc=%d, erkannt=%d\n", usbMode, rc, now);
    if (usbMode && !now) {   // letzte Fehlerzeile aus dem Log anzeigen
        usbMode = 0;
        char lp[700], line[200], err[200];
        snprintf(err, sizeof err, "%s", TR("PC-MODUS NICHT MÖGLICH (PC-MODUS.LOG)", "PC MODE NOT POSSIBLE (PC-MODUS.LOG)"));
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
        char lp[700], line[200], okm[200];
        snprintf(okm, sizeof okm, "%s", TR("PC-MODUS AKTIV", "PC MODE ACTIVE"));
        snprintf(lp, sizeof lp, "%spc-modus.log", basePath);
        FILE* lf = fopen(lp, "r");
        if (lf) {
            while (fgets(line, sizeof line, lf))
                if (!strncmp(line, "OK:", 3)) { line[strcspn(line, "\r\n")] = 0; snprintf(okm, sizeof okm, "%.60s", line + 4); }
            fclose(lf);
        }
        toast("%s", okm);
        msgUntil = SDL_GetTicks() + 4000;
    } else toast(TR("USB: HOST-MODUS", "USB: HOST MODE"));
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
    int ns = libScan(sdir);
    printf("Sample-Bibliothek: %d WAV-Dateien in %s (%d in anderen Formaten werden nicht gelesen)\n", ns, sdir, libSkipped());
}

int runDhcpd(const char* ifname);

// ---------------------------------------------------------------- Sample-Pool des Projekts
// Spur-Parameter SMPL = Pool-Platz; das Projekt speichert zu jedem benutzten Platz den Bibliotheksnamen.
static bool poolSetSlot(int slot, const char* name) {   // Datei in den Platz laden (leerer Name = Platz leeren)
    Sample s; memset(&s, 0, sizeof s);
    if (name && name[0] && !sampleRead(name, s)) { printf("Sample nicht lesbar: %s (%s)\n", name, sampleLastError()); return false; }
    lockA(); poolSwap(slot, s); unlockA();
    sampleFree(s);
    return true;
}
static void poolUsed(const Project& J, bool* used);
static int poolAssign(const char* name) {   // Platz fuer ein Bibliotheks-Sample: vorhandenen nutzen oder neu laden
    int sl = poolFind(name);
    if (sl >= 0) return sl;
    sl = poolFreeSlot();
    if (sl < 0) {   // Pool voll: Platz nehmen, den das Projekt nicht mehr benutzt (z.B. nach vielem Ausprobieren im Explorer)
        static bool used[MAX_SAMPLES]; poolUsed(eng.proj, used);
        for (int i = 0; i < PREVIEW_SLOT && sl < 0; i++) if (!used[i]) sl = i;
    }
    if (sl < 0) { toast(TR("SAMPLE-POOL VOLL (%d)", "SAMPLE POOL FULL (%d)"), (int)PREVIEW_SLOT); return -1; }
    if (!poolSetSlot(sl, name)) { toast(TR("NICHT LESBAR: %s", "CANNOT READ: %s"), sampleLastError()); return -1; }
    return sl;
}
static void poolUsed(const Project& J, bool* used) {   // welche Plaetze benutzt das Projekt (Grundwerte + Locks)
    for (int i = 0; i < MAX_SAMPLES; i++) used[i] = false;
    for (int p = 0; p < NPATTERNS; p++) for (int t = 0; t < NTRACKS; t++) {
        const Pattern& P = J.pat[p];
        int si = machineSampleParam(P.machine[t] % M_COUNT);
        if (si < 0) continue;
        if (si == P_X3 && P.p[t][P_S0] % 5 != 4) { bool lk = false; for (int st = 0; st < NSTEPS && !lk; st++) lk = P.s[t][st].lock[P_S0] == 4; if (!lk) continue; }   // WTBL nur mit TABL = USER
        if (P.p[t][si] < MAX_SAMPLES) used[P.p[t][si]] = true;
        for (int st = 0; st < NSTEPS; st++) { uint8_t v = P.s[t][st].lock[si]; if (v < MAX_SAMPLES) used[v] = true; }
    }
}
static void writePoolTrailer(FILE* f, const Project& J) {   // "SPL1", Anzahl, je Eintrag: Platz, Laenge, Name
    bool used[MAX_SAMPLES]; poolUsed(J, used);
    uint16_t n = 0;
    for (int i = 0; i < PREVIEW_SLOT; i++) if (used[i] && sampleName(i)[0]) n++;
    fwrite("SPL1", 1, 4, f); fwrite(&n, 2, 1, f);
    for (int i = 0; i < PREVIEW_SLOT; i++) {
        if (!used[i] || !sampleName(i)[0]) continue;
        uint8_t hdr[2] = {(uint8_t)i, (uint8_t)strlen(sampleName(i))};
        fwrite(hdr, 1, 2, f); fwrite(sampleName(i), 1, hdr[1], f);
    }
}
static bool readPoolTrailer(const char* path, char (*names)[96]) {
    for (int i = 0; i < MAX_SAMPLES; i++) names[i][0] = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    bool ok = false;
    char mg[4]; uint16_t n = 0;
    uint32_t ver = 0;   // Namensliste steht direkt hinter den Projektdaten (Groesse je nach Format)
    long at = (fseek(f, 4, SEEK_SET) == 0 && fread(&ver, 4, 1, f) == 1) ? projectDataSize(ver) : (long)sizeof(Project);
    if (fseek(f, at, SEEK_SET) == 0 && fread(mg, 1, 4, f) == 4 && !memcmp(mg, "SPL1", 4) && fread(&n, 2, 1, f) == 1) {
        ok = true;
        for (int k = 0; k < n; k++) {
            uint8_t hdr[2];
            if (fread(hdr, 1, 2, f) != 2) break;
            char nm[256] = "";
            if (fread(nm, 1, hdr[1], f) != hdr[1]) break;
            nm[hdr[1]] = 0;
            if (hdr[0] < MAX_SAMPLES) snprintf(names[hdr[0]], 96, "%s", nm);
        }
    }
    fclose(f);
    return ok;
}
// nach dem Laden eines Projekts: Pool passend fuellen (neue Projekte per Namensliste, alte per frueherer Nummerierung)
static void poolLoadForProject(const char* path, int force) {
    static char names[MAX_SAMPLES][96];
    bool used[MAX_SAMPLES]; poolUsed(eng.proj, used);
    bool have = readPoolTrailer(path, names);
    poolLegacy = -1;
    if (!have) {   // altes Projekt: Nummer = Position in der Liste der damaligen Version
        static char l1[128][96], l2[250][96];
        int n1 = libLegacyList(l1, 128, 1, 256);              // bis 11/2026: 1 Unterordner-Ebene, 128 Samples
        int n2 = libLegacyList(l2, 250, SAMPLE_DEPTH, 500);   // danach: alle Ebenen, 250 Samples
        int maxUsed = -1; for (int i = 0; i < MAX_SAMPLES; i++) if (used[i]) maxUsed = i;
        const char* base = strrchr(path, '/'); base = base ? base + 1 : path;
        bool songNN = !strncmp(base, "song", 4) && isdigit((unsigned char)base[4]) && isdigit((unsigned char)base[5]) && !strcmp(base + 6, ".gbx");
        // Plaetze, die es nur in der 250er-Liste gibt, entscheiden; sonst stammen songNN-Projekte aus der Zeit der 1-Ebenen-Liste
        int var = force >= 0 ? force : (maxUsed >= n1 ? 1 : (songNN ? 0 : 1));
        if (var == 1 && maxUsed >= n2 && maxUsed < n1) var = 0;
        for (int i = 0; i < MAX_SAMPLES; i++) names[i][0] = 0;
        for (int i = 0; i < MAX_SAMPLES; i++) if (used[i] && i < (var ? n2 : n1)) snprintf(names[i], 96, "%s", var ? l2[i] : l1[i]);
        poolLegacy = var;
        printf("Projekt ohne Sample-Liste: alte Nummern ueber die fruehere Reihenfolge (%s, %d bzw. %d Dateien), hoechster Platz %d\n",
               var ? "alle Ebenen bis 250" : "1 Ebene bis 128", n1, n2, maxUsed);
        for (int i = 0; i < MAX_SAMPLES; i++) if (names[i][0]) printf("  Platz %d -> %s\n", i, names[i]);
    }
    poolMissing = 0;
    for (int i = 0; i < MAX_SAMPLES; i++) {
        const char* want = (i < PREVIEW_SLOT && used[i]) ? names[i] : "";
        if (!strcmp(sampleName(i), want) && (want[0] == 0 || sampleGet(i))) continue;
        if (!poolSetSlot(i, want)) { poolMissing++; poolSetSlot(i, ""); }
    }
    if (poolMissing) toast(TR("%d SAMPLES FEHLEN (LOG)", "%d SAMPLES MISSING (LOG)"), poolMissing);
}
// neues Projekt: Pool leeren, Sample-Spuren auf das Start-Sample (falls vorhanden)
static void poolDefaults() {
    poolLegacy = -1;
    for (int i = 0; i < MAX_SAMPLES; i++) if (sampleName(i)[0]) poolSetSlot(i, "");
    const char* def = libFind("keys/stab-minor") >= 0 ? "keys/stab-minor" : (libCount() ? libName(0) : nullptr);
    int sl = def ? poolAssign(def) : -1;
    lockA();
    for (int p = 0; p < NPATTERNS; p++) for (int t = 0; t < NTRACKS; t++)
        if (machineUsesSample(eng.proj.pat[p].machine[t] % M_COUNT)) eng.proj.pat[p].p[t][P_S0] = (uint8_t)(sl >= 0 ? sl : 0);
    unlockA();
}
// nach Upload/Loeschen im Browser oder USB-Laufwerk: Bibliothek neu einlesen, geaenderte Pool-Samples neu laden
static void reloadSamples() {
    char sdir[600]; snprintf(sdir, sizeof sdir, "%ssamples", basePath);
    int n = libScan(sdir);
    int re = 0;
    for (int i = 0; i < MAX_SAMPLES; i++) {
        const Sample* sm = sampleGet(i);
        if (!sm) continue;
        char full[800]; snprintf(full, sizeof full, "%s/%s.wav", sdir, sm->name);
        struct stat st;
        if (stat(full, &st)) continue;   // Datei weg: geladene Daten behalten (Projekt klingt weiter)
        if ((long)st.st_mtime != sm->mtime || (long)st.st_size != sm->fsize) { char nm[96]; snprintf(nm, sizeof nm, "%s", sm->name); if (poolSetSlot(i, nm)) re++; }
    }
    if (fbOpen && fbMode == FB_SAMPLE) fbBuild();   // offener Explorer: Eintraege zeigen auf Bibliotheksindizes
    printf("Bibliothek: %d Samples (%d andere Formate), %d im Pool neu geladen\n", n, libSkipped(), re);
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
        else if (eng.load(projPath)) { printf("Projekt geladen: %s\n", projPath); poolLoadForProject(projPath); }
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
        { char sd[600]; snprintf(sd, sizeof sd, "%ssamples", basePath); samplesLoad(sd); }
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
        { char sd[600]; snprintf(sd, sizeof sd, "%ssamples", basePath); samplesLoad(sd); }   // Screenshots: ganze Bibliothek in den Pool
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
        page = shotPage < 20 ? shotPage : (shotPage >= 100 && shotPage - 100 < NUI_PAGES ? shotPage - 100 : PG_TRG); track = 4; cursor = 3; slot = 1;   // neue Seiten: 100 + Seitennummer
        if (shotPage >= 60 && shotPage <= 64) {   // 60 FM-Algorithmus, 61 FM-Operator 2, 62 SYNTH Oszillatoren, 63 Effekt-Bus B (Spring)
            if (shotPage <= 61) { applyFactory(curP(), 5, factoryByName("DX E-PIANO")); track = 5; page = shotPage == 60 ? PG_SRC : PG_SRC3; slot = shotPage == 60 ? 0 : 5; }
            if (shotPage == 62) { page = PG_SRC2; track = 4; slot = 1; curP().p[4][P_X1] = 4; curP().p[4][P_X5] = 1; }
            if (shotPage == 64) { track = 6; Engine::setMachine(curP(), 6, M_WTBL); page = PG_SRC2; slot = 4; openWtEditor(); wteShape(3); wteKey = 1; wteShape(0); wteCur = 20; wtePts[1][20] = 0.4f; wteKey = 1; }
            if (shotPage == 63) { page = PG_FXB; Engine::setBusType(eng.proj, 1, BF_SPRING); slot = 3; }
        }
        refreshDevices();
        if (page == PG_SRC) { track = 7; slot = 0; }
        if (page == PG_SRC || page == PG_SRC2 || shotPage == 27) { }
        if (shotPage == 27) { page = PG_SRC2; track = 6; slot = 0; }   // FM Operator 1
        if (page == PG_INS) { track = 5; curP().p[5][P_ITYP] = getenv("GB_INS") ? (uint8_t)atoi(getenv("GB_INS")) : INS_CHORUS; editAt = getenv("GB_INS") ? 0 : 1; editSlot = 0; slot = 0; }
        if (page == PG_MIX) { track = 2; mixRow = 0; eng.solo[1] = true; eng.setMute(3, true); for (int k = 0; k < 3; k++) { eng.render(tmp, 22050 / 2); animate(); } }
        if (page == PG_PRJ) { slot = 2; }
        if (shotPage >= 30 && shotPage <= 33) {   // Engine-Grafiken: 30 KICK, 31 WTBL, 32 CHOP, 33 GRAN
            static const int em[4] = {M_KICK, M_WTBL, M_CHOP, M_GRAN};
            page = PG_SRC; track = shotPage == 30 ? 0 : 5; slot = 1;
            if (shotPage != 30) { Engine::setMachine(curP(), 5, em[shotPage - 30]); if (shotPage >= 32) curP().p[5][P_S0] = 11; curP().p[5][P_S1] = shotPage == 31 ? 70 : (shotPage == 32 ? 7 : 40); }
            for (int t2 = 0; t2 < 8; t2++) eng.setMute(t2, false);
            lockA(); eng.start(); unlockA();
            for (int k2 = 0; k2 < 5; k2++) { eng.render(tmp, 22050 / 2 + 300); animate(); }
        }
        if (shotPage == 34) { page = PG_TRG; track = 4; slot = 6; editSlot = 6; editAt = 1; Engine::setMachine(curP(), 4, M_WTBL); }
        if (shotPage == 35) {
            page = PG_TRG; track = 1; slot = 5; Pattern& Pp = curP(); Pp.s[1][4].lock[P_MTIM] = 23 - 9; Pp.s[1][12].lock[P_MTIM] = 23 + 12;
            holdPat = curPi(); holdTrack = 1; holdStep = 12; aHeld = true; cursor = 12; editSlot = 5; editAt = 1;
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
            holdPat = curPi(); holdTrack = 4; holdStep = 37; aHeld = true; eng.fill = true;
        }
        if (page == PG_SYS) { setSel = SI_OUT; setIn = true; setScroll = SI_OUT - 7.5f; animate(); }
        if (page == PG_CMP) {   // Sidechain-Beispiel
            eng.proj.cmp[CM_SRC] = 1; eng.proj.cmp[CM_THR] = 40; eng.proj.cmp[CM_RAT] = 6; slot = 5;
            for (int k = 0; k < 4; k++) eng.render(tmp, 22050 / 2);
            animate();
        }
        if (page == PG_TRG && shotPage == 0) { Step& tq = curP().s[4][5]; tq.on = 2; memset(tq.lock, NO_LOCK, NPARAMS); tq.lock[P_FREQ] = 20; }
        if (page == PG_ARP) { Pattern& Pa = curP(); Pa.p[track][P_AMOD] = 3; Pa.p[track][P_AOCT] = 1; Pa.p[track][P_ARAT] = 0; Pa.p[track][P_LEN] = 80; Pa.p[track][P_AACC] = 60; }
        if (page == PG_SONG) { eng.proj.songLen = 5; uint8_t sp[5][2] = {{0, 4}, {1, 2}, {0, 2}, {2, 1}, {3, 4}}; for (int k = 0; k < 5; k++) { Engine::rowFromPattern(eng.proj, eng.proj.row[k], sp[k][0]); eng.proj.row[k].rep = sp[k][1]; }
                               eng.proj.row[2].pat[4] = 3; eng.proj.row[2].pat[7] = SONG_EMPTY; eng.proj.row[3].mute = 0x0C; songCol = SC_T1 + 4;
                               eng.songOn = true; eng.songPos = 1; eng.songRep = 1; eng.applyRow(1); eng.playing = true; songSel = 2; }
        if (shotPage == 21) { holdPat = 0; holdTrack = 4; holdStep = 3; aHeld = true; page = 0; track = 4; slot = 0; }
        if (shotPage == 22) { page = PG_FLT; slot = 0; track = 4; animate(); editAt = 1; editSlot = 0; eng.recording = true; }
        if (shotPage == 23) { page = PG_TRG; slot = 6; track = 4; editSlot = 6; editAt = 1; }
        if (shotPage == 24) { page = PG_MST; slot = MS_SCALE; eng.proj.mst[MS_SCALE] = 1 + 1 * 12 + 9; eng.proj.mst[MS_ROOT] = 9; editSlot = MS_SCALE; editAt = 1; }
        if (shotPage == 25) { page = PG_AMP; menuOpen = true; menuSel = PG_FLT; pageClipStore = 2; pageClipTrack = 1; }
        if (getenv("GB_LANG")) g_lang = atoi(getenv("GB_LANG"));   // Screenshots auf Englisch: GB_LANG=1
        if (getenv("GB_SHOT_FT")) { curP().p[4][P_FTYP] = (uint8_t)atoi(getenv("GB_SHOT_FT")); curP().p[4][P_RESO] = 90; slot = 2; }   // Filtertyp fuer Screenshots
        if (shotPage >= 50 && shotPage <= 53) {   // Datei-Dialoge: 50 Tastatur, 51 Samples, 52 Projekte, 53 Sounds
            page = PG_SRC; track = 7; slot = 0;
            if (shotPage == 50) { page = PG_PRJ; openKeyboard(KB_SAVEAS, "Night Drive"); kbRow = 1; kbCol = 4; }
            if (shotPage == 51) { openBrowser(FB_SAMPLE); if (fbN > 1) fbSel = 1; }
            if (shotPage == 52) { page = PG_PRJ; openBrowser(FB_PROJECT); }
            if (shotPage == 53) { page = PG_PRJ; track = 4; openBrowser(FB_PRESET); fbPick(); fbSel = 3; }
        }
        if (shotPage == 54) { page = PG_TRG; track = 4; slot = 6; openBrowser(FB_MACH); }   // Engine-Liste
        if (shotPage == 55) { page = PG_LFO1; track = 4; slot = LF_DEST; fbParam = P_L1 + LF_DEST; openBrowser(FB_LDEST); }   // LFO-Ziel-Liste
        if (shotPage == 26) { page = PG_PRJ; slot = 1; askConfirm(ACT_LOAD, 2, "SONG 03 LADEN? UNGESPEICHERTES GEHT VERLOREN"); }
        if (page == PG_SRC && curP().machine[track] == M_SAMPLE) { }
        {   // Oszilloskop/Spektrum mit echtem Signal fuellen (sonst Ruhe-Animation)
            bool wasPlaying = eng.playing;
            if (!wasPlaying && shotPage != 26) { lockA(); eng.start(); unlockA(); }
            for (int k2 = 0; k2 < 4; k2++) { eng.render(tmp, 2048); scopePush(tmp, 2048); }
            for (int k2 = 0; k2 < 3; k2++) animate();
        }
        if (getenv("GB_ANIMT")) animTime += (float)atof(getenv("GB_ANIMT"));   // Screenshots zu anderer Animationszeit (Vergleich)
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
    bool firstRun = !anyProject();   // vor dem ersten Speichern pruefen
    if (!projFromArg) {   // Projekt aus den Einstellungen (Name); altes project1.gbx wird zu song01.gbx
        char old[700], s1[700];
        snprintf(old, sizeof old, "%sprojects/project1.gbx", basePath);
        projectPath("song01", s1, sizeof s1);
        if (fileExists(old) && !fileExists(s1)) { rename(old, s1); firstRun = false; }
        if (!cfg.project[0]) snprintf(cfg.project, sizeof cfg.project, "%s", firstRun ? "Neon Nights" : "song01");
        projectPath(cfg.project, projPath, sizeof projPath);
    } else {   // --project DATEI: Name aus dem Dateinamen
        const char* b = strrchr(projPath, '/'); b = b ? b + 1 : projPath;
        snprintf(cfg.project, sizeof cfg.project, "%.*s", (int)(strcspn(b, ".")), b);
    }
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
    if (eng.load(projPath)) { printf("Projekt geladen: %s\n", projPath); poolLoadForProject(projPath); }
    else if (firstRun) { buildDemo(eng); eng.songOn = true; eng.songStart(0); poolDefaults(); printf("Erster Start: Demo-Song angelegt\n"); }
    else poolDefaults();   // Einsteiger: sofort etwas zum Hoeren
    memcpy(&lastSaved, &eng.proj, sizeof(Project));
    applyMidiSettings();
    eng.midiOut = midiOutHook;
    midiStart();
    openAudio();
    if (firstRun) toast(TR("WILLKOMMEN! START = DEMO ABSPIELEN, SELECT = MENÜ", "WELCOME! START = PLAY DEMO, SELECT = MENU"));
    else toast(TR("%d SAMPLES GELADEN", "%d SAMPLES LOADED"), sampleCount());
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
        if (recDoneNo) { if (recDoneNo > 0) toast(TR("GESPEICHERT: SAMPLES/REC/REC-%03d.WAV", "SAVED: SAMPLES/REC/REC-%03d.WAV"), recDoneNo); else toast(TR("AUFNAHME: SPEICHERN FEHLGESCHLAGEN", "RECORDING: SAVING FAILED")); recDoneNo = 0; }
        if (audioApplyAt && now >= audioApplyAt) { audioApplyAt = 0; openAudio(); }
        if (usbApplyAt && now >= usbApplyAt) { usbApplyAt = 0; applyUsbMode(); }
        {   // MIDI kommt an, wird aber ignoriert -> einmal je 20 s sagen, wo man es einschaltet
            static uint32_t lastNotes = 0, lastClock = 0, hintAt = 0;
            uint32_t ne = midiNoteEvents(), ce = midiClockEvents();
            bool notesIgnored = ne != lastNotes && !cfg.midiIn, clockIgnored = ce != lastClock && cfg.clk == 0;   // beim Senden kommt Clock oft zurueck
            lastNotes = ne; lastClock = ce;
            if ((notesIgnored || clockIgnored) && !selfTesting && now - hintAt > 20000) {
                hintAt = now;
                if (notesIgnored) toast(TR("MIDI KOMMT AN - MIDI-EINGANG IST AUS (SET > MIDI)", "MIDI ARRIVING - MIDI INPUT IS OFF (SET > MIDI)"));
                else toast(TR("MIDI-CLOCK KOMMT AN - SET > MIDI > CLOCK = EMPFANGEN", "MIDI CLOCK ARRIVING - SET > MIDI > CLOCK = RECEIVE"));
                msgUntil = now + 3500;
            }
        }
        {   // Browser-Upload: Meldung und Samples neu laden, sobald 1,5 s nichts mehr kommt
            char wm[96];
            int sq = webLastEvent(wm, sizeof wm);
            if (sq != webSeq) { webSeq = sq; toast("%s", wm); }
            if (expState != 1 && webTakeChange(now, 1500)) { reloadSamples(); toast(TR("%d SAMPLES GELADEN", "%d SAMPLES LOADED"), sampleCount()); }
            static Uint32 updAt = 0;
            if (webUpdateReady() && !updAt) { updAt = now; toast(TR("UPDATE EMPFANGEN - NEUSTART ...", "UPDATE RECEIVED - RESTARTING ...")); msgUntil = now + 5000; saveProject(); }
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
                    printf("Audio: %d verspaetete Aufrufe (gesamt %d), groesster Abstand %.1f ms, laengste Rechnung %.1f ms, Kerne %d, Limiter max -%.1f dB\n",
                           audLate, audLateTotal, audGapMax, audWorkMax, eng.audioThreads, (double)eng.limGr);
                    audLate = 0; audGapMax = 0; audWorkMax = 0; eng.limGr = 0;
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
