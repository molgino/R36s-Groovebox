#include "samples.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

static Sample g_bank[2][MAX_SAMPLES];
static int g_cnt[2] = {0, 0};
static long g_tot[2] = {0, 0};
static int g_cur = 0;                 // aktive Bank, die andere wird beim Neuladen befuellt
static Sample* g_s = g_bank[0];
static int g_n = 0;
static const long MAX_TOTAL = 48L * 1024 * 1024;   // max. 48M Frames (~190 MB) insgesamt
static const int MAX_LEN = 60 * 48000;             // max. 60 s pro Sample

static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static bool loadWav(const char* path, Sample& s, long& g_total) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return false; }
    int fmt = 0, ch = 0, bits = 0, rate = 0;
    uint8_t* raw = nullptr; uint32_t rawLen = 0;
    uint8_t ck[8];
    while (fread(ck, 1, 8, f) == 8) {
        uint32_t sz = rd32(ck + 4);
        if (!memcmp(ck, "fmt ", 4)) {
            uint8_t b[64] = {0};
            uint32_t r = sz < 64 ? sz : 64;
            if (fread(b, 1, r, f) != r) break;
            if (sz > r) fseek(f, sz - r, SEEK_CUR);
            fmt = rd16(b); ch = rd16(b + 2); rate = (int)rd32(b + 4); bits = rd16(b + 14);
            if (fmt == 0xFFFE && r >= 26) fmt = rd16(b + 24);
        } else if (!memcmp(ck, "data", 4)) {
            rawLen = sz;
            raw = (uint8_t*)malloc(sz ? sz : 1);
            if (!raw) break;
            rawLen = (uint32_t)fread(raw, 1, sz, f);
            break;
        } else {
            fseek(f, sz + (sz & 1), SEEK_CUR);
        }
    }
    fclose(f);
    bool ok = raw && ch > 0 && rate > 0 && (fmt == 1 || fmt == 3) &&
              (bits == 8 || bits == 16 || bits == 24 || bits == 32);
    if (fmt == 3 && bits != 32) ok = false;
    if (!ok) { free(raw); return false; }
    int bps = bits / 8;
    int frames = (int)(rawLen / (uint32_t)(bps * ch));
    if (frames > MAX_LEN) frames = MAX_LEN;
    if (frames < 1 || g_total + frames > MAX_TOTAL) { free(raw); return false; }
    s.data = (float*)malloc(sizeof(float) * frames);
    if (!s.data) { free(raw); return false; }
    for (int i = 0; i < frames; i++) {
        float acc = 0;
        for (int c = 0; c < ch; c++) {
            const uint8_t* p = raw + ((size_t)i * ch + c) * bps;
            float v;
            if (fmt == 3) { float x; memcpy(&x, p, 4); v = x; }
            else if (bits == 8) v = (p[0] - 128) / 128.f;
            else if (bits == 16) v = (int16_t)rd16(p) / 32768.f;
            else if (bits == 24) v = (int32_t)((p[0] << 8) | (p[1] << 16) | ((uint32_t)p[2] << 24)) / 2147483648.f;
            else v = (int32_t)rd32(p) / 2147483648.f;
            acc += v;
        }
        s.data[i] = acc / ch;
    }
    free(raw);
    s.len = frames;
    s.rate = rate;
    g_total += frames;
    return true;
}

static bool isWav(const char* n) {
    size_t l = strlen(n);
    return l > 4 && !strcasecmp(n + l - 4, ".wav") && n[0] != '.';
}

struct Entry { char rel[160]; };
static int cmpEntry(const void* a, const void* b) { return strcasecmp(((const Entry*)a)->rel, ((const Entry*)b)->rel); }

static void scan(const char* root, const char* sub, Entry* out, int& n, int depth) {
    char path[512];
    snprintf(path, sizeof path, "%s%s%s", root, sub[0] ? "/" : "", sub);
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) && n < MAX_SAMPLES * 2) {
        if (e->d_name[0] == '.') continue;
        char rel[160], full[700];
        snprintf(rel, sizeof rel, "%s%s%s", sub, sub[0] ? "/" : "", e->d_name);
        snprintf(full, sizeof full, "%s/%s", root, rel);
        struct stat st;
        if (stat(full, &st)) continue;
        if (S_ISDIR(st.st_mode)) { if (depth < SAMPLE_DEPTH && strcmp(rel, "rec")) scan(root, rel, out, n, depth + 1); }   // rec/ = WAV-Aufnahmen (nur zum Herunterladen)
        else if (isWav(e->d_name)) { snprintf(out[n].rel, sizeof out[n].rel, "%s", rel); n++; }
    }
    closedir(d);
}

static int loadInto(const char* dir, int b) {
    static Entry ents[MAX_SAMPLES * 2];
    for (int i = 0; i < g_cnt[b]; i++) free(g_bank[b][i].data);
    g_cnt[b] = 0; g_tot[b] = 0;
    int n = 0;
    scan(dir, "", ents, n, 0);
    qsort(ents, n, sizeof(Entry), cmpEntry);
    for (int i = 0; i < n && g_cnt[b] < MAX_SAMPLES; i++) {
        char full[700];
        snprintf(full, sizeof full, "%s/%s", dir, ents[i].rel);
        Sample s;
        memset(&s, 0, sizeof s);
        if (!loadWav(full, s, g_tot[b])) { printf("Sample uebersprungen: %s\n", ents[i].rel); continue; }
        size_t l = strlen(ents[i].rel);
        if (l > 4) ents[i].rel[l - 4] = 0;   // ".wav" abschneiden
        snprintf(s.name, sizeof s.name, "%s", ents[i].rel);
        g_bank[b][g_cnt[b]++] = s;
    }
    return g_cnt[b];
}

int samplesLoad(const char* dir) {
    g_n = loadInto(dir, g_cur);
    g_s = g_bank[g_cur];
    return g_n;
}

int samplesPrepare(const char* dir) { return loadInto(dir, 1 - g_cur); }

void samplesSwap() {
    g_cur = 1 - g_cur;
    g_s = g_bank[g_cur];
    g_n = g_cnt[g_cur];
}

void samplesFreeBack() {
    int b = 1 - g_cur;
    for (int i = 0; i < g_cnt[b]; i++) free(g_bank[b][i].data);
    g_cnt[b] = 0; g_tot[b] = 0;
}

int sampleCount() { return g_n; }
const Sample* sampleGet(int i) { return (i >= 0 && i < g_n) ? &g_s[i] : nullptr; }
const char* sampleName(int i) { return (i >= 0 && i < g_n) ? g_s[i].name : "---"; }
