// Bildschirm-Spiegelung + Fernbedienung (siehe remote.h)
// Bildformat fuer den Browser: Kopf (u32 Nummer, u16 Breite, u16 Hoehe, u8 Art 0=voll 1=Differenz, 3 Byte frei),
// danach 16-Bit-Woerter (little endian): 00nnnnnn.. n Pixel ueberspringen (unveraendert),
// 01nnnnnn.. n Pixel einer Farbe (+1 Farbwort), 10nnnnnn.. n einzelne Farben (+n Farbwoerter). Farben RGB565.
#include "remote.h"
#include <SDL.h>
#include <string.h>
#include <stdlib.h>

volatile bool remoteDown[BT_COUNT];
volatile bool remoteHit[BT_COUNT];
volatile float remoteAxis[4];

void remoteInput(const char* name, int value) {
    static const char* const names[BT_COUNT] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y",
                                                "L1", "R1", "L2", "R2", "SELECT", "START"};
    for (int b = 0; b < BT_COUNT; b++)
        if (!strcmp(name, names[b])) { remoteDown[b] = value != 0; if (value) remoteHit[b] = true; return; }
    static const char* const ax[4] = {"LX", "LY", "RX", "RY"};
    for (int a = 0; a < 4; a++)
        if (!strcmp(name, ax[a])) { remoteAxis[a] = value > 0 ? 1.f : (value < 0 ? -1.f : 0.f); return; }
}

enum { MAXW = 640, MAXH = 480 };
static SDL_mutex* g_mx = nullptr;
static uint16_t g_cur[MAXW * MAXH], g_sent[MAXW * MAXH];
static int g_w = 0, g_h = 0;
static volatile unsigned g_seq = 0;
static unsigned g_sentSeq = 0;
static volatile Uint32 g_wantAt = 0;

bool remoteWanted() { return g_wantAt && SDL_GetTicks() - g_wantAt < 2000; }

void remoteSnapshot(const uint32_t* px, int pitch, int w, int h) {
    if (!g_mx) g_mx = SDL_CreateMutex();
    if (w > MAXW) w = MAXW;
    if (h > MAXH) h = MAXH;
    SDL_LockMutex(g_mx);
    bool changed = w != g_w || h != g_h;
    for (int y = 0; y < h; y++) {
        const uint32_t* s = px + (size_t)y * pitch;
        uint16_t* d = g_cur + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            uint32_t p = s[x];
            uint16_t v = (uint16_t)(((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0) | ((p >> 3) & 0x001f));
            if (d[x] != v) { d[x] = v; changed = true; }
        }
    }
    g_w = w; g_h = h;
    if (changed) g_seq++;
    SDL_UnlockMutex(g_mx);
}

size_t remoteEncode(unsigned since, uint8_t** out, size_t* cap) {
    g_wantAt = SDL_GetTicks();
    if (!g_mx || !g_w) return 0;
    if (since == g_seq && since) return 0;   // nichts Neues
    SDL_LockMutex(g_mx);
    int n = g_w * g_h;
    bool delta = since && since == g_sentSeq;
    size_t need = 12 + (size_t)n * 4 + 64;
    if (*cap < need) { free(*out); *out = (uint8_t*)malloc(need); *cap = *out ? need : 0; }
    if (!*out) { SDL_UnlockMutex(g_mx); return 0; }
    uint8_t* o = *out;
    unsigned seq = g_seq;
    memcpy(o, &seq, 4);
    o[4] = (uint8_t)(g_w & 255); o[5] = (uint8_t)(g_w >> 8); o[6] = (uint8_t)(g_h & 255); o[7] = (uint8_t)(g_h >> 8);
    o[8] = delta ? 1 : 0; o[9] = o[10] = o[11] = 0;
    uint16_t* t = (uint16_t*)(o + 12);
    const uint16_t* c = g_cur;
    const uint16_t* p = g_sent;
    int i = 0;
    while (i < n) {
        if (delta && c[i] == p[i]) {   // unveraendert
            int k = i;
            while (k < n && k - i < 16383 && c[k] == p[k]) k++;
            *t++ = (uint16_t)(k - i);
            i = k;
            continue;
        }
        int r = i + 1;   // gleiche Farbe?
        while (r < n && r - i < 16383 && c[r] == c[i]) r++;
        if (r - i >= 3) { *t++ = (uint16_t)(0x4000 | (r - i)); *t++ = c[i]; i = r; continue; }
        // einzelne Farben, bis eine Wiederholung oder ein unveraenderter Block kommt
        uint16_t* hdr = t++;
        int s = i;
        while (i < n && i - s < 16383) {
            if (i + 2 < n && c[i] == c[i + 1] && c[i] == c[i + 2]) break;
            if (delta && i + 3 < n && c[i] == p[i] && c[i + 1] == p[i + 1] && c[i + 2] == p[i + 2] && c[i + 3] == p[i + 3]) break;
            *t++ = c[i++];
        }
        if (i == s) { *t++ = c[i++]; }
        *hdr = (uint16_t)(0x8000 | (i - s));
    }
    memcpy(g_sent, g_cur, (size_t)n * 2);
    g_sentSeq = seq;
    SDL_UnlockMutex(g_mx);
    return (size_t)((uint8_t*)t - *out);
}
