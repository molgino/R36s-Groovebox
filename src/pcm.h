// PCM-Hilfen fuer alles, was die Groovebox verlaesst (WAV-Dateien, USB-Audio): Float -> 16/24 Bit, WAV-Kopf
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>

// TPDF-Dither (Summe zweier Gleichverteilungen, +-1 LSB): Quantisierungsfehler wird zu leisem Rauschen statt Verzerrung
struct Dither {
    uint32_t s = 22222;
    inline float next() {
        s = s * 1664525u + 1013904223u; float a = (s >> 8) * (1.f / 16777216.f);
        s = s * 1664525u + 1013904223u; float b = (s >> 8) * (1.f / 16777216.f);
        return a - b;
    }
};

// Float (-1..1) -> Ganzzahl mit `bits` Bit, gerundet, mit Dither, begrenzt
static inline int32_t pcmQuant(float x, int bits, Dither& d) {
    const float full = bits >= 24 ? 8388607.f : 32767.f;
    float v = x * full + d.next();
    if (v > full) v = full;
    if (v < -full - 1.f) v = -full - 1.f;
    return (int32_t)lrintf(v);
}
static inline void put24(uint8_t* p, int32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); }
static inline int32_t get24(const uint8_t* p) { return (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8; }

// Float-Stereo/Mono -> 24-Bit-PCM (3 Byte je Sample, little endian); n = Anzahl Samples (nicht Frames)
static inline void pcmTo24(uint8_t* out, const float* in, int n, Dither& d) {
    for (int i = 0; i < n; i++) put24(out + i * 3, pcmQuant(in[i], 24, d));
}

// 44-Byte-WAV-Kopf (PCM) fuer `frames` Frames
static inline void wavHeader(uint8_t* h, int sr, int ch, int bits, uint32_t frames) {
    auto p32 = [&](int o, uint32_t v) { h[o] = v & 255; h[o + 1] = (v >> 8) & 255; h[o + 2] = (v >> 16) & 255; h[o + 3] = v >> 24; };
    auto p16 = [&](int o, uint32_t v) { h[o] = v & 255; h[o + 1] = (v >> 8) & 255; };
    uint32_t ba = (uint32_t)(ch * bits / 8), bytes = frames * ba;
    memcpy(h, "RIFF", 4); p32(4, 36 + bytes); memcpy(h + 8, "WAVEfmt ", 8); p32(16, 16); p16(20, 1); p16(22, (uint32_t)ch);
    p32(24, (uint32_t)sr); p32(28, (uint32_t)sr * ba); p16(32, ba); p16(34, (uint32_t)bits); memcpy(h + 36, "data", 4); p32(40, bytes);
}
