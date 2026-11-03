// USB-Audio zum PC (Groovebox-OS, UAC2-Gadget) als ZWEITER Ausgang:
// Der Takt kommt weiter von der internen Soundkarte. Der Audio-Callback legt jede Mischung zusaetzlich
// in einen Ringpuffer, ein eigener Thread schreibt sie nicht-blockierend auf das Gadget.
// Liest der PC gerade nicht mit, laeuft der Puffer ueber und wird verworfen - der Sequencer bleibt nie stehen.
#include "usbaudio.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#ifdef WITH_MIDI
#include <alsa/asoundlib.h>
#include <SDL.h>

enum { RING = 16384, RMASK = RING - 1, MAXLAT = 4096, PERIOD = 256 };   // Frames
static int16_t g_ring[RING * 2];
static volatile unsigned g_w = 0, g_r = 0;     // Schreib-/Leseposition (Frames), nur je ein Thread schreibt
static volatile bool g_run = false, g_active = false;
static volatile int g_status = 0;              // 0 aus, 1 wartet auf PC, 2 PC liest
static SDL_Thread* g_thread = nullptr;
static char g_dev[32];
static volatile int g_peak = 0;          // groesster Betrag der uebergebenen Samples (seit letzter Statistik)
static volatile Uint32 g_toneUntil = 0;  // Testton bis zu diesem Zeitpunkt
static unsigned g_tonePh = 0;
void usbAudioTestTone(int ms) { g_toneUntil = SDL_GetTicks() + (Uint32)ms; }

// Hat die Karte einen Wiedergabe-PCM (Geraet 0..)? Liefert die Geraetenummer oder -1
static int playbackDevice(int card) {
    char nm[16]; snprintf(nm, sizeof nm, "hw:%d", card);
    snd_ctl_t* ctl;
    if (snd_ctl_open(&ctl, nm, 0) < 0) return -1;
    int dev = -1, found = -1;
    snd_pcm_info_t* info; snd_pcm_info_alloca(&info);
    while (snd_ctl_pcm_next_device(ctl, &dev) == 0 && dev >= 0) {
        snd_pcm_info_set_device(info, (unsigned)dev);
        snd_pcm_info_set_subdevice(info, 0);
        snd_pcm_info_set_stream(info, SND_PCM_STREAM_PLAYBACK);
        if (snd_ctl_pcm_info(ctl, info) >= 0) { found = dev; break; }
    }
    snd_ctl_close(ctl);
    return found;
}

// UAC2-Gadget-Soundkarte suchen: nur Karten mit Wiedergabe-PCM, nie MIDI ("MIDI Gadget") oder den internen Codec.
// Bevorzugt Namen mit "uac"/"gadget", sonst (im PC-Modus gibt es keine USB-Host-Geraete) die erste andere Karte mit Audio.
static bool findGadget(char* out, int n) {
    int card = -1, best = -1, bestDev = 0, other = -1, otherDev = 0;
    static char lastList[512] = "";
    char list[512] = "";
    while (snd_card_next(&card) == 0 && card >= 0) {
        char *nm = nullptr, *ln = nullptr;
        snd_card_get_name(card, &nm);
        snd_card_get_longname(card, &ln);
        int pd = playbackDevice(card);
        size_t ll = strlen(list);
        snprintf(list + ll, sizeof list - ll, "%d:'%s'%s ", card, nm ? nm : "?", pd >= 0 ? "(Audio)" : "");
        bool midi = (nm && strcasestr(nm, "midi")) || (ln && strcasestr(ln, "midi"));
        bool internal = (nm && strcasestr(nm, "rk817")) || (ln && strcasestr(ln, "rk817"));
        bool named = (nm && (strcasestr(nm, "uac") || strcasestr(nm, "gadget"))) ||
                     (ln && (strcasestr(ln, "uac") || strcasestr(ln, "gadget")));
        free(nm); free(ln);
        if (pd < 0 || midi || internal) continue;
        if (named && best < 0) { best = card; bestDev = pd; }
        if (other < 0) { other = card; otherDev = pd; }
    }
    if (strcmp(list, lastList)) { printf("USB-Audio: Soundkarten %s\n", list); snprintf(lastList, sizeof lastList, "%s", list); }
    if (best < 0) { best = other; bestDev = otherDev; }
    if (best < 0) return false;
    snprintf(out, n, "hw:%d,%d", best, bestDev);
    return true;
}

static int usbThread(void*) {
    snd_pcm_t* pcm = nullptr;
    int e = snd_pcm_open(&pcm, g_dev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (e < 0) {
        static char lastErr[64] = "";
        if (strcmp(lastErr, g_dev)) { printf("USB-Audio: %s nicht zu oeffnen (%s)\n", g_dev, snd_strerror(e)); snprintf(lastErr, sizeof lastErr, "%s", g_dev); }
        g_active = false; return 0;
    }
    if (snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, 44100, 1, 30000) < 0) {
        printf("USB-Audio: Format nicht moeglich\n");
        snd_pcm_close(pcm); g_active = false; return 0;
    }
    printf("USB-Audio zum PC aktiv (%s)\n", g_dev);
    // Taktausgleich: interne Soundkarte und USB-Host laufen mit leicht unterschiedlicher Uhr.
    // Wir lesen den Ring mit fein geregelter Rate (lineare Interpolation), Ziel-Fuellstand TARGET Frames.
    enum { OUT = 256, TARGET = 1024 };
    static int16_t outb[OUT * 2];
    int pendOff = 0, pendLen = 0;      // noch nicht geschriebener Teil von outb
    double frac = 0, ratio = 1.0, fillAvg = TARGET;
    Uint32 lastOk = 0, statAt = SDL_GetTicks();
    unsigned long wrote = 0, full = 0, dropped = 0, errs = 0; int lastErr = 0;
    while (g_run) {
        if (SDL_GetTicks() - statAt >= 10000) {   // alle 10 s ins Log: kommt etwas beim PC an?
            snd_pcm_sframes_t av = snd_pcm_avail(pcm);
            printf("USB-Audio: %lu Frames an PC, Pegel %d%%, Takt %+.0f ppm, voll %lux, verworfen %lu, Fehler %lu%s%s, Zustand %s, frei %ld\n",
                   wrote, g_peak * 100 / 32767, (ratio - 1.0) * 1e6, full, dropped, errs, lastErr ? " letzter: " : "",
                   lastErr ? snd_strerror(lastErr) : "", snd_pcm_state_name(snd_pcm_state(pcm)), (long)av);
            g_peak = 0;
            fflush(stdout);
            wrote = full = dropped = errs = 0; lastErr = 0; statAt = SDL_GetTicks();
        }
        if (pendLen == 0) {   // naechsten Block erzeugen
            unsigned w = g_w, r = g_r, fill = w - r;
            if (fill > MAXLAT) {   // PC liest nicht mit -> Altes verwerfen, Regelung zuruecksetzen
                dropped += fill - TARGET; r = w - TARGET; g_r = r; fill = TARGET; frac = 0; ratio = 1.0; fillAvg = TARGET;
            }
            if (fill < (unsigned)(OUT * 1.01) + 2) { SDL_Delay(2); continue; }
            fillAvg += 0.02 * ((double)fill - fillAvg);
            double want = 1.0 + 0.003 * ((fillAvg - TARGET) / TARGET);   // zu voll -> schneller lesen
            if (want > 1.003) want = 1.003;
            if (want < 0.997) want = 0.997;
            ratio += 0.05 * (want - ratio);
            double pos = frac;
            for (int i = 0; i < OUT; i++, pos += ratio) {
                unsigned ip = (unsigned)pos;
                float f = (float)(pos - ip);
                const int16_t* a = &g_ring[((r + ip) & RMASK) * 2];
                const int16_t* b = &g_ring[((r + ip + 1) & RMASK) * 2];
                outb[i * 2] = (int16_t)(a[0] + (b[0] - a[0]) * f);
                outb[i * 2 + 1] = (int16_t)(a[1] + (b[1] - a[1]) * f);
            }
            unsigned used = (unsigned)pos;
            frac = pos - used;
            g_r = r + used;
            pendOff = 0; pendLen = OUT;
        }
        snd_pcm_sframes_t n = snd_pcm_writei(pcm, &outb[pendOff * 2], (snd_pcm_uframes_t)pendLen);
        if (n > 0) { pendOff += (int)n; pendLen -= (int)n; wrote += (unsigned long)n; lastOk = SDL_GetTicks(); g_status = 2; }
        else if (n == -EAGAIN) { full++; SDL_Delay(2); }
        else if (n == -EPIPE || n == -ESTRPIPE) { errs++; lastErr = (int)n; snd_pcm_prepare(pcm); }
        else { errs++; lastErr = (int)n; SDL_Delay(50); snd_pcm_prepare(pcm); }
        if (SDL_GetTicks() - lastOk > 500) g_status = 1;
    }
    snd_pcm_drop(pcm);
    snd_pcm_close(pcm);
    g_status = 0;
    return 0;
}

bool usbAudioStart() {
    if (g_active) return true;
    if (g_thread) { SDL_WaitThread(g_thread, nullptr); g_thread = nullptr; }   // vorheriger Versuch beendet
    if (!findGadget(g_dev, sizeof g_dev)) return false;
    g_w = g_r = 0;
    g_run = true; g_active = true; g_status = 1;
    g_thread = SDL_CreateThread(usbThread, "usbaudio", nullptr);
    if (!g_thread) { g_run = false; g_active = false; return false; }
    return true;
}

void usbAudioStop() {
    if (!g_thread) { g_active = false; return; }
    g_run = false;
    SDL_WaitThread(g_thread, nullptr);
    g_thread = nullptr;
    g_active = false; g_status = 0;
}

bool usbAudioActive() { return g_active; }
int usbAudioStatus() { return g_active ? g_status : 0; }

void usbAudioPush(const float* st, int frames) {   // nur aus dem Audio-Callback
    if (!g_active) return;
    unsigned w = g_w;
    if ((w - g_r) + (unsigned)frames > RING - 1) return;   // voll: neue Daten verwerfen
    bool tone = g_toneUntil && (int)(g_toneUntil - SDL_GetTicks()) > 0;
    int pk = g_peak;
    for (int i = 0; i < frames; i++) {
        unsigned k = ((w + i) & RMASK) * 2;
        for (int c = 0; c < 2; c++) {
            float x = tone ? 0.5f * sinf((float)g_tonePh * (6.2831853f * 1000.f / 44100.f)) : st[i * 2 + c];
            x = x < -1.f ? -1.f : (x > 1.f ? 1.f : x);
            int16_t v = (int16_t)(x * 32767.f);
            g_ring[k + c] = v;
            int a = v < 0 ? -v : v;
            if (a > pk) pk = a;
        }
        if (tone) g_tonePh = (g_tonePh + 1) % 44100;
    }
    g_peak = pk;
    __atomic_store_n(&g_w, w + (unsigned)frames, __ATOMIC_RELEASE);
}

// ---------------------------------------------------------------- PC -> R36S (Gadget-Aufnahme-PCM)
// Ein Thread liest, was der PC auf "Synth City" abspielt; der Audio-Callback mischt es mit geregelter Rate
// (Taktausgleich wie oben) in den Ausgang und kann es als Sample aufnehmen.
enum { IRING = 16384, IMASK = IRING - 1, ITARGET = 2048 };
static int16_t g_iring[IRING * 2];
static volatile unsigned g_iw = 0, g_ir = 0;
static volatile bool g_irun = false, g_iactive = false;
static volatile int g_istatus = 0;
static SDL_Thread* g_ithread = nullptr;
static double g_ifrac = 0, g_iratio = 1.0, g_ifill = ITARGET;
static bool g_iprimed = false;
static int16_t* g_rec = nullptr;
static volatile int g_recLen = 0, g_recState = 0;   // 0 aus, 1 wartet auf Signal, 2 nimmt auf, 3 fertig
enum { REC_MAX = 44100 * 60 };

static int pcInThread(void*) {
    snd_pcm_t* pcm = nullptr;
    int e = snd_pcm_open(&pcm, g_dev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if (e < 0) { printf("PC-Audio: %s nicht zu oeffnen (%s)\n", g_dev, snd_strerror(e)); g_iactive = false; return 0; }
    if (snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, 44100, 1, 30000) < 0) {
        printf("PC-Audio: Format nicht moeglich\n"); snd_pcm_close(pcm); g_iactive = false; return 0;
    }
    printf("PC-Audio vom PC aktiv (%s)\n", g_dev);
    static int16_t tmp[256 * 2];
    Uint32 lastOk = 0, statAt = SDL_GetTicks();
    unsigned long got = 0;
    snd_pcm_start(pcm);
    while (g_irun) {
        snd_pcm_sframes_t n = snd_pcm_readi(pcm, tmp, 256);
        if (n > 0) {
            unsigned w = g_iw;
            if ((w - g_ir) + (unsigned)n < IRING - 1) {
                for (int i = 0; i < n; i++) { unsigned k = ((w + i) & IMASK) * 2; g_iring[k] = tmp[i * 2]; g_iring[k + 1] = tmp[i * 2 + 1]; }
                __atomic_store_n(&g_iw, w + (unsigned)n, __ATOMIC_RELEASE);
            }
            got += (unsigned long)n; lastOk = SDL_GetTicks(); g_istatus = 2;
        } else if (n == -EAGAIN) SDL_Delay(2);
        else if (n == -EPIPE || n == -ESTRPIPE) { snd_pcm_prepare(pcm); snd_pcm_start(pcm); }
        else { SDL_Delay(50); snd_pcm_prepare(pcm); snd_pcm_start(pcm); }
        if (SDL_GetTicks() - lastOk > 500) g_istatus = 1;
        if (SDL_GetTicks() - statAt >= 10000) {
            if (got) printf("PC-Audio: %lu Frames vom PC, Takt %+.0f ppm\n", got, (g_iratio - 1.0) * 1e6);
            got = 0; statAt = SDL_GetTicks();
        }
    }
    snd_pcm_drop(pcm);
    snd_pcm_close(pcm);
    g_istatus = 0;
    return 0;
}

bool pcInStart() {
    if (g_iactive) return true;
    if (g_ithread) { SDL_WaitThread(g_ithread, nullptr); g_ithread = nullptr; }
    if (!findGadget(g_dev, sizeof g_dev)) return false;
    g_iw = g_ir = 0; g_iprimed = false; g_ifrac = 0; g_iratio = 1.0; g_ifill = ITARGET;
    g_irun = true; g_iactive = true; g_istatus = 1;
    g_ithread = SDL_CreateThread(pcInThread, "pcaudio", nullptr);
    if (!g_ithread) { g_irun = false; g_iactive = false; return false; }
    return true;
}
void pcInStop() {
    if (!g_ithread) { g_iactive = false; return; }
    g_irun = false;
    SDL_WaitThread(g_ithread, nullptr);
    g_ithread = nullptr; g_iactive = false; g_istatus = 0;
}
bool pcInActive() { return g_iactive; }
int pcInStatus() { return g_iactive ? g_istatus : 0; }

void pcInMix(float* st, int frames, float gain) {   // nur aus dem Audio-Callback
    if (!g_iactive) return;
    unsigned w = g_iw, r = g_ir, fill = w - r;
    if (!g_iprimed) { if (fill < ITARGET) return; g_iprimed = true; g_ifrac = 0; g_iratio = 1.0; g_ifill = fill; }
    if (fill > IRING / 2) { r = w - ITARGET; fill = ITARGET; g_ifrac = 0; }   // zu viel aufgelaufen
    if (fill < (unsigned)(frames * 1.01f) + 2) { g_iprimed = false; return; }   // PC spielt nicht mehr -> neu anlaufen
    g_ifill += 0.02 * ((double)fill - g_ifill);
    double want = 1.0 + 0.003 * ((g_ifill - ITARGET) / ITARGET);
    if (want > 1.003) want = 1.003;
    if (want < 0.997) want = 0.997;
    g_iratio += 0.05 * (want - g_iratio);
    double pos = g_ifrac;
    int rs = g_recState;
    for (int i = 0; i < frames; i++, pos += g_iratio) {
        unsigned ip = (unsigned)pos;
        float f = (float)(pos - ip);
        const int16_t* a = &g_iring[((r + ip) & IMASK) * 2];
        const int16_t* b = &g_iring[((r + ip + 1) & IMASK) * 2];
        float L = (a[0] + (b[0] - a[0]) * f) * (1.f / 32768.f), R = (a[1] + (b[1] - a[1]) * f) * (1.f / 32768.f);
        st[i * 2] += L * gain;
        st[i * 2 + 1] += R * gain;
        if (rs == 1 && (fabsf(L) > 0.01f || fabsf(R) > 0.01f)) rs = 2;   // Aufnahme beginnt mit dem ersten Signal
        if (rs == 2 && g_rec) {
            int n = g_recLen;
            if (n >= REC_MAX) rs = 3;
            else { g_rec[n * 2] = (int16_t)(L * 32767.f); g_rec[n * 2 + 1] = (int16_t)(R * 32767.f); g_recLen = n + 1; }
        }
    }
    g_recState = rs;
    unsigned used = (unsigned)pos;
    g_ifrac = pos - used;
    __atomic_store_n(&g_ir, r + used, __ATOMIC_RELEASE);
}

bool pcRecStart() {
    if (g_recState == 1 || g_recState == 2) return true;
    if (!g_rec) g_rec = (int16_t*)malloc(sizeof(int16_t) * 2 * REC_MAX);
    if (!g_rec) return false;
    g_recLen = 0; g_recState = 1;
    return true;
}
void pcRecStop() { if (g_recState == 1) g_recState = 0; else if (g_recState == 2) g_recState = 3; }
int pcRecState() { return g_recState; }
float pcRecSeconds() { return g_recLen / 44100.f; }
int pcRecTake(int16_t** data) {   // fertige Aufnahme abholen (Hauptthread), danach pcRecDone()
    if (g_recState != 3) return 0;
    *data = g_rec; return g_recLen;
}
void pcRecDone() { g_recState = 0; g_recLen = 0; }

#else
bool pcInStart() { return false; }
void pcInStop() {}
bool pcInActive() { return false; }
int pcInStatus() { return 0; }
void pcInMix(float*, int, float) {}
bool pcRecStart() { return false; }
void pcRecStop() {}
int pcRecState() { return 0; }
float pcRecSeconds() { return 0; }
int pcRecTake(int16_t**) { return 0; }
void pcRecDone() {}
void usbAudioTestTone(int) {}
bool usbAudioStart() { return false; }
void usbAudioStop() {}
bool usbAudioActive() { return false; }
int usbAudioStatus() { return 0; }
void usbAudioPush(const float*, int) {}
#endif
