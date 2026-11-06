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
static uint16_t g_cur[MAXW * MAXH];
static int g_w = 0, g_h = 0;
static volatile unsigned g_seq = 0;
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

// Ein Zuschauer: zuletzt gesendetes Bild (fuer Differenzen) und dessen Nummer
struct RemoteClient { uint16_t prev[MAXW * MAXH]; uint16_t cur[MAXW * MAXH]; unsigned seq = 0; bool has = false; int w = 0, h = 0; };

// Bild fuer einen Zuschauer kodieren (0 = nichts Neues); force = volles Bild
static size_t encodeFor(RemoteClient& cl, bool force, uint8_t** out, size_t* cap) {
    g_wantAt = SDL_GetTicks();
    if (!g_mx || !g_w) return 0;
    if (!force && cl.has && cl.seq == g_seq) return 0;
    SDL_LockMutex(g_mx);
    int w = g_w, h = g_h, n = w * h;
    unsigned seq = g_seq;
    memcpy(cl.cur, g_cur, (size_t)n * 2);
    SDL_UnlockMutex(g_mx);
    bool delta = !force && cl.has && cl.w == w && cl.h == h;
    size_t need = 12 + (size_t)n * 4 + 64;
    if (*cap < need) { free(*out); *out = (uint8_t*)malloc(need); *cap = *out ? need : 0; }
    if (!*out) return 0;
    uint8_t* o = *out;
    memcpy(o, &seq, 4);
    o[4] = (uint8_t)(w & 255); o[5] = (uint8_t)(w >> 8); o[6] = (uint8_t)(h & 255); o[7] = (uint8_t)(h >> 8);
    o[8] = delta ? 1 : 0; o[9] = o[10] = o[11] = 0;
    uint16_t* t = (uint16_t*)(o + 12);
    const uint16_t* c = cl.cur;
    const uint16_t* p = cl.prev;
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
        int s0 = i;
        while (i < n && i - s0 < 16383) {
            if (i + 2 < n && c[i] == c[i + 1] && c[i] == c[i + 2]) break;
            if (delta && i + 3 < n && c[i] == p[i] && c[i + 1] == p[i + 1] && c[i + 2] == p[i + 2] && c[i + 3] == p[i + 3]) break;
            *t++ = c[i++];
        }
        if (i == s0) { *t++ = c[i++]; }
        *hdr = (uint16_t)(0x8000 | (i - s0));
    }
    memcpy(cl.prev, cl.cur, (size_t)n * 2);
    cl.seq = seq; cl.has = true; cl.w = w; cl.h = h;
    return (size_t)((uint8_t*)t - *out);
}

// Abfrage-Weg (/api/screen, Rueckfall ohne WebSocket): ein gemeinsamer Zuschauer
size_t remoteEncode(unsigned since, uint8_t** out, size_t* cap) {
    static RemoteClient* poll = nullptr;
    static SDL_mutex* pm = nullptr;
    if (!pm) pm = SDL_CreateMutex();
    if (!poll) poll = (RemoteClient*)calloc(1, sizeof(RemoteClient));
    if (!poll) return 0;
    SDL_LockMutex(pm);
    if (!since) poll->has = false;
    size_t n = (since && since == poll->seq) ? encodeFor(*poll, false, out, cap) : encodeFor(*poll, true, out, cap);
    SDL_UnlockMutex(pm);
    return n;
}

// ---------------------------------------------------------------- Ton in den Browser (/wsa)
// audioCb schreibt die Summe als 16 Bit Stereo in einen Ring (nur solange jemand zuhoert), jeder Zuhoerer liest mit eigenem Zeiger.
enum { AUD_N = 1 << 15 };                    // Frames im Ring (~0,7 s)
static int16_t g_aud[AUD_N * 2];
static volatile unsigned g_audW = 0;          // geschriebene Frames (laeuft ueber)
static volatile int g_audRate = 44100;
static volatile int g_audClients = 0;
void remoteAudioPush(const float* st, int frames, int rate) {
    if (!g_audClients) return;
    g_audRate = rate;
    unsigned w = g_audW;
    for (int i = 0; i < frames; i++) {
        unsigned k = (w + (unsigned)i) & (AUD_N - 1);
        for (int c = 0; c < 2; c++) {
            float v = st[i * 2 + c] * 32767.f;
            g_aud[k * 2 + c] = (int16_t)(v > 32767.f ? 32767 : (v < -32768.f ? -32768 : (int)v));
        }
    }
    __sync_synchronize();
    g_audW = w + (unsigned)frames;
}
bool remoteAudioWanted() { return g_audClients > 0; }

// ---------------------------------------------------------------- Touch-Warteschlange (Web-Thread -> Hauptthread)
static RemoteTouch g_tq[256];
static unsigned g_tqHead = 0, g_tqTail = 0;
static SDL_mutex* g_tqMx = nullptr;
static void touchPush(char type, int id, int x, int y) {
    if (!g_tqMx) return;
    SDL_LockMutex(g_tqMx);
    if (g_tqHead - g_tqTail < 256) {
        // aufeinanderfolgende Bewegungen desselben Fingers zusammenfassen
        if (type == 'm' && g_tqHead != g_tqTail) { RemoteTouch& l = g_tq[(g_tqHead - 1) & 255]; if (l.type == 'm' && l.id == id) { l.x = x; l.y = y; SDL_UnlockMutex(g_tqMx); return; } }
        g_tq[g_tqHead++ & 255] = RemoteTouch{type, id, x, y};
    }
    SDL_UnlockMutex(g_tqMx);
}
bool remoteTouchPop(RemoteTouch& e) {
    if (!g_tqMx || g_tqHead == g_tqTail) return false;
    SDL_LockMutex(g_tqMx);
    bool ok = g_tqHead != g_tqTail;
    if (ok) e = g_tq[g_tqTail++ & 255];
    SDL_UnlockMutex(g_tqMx);
    return ok;
}

// ---------------------------------------------------------------- WebSocket (/ws): Bilder als Strom, Tasten zurueck
// Ein Thread je Zuschauer (max. 3): sendet ein neues Bild, sobald das vorige raus ist (passt sich der Leitung an),
// liest dazwischen Tasten ("d A" / "u A" / "a LX -1") und Pings ("p <zahl>" -> "P <zahl>").
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <pthread.h>

static void sha1(const uint8_t* msg, size_t len, uint8_t out[20]) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;
    size_t ml = len + 9, nb = (ml + 63) / 64;
    uint8_t blk[64];
    for (size_t b = 0; b < nb; b++) {
        for (int i = 0; i < 64; i++) {
            size_t k = b * 64 + i;
            uint8_t v = 0;
            if (k < len) v = msg[k];
            else if (k == len) v = 0x80;
            else if (k >= nb * 64 - 8) v = (uint8_t)(((uint64_t)len * 8) >> (8 * (nb * 64 - 1 - k)));
            blk[i] = v;
        }
        uint32_t w[80];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)blk[i * 4] << 24 | (uint32_t)blk[i * 4 + 1] << 16 | (uint32_t)blk[i * 4 + 2] << 8 | blk[i * 4 + 3];
        for (int i = 16; i < 80; i++) { uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]; w[i] = x << 1 | x >> 31; }
        uint32_t a = h0, bb = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (bb & c) | (~bb & d); k = 0x5A827999; }
            else if (i < 40) { f = bb ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (bb & c) | (bb & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = bb ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = (a << 5 | a >> 27) + f + e + k + w[i];
            e = d; d = c; c = bb << 30 | bb >> 2; bb = a; a = t;
        }
        h0 += a; h1 += bb; h2 += c; h3 += d; h4 += e;
    }
    uint32_t hh[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; i++) { out[i * 4] = (uint8_t)(hh[i] >> 24); out[i * 4 + 1] = (uint8_t)(hh[i] >> 16); out[i * 4 + 2] = (uint8_t)(hh[i] >> 8); out[i * 4 + 3] = (uint8_t)hh[i]; }
}
static void b64(const uint8_t* in, int n, char* out) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int o = 0;
    for (int i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
        out[o++] = T[v >> 18 & 63]; out[o++] = T[v >> 12 & 63];
        out[o++] = i + 1 < n ? T[v >> 6 & 63] : '='; out[o++] = i + 2 < n ? T[v & 63] : '=';
    }
    out[o] = 0;
}
static bool wsSendAll(int fd, const void* p, size_t n) {
    const char* c = (const char*)p;
    while (n) { ssize_t w = send(fd, c, n, MSG_NOSIGNAL); if (w <= 0) return false; c += w; n -= (size_t)w; }
    return true;
}
static bool wsSend(int fd, int op, const void* data, size_t n) {
    uint8_t h[10]; int hl;
    h[0] = (uint8_t)(0x80 | op);
    if (n < 126) { h[1] = (uint8_t)n; hl = 2; }
    else if (n < 65536) { h[1] = 126; h[2] = (uint8_t)(n >> 8); h[3] = (uint8_t)n; hl = 4; }
    else { h[1] = 127; for (int i = 0; i < 8; i++) h[2 + i] = (uint8_t)((uint64_t)n >> (56 - 8 * i)); hl = 10; }
    return wsSendAll(fd, h, (size_t)hl) && (n == 0 || wsSendAll(fd, data, n));
}

static volatile int g_wsClients = 0;
struct WsArg { int fd; bool audio; };

static int wsThread(void* arg);
static void* wsThreadP(void* arg) { wsThread(arg); return nullptr; }
static int wsAudioThread(int fd);
static int wsThread(void* arg) {
    int fd = ((WsArg*)arg)->fd;
    bool audio = ((WsArg*)arg)->audio;
    free(arg);
    if (audio) return wsAudioThread(fd);
    RemoteClient* cl = (RemoteClient*)calloc(1, sizeof(RemoteClient));
    uint8_t* buf = nullptr; size_t cap = 0;
    uint8_t in[4096]; size_t inN = 0;
    bool alive = cl != nullptr, needFull = true;
    Uint32 lastSend = 0;
    while (alive) {
        struct pollfd pf = {fd, POLLIN, 0};
        int pr = poll(&pf, 1, 8);
        if (pr < 0) break;
        if (pr > 0) {
            if (pf.revents & (POLLERR | POLLHUP)) break;
            ssize_t r = recv(fd, in + inN, sizeof in - inN, 0);
            if (r <= 0) break;
            inN += (size_t)r;
            for (;;) {   // vollstaendige Rahmen auswerten (vom Browser immer maskiert)
                if (inN < 2) break;
                int op = in[0] & 15; bool masked = in[1] & 0x80; size_t len = in[1] & 127, hl = 2;
                if (len == 126) { if (inN < 4) break; len = (size_t)in[2] << 8 | in[3]; hl = 4; }
                else if (len == 127) { alive = false; break; }   // so grosse Nachrichten schickt die Seite nie
                size_t tot = hl + (masked ? 4 : 0) + len;
                if (len > sizeof in - 16) { alive = false; break; }
                if (inN < tot) break;
                uint8_t* m = in + hl; uint8_t* d = m + (masked ? 4 : 0);
                if (masked) for (size_t i = 0; i < len; i++) d[i] ^= m[i & 3];
                if (op == 8) { wsSend(fd, 8, nullptr, 0); alive = false; break; }
                if (op == 9) wsSend(fd, 10, d, len);
                if (op == 1 && len < 64) {
                    char msg[64]; memcpy(msg, d, len); msg[len] = 0;
                    char b[16] = ""; int v = 0;
                    if (msg[0] == 'd' && sscanf(msg + 2, "%15s", b) == 1) remoteInput(b, 1);
                    else if (msg[0] == 'u' && sscanf(msg + 2, "%15s", b) == 1) remoteInput(b, 0);
                    else if (msg[0] == 'a' && sscanf(msg + 2, "%15s %d", b, &v) == 2) remoteInput(b, v);
                    else if (msg[0] == 'p') { msg[0] = 'P'; wsSend(fd, 1, msg, len); }
                    else if (msg[0] == 'f') needFull = true;
                    else if ((msg[0] == 't' || msg[0] == 'm' || msg[0] == 'e') && msg[1] == ' ') {   // Touch: "t id x y"
                        int id, x, y;
                        if (sscanf(msg + 2, "%d %d %d", &id, &x, &y) == 3) touchPush(msg[0], id, x, y);
                    }   // Seite war im Hintergrund: volles Bild
                }
                memmove(in, in + tot, inN - tot); inN -= tot;
            }
        }
        if (!alive) break;
        g_wantAt = SDL_GetTicks();   // Hauptschleife soll Bilder liefern
        size_t n = encodeFor(*cl, needFull, &buf, &cap);
        if (n) { needFull = false; lastSend = SDL_GetTicks(); if (!wsSend(fd, 2, buf, n)) break; }
        else if (SDL_GetTicks() - lastSend > 5000) { lastSend = SDL_GetTicks(); if (!wsSend(fd, 9, "k", 1)) break; }   // Lebenszeichen
    }
    for (int b = 0; b < BT_COUNT; b++) remoteDown[b] = false;   // Verbindung weg: nichts bleibt gedrueckt
    touchPush('c', 0, 0, 0);
    for (int a = 0; a < 4; a++) remoteAxis[a] = 0.f;
    free(buf); free(cl);
    close(fd);
    __sync_fetch_and_sub(&g_wsClients, 1);
    return 0;
}

// Ton-Strom: Pakete "u32 Rate, u32 Frame-Nummer, int16 L/R ..." (~12 ms), nur gesendet wenn das vorige raus ist;
// faellt der Zuhoerer > 0,25 s zurueck (schlechtes Netz), springt er nach vorn (lieber kurz Luecke als wachsende Verzoegerung)
static int wsAudioThread(int fd) {
    __sync_fetch_and_add(&g_audClients, 1);
    unsigned rd = g_audW;
    uint8_t in[512]; size_t inN = 0;
    static const int MAXF = 2048;
    uint8_t* pk = (uint8_t*)malloc(8 + MAXF * 4);
    bool alive = pk != nullptr;
    Uint32 lastSend = SDL_GetTicks();
    while (alive) {
        struct pollfd pf = {fd, POLLIN, 0};
        int pr = poll(&pf, 1, 5);
        if (pr < 0) break;
        if (pr > 0) {   // nur Ping/Schliessen erwartet
            if (pf.revents & (POLLERR | POLLHUP)) break;
            ssize_t r = recv(fd, in + inN, sizeof in - inN, 0);
            if (r <= 0) break;
            inN += (size_t)r;
            while (inN >= 2) {
                int op = in[0] & 15; bool masked = in[1] & 0x80; size_t len = in[1] & 127;
                if (len >= 126) { alive = false; break; }
                size_t tot = 2 + (masked ? 4 : 0) + len;
                if (inN < tot) break;
                uint8_t* m = in + 2; uint8_t* d = m + (masked ? 4 : 0);
                if (masked) for (size_t i = 0; i < len; i++) d[i] ^= m[i & 3];
                if (op == 8) { wsSend(fd, 8, nullptr, 0); alive = false; break; }
                if (op == 1 && len > 0 && d[0] == 'p') { d[0] = 'P'; wsSend(fd, 1, d, len); }
                memmove(in, in + tot, inN - tot); inN -= tot;
            }
        }
        if (!alive) break;
        unsigned w = g_audW;
        unsigned avail = w - rd;
        if (avail > (unsigned)(g_audRate / 4)) { rd = w - 1024; avail = 1024; }   // zu weit zurueck: aufholen
        if (avail >= 512) {
            int n = avail > (unsigned)MAXF ? MAXF : (int)avail;
            uint32_t rate = (uint32_t)g_audRate, pos = rd;
            memcpy(pk, &rate, 4); memcpy(pk + 4, &pos, 4);
            int16_t* o = (int16_t*)(pk + 8);
            for (int i = 0; i < n; i++) { unsigned k = (rd + (unsigned)i) & (AUD_N - 1); o[i * 2] = g_aud[k * 2]; o[i * 2 + 1] = g_aud[k * 2 + 1]; }
            rd += (unsigned)n;
            if (!wsSend(fd, 2, pk, 8 + (size_t)n * 4)) break;
            lastSend = SDL_GetTicks();
        } else if (SDL_GetTicks() - lastSend > 5000) { lastSend = SDL_GetTicks(); if (!wsSend(fd, 9, "k", 1)) break; }
    }
    free(pk);
    close(fd);
    __sync_fetch_and_sub(&g_audClients, 1);
    __sync_fetch_and_sub(&g_wsClients, 1);
    return 0;
}

bool remoteWsAccept(int fd, const char* headers, bool audio) {
    const char* k = nullptr;
    for (const char* l = headers; l && *l; l = strstr(l, "\r\n")) {
        if (*l == '\r') l += 2;
        if (!strncasecmp(l, "Sec-WebSocket-Key:", 18)) { k = l + 18; break; }
    }
    if (!k) return false;
    while (*k == ' ') k++;
    char key[128]; int n = 0;
    while (k[n] && k[n] != '\r' && n < 60) { key[n] = k[n]; n++; }
    key[n] = 0;
    if (g_wsClients >= 6) {
        const char* busy = "HTTP/1.1 503 Busy\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        wsSendAll(fd, busy, strlen(busy));
        return false;
    }
    char cat[200]; snprintf(cat, sizeof cat, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    uint8_t dg[20]; sha1((const uint8_t*)cat, strlen(cat), dg);
    char acc[40]; b64(dg, 20, acc);
    char resp[300];
    int rl = snprintf(resp, sizeof resp, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", acc);
    if (!wsSendAll(fd, resp, (size_t)rl)) return false;
    int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    struct timeval tv = {10, 0}; setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    struct timeval rt = {0, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof rt);
    WsArg* a = (WsArg*)malloc(sizeof(WsArg));
    if (!a) return false;
    if (!g_tqMx) g_tqMx = SDL_CreateMutex();
    a->fd = fd; a->audio = audio;
    __sync_fetch_and_add(&g_wsClients, 1);
    pthread_attr_t at; pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 256 * 1024);   // kleiner Stack (mlockall sperrt sonst 8 MB je Thread)
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    pthread_t th;
    int rc = pthread_create(&th, &at, wsThreadP, a);
    pthread_attr_destroy(&at);
    if (rc) { __sync_fetch_and_sub(&g_wsClients, 1); free(a); return false; }
    return true;
}
