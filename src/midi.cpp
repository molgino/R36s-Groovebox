#include "midi.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>

volatile uint32_t midiActivity = 0;

// ---------------------------------------------------------------- lock-freie Ringpuffer (1 Erzeuger, 1 Verbraucher)
struct Ring { MidiMsg buf[1024]; uint32_t head = 0, tail = 0; };
static Ring rIn, rOut;

static bool ringPush(Ring& r, const MidiMsg& m) {
    uint32_t h = __atomic_load_n(&r.head, __ATOMIC_RELAXED), t = __atomic_load_n(&r.tail, __ATOMIC_ACQUIRE);
    if (h - t >= 1024) return false;
    r.buf[h & 1023] = m;
    __atomic_store_n(&r.head, h + 1, __ATOMIC_RELEASE);
    return true;
}
static bool ringPeek(Ring& r, MidiMsg& m) {
    uint32_t t = __atomic_load_n(&r.tail, __ATOMIC_RELAXED), h = __atomic_load_n(&r.head, __ATOMIC_ACQUIRE);
    if (t == h) return false;
    m = r.buf[t & 1023];
    return true;
}
static void ringDrop(Ring& r) { __atomic_store_n(&r.tail, r.tail + 1, __ATOMIC_RELEASE); }
static bool ringPop(Ring& r, MidiMsg& m) { if (!ringPeek(r, m)) return false; ringDrop(r); return true; }

bool midiInPop(MidiMsg& m) { return ringPop(rIn, m); }
bool midiOutPush(const MidiMsg& m) { return ringPush(rOut, m); }
double midiNow() { return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency(); }

static SDL_mutex* infoMx = nullptr;
static char infoNames[256] = "";
static int infoCount = 0;
static const char* backend = "AUS";

static volatile int serialOpen = 0;

int midiPortInfo(char* buf, int n) {
    if (!infoMx) { snprintf(buf, n, "kein MIDI"); return 0; }
    SDL_LockMutex(infoMx);
    if (serialOpen) snprintf(buf, n, "PC (USB-SERIELL)%s%s", infoCount ? ", " : "", infoCount ? infoNames : "");
    else snprintf(buf, n, "%s", infoCount ? infoNames : "kein Geraet");
    int c = infoCount + (serialOpen ? 1 : 0);
    SDL_UnlockMutex(infoMx);
    return c;
}
const char* midiBackend() { return backend; }

static void setInfo(const char* names, int count) {
    SDL_LockMutex(infoMx);
    snprintf(infoNames, sizeof infoNames, "%s", names);
    infoCount = count;
    SDL_UnlockMutex(infoMx);
}

static void pushIn(uint8_t st, uint8_t d1, uint8_t d2) {
    MidiMsg m = {st, d1, d2, 0, 0};
    ringPush(rIn, m);
    if (st < 0xF8) midiActivity = SDL_GetTicks();
}

#ifdef WITH_MIDI
#include <alsa/asoundlib.h>
#include <poll.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

static void quietErr(const char*, int, const char*, int, const char*, ...) {}

static SDL_Thread* thread = nullptr;
static volatile bool running = false;
static snd_seq_t* seq = nullptr;
static int inPort = -1, outPort = -1, myClient = -1;

// ---- rawmidi-Fallback
struct RawDev { snd_rawmidi_t* in = nullptr; snd_rawmidi_t* out = nullptr; int card = -1; char name[40];
                uint8_t status = 0, d[2]; int cnt = 0; bool sysex = false; };
static RawDev raw[8];

static void rawParse(RawDev& r, uint8_t b) {
    if (b >= 0xF8) { pushIn(b, 0, 0); return; }
    if (b == 0xF0) { r.sysex = true; return; }
    if (b == 0xF7) { r.sysex = false; return; }
    if (r.sysex) return;
    if (b & 0x80) { r.status = b; r.cnt = 0; return; }
    if (!r.status || r.status >= 0xF0) return;
    r.d[r.cnt++] = b;
    int need = ((r.status & 0xF0) == 0xC0 || (r.status & 0xF0) == 0xD0) ? 1 : 2;
    if (r.cnt >= need) { pushIn(r.status, r.d[0], need > 1 ? r.d[1] : 0); r.cnt = 0; }
}

static void rawScan() {
    char names[256] = ""; int count = 0;
    for (int c = 0; c < 8; c++) {
        RawDev& r = raw[c];
        char dev[32];
        snprintf(dev, sizeof dev, "/dev/snd/midiC%dD0", c);
        struct stat stt;
        if (!r.in && !r.out && stat(dev, &stt) == 0) {
            char hw[16]; snprintf(hw, sizeof hw, "hw:%d,0", c);
            if (snd_rawmidi_open(&r.in, &r.out, hw, SND_RAWMIDI_NONBLOCK) < 0) {
                r.in = r.out = nullptr;
                if (snd_rawmidi_open(&r.in, nullptr, hw, SND_RAWMIDI_NONBLOCK) < 0) r.in = nullptr;
            }
            if (r.in || r.out) {
                char* nm = nullptr;
                snd_card_get_name(c, &nm);
                snprintf(r.name, sizeof r.name, "%s", nm ? nm : hw);
                free(nm);
                r.card = c; r.status = 0; r.cnt = 0; r.sysex = false;
                printf("MIDI (raw): %s verbunden\n", r.name);
            }
        }
        if (r.in || r.out) {
            if (count) strncat(names, ", ", sizeof names - strlen(names) - 1);
            strncat(names, r.name, sizeof names - strlen(names) - 1);
            count++;
        }
    }
    setInfo(names, count);
}
static void rawClose(RawDev& r) {
    if (r.in) snd_rawmidi_close(r.in);
    if (r.out) snd_rawmidi_close(r.out);
    r.in = r.out = nullptr;
    printf("MIDI (raw): %s getrennt\n", r.name);
}
// ---- USB-Seriell (PC-Modus ohne usb_f_midi): MIDI-Bytes ueber /dev/ttyGS0
static int serFd = -1;
static RawDev serDev;
static void serialCheck() {
    struct stat st;
    if (serFd >= 0 || stat("/dev/ttyGS0", &st) != 0) return;
    serFd = open("/dev/ttyGS0", O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serFd < 0) return;
    struct termios tio;
    if (tcgetattr(serFd, &tio) == 0) { cfmakeraw(&tio); cfsetspeed(&tio, B115200); tcsetattr(serFd, TCSANOW, &tio); }
    serDev.status = 0; serDev.cnt = 0; serDev.sysex = false;
    serialOpen = 1;
    printf("MIDI: USB-Seriell zum PC aktiv (/dev/ttyGS0)\n");
}
static void serialClose() {
    if (serFd >= 0) close(serFd);
    serFd = -1; serialOpen = 0;
    printf("MIDI: USB-Seriell geschlossen\n");
}
static void serialPoll() {
    if (serFd < 0) return;
    uint8_t b[256];
    int n;
    while ((n = (int)read(serFd, b, sizeof b)) > 0) for (int i = 0; i < n; i++) rawParse(serDev, b[i]);
    if (n < 0 && errno != EAGAIN && errno != EINTR && errno != EIO) serialClose();
    struct stat st;
    if (serFd >= 0 && stat("/dev/ttyGS0", &st) != 0) serialClose();
}
static void serialSend(const MidiMsg& m) {
    if (serFd < 0) return;
    uint8_t b[3] = {m.st, m.d1, m.d2};
    int n = m.st >= 0xF8 ? 1 : (((m.st & 0xF0) == 0xC0 || (m.st & 0xF0) == 0xD0) ? 2 : 3);
    if (write(serFd, b, n) < 0 && errno != EAGAIN && errno != EIO && errno != ESHUTDOWN) serialClose();
}

static void rawSend(const MidiMsg& m) {
    uint8_t b[3] = {m.st, m.d1, m.d2};
    int n = m.st >= 0xF8 ? 1 : (((m.st & 0xF0) == 0xC0 || (m.st & 0xF0) == 0xD0) ? 2 : 3);
    for (auto& r : raw) if (r.out && snd_rawmidi_write(r.out, b, n) == -ENODEV) rawClose(r);
}

// ---- Sequencer
static void seqScan() {
    snd_seq_client_info_t* ci; snd_seq_port_info_t* pi;
    snd_seq_client_info_alloca(&ci);
    snd_seq_port_info_alloca(&pi);
    char names[256] = ""; int count = 0;
    snd_seq_client_info_set_client(ci, -1);
    while (snd_seq_query_next_client(seq, ci) >= 0) {
        int c = snd_seq_client_info_get_client(ci);
        if (c == myClient || c == SND_SEQ_CLIENT_SYSTEM || c == 14) continue;  // 14 = Midi Through
        snd_seq_port_info_set_client(pi, c);
        snd_seq_port_info_set_port(pi, -1);
        bool any = false;
        while (snd_seq_query_next_port(seq, pi) >= 0) {
            unsigned caps = snd_seq_port_info_get_capability(pi);
            if (caps & SND_SEQ_PORT_CAP_NO_EXPORT) continue;
            int p = snd_seq_port_info_get_port(pi);
            if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_SUBS_READ)) { snd_seq_connect_from(seq, inPort, c, p); any = true; }
            if ((caps & SND_SEQ_PORT_CAP_WRITE) && (caps & SND_SEQ_PORT_CAP_SUBS_WRITE)) { snd_seq_connect_to(seq, outPort, c, p); any = true; }
        }
        if (any) {
            if (count) strncat(names, ", ", sizeof names - strlen(names) - 1);
            strncat(names, snd_seq_client_info_get_name(ci), sizeof names - strlen(names) - 1);
            count++;
        }
    }
    setInfo(names, count);
}

static bool seqHandle(snd_seq_event_t* ev) {
    switch (ev->type) {
    case SND_SEQ_EVENT_NOTEON: pushIn(0x90 | (ev->data.note.channel & 15), ev->data.note.note, ev->data.note.velocity); break;
    case SND_SEQ_EVENT_NOTEOFF: pushIn(0x80 | (ev->data.note.channel & 15), ev->data.note.note, 0); break;
    case SND_SEQ_EVENT_CONTROLLER: pushIn(0xB0 | (ev->data.control.channel & 15), ev->data.control.param & 127, ev->data.control.value & 127); break;
    case SND_SEQ_EVENT_CLOCK: pushIn(0xF8, 0, 0); break;
    case SND_SEQ_EVENT_START: pushIn(0xFA, 0, 0); break;
    case SND_SEQ_EVENT_CONTINUE: pushIn(0xFB, 0, 0); break;
    case SND_SEQ_EVENT_STOP: pushIn(0xFC, 0, 0); break;
    case SND_SEQ_EVENT_CLIENT_START: case SND_SEQ_EVENT_PORT_START:
    case SND_SEQ_EVENT_CLIENT_EXIT: case SND_SEQ_EVENT_PORT_EXIT:
        return true;  // neu scannen
    default: break;
    }
    return false;
}

static void seqSend(const MidiMsg& m) {
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, outPort);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    int ch = m.st & 15;
    switch (m.st & 0xF0) {
    case 0x90: snd_seq_ev_set_noteon(&ev, ch, m.d1, m.d2); break;
    case 0x80: snd_seq_ev_set_noteoff(&ev, ch, m.d1, 0); break;
    case 0xB0: snd_seq_ev_set_controller(&ev, ch, m.d1, m.d2); break;
    case 0xF0:
        if (m.st == 0xF8) ev.type = SND_SEQ_EVENT_CLOCK;
        else if (m.st == 0xFA) ev.type = SND_SEQ_EVENT_START;
        else if (m.st == 0xFB) ev.type = SND_SEQ_EVENT_CONTINUE;
        else if (m.st == 0xFC) ev.type = SND_SEQ_EVENT_STOP;
        else return;
        break;
    default: return;
    }
    snd_seq_event_output_direct(seq, &ev);
}

static int midiThread(void*) {
    double lastScan = 0;
    bool rescan = true;
    while (running) {
        double now = midiNow();
        if (seq) {
            snd_seq_event_t* ev = nullptr;
            while (snd_seq_event_input(seq, &ev) >= 0 && ev) { if (seqHandle(ev)) rescan = true; ev = nullptr; }
        } else {
            for (auto& r : raw) {
                if (!r.in) continue;
                uint8_t b[256];
                int n;
                while ((n = (int)snd_rawmidi_read(r.in, b, sizeof b)) > 0) for (int i = 0; i < n; i++) rawParse(r, b[i]);
                if (n == -ENODEV) { rawClose(r); rescan = true; }
            }
        }
        serialPoll();
        MidiMsg m;
        while (ringPeek(rOut, m) && m.due <= now + 0.0003) {
            if (seq) seqSend(m); else rawSend(m);
            serialSend(m);
            ringDrop(rOut);
        }
        if (rescan || now - lastScan > 3.0) {
            if (seq) seqScan(); else rawScan();
            serialCheck();
            lastScan = now; rescan = false;
        }
        SDL_Delay(1);
    }
    return 0;
}

bool midiStart() {
    infoMx = SDL_CreateMutex();
    snd_lib_error_set_handler(quietErr);
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) >= 0) {
        snd_seq_set_client_name(seq, "Groovebox");
        myClient = snd_seq_client_id(seq);
        inPort = snd_seq_create_simple_port(seq, "Groovebox In", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                            SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
        outPort = snd_seq_create_simple_port(seq, "Groovebox Out", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                             SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
        snd_seq_connect_from(seq, inPort, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);
        backend = "SEQ";
        printf("MIDI: ALSA-Sequencer aktiv (Client %d)\n", myClient);
    } else {
        seq = nullptr;
        backend = "RAW";
        printf("MIDI: kein ALSA-Sequencer (/dev/snd/seq) - nutze rawmidi\n");
    }
    running = true;
    thread = SDL_CreateThread(midiThread, "midi", nullptr);
    return thread != nullptr;
}

void midiStop() {
    if (!running) return;
    running = false;
    SDL_WaitThread(thread, nullptr);
    if (seq) snd_seq_close(seq);
    for (auto& r : raw) if (r.in || r.out) rawClose(r);
    if (serFd >= 0) serialClose();
}

#else  // ohne ALSA

bool midiStart() { infoMx = SDL_CreateMutex(); return false; }
void midiStop() {}

#endif
