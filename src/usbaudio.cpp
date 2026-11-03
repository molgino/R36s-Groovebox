// USB-Audio zum PC (Groovebox-OS, UAC2-Gadget) als ZWEITER Ausgang:
// Der Takt kommt weiter von der internen Soundkarte. Der Audio-Callback legt jede Mischung zusaetzlich
// in einen Ringpuffer, ein eigener Thread schreibt sie nicht-blockierend auf das Gadget.
// Liest der PC gerade nicht mit, laeuft der Puffer ueber und wird verworfen - der Sequencer bleibt nie stehen.
#include "usbaudio.h"
#include <stdio.h>
#include <string.h>
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
    if (snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, 44100, 1, 40000) < 0) {
        printf("USB-Audio: Format nicht moeglich\n");
        snd_pcm_close(pcm); g_active = false; return 0;
    }
    printf("USB-Audio zum PC aktiv (%s)\n", g_dev);
    Uint32 lastOk = 0, statAt = SDL_GetTicks();
    unsigned long wrote = 0, full = 0, dropped = 0, errs = 0; int lastErr = 0;
    while (g_run) {
        if (SDL_GetTicks() - statAt >= 10000) {   // alle 10 s ins Log: kommt etwas beim PC an?
            snd_pcm_sframes_t av = snd_pcm_avail(pcm);
            printf("USB-Audio: %lu Frames an PC, Pegel %d%%, voll %lux, verworfen %lu, Fehler %lu%s%s, Zustand %s, frei %ld\n",
                   wrote, g_peak * 100 / 32767, full, dropped, errs, lastErr ? " letzter: " : "", lastErr ? snd_strerror(lastErr) : "",
                   snd_pcm_state_name(snd_pcm_state(pcm)), (long)av);
            g_peak = 0;
            fflush(stdout);
            wrote = full = dropped = errs = 0; lastErr = 0; statAt = SDL_GetTicks();
        }
        unsigned w = g_w, r = g_r, fill = w - r;
        if (fill > MAXLAT) { dropped += fill - MAXLAT / 2; r = w - MAXLAT / 2; g_r = r; fill = MAXLAT / 2; }   // PC liest nicht mit -> Altes verwerfen
        if (fill < PERIOD) { SDL_Delay(2); continue; }
        unsigned chunk = fill;
        unsigned toEnd = RING - (r & RMASK);
        if (chunk > toEnd) chunk = toEnd;
        snd_pcm_sframes_t n = snd_pcm_writei(pcm, &g_ring[(r & RMASK) * 2], chunk);
        if (n > 0) { g_r = r + (unsigned)n; wrote += (unsigned long)n; lastOk = SDL_GetTicks(); g_status = 2; }
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

#else
void usbAudioTestTone(int) {}
bool usbAudioStart() { return false; }
void usbAudioStop() {}
bool usbAudioActive() { return false; }
int usbAudioStatus() { return 0; }
void usbAudioPush(const float*, int) {}
#endif
