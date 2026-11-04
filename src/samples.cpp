#include "samples.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

static Sample g_pool[MAX_SAMPLES];
static int g_used = 0;               // hoechster belegter Platz + 1 (ohne Vorhoer-Platz)
static long g_total = 0;             // Frames im Pool
static const long MAX_TOTAL = 64L * 1024 * 1024;   // max. 64M Frames (~256 MB) im Pool
static const int MAX_LEN = 60 * 48000;             // max. 60 s pro Sample
static char g_err[96] = "";
static char g_dir[600] = "";
static char (*g_lib)[96] = nullptr;
static int g_libN = 0, g_libSkip = 0;

static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static bool loadWav(const char* path, Sample& s, long& total) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); snprintf(g_err, sizeof g_err, "kein RIFF/WAVE"); return false; }
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
    if (!ok) { snprintf(g_err, sizeof g_err, "Format %d / %d Bit nicht unterstuetzt", fmt, bits); free(raw); return false; }
    int bps = bits / 8;
    int frames = (int)(rawLen / (uint32_t)(bps * ch));
    if (frames > MAX_LEN) frames = MAX_LEN;
    if (frames < 1 || total + frames > MAX_TOTAL) { snprintf(g_err, sizeof g_err, frames < 1 ? "leer" : "Speicher voll"); free(raw); return false; }
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
    total += frames;
    return true;
}

static bool isWav(const char* n) {
    size_t l = strlen(n);
    return l > 4 && !strcasecmp(n + l - 4, ".wav") && n[0] != '.';
}

static bool isAudioOther(const char* n) {
    static const char* const ext[] = {".mp3", ".aif", ".aiff", ".flac", ".ogg", ".m4a", ".wma", 0};
    size_t l = strlen(n);
    for (int i = 0; ext[i]; i++) { size_t e = strlen(ext[i]); if (l > e && !strcasecmp(n + l - e, ext[i])) return true; }
    return false;
}
static void scan(const char* root, const char* sub, int depth) {
    char path[1200];
    snprintf(path, sizeof path, "%s%s%s", root, sub[0] ? "/" : "", sub);
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) && g_libN < MAX_LIB) {
        if (e->d_name[0] == '.') continue;
        char rel[400], full[1700];
        snprintf(rel, sizeof rel, "%s%s%s", sub, sub[0] ? "/" : "", e->d_name);
        snprintf(full, sizeof full, "%s/%s", root, rel);
        struct stat st;
        if (stat(full, &st)) continue;
        if (S_ISDIR(st.st_mode)) { if (depth < SAMPLE_DEPTH && strcmp(rel, "rec")) scan(root, rel, depth + 1); }   // rec/ = WAV-Aufnahmen
        else if (isWav(e->d_name)) {
            size_t l = strlen(rel);
            if (l - 4 >= sizeof g_lib[0]) { g_libSkip++; printf("Sample-Pfad zu lang: %s\n", rel); continue; }
            snprintf(g_lib[g_libN], sizeof g_lib[0], "%.*s", (int)(l - 4), rel);
            g_libN++;
        } else if (isAudioOther(e->d_name)) g_libSkip++;
    }
    closedir(d);
}
static int cmpName(const void* a, const void* b) { return strcasecmp((const char*)a, (const char*)b); }

int libScan(const char* dir) {
    if (!g_lib) g_lib = (char(*)[96])malloc(sizeof(*g_lib) * MAX_LIB);
    if (!g_lib) return 0;
    snprintf(g_dir, sizeof g_dir, "%s", dir);
    g_libN = 0; g_libSkip = 0;
    scan(dir, "", 0);
    qsort(g_lib, g_libN, sizeof g_lib[0], cmpName);
    return g_libN;
}
int libCount() { return g_libN; }
const char* libName(int i) { return (i >= 0 && i < g_libN) ? g_lib[i] : ""; }
int libFind(const char* name) {
    int lo = 0, hi = g_libN - 1;
    while (lo <= hi) { int m = (lo + hi) / 2, c = strcasecmp(g_lib[m], name); if (!c) return m; if (c < 0) lo = m + 1; else hi = m - 1; }
    return -1;
}
int libSkipped() { return g_libSkip; }
const char* libDir() { return g_dir; }

bool sampleRead(const char* name, Sample& out) {
    memset(&out, 0, sizeof out);
    char full[800]; snprintf(full, sizeof full, "%s/%s.wav", g_dir, name);
    struct stat st;
    if (stat(full, &st)) { snprintf(g_err, sizeof g_err, "Datei fehlt"); return false; }
    long t = g_total;
    if (!loadWav(full, out, t)) return false;
    snprintf(out.name, sizeof out.name, "%s", name);
    out.mtime = (long)st.st_mtime; out.fsize = (long)st.st_size;
    return true;
}
void poolSwap(int slot, Sample& s) {
    if (slot < 0 || slot >= MAX_SAMPLES) return;
    Sample old = g_pool[slot];
    g_pool[slot] = s;
    s = old;
    g_total += g_pool[slot].len - s.len;
    g_used = 0;
    for (int i = 0; i < PREVIEW_SLOT; i++) if (g_pool[i].data) g_used = i + 1;
}
void sampleFree(Sample& s) { free(s.data); memset(&s, 0, sizeof s); }
const char* sampleLastError() { return g_err; }

int sampleCount() { return g_used; }
const Sample* sampleGet(int i) { return (i >= 0 && i < MAX_SAMPLES && g_pool[i].data) ? &g_pool[i] : nullptr; }
const char* sampleName(int i) { return (i >= 0 && i < MAX_SAMPLES) ? g_pool[i].name : ""; }
int poolFind(const char* name) { for (int i = 0; i < PREVIEW_SLOT; i++) if (g_pool[i].data && !strcmp(g_pool[i].name, name)) return i; return -1; }
int poolFreeSlot() { for (int i = 0; i < PREVIEW_SLOT; i++) if (!g_pool[i].data) return i; return -1; }

int samplesLoad(const char* dir) {
    libScan(dir);
    int n = 0;
    for (int i = 0; i < g_libN && n < PREVIEW_SLOT; i++) {
        Sample s;
        if (!sampleRead(g_lib[i], s)) { printf("Sample uebersprungen: %s (%s)\n", g_lib[i], g_err); continue; }
        poolSwap(n++, s);
        sampleFree(s);
    }
    return n;
}

// alte Projekte (bis Nov. 2026): Sample-Nummer = Position in der Liste, die die damalige Version geladen hat.
// Nachbau des alten Scans: Ordner in Verzeichnisreihenfolge (readdir) bis zur Tiefe, Abbruch nach cap Dateien,
// dann alphabetisch (mit ".wav") sortiert, nur lesbare WAVs, hoechstens max.
static bool wavProbe(const char* full) {
    FILE* f = fopen(full, "rb");
    if (!f) return false;
    uint8_t h[12], ck[8];
    bool ok = false, fmtOk = false;
    if (fread(h, 1, 12, f) == 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVE", 4)) {
        while (fread(ck, 1, 8, f) == 8) {
            uint32_t sz = rd32(ck + 4);
            if (!memcmp(ck, "fmt ", 4)) {
                uint8_t b[40] = {0}; uint32_t r = sz < 40 ? sz : 40;
                if (fread(b, 1, r, f) != r) break;
                if (sz > r) fseek(f, sz - r, SEEK_CUR);
                int fmt = rd16(b), bits = rd16(b + 14);
                if (fmt == 0xFFFE && r >= 26) fmt = rd16(b + 24);
                fmtOk = (fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && bits == 32);
            } else if (!memcmp(ck, "data", 4)) { ok = fmtOk && sz > 0; break; }
            else fseek(f, sz + (sz & 1), SEEK_CUR);
        }
    }
    fclose(f);
    return ok;
}
static void legacyScan(const char* sub, int depth, int maxDepth, char (*tmp)[160], int& n, int cap) {
    char path[1200];
    snprintf(path, sizeof path, "%s%s%s", g_dir, sub[0] ? "/" : "", sub);
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d)) && n < cap) {
        if (e->d_name[0] == '.') continue;
        char rel[160], full[1400];
        snprintf(rel, sizeof rel, "%s%s%s", sub, sub[0] ? "/" : "", e->d_name);
        snprintf(full, sizeof full, "%s/%s", g_dir, rel);
        struct stat st;
        if (stat(full, &st)) continue;
        if (S_ISDIR(st.st_mode)) { if (depth < maxDepth && strcmp(rel, "rec")) legacyScan(rel, depth + 1, maxDepth, tmp, n, cap); }
        else if (isWav(e->d_name)) snprintf(tmp[n++], 160, "%s", rel);
    }
    closedir(d);
}
static int cmpLegacy(const void* a, const void* b) { return strcasecmp((const char*)a, (const char*)b); }
int libLegacyList(char (*out)[96], int max, int depth, int cap) {
    static char tmp[1024][160];
    if (cap > 1024) cap = 1024;
    int n = 0;
    legacyScan("", 0, depth, tmp, n, cap);
    qsort(tmp, n, sizeof tmp[0], cmpLegacy);
    int k = 0;
    for (int i = 0; i < n && k < max; i++) {
        char full[800]; snprintf(full, sizeof full, "%s/%s", g_dir, tmp[i]);
        if (!wavProbe(full)) continue;
        size_t l = strlen(tmp[i]);
        snprintf(out[k++], 96, "%.*s", (int)(l - 4), tmp[i]);
    }
    return k;
}
