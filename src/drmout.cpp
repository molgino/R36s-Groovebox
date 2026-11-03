// Direkte Bildausgabe ueber DRM/KMS (Groovebox-OS): die App zeichnet mit dem SDL-Software-Renderer
// in eine Surface, die hier in einen von zwei "Dumb-Buffern" kopiert und per Page-Flip angezeigt wird.
// Kein OpenGL/Mesa noetig. Nur mit WITH_DRM (make DRM=1) gebaut, sonst Stubs.
#include "drmout.h"
#include <stdio.h>

#ifdef WITH_DRM
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static int g_fd = -1;
static uint32_t g_crtc = 0, g_conn = 0;
static drmModeModeInfo g_mode;
struct DumbBuf { uint32_t handle, fb, pitch; uint64_t size; uint8_t* map; };
static DumbBuf g_buf[2];
static int g_back = 1;
static bool g_flipPending = false;
static drmModeCrtc* g_saved = nullptr;
static bool g_single = false;   // Page-Flip geht nicht -> direkt in den angezeigten Puffer schreiben
static double g_copyMs = 0, g_flipMs = 0; static int g_frames = 0;
static double nowMs() { return SDL_GetPerformanceCounter() * 1000.0 / SDL_GetPerformanceFrequency(); }
void drmOutStats(double* copyMs, double* flipMs) {
    *copyMs = g_frames ? g_copyMs / g_frames : 0; *flipMs = g_frames ? g_flipMs / g_frames : 0;
    g_copyMs = g_flipMs = 0; g_frames = 0;
}
bool drmOutSingleBuffer() { return g_single; }
static int g_off = 0;   // Byte-Versatz im RGB-Datenstrom einer Zeile (siehe drmOutSetByteOffset)
void drmOutSetByteOffset(int bytes) { g_off = bytes; }

static bool makeBuf(DumbBuf& b, int w, int h) {
    struct drm_mode_create_dumb c;
    memset(&c, 0, sizeof c);
    c.width = (uint32_t)w; c.height = (uint32_t)h; c.bpp = 32;
    if (drmIoctl(g_fd, DRM_IOCTL_MODE_CREATE_DUMB, &c) < 0) return false;
    b.handle = c.handle; b.pitch = c.pitch; b.size = c.size;
    if (drmModeAddFB(g_fd, (uint32_t)w, (uint32_t)h, 24, 32, b.pitch, b.handle, &b.fb)) return false;
    struct drm_mode_map_dumb m;
    memset(&m, 0, sizeof m);
    m.handle = b.handle;
    if (drmIoctl(g_fd, DRM_IOCTL_MODE_MAP_DUMB, &m) < 0) return false;
    b.map = (uint8_t*)mmap(nullptr, b.size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, (off_t)m.offset);
    if (b.map == MAP_FAILED) { b.map = nullptr; return false; }
    memset(b.map, 0, b.size);
    return true;
}

bool drmOutOpen() {
    for (int card = 0; card < 4; card++) {
        char path[32];
        snprintf(path, sizeof path, "/dev/dri/card%d", card);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        drmModeRes* res = drmModeGetResources(fd);
        if (!res) { close(fd); continue; }   // z. B. reine GPU (Panfrost) ohne Anzeige
        for (int i = 0; i < res->count_connectors && !g_conn; i++) {
            drmModeConnector* c = drmModeGetConnector(fd, res->connectors[i]);
            if (!c) continue;
            if (c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
                g_mode = c->modes[0];
                for (int k = 0; k < c->count_modes; k++)
                    if (c->modes[k].type & DRM_MODE_TYPE_PREFERRED) { g_mode = c->modes[k]; break; }
                uint32_t crtc = 0;
                drmModeEncoder* e = c->encoder_id ? drmModeGetEncoder(fd, c->encoder_id) : nullptr;
                if (e) { crtc = e->crtc_id; drmModeFreeEncoder(e); }
                for (int k = 0; !crtc && k < c->count_encoders; k++) {
                    drmModeEncoder* e2 = drmModeGetEncoder(fd, c->encoders[k]);
                    if (!e2) continue;
                    for (int j = 0; j < res->count_crtcs; j++)
                        if (e2->possible_crtcs & (1u << j)) { crtc = res->crtcs[j]; break; }
                    drmModeFreeEncoder(e2);
                }
                if (crtc) { g_conn = c->connector_id; g_crtc = crtc; }
            }
            drmModeFreeConnector(c);
        }
        drmModeFreeResources(res);
        if (!g_conn) { close(fd); continue; }
        g_fd = fd;
        int w = g_mode.hdisplay, h = g_mode.vdisplay;
        if (!makeBuf(g_buf[0], w, h) || !makeBuf(g_buf[1], w, h)) {
            printf("DRM: Bildspeicher fehlgeschlagen\n");
            close(fd); g_fd = -1; g_conn = 0;
            return false;
        }
        g_saved = drmModeGetCrtc(fd, g_crtc);
        // Standard: ein Bildspeicher, direkt beschrieben (Page-Flip war auf dem R36S langsam/unzuverlaessig);
        // GB_DRM_FLIP=1 in config/groovebox.env schaltet den Puffer-Wechsel zum Testen ein
        const char* fl = getenv("GB_DRM_FLIP");
        g_single = !(fl && fl[0] == '1');
        if (drmModeSetCrtc(fd, g_crtc, g_buf[0].fb, 0, 0, &g_conn, 1, &g_mode)) {
            printf("DRM: Modus setzen fehlgeschlagen (%s)\n", path);
            close(fd); g_fd = -1; g_conn = 0;
            return false;
        }
        printf("DRM-Ausgabe: %s, %dx%d@%d, direkt ohne GPU, %s\n", path, w, h, g_mode.vrefresh, g_single ? "1 Bildspeicher" : "Page-Flip");
        return true;
    }
    printf("DRM: keine Anzeige gefunden\n");
    return false;
}

bool drmOutActive() { return g_fd >= 0; }

static void onFlip(int, unsigned int, unsigned int, unsigned int, void*) { g_flipPending = false; }

static int g_flipTimeouts = 0;
static void waitFlip(int timeoutMs) {
    while (g_flipPending) {
        struct pollfd p = {g_fd, POLLIN, 0};
        if (poll(&p, 1, timeoutMs) <= 0) {   // keine Flip-Meldung: nach 3 Mal ohne Page-Flip weiter
            g_flipPending = false;
            if (++g_flipTimeouts >= 3 && !g_single) {
                printf("DRM: keine Page-Flip-Meldungen - nutze einen Bildspeicher\n");
                g_single = true;
                drmModeSetCrtc(g_fd, g_crtc, g_buf[0].fb, 0, 0, &g_conn, 1, &g_mode);
            }
            break;
        }
        g_flipTimeouts = 0;
        drmEventContext ev;
        memset(&ev, 0, sizeof ev);
        ev.version = 2;
        ev.page_flip_handler = onFlip;
        drmHandleEvent(g_fd, &ev);
    }
}

void drmOutPresent(SDL_Surface* s) {
    if (g_fd < 0 || !s) return;
    double t0 = nowMs();
    waitFlip(40);
    double t1 = nowMs();
    DumbBuf& b = g_single ? g_buf[0] : g_buf[g_back];
    static uint32_t tmp[4096];   // Zeile erst im (gecachten) Speicher bauen, dann am Stueck kopieren
    int w = g_mode.hdisplay, h = g_mode.vdisplay;
    SDL_LockSurface(s);
    if (g_off != 0 && s->w == w && s->h == h && s->format->BytesPerPixel == 4) {
        // Das Display zeigt an Byte k den Byte k+off des Zeilenstroms (R,G,B je Pixel) -> vorab um -off rotieren.
        // off = 3*N + r: Pixel um N verschieben, bei r=1/2 die Kanaele aus zwei Nachbarpixeln zusammensetzen.
        int L = w * 3, o = ((g_off % L) + L) % L, N = o / 3, r = o % 3;
        for (int y = 0; y < h; y++) {
            const uint32_t* src = (const uint32_t*)((uint8_t*)s->pixels + (size_t)y * s->pitch);
            uint32_t* dst = tmp;
            int q = (w - N) % w, qm = (q + w - 1) % w;
            for (int x = 0; x < w; x++) {
                uint32_t a = src[qm], c = src[q], v;
                if (r == 0) v = c;
                else if (r == 1) v = ((a & 0xffu) << 16) | ((c >> 8) & 0xffffu);
                else v = ((a & 0xffffu) << 8) | ((c >> 16) & 0xffu);
                dst[x] = 0xff000000u | v;
                qm = q; if (++q == w) q = 0;
            }
            memcpy(b.map + (size_t)y * b.pitch, tmp, (size_t)w * 4);
        }
    } else if (s->w == w && s->h == h && s->format->BytesPerPixel == 4) {
        for (int y = 0; y < h; y++) memcpy(b.map + (size_t)y * b.pitch, (uint8_t*)s->pixels + (size_t)y * s->pitch, (size_t)w * 4);
    } else {   // Groesse weicht ab: nach naechstem Nachbarn skalieren
        for (int y = 0; y < h; y++) {
            uint32_t* dst = (uint32_t*)(b.map + (size_t)y * b.pitch);
            const uint32_t* src = (const uint32_t*)((uint8_t*)s->pixels + (size_t)(y * s->h / h) * s->pitch);
            for (int x = 0; x < w; x++) dst[x] = src[x * s->w / w];
        }
    }
    SDL_UnlockSurface(s);
    double t2 = nowMs();
    if (!g_single) {
        if (drmModePageFlip(g_fd, g_crtc, b.fb, DRM_MODE_PAGE_FLIP_EVENT, nullptr) == 0) { g_flipPending = true; g_back ^= 1; }
        else {   // kein Page-Flip moeglich: einmal Puffer 0 anzeigen und kuenftig direkt hineinschreiben (kein Modus-Neusetzen pro Bild!)
            printf("DRM: Page-Flip nicht moeglich (%s) - nutze einen Bildspeicher\n", strerror(errno));
            g_single = true;
            drmModeSetCrtc(g_fd, g_crtc, g_buf[0].fb, 0, 0, &g_conn, 1, &g_mode);
        }
    }
    double t3 = nowMs();
    g_copyMs += t2 - t1; g_flipMs += (t1 - t0) + (t3 - t2); g_frames++;
}

void drmOutClose() {
    if (g_fd < 0) return;
    waitFlip(40);
    if (g_saved) {
        drmModeSetCrtc(g_fd, g_saved->crtc_id, g_saved->buffer_id, g_saved->x, g_saved->y, &g_conn, 1, &g_saved->mode);
        drmModeFreeCrtc(g_saved);
        g_saved = nullptr;
    }
    close(g_fd);
    g_fd = -1;
}

#else
void drmOutStats(double* a, double* b) { *a = *b = 0; }
bool drmOutSingleBuffer() { return false; }
void drmOutSetByteOffset(int) {}
bool drmOutOpen() { return false; }
bool drmOutActive() { return false; }
void drmOutPresent(SDL_Surface*) {}
void drmOutClose() {}
#endif
