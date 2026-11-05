// USB-Audio zum PC (Groovebox-OS, UAC2-Gadget) als ZWEITER Ausgang:
// Der Takt kommt weiter von der internen Soundkarte. Der Audio-Callback legt jede Mischung zusaetzlich
// in einen Ringpuffer, ein eigener Thread schreibt sie nicht-blockierend auf das Gadget.
// Liest der PC gerade nicht mit, laeuft der Puffer ueber und wird verworfen - der Sequencer bleibt nie stehen.
#include "usbaudio.h"
#include "pcm.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>

#ifdef WITH_MIDI
#include <alsa/asoundlib.h>
#include <SDL.h>

enum { RING = 16384, RMASK = RING - 1, MAXLAT = 8192 };   // Frames
static float g_ring[RING * 2];                 // Float: erst beim Senden auf 24 (bzw. 16) Bit mit Dither runter
static volatile unsigned g_w = 0, g_r = 0;     // Schreib-/Leseposition (Frames), nur je ein Thread schreibt
static volatile Uint64 g_tPush = 0;            // Zeitpunkt der letzten Uebergabe (fuer die Taktregelung)
static volatile int g_lastN = 256;             // Frames pro Uebergabe (Puffergroesse der internen Karte)
static volatile bool g_run = false, g_active = false;
static volatile int g_status = 0;              // 0 aus, 1 wartet auf PC, 2 PC liest
static SDL_Thread* g_thread = nullptr;
static char g_dev[32];
static volatile int g_peak = 0;          // groesster Betrag der uebergebenen Samples (seit letzter Statistik)
static volatile Uint32 g_toneUntil = 0;  // Testton bis zu diesem Zeitpunkt
static volatile float g_gain = 1.f;      // USB-Lautstaerke (unabhaengig von der internen)
void usbAudioSetGain(float g) { g_gain = g; }
static unsigned g_tonePh = 0;
void usbAudioTestTone(int ms) { g_toneUntil = SDL_GetTicks() + (Uint32)ms; }
static volatile int g_srcRate = 44100;   // Rate der Engine (= des Rings); der Audio-Ausgang kann z. B. 48 kHz liefern
void usbAudioSetEngineRate(int sr) { if (sr >= 8000 && sr <= 192000) g_srcRate = sr; }

// Sample-Format fuer das Gadget: 24 Bit bevorzugt (p_ssize/c_ssize = 3), sonst 32 oder 16 Bit.
// Liefert die Bytes je Sample (3/4/2) oder 0, wenn nichts geht.
static int openFormat(snd_pcm_t* pcm, unsigned rate, unsigned latUs) {
    static const snd_pcm_format_t fm[3] = {SND_PCM_FORMAT_S24_3LE, SND_PCM_FORMAT_S32_LE, SND_PCM_FORMAT_S16_LE};
    static const int by[3] = {3, 4, 2};
    for (int k = 0; k < 3; k++) {
        snd_pcm_hw_params_t* hw; snd_pcm_hw_params_alloca(&hw);
        if (snd_pcm_hw_params_any(pcm, hw) < 0 || snd_pcm_hw_params_test_format(pcm, hw, fm[k]) < 0) continue;
        if (snd_pcm_set_params(pcm, fm[k], SND_PCM_ACCESS_RW_INTERLEAVED, 2, rate, 0, latUs) == 0) return by[k];
    }
    return 0;
}
static inline void putSample(uint8_t* p, int bytes, float x, Dither& d) {
    if (bytes == 2) { int16_t v = (int16_t)pcmQuant(x, 16, d); memcpy(p, &v, 2); }
    else if (bytes == 3) put24(p, pcmQuant(x, 24, d));
    else { int32_t v = pcmQuant(x, 24, d) * 256; memcpy(p, &v, 4); }
}
static inline float getSample(const uint8_t* p, int bytes) {
    if (bytes == 2) { int16_t v; memcpy(&v, p, 2); return v * (1.f / 32768.f); }
    if (bytes == 3) return get24(p) * (1.f / 8388608.f);
    int32_t v; memcpy(&v, p, 4); return (float)v * (1.f / 2147483648.f);
}

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

static inline float hermite(const float* r, unsigned i, float t, int c, unsigned m = RMASK) {   // 4-Punkt, wie smpRead in der Engine
    float y0 = r[((i - 1) & m) * 2 + c], y1 = r[(i & m) * 2 + c], y2 = r[((i + 1) & m) * 2 + c], y3 = r[((i + 2) & m) * 2 + c];
    float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

// Abtastrate, die der PC gewaehlt hat (Gadget bietet 44,1 und 48 kHz an). u_audio meldet sie ueber die Regler
// "Playback Rate" (Groovebox -> PC) und "Capture Rate" (PC -> Groovebox); 0 = PC hat den Strom gerade nicht offen.
static int gadgetRate(bool toPc) {
    const char* c = strchr(g_dev, ':');
    if (!c) return 0;
    char card[16]; snprintf(card, sizeof card, "hw:%d", atoi(c + 1));
    snd_ctl_t* ctl;
    if (snd_ctl_open(&ctl, card, SND_CTL_NONBLOCK) < 0) return 0;
    snd_ctl_elem_value_t* v; snd_ctl_elem_value_alloca(&v);
    int r = 0;
    for (int k = 0; k < 2 && r <= 0; k++) {
        snd_ctl_elem_value_set_interface(v, k ? SND_CTL_ELEM_IFACE_MIXER : SND_CTL_ELEM_IFACE_PCM);
        snd_ctl_elem_value_set_name(v, toPc ? "Playback Rate" : "Capture Rate");
        if (snd_ctl_elem_read(ctl, v) >= 0) r = (int)snd_ctl_elem_value_get_integer(v, 0);
    }
    snd_ctl_close(ctl);
    return r;
}
// Rate fuer das Oeffnen: die vom PC gewaehlte, sonst 44,1 kHz, sonst was das Gadget gerade zulaesst
static int pickRate(snd_pcm_t* pcm, int want) {
    snd_pcm_hw_params_t* hw; snd_pcm_hw_params_alloca(&hw);
    if (snd_pcm_hw_params_any(pcm, hw) < 0) return 44100;
    unsigned lo = 0, hi = 0;
    snd_pcm_hw_params_get_rate_min(hw, &lo, nullptr);
    snd_pcm_hw_params_get_rate_max(hw, &hi, nullptr);
    if (want > 0 && (unsigned)want >= lo && (unsigned)want <= hi) return want;
    if (44100 >= lo && 44100 <= hi) return 44100;
    return lo ? (int)lo : 44100;
}
static volatile int g_rateOut = 0, g_rateIn = 0;   // aktuelle Raten am USB (0 = nicht offen)
int usbAudioRate() { return g_rateOut ? g_rateOut : g_rateIn; }

static int usbThread(void*) {
    {   // knapp unter dem Audio-Thread (70): sonst kann die Oberflaeche den Sender verdraengen -> Unterlauf = Knacken beim PC
        struct sched_param sp; sp.sched_priority = 60;
        printf("USB-Audio: Echtzeit-Prioritaet %s\n", pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) == 0 ? "ja" : "nein");
    }
  while (g_run) {   // aeussere Schleife: neu oeffnen, wenn der PC die Abtastrate wechselt
    snd_pcm_t* pcm = nullptr;
    int e = snd_pcm_open(&pcm, g_dev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (e < 0) {
        static char lastErr[64] = "";
        if (strcmp(lastErr, g_dev)) { printf("USB-Audio: %s nicht zu oeffnen (%s)\n", g_dev, snd_strerror(e)); snprintf(lastErr, sizeof lastErr, "%s", g_dev); }
        g_active = false; return 0;
    }
    const int rate = pickRate(pcm, gadgetRate(true));
    const int bps = openFormat(pcm, (unsigned)rate, 40000);
    if (!bps) {
        printf("USB-Audio: Format nicht moeglich (%d Hz)\n", rate);
        snd_pcm_close(pcm); g_active = false; return 0;
    }
    g_rateOut = rate;
    const int src = g_srcRate;
    printf("USB-Audio zum PC aktiv (%s, %d Hz, %d Bit, Engine %d Hz)\n", g_dev, rate, bps == 2 ? 16 : 24, src);
    const double base = (double)src / rate;   // Ring-Frames (Engine-Rate) je USB-Frame
    Uint32 rateCheckAt = SDL_GetTicks();
    // Taktausgleich: interne Soundkarte und USB-Host laufen mit leicht unterschiedlicher Uhr. Wir lesen den Ring mit
    // fein geregelter Rate (kubische Interpolation). Der Fuellstand springt mit jeder Uebergabe um eine Puffergroesse;
    // dieser Saegezahn wird ueber den Zeitpunkt der letzten Uebergabe herausgerechnet, sonst zittert die Regelung.
    enum { OUT = 256, TARGET = 2048 };
    static uint8_t outb[OUT * 2 * 4];
    const int fb = bps * 2;            // Bytes je Frame
    int pendOff = 0, pendLen = 0;      // noch nicht geschriebener Teil von outb
    double frac = 0, ratio = base, fillAvg = TARGET;
    bool primed = false; int fadeIn = 0;   // nach Start/Neusynchronisation weich einblenden
    const double pf = (double)SDL_GetPerformanceFrequency();
    Dither dith;
    Uint32 lastOk = 0, statAt = SDL_GetTicks();
    unsigned long wrote = 0, full = 0, dropped = 0, errs = 0, starve = 0; int lastErr = 0;
    double ppmMin = 1e9, ppmMax = -1e9;
    while (g_run) {
        if (SDL_GetTicks() - rateCheckAt >= 500) {   // PC hat eine andere Rate gewaehlt?
            rateCheckAt = SDL_GetTicks();
            int r = gadgetRate(true);
            if (r > 0 && r != rate) { printf("USB-Audio: PC wechselt auf %d Hz\n", r); break; }
            if (g_srcRate != src) { printf("USB-Audio: Engine-Rate jetzt %d Hz\n", g_srcRate); break; }
        }
        if (SDL_GetTicks() - statAt >= 10000) {   // alle 10 s ins Log: kommt etwas beim PC an?
            snd_pcm_sframes_t av = snd_pcm_avail(pcm);
            printf("USB-Audio: %lu Frames an PC, Pegel %d%%, Takt %+.0f ppm (%+.0f..%+.0f), Puffer %.0f, voll %lux, leer %lux, verworfen %lu, Fehler %lu%s%s, Zustand %s, frei %ld\n",
                   wrote, g_peak * 100 / 32767, (ratio / base - 1.0) * 1e6, ppmMin > 1e8 ? 0 : ppmMin, ppmMax < -1e8 ? 0 : ppmMax, fillAvg, full, starve, dropped, errs,
                   lastErr ? " letzter: " : "", lastErr ? snd_strerror(lastErr) : "", snd_pcm_state_name(snd_pcm_state(pcm)), (long)av);
            g_peak = 0;
            fflush(stdout);
            wrote = full = dropped = errs = starve = 0; lastErr = 0; statAt = SDL_GetTicks(); ppmMin = 1e9; ppmMax = -1e9;
        }
        if (pendLen == 0) {   // naechsten Block erzeugen
            Uint64 tp; unsigned w;
            do { tp = g_tPush; w = __atomic_load_n(&g_w, __ATOMIC_ACQUIRE); } while (tp != g_tPush);
            unsigned r = g_r, fill = w - r;
            if (fill > MAXLAT) {   // PC hat nicht mitgelesen -> Altes verwerfen, neu anlaufen
                dropped += fill - TARGET; r = w - TARGET; g_r = r; fill = TARGET; frac = 0; primed = false;
            }
            if (!primed) {         // erst ab Ziel-Fuellstand starten, Regelung zuruecksetzen
                if (fill < TARGET) { SDL_Delay(1); continue; }
                primed = true; fillAvg = TARGET; ratio = base; fadeIn = 0;
            }
            if (fill < (unsigned)(OUT * ratio) + 4) { starve++; SDL_Delay(1); continue; }   // interne Karte haengt
            double dt = (double)(SDL_GetPerformanceCounter() - tp) / pf;
            if (dt > 0.05) dt = 0.05;
            double est = (double)fill - frac + dt * src - g_lastN * 0.5;       // stetiger Fuellstand
            fillAvg += 0.01 * (est - fillAvg);                                 // ~0,6 s Mittelung
            double want = base * (1.0 + 1e-6 * (fillAvg - TARGET));            // 100 Frames zu voll -> 100 ppm schneller
            if (want > base * 1.002) want = base * 1.002;
            if (want < base * 0.998) want = base * 0.998;
            ratio += 0.02 * (want - ratio);
            double ppm = (ratio / base - 1.0) * 1e6; if (ppm < ppmMin) ppmMin = ppm; if (ppm > ppmMax) ppmMax = ppm;
            double pos = frac;
            for (int i = 0; i < OUT; i++, pos += ratio) {
                unsigned ip = (unsigned)pos;
                const int fadeLen = rate / 100;   // 10 ms
                float t = (float)(pos - ip), g = fadeIn < fadeLen ? fadeIn++ / (float)fadeLen : 1.f;
                for (int c = 0; c < 2; c++)   // TPDF-Dither (+-1 LSB) gegen Quantisierungs-Verzerrung bei leisen Stellen
                    putSample(&outb[i * fb + c * bps], bps, hermite(g_ring, r + ip, t, c) * g, dith);
            }
            unsigned used = (unsigned)pos;
            frac = pos - used;
            __atomic_store_n(&g_r, r + used, __ATOMIC_RELEASE);
            pendOff = 0; pendLen = OUT;
        }
        snd_pcm_sframes_t n = snd_pcm_writei(pcm, &outb[pendOff * fb], (snd_pcm_uframes_t)pendLen);
        if (n > 0) { pendOff += (int)n; pendLen -= (int)n; wrote += (unsigned long)n; lastOk = SDL_GetTicks(); g_status = 2; }
        else if (n == -EAGAIN) { full++; snd_pcm_wait(pcm, 20); }
        else if (n == -EPIPE || n == -ESTRPIPE) { errs++; lastErr = (int)n; snd_pcm_prepare(pcm); fadeIn = 0; }
        else { errs++; lastErr = (int)n; SDL_Delay(50); snd_pcm_prepare(pcm); fadeIn = 0; }
        if (SDL_GetTicks() - lastOk > 500) { g_status = 1; primed = false; }   // PC liest nicht: beim Wiederkommen neu einpendeln
    }
    snd_pcm_drop(pcm);
    snd_pcm_close(pcm);
    g_rateOut = 0;
  }
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
    float gain = g_gain, pk = 0.f;
    const int sr = g_srcRate;
    for (int i = 0; i < frames; i++) {
        unsigned k = ((w + i) & RMASK) * 2;
        for (int c = 0; c < 2; c++) {
            float x = tone ? 0.5f * sinf((float)g_tonePh * (6.2831853f * 1000.f / sr)) : st[i * 2 + c] * gain;
            g_ring[k + c] = x;
            float a = fabsf(x);
            if (a > pk) pk = a;
        }
        if (tone) g_tonePh = (g_tonePh + 1) % (unsigned)sr;
    }
    int ipk = (int)(fminf(pk, 1.f) * 32767.f);
    if (ipk > g_peak) g_peak = ipk;
    g_lastN = frames;
    __atomic_store_n(&g_w, w + (unsigned)frames, __ATOMIC_RELEASE);
    g_tPush = SDL_GetPerformanceCounter();
}

// ---------------------------------------------------------------- PC -> R36S (Gadget-Aufnahme-PCM)
// Ein Thread liest, was der PC auf "Synth City" abspielt; der Audio-Callback mischt es mit geregelter Rate
// (Taktausgleich wie oben) in den Ausgang und kann es als Sample aufnehmen.
enum { IRING = 16384, IMASK = IRING - 1, ITARGET = 2048 };
static float g_iring[IRING * 2];          // Float: 24 Bit vom PC bleiben erhalten
static volatile unsigned g_iw = 0, g_ir = 0;
static volatile bool g_irun = false, g_iactive = false;
static volatile int g_istatus = 0;
static SDL_Thread* g_ithread = nullptr;
static double g_ifrac = 0, g_iratio = 1.0, g_ifill = ITARGET;
static bool g_iprimed = false;
static volatile double g_ibase = 1.0;     // USB-Frames je Engine-Frame (48 kHz vom PC, Engine 44,1 kHz: 1,088)
static volatile int g_ireset = 0;         // Rate gewechselt: Mischer faengt neu an
static float* g_rec = nullptr;            // Aufnahme in Float (Stereo), Rate = Engine-Rate
static volatile int g_recLen = 0, g_recState = 0;   // 0 aus, 1 wartet auf Signal, 2 nimmt auf, 3 fertig
static volatile int g_recRate = 44100;
enum { REC_SEC = 60 };
static int g_recAlloc = 0;                // Frames im Puffer

static int pcInThread(void*) {
  while (g_irun) {   // neu oeffnen, wenn der PC die Abtastrate wechselt
    snd_pcm_t* pcm = nullptr;
    int e = snd_pcm_open(&pcm, g_dev, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if (e < 0) { printf("PC-Audio: %s nicht zu oeffnen (%s)\n", g_dev, snd_strerror(e)); g_iactive = false; return 0; }
    const int rate = pickRate(pcm, gadgetRate(false));
    const int bps = openFormat(pcm, (unsigned)rate, 30000);
    if (!bps) {
        printf("PC-Audio: Format nicht moeglich (%d Hz)\n", rate); snd_pcm_close(pcm); g_iactive = false; return 0;
    }
    const int src = g_srcRate;
    g_ibase = (double)rate / src; g_ireset = 1; g_rateIn = rate;
    printf("PC-Audio vom PC aktiv (%s, %d Hz, %d Bit)\n", g_dev, rate, bps == 2 ? 16 : 24);
    static uint8_t tmp[256 * 2 * 4];
    Uint32 lastOk = 0, statAt = SDL_GetTicks(), rateCheckAt = SDL_GetTicks();
    unsigned long got = 0;
    snd_pcm_start(pcm);
    while (g_irun) {
        if (SDL_GetTicks() - rateCheckAt >= 500) {   // PC hat eine andere Rate gewaehlt?
            rateCheckAt = SDL_GetTicks();
            int r = gadgetRate(false);
            if (r > 0 && r != rate) { printf("PC-Audio: PC wechselt auf %d Hz\n", r); break; }
            if (g_srcRate != src) break;   // Engine-Rate gewechselt: Verhaeltnis neu
        }
        snd_pcm_sframes_t n = snd_pcm_readi(pcm, tmp, 256);
        if (n > 0) {
            unsigned w = g_iw;
            if ((w - g_ir) + (unsigned)n < IRING - 1) {
                for (int i = 0; i < n; i++) {
                    unsigned k = ((w + i) & IMASK) * 2;
                    g_iring[k] = getSample(&tmp[(i * 2) * bps], bps); g_iring[k + 1] = getSample(&tmp[(i * 2 + 1) * bps], bps);
                }
                __atomic_store_n(&g_iw, w + (unsigned)n, __ATOMIC_RELEASE);
            }
            got += (unsigned long)n; lastOk = SDL_GetTicks(); g_istatus = 2;
        } else if (n == -EAGAIN) SDL_Delay(2);
        else if (n == -EPIPE || n == -ESTRPIPE) { snd_pcm_prepare(pcm); snd_pcm_start(pcm); }
        else { SDL_Delay(50); snd_pcm_prepare(pcm); snd_pcm_start(pcm); }
        if (SDL_GetTicks() - lastOk > 500) g_istatus = 1;
        if (SDL_GetTicks() - statAt >= 10000) {
            if (got) printf("PC-Audio: %lu Frames vom PC (%d Hz), Takt %+.0f ppm\n", got, rate, (g_iratio / g_ibase - 1.0) * 1e6);
            got = 0; statAt = SDL_GetTicks();
        }
    }
    snd_pcm_drop(pcm);
    snd_pcm_close(pcm);
    g_rateIn = 0;
  }
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
    const double base = g_ibase;
    if (g_ireset) { g_ireset = 0; g_iprimed = false; }
    if (!g_iprimed) { if (fill < ITARGET) return; g_iprimed = true; g_ifrac = 0; g_iratio = base; g_ifill = fill; }
    if (fill > IRING / 2) { r = w - ITARGET; fill = ITARGET; g_ifrac = 0; }   // zu viel aufgelaufen
    if (fill < (unsigned)(frames * base * 1.01) + 4) { g_iprimed = false; return; }   // PC spielt nicht mehr (+4: Hermite liest voraus)
    g_ifill += 0.02 * ((double)fill - g_ifill);
    double want = base * (1.0 + 0.003 * ((g_ifill - ITARGET) / ITARGET));
    if (want > base * 1.003) want = base * 1.003;
    if (want < base * 0.997) want = base * 0.997;
    g_iratio += 0.05 * (want - g_iratio);
    double pos = g_ifrac;
    int rs = g_recState;
    const int recMax = g_recAlloc;
    for (int i = 0; i < frames; i++, pos += g_iratio) {
        unsigned ip = (unsigned)pos;
        float f = (float)(pos - ip);
        float L = hermite(g_iring, r + ip, f, 0, IMASK), R = hermite(g_iring, r + ip, f, 1, IMASK);   // kubisch statt linear
        st[i * 2] += L * gain;
        st[i * 2 + 1] += R * gain;
        if (rs == 1 && (fabsf(L) > 0.01f || fabsf(R) > 0.01f)) rs = 2;   // Aufnahme beginnt mit dem ersten Signal
        if (rs == 2 && g_rec) {
            int n = g_recLen;
            if (n >= recMax) rs = 3;
            else { g_rec[n * 2] = L; g_rec[n * 2 + 1] = R; g_recLen = n + 1; }
        }
    }
    g_recState = rs;
    unsigned used = (unsigned)pos;
    g_ifrac = pos - used;
    __atomic_store_n(&g_ir, r + used, __ATOMIC_RELEASE);
}

bool pcRecStart() {
    if (g_recState == 1 || g_recState == 2) return true;
    const int need = g_srcRate * REC_SEC;
    if (g_rec && g_recAlloc < need) { free(g_rec); g_rec = nullptr; g_recAlloc = 0; }
    if (!g_rec) { g_rec = (float*)malloc(sizeof(float) * 2 * (size_t)need); if (g_rec) g_recAlloc = need; }
    if (!g_rec) return false;
    g_recRate = g_srcRate; g_recLen = 0; g_recState = 1;
    return true;
}
void pcRecStop() { if (g_recState == 1) g_recState = 0; else if (g_recState == 2) g_recState = 3; }
int pcRecState() { return g_recState; }
float pcRecSeconds() { return g_recLen / (float)g_recRate; }
int pcRecRate() { return g_recRate; }
int pcRecTake(float** data) {   // fertige Aufnahme abholen (Hauptthread), danach pcRecDone()
    if (g_recState != 3) return 0;
    *data = g_rec; return g_recLen;
}
void pcRecDone() { g_recState = 0; g_recLen = 0; }

#else
void usbAudioSetGain(float) {}
bool pcInStart() { return false; }
void pcInStop() {}
bool pcInActive() { return false; }
int pcInStatus() { return 0; }
void pcInMix(float*, int, float) {}
bool pcRecStart() { return false; }
void pcRecStop() {}
int pcRecState() { return 0; }
float pcRecSeconds() { return 0; }
int pcRecTake(float**) { return 0; }
int pcRecRate() { return 44100; }
void usbAudioSetEngineRate(int) {}
void pcRecDone() {}
void usbAudioTestTone(int) {}
bool usbAudioStart() { return false; }
int usbAudioRate() { return 0; }
void usbAudioStop() {}
bool usbAudioActive() { return false; }
int usbAudioStatus() { return 0; }
void usbAudioPush(const float*, int) {}
#endif
