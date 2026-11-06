// Mini-Webserver: Samples und Projekte per Browser hochladen (PC-Modus: http://192.168.77.1)
// Ein Thread, eine Verbindung nach der anderen (Fernbedienung /ws: je Zuschauer ein eigener Thread, remote.cpp). Nur Dateien unter samples/ und projects/.
#include "web.h"
#include "samples.h"
#include "remote.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static char g_base[600];
static int g_port = 0;
static volatile int g_changed = 0;
static volatile Uint32 g_changedAt = 0;
static char g_last[96];
static volatile int g_lastSeq = 0;
static volatile int g_updateReady = 0;
static bool isOS() { struct stat st; return stat("/etc/groovebox-os", &st) == 0; }
bool webUpdateReady() { return g_updateReady != 0; }

extern const char WEB_PAGE[];
extern const char WEB_SCREEN[];

int webPort() { return g_port; }
bool webTakeChange(Uint32 now, Uint32 quietMs) {
    if (!g_changed || now - g_changedAt < quietMs) return false;
    g_changed = 0;
    return true;
}
int webLastEvent(char* out, int n) { snprintf(out, n, "%s", g_last); return g_lastSeq; }

extern int g_lang;   // Sprache der Oberflaeche (main.cpp)
static void note(const char* fmt, const char* a) {
    snprintf(g_last, sizeof g_last, fmt, a);
    g_lastSeq++;
}

// ---------------------------------------------------------------- Hilfen

static bool sendAll(int fd, const void* p, size_t n) {
    const char* c = (const char*)p;
    while (n) {
        ssize_t w = send(fd, c, n, MSG_NOSIGNAL);
        if (w <= 0) return false;
        c += w; n -= (size_t)w;
    }
    return true;
}

static void reply(int fd, int code, const char* type, const char* body, size_t len) {
    char h[256];
    const char* msg = code == 200 ? "OK" : code == 204 ? "No Content" : code == 404 ? "Not Found" : code == 413 ? "Too Large" : "Bad Request";
    int hl = snprintf(h, sizeof h, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                      "Cache-Control: no-store\r\nConnection: close\r\n\r\n", code, msg, type, len);
    sendAll(fd, h, (size_t)hl);
    if (len) sendAll(fd, body, len);
}
static void replyText(int fd, int code, const char* t) { reply(fd, code, "text/plain; charset=utf-8", t, strlen(t)); }

static int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
// Wert eines Query-Parameters (URL-dekodiert)
static bool queryGet(const char* q, const char* key, char* out, int n) {
    size_t kl = strlen(key);
    while (q && *q) {
        if (!strncmp(q, key, kl) && q[kl] == '=') {
            const char* v = q + kl + 1;
            int o = 0;
            while (*v && *v != '&' && o < n - 1) {
                if (*v == '%' && hexv(v[1]) >= 0 && hexv(v[2]) >= 0) { out[o++] = (char)(hexv(v[1]) * 16 + hexv(v[2])); v += 3; }
                else if (*v == '+') { out[o++] = ' '; v++; }
                else out[o++] = *v++;
            }
            out[o] = 0;
            return true;
        }
        q = strchr(q, '&');
        if (q) q++;
    }
    return false;
}

// Erlaubt: "name.wav" oder "ordner/unter/name.wav" (bis maxDepth Ebenen). Kein "..", kein Teil mit "." am Anfang,
// keine Steuerzeichen und keine Zeichen, die FAT/exFAT nicht kennt. Umlaute usw. (UTF-8) sind erlaubt (Ordner vom USB-Stick).
static bool safeRel(const char* p, int maxDepth) {
    if (!p[0] || p[0] == '/' || p[0] == '.') return false;
    int slashes = 0;
    for (const char* c = p; *c; c++) {
        unsigned char u = (unsigned char)*c;
        if (*c == '/') { slashes++; if (c[1] == '.' || c[1] == '/' || c[1] == 0) return false; continue; }
        if (u < 32 || u == 127 || strchr("\\:*?\"<>|", u)) return false;
    }
    return slashes <= maxDepth && !strstr(p, "..") && strlen(p) < 380;
}
static bool hasExt(const char* p, const char* ext) {
    size_t l = strlen(p), e = strlen(ext);
    return l > e && !strcasecmp(p + l - e, ext);
}
// root: "samples" oder "projects"
static bool rootDir(const char* root, char* out, int n) {
    if (strcmp(root, "samples") && strcmp(root, "projects")) return false;
    snprintf(out, n, "%s%s", g_base, root);
    return true;
}

// ---------------------------------------------------------------- JSON-Liste

struct Buf { char* p; size_t n, cap; };
static void bput(Buf& b, const char* s, size_t l) {
    if (b.n + l + 1 > b.cap) { b.cap = (b.n + l + 1) * 2; b.p = (char*)realloc(b.p, b.cap); }
    memcpy(b.p + b.n, s, l); b.n += l; b.p[b.n] = 0;
}
static void bstr(Buf& b, const char* s) { bput(b, s, strlen(s)); }
static void bjson(Buf& b, const char* s) {   // JSON-String mit Escapes
    bstr(b, "\"");
    for (; *s; s++) {
        char e[8];
        if (*s == '"' || *s == '\\') { e[0] = '\\'; e[1] = *s; bput(b, e, 2); }
        else if ((unsigned char)*s < 32) { snprintf(e, sizeof e, "\\u%04x", *s); bstr(b, e); }
        else bput(b, s, 1);
    }
    bstr(b, "\"");
}

static void listDir(Buf& b, const char* dir, const char* sub, const char* ext, bool& first, int depth, Buf* folders, bool& ffirst) {
    char path[800];
    snprintf(path, sizeof path, "%s%s%s", dir, sub[0] ? "/" : "", sub);
    DIR* d = opendir(path);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char rel[400], full[1300];
        snprintf(rel, sizeof rel, "%s%s%s", sub, sub[0] ? "/" : "", e->d_name);
        snprintf(full, sizeof full, "%s/%s", dir, rel);
        struct stat st;
        if (stat(full, &st)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < SAMPLE_DEPTH) {
                if (folders) { if (!ffirst) bstr(*folders, ","); ffirst = false; bjson(*folders, rel); }
                listDir(b, dir, rel, ext, first, depth + 1, folders, ffirst);
            }
        } else if (hasExt(e->d_name, ext)) {
            char num[64];
            if (!first) bstr(b, ",");
            first = false;
            bstr(b, "{\"path\":"); bjson(b, rel);
            snprintf(num, sizeof num, ",\"size\":%lld}", (long long)st.st_size);
            bstr(b, num);
        }
    }
    closedir(d);
}

// Anzahl Eintraege (WAV + Unterordner) eines Ordners, ohne stat (fuer die Ordnerzeilen im Explorer)
static int countEntries(const char* path, const char* ext) {
    DIR* d = opendir(path); if (!d) return 0;
    int n = 0; struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (e->d_type == DT_DIR || (e->d_type == DT_UNKNOWN && !strchr(e->d_name, '.'))) n++;
        else if (hasExt(e->d_name, ext)) n++;
    }
    closedir(d);
    return n;
}
// Ein Ordner: Unterordner (mit Anzahl) und Dateien (mit Groesse)
static void handleDir(int fd, const char* dir, const char* rel, const char* ext) {
    char path[1200];
    snprintf(path, sizeof path, "%s%s%s", dir, rel[0] ? "/" : "", rel);
    DIR* d = opendir(path);
    if (!d) { replyText(fd, 404, "Ordner nicht gefunden"); return; }
    Buf b = {nullptr, 0, 0}, f = {nullptr, 0, 0};
    bool fd1 = true, ff1 = true;
    bstr(b, "{\"path\":"); bjson(b, rel); bstr(b, ",\"dirs\":[");
    bstr(f, "\"files\":[");
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char full[1500]; snprintf(full, sizeof full, "%s/%s", path, e->d_name);
        struct stat st; if (stat(full, &st)) continue;
        char num[80];
        if (S_ISDIR(st.st_mode)) {
            if (!fd1) bstr(b, ",");
            fd1 = false;
            bstr(b, "{\"name\":"); bjson(b, e->d_name);
            snprintf(num, sizeof num, ",\"n\":%d}", countEntries(full, ext)); bstr(b, num);
        } else if (hasExt(e->d_name, ext)) {
            if (!ff1) bstr(f, ",");
            ff1 = false;
            bstr(f, "{\"name\":"); bjson(f, e->d_name);
            snprintf(num, sizeof num, ",\"size\":%lld,\"t\":%lld}", (long long)st.st_size, (long long)st.st_mtime); bstr(f, num);
        }
    }
    closedir(d);
    bstr(b, "],"); bput(b, f.p, f.n); bstr(b, "]}");
    reply(fd, 200, "application/json", b.p, b.n);
    free(b.p); free(f.p);
}
// Suche: alle WAVs, deren Pfad den Text enthaelt (ohne Gross/Klein), hoechstens 400 Treffer
static void searchDir(Buf& b, const char* dir, const char* sub, const char* q, int depth, int& hits, long long& total) {
    char path[1200]; snprintf(path, sizeof path, "%s%s%s", dir, sub[0] ? "/" : "", sub);
    DIR* d = opendir(path); if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char rel[600]; snprintf(rel, sizeof rel, "%s%s%s", sub, sub[0] ? "/" : "", e->d_name);
        bool isDir = e->d_type == DT_DIR;
        if (e->d_type == DT_UNKNOWN) { char full[1500]; struct stat st; snprintf(full, sizeof full, "%s/%s", dir, rel); isDir = !stat(full, &st) && S_ISDIR(st.st_mode); }
        if (isDir) { if (depth < SAMPLE_DEPTH) searchDir(b, dir, rel, q, depth + 1, hits, total); continue; }
        if (!hasExt(e->d_name, ".wav")) continue;
        total++;
        if (!strcasestr(rel, q)) continue;
        if (hits < 400) { if (hits) bstr(b, ","); bjson(b, rel); }
        hits++;
    }
    closedir(d);
}
// WAV-Kopf lesen: Format, Kanaele, Rate, Bits, Laenge
static void handleInfo(int fd, const char* full) {
    FILE* f = fopen(full, "rb");
    if (!f) { replyText(fd, 404, "nicht gefunden"); return; }
    uint8_t h[12]; char out[300];
    int fmt = 0, ch = 0, bits = 0; unsigned rate = 0; unsigned long long data = 0;
    if (fread(h, 1, 12, f) == 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVE", 4)) {
        uint8_t c[8];
        for (int guard = 0; guard < 64 && fread(c, 1, 8, f) == 8; guard++) {
            uint32_t sz = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t)c[7] << 24;
            if (!memcmp(c, "fmt ", 4)) {
                uint8_t fb[16];
                if (sz < 16 || fread(fb, 1, 16, f) != 16) break;
                fmt = fb[0] | fb[1] << 8; ch = fb[2] | fb[3] << 8; rate = fb[4] | fb[5] << 8 | fb[6] << 16 | (uint32_t)fb[7] << 24; bits = fb[14] | fb[15] << 8;
                fseek(f, (long)(sz - 16 + (sz & 1)), SEEK_CUR);
            } else if (!memcmp(c, "data", 4)) { data = sz; break; }
            else fseek(f, (long)(sz + (sz & 1)), SEEK_CUR);
        }
    }
    fclose(f);
    double sec = (rate && ch && bits) ? (double)data / ((double)rate * ch * (bits / 8)) : 0;
    const char* fn = fmt == 1 ? "PCM" : fmt == 3 ? "Float" : fmt == 0xFFFE ? "Extensible" : fmt ? "anderes" : "kein WAV";
    snprintf(out, sizeof out, "{\"format\":\"%s\",\"ch\":%d,\"rate\":%u,\"bits\":%d,\"sec\":%.3f,\"ok\":%s}", fn, ch, rate, bits, sec,
             (fmt == 1 || fmt == 3 || fmt == 0xFFFE) && ch > 0 ? "true" : "false");
    reply(fd, 200, "application/json", out, strlen(out));
}
// Ordner samt Inhalt loeschen (nur unter samples/, Tiefe begrenzt)
static bool rmTree(const char* path, int depth) {
    if (depth > SAMPLE_DEPTH + 1) return false;
    DIR* d = opendir(path); if (!d) return false;
    struct dirent* e; bool ok = true;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[1500]; snprintf(full, sizeof full, "%s/%s", path, e->d_name);
        struct stat st; if (lstat(full, &st)) { ok = false; continue; }
        if (S_ISDIR(st.st_mode)) ok = rmTree(full, depth + 1) && ok;
        else if (unlink(full)) ok = false;
    }
    closedir(d);
    return rmdir(path) == 0 && ok;
}
static void mkParents(const char* dir, const char* rel) {   // Unterordner fuer rel anlegen
    char p[1200];
    for (const char* c = rel; (c = strchr(c, '/')); c++) { snprintf(p, sizeof p, "%s/%.*s", dir, (int)(c - rel), rel); mkdir(p, 0755); }
}

static void handleList(int fd) {
    Buf b = {nullptr, 0, 0}, f = {nullptr, 0, 0};
    char sd[700], pd[700], num[160];
    rootDir("samples", sd, sizeof sd);
    rootDir("projects", pd, sizeof pd);
    bool first = true, ffirst = true, pf = true, dummy = true;
    bstr(f, "[");
    {   // nur zaehlen (die Liste holt der Explorer ordnerweise)
        int hits = 0; long long total = 0; Buf dummyB = {nullptr, 0, 0};
        searchDir(dummyB, sd, "", "\x01", 0, hits, total);
        free(dummyB.p);
        snprintf(num, sizeof num, "{\"count\":%lld,", total); bstr(b, num);
    }
    (void)f; (void)first; (void)ffirst;
    bstr(b, "\"projects\":[");
    listDir(b, pd, "", ".gbx", pf, SAMPLE_DEPTH, nullptr, dummy);   // Projekte ohne Unterordner
    struct statvfs vs;
    long long freeB = statvfs(sd, &vs) == 0 ? (long long)vs.f_bavail * (long long)vs.f_frsize : -1;
    char ver[64] = "";
    { char vp[700]; snprintf(vp, sizeof vp, "%sVERSION", g_base); FILE* vf = fopen(vp, "r");
      if (vf) { if (fgets(ver, sizeof ver, vf)) ver[strcspn(ver, "\r\n\"\\")] = 0; fclose(vf); } }
    snprintf(num, sizeof num, "],\"free\":%lld,\"max\":%d,\"os\":%s,\"version\":\"%s\"}", freeB, (int)MAX_LIB, isOS() ? "true" : "false", ver);
    bstr(b, num);
    reply(fd, 200, "application/json", b.p, b.n);
    free(b.p); free(f.p);
}

// ---------------------------------------------------------------- Anfragen

static bool handle(int fd) {   // true = Verbindung wurde uebernommen (WebSocket), nicht schliessen
    struct timeval tv = {15, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    static char hb[8192];
    int hl = 0;
    char* bodyStart = nullptr;
    while (hl < (int)sizeof hb - 1) {
        ssize_t r = recv(fd, hb + hl, sizeof hb - 1 - hl, 0);
        if (r <= 0) return false;
        hl += (int)r; hb[hl] = 0;
        if ((bodyStart = strstr(hb, "\r\n\r\n"))) { bodyStart += 4; break; }
    }
    if (!bodyStart) { replyText(fd, 400, "zu grosser Header"); return false; }
    char method[8] = {0}, target[1024] = {0};
    if (sscanf(hb, "%7s %1023s", method, target) != 2) { replyText(fd, 400, "?"); return false; }
    long long clen = 0;
    for (char* l = strstr(hb, "\r\n"); l && l < bodyStart; l = strstr(l + 2, "\r\n"))
        if (!strncasecmp(l + 2, "Content-Length:", 15)) clen = atoll(l + 17);
    char* q = strchr(target, '?');
    if (q) *q++ = 0;
    char root[16] = "samples", rel[400] = "", dir[700], full[1200];
    if (q) { queryGet(q, "root", root, sizeof root); queryGet(q, "path", rel, sizeof rel); }

    if (!strcmp(method, "GET") && (!strcmp(target, "/") || !strcmp(target, "/index.html"))) {
        reply(fd, 200, "text/html; charset=utf-8", WEB_PAGE, strlen(WEB_PAGE));
        return false;
    }
    if (!strcmp(method, "GET") && !strcmp(target, "/api/list")) { handleList(fd); return false; }
    if (!strcmp(method, "GET") && !strcmp(target, "/ws")) {   // Fernbedienung: Bildstrom + Tasten ueber WebSocket
        *bodyStart = 0;
        if (remoteWsAccept(fd, hb)) return true;
        replyText(fd, 400, "WebSocket?");
        return false;
    }
    if (!strcmp(method, "GET") && !strcmp(target, "/screen")) { reply(fd, 200, "text/html; charset=utf-8", WEB_SCREEN, strlen(WEB_SCREEN)); return false; }
    if (!strcmp(method, "GET") && !strcmp(target, "/api/screen")) {   // Bild (Differenz seit "since")
        char sv[16] = "0";
        if (q) queryGet(q, "since", sv, sizeof sv);
        static uint8_t* sbuf = nullptr; static size_t scap = 0;
        size_t n = remoteEncode((unsigned)strtoul(sv, nullptr, 10), &sbuf, &scap);
        if (!n) { reply(fd, 204, "application/octet-stream", "", 0); return false; }
        reply(fd, 200, "application/octet-stream", (const char*)sbuf, n);
        return false;
    }
    if (!strcmp(method, "POST") && !strcmp(target, "/api/input")) {   // Fernbedienung
        char bn[12] = "", vv[8] = "0";
        if (q) { queryGet(q, "b", bn, sizeof bn); queryGet(q, "v", vv, sizeof vv); }
        remoteInput(bn, atoi(vv));
        reply(fd, 200, "text/plain", "ok", 2);
        return false;
    }
    if (!strcmp(method, "GET") && !strcmp(target, "/api/log")) {   // Logs ansehen (feste Liste, keine beliebigen Pfade)
        char nm[40] = "", path[800] = "";
        if (q) queryGet(q, "name", nm, sizeof nm);
        static const char* const LOGS[][2] = {
            {"groovebox.log", "/var/log/groovebox/groovebox.log"}, {"groovebox.old.log", "/var/log/groovebox/groovebox.old.log"},
            {"boot.txt", "/data/logs/boot.txt"}, {"update.log", "/data/logs/update.log"}, {"bench.txt", "/data/logs/bench.txt"}, {"pc-modus.log", ""}, {"usb-diagnose", "-"}};
        for (auto& l : LOGS)
            if (!strcmp(nm, l[0])) {
                if (l[1][0]) snprintf(path, sizeof path, "%s", l[1]);
                else snprintf(path, sizeof path, "%s%s", g_base, l[0]);
            }
        if (!path[0] && !strcmp(nm, "groovebox.log")) snprintf(path, sizeof path, "%sgroovebox.log", g_base);
        if (!strcmp(path, "-")) {   // feste Diagnose-Befehle (keine Parameter aus der Anfrage)
            FILE* pp = popen("{ echo '=== Soundkarten'; cat /proc/asound/cards; echo '=== Gadget'; G=/sys/kernel/config/usb_gadget/groovebox;"
                             " echo UDC=$(cat $G/UDC 2>/dev/null); ls $G/configs/c.1 2>/dev/null; for f in $G/functions/*/*; do [ -f $f ] && echo \"$(basename $(dirname $f))/$(basename $f)=$(cat $f 2>/dev/null)\"; done;"
                             " echo '=== USB-Geschwindigkeit'; cat /sys/class/udc/*/current_speed /sys/class/udc/*/state 2>/dev/null;"
                             " for c in $(ls /proc/asound | grep -o 'card[0-9]*' | tr -d card); do grep -qi uac /proc/asound/card$c/id 2>/dev/null && { echo \"=== Regler Karte $c\"; amixer -c $c contents; echo \"=== PCM Karte $c\"; cat /proc/asound/card$c/pcm0p/sub0/hw_params /proc/asound/card$c/pcm0p/sub0/status 2>/dev/null; }; done;"
                             " echo '=== MIDI (ALSA-Sequencer)'; cat /proc/asound/seq/clients 2>/dev/null | grep -E '^Client|Port|Connect';"
                             " echo '=== Kernel (USB/Audio)'; dmesg | grep -iE 'dwc2|uac|gadget|u_audio|usb|configfs' | tail -80; } 2>&1", "r");
            if (!pp) { replyText(fd, 404, "(Diagnose nicht moeglich)"); return false; }
            static char db[65536];
            size_t dn = fread(db, 1, sizeof db, pp);
            pclose(pp);
            reply(fd, 200, "text/plain; charset=utf-8", db, dn);
            return false;
        }
        FILE* f = path[0] ? fopen(path, "rb") : nullptr;
        if (!f && !strcmp(nm, "groovebox.log")) { snprintf(path, sizeof path, "%sgroovebox.log", g_base); f = fopen(path, "rb"); }   // ArkOS
        if (!f) { replyText(fd, 404, "(Log nicht vorhanden)"); return false; }
        fseek(f, 0, SEEK_END); long n = ftell(f);
        long start = n > 300000 ? n - 300000 : 0;   // nur das Ende grosser Logs
        fseek(f, start, SEEK_SET);
        char* buf = (char*)malloc((size_t)(n - start) + 1);
        size_t r = buf ? fread(buf, 1, (size_t)(n - start), f) : 0;
        fclose(f);
        reply(fd, 200, "text/plain; charset=utf-8", buf ? buf : "", r);
        free(buf);
        return false;
    }
    if (!strcmp(method, "PUT") && !strcmp(target, "/api/update")) {   // Groovebox-OS: Programm-Update (.tgz)
        if (!isOS()) { replyText(fd, 400, "Updates per Browser nur im Groovebox-OS (ArkOS: deploy.ps1)"); return false; }
        if (clen <= 1000 || clen > 64LL * 1024 * 1024) { replyText(fd, 413, "Update-Datei ungueltig"); return false; }
        char udir[700], tmp[800], fin[800];
        snprintf(udir, sizeof udir, "%supdate", g_base);
        mkdir(udir, 0755);
        snprintf(tmp, sizeof tmp, "%s/groovebox-update.part", udir);
        snprintf(fin, sizeof fin, "%s/groovebox-update.tgz", udir);
        FILE* f = fopen(tmp, "wb");
        if (!f) { replyText(fd, 400, "Kann Update nicht speichern"); return false; }
        long long got = 0;
        int pre = hl - (int)(bodyStart - hb);
        if (pre > 0) { if (pre > clen) pre = (int)clen; fwrite(bodyStart, 1, (size_t)pre, f); got = pre; }
        static char ubuf[65536];
        bool ok = true;
        while (got < clen) {
            ssize_t r = recv(fd, ubuf, (size_t)(clen - got < (long long)sizeof ubuf ? clen - got : (long long)sizeof ubuf), 0);
            if (r <= 0 || fwrite(ubuf, 1, (size_t)r, f) != (size_t)r) { ok = false; break; }
            got += r;
        }
        if (fclose(f) != 0) ok = false;
        unsigned char mg[2] = {0, 0};
        FILE* chk = ok ? fopen(tmp, "rb") : nullptr;
        if (chk) { if (fread(mg, 1, 2, chk) != 2) mg[0] = 0; fclose(chk); }
        if (!ok || mg[0] != 0x1f || mg[1] != 0x8b) { unlink(tmp); replyText(fd, 400, "Keine gueltige Update-Datei (.tgz)"); return false; }
        sync();
        rename(tmp, fin);
        note("UPDATE: %.60s", g_lang ? "INSTALLING" : "WIRD INSTALLIERT");
        replyText(fd, 200, "ok - Groovebox startet neu und installiert das Update");
        g_updateReady = 1;
        return false;
    }

    if (!rootDir(root, dir, sizeof dir)) { replyText(fd, 400, "root?"); return false; }
    bool isS = !strcmp(root, "samples");
    const char* ext = isS ? ".wav" : ".gbx";
    snprintf(full, sizeof full, "%s/%s", dir, rel);

    bool relOk = rel[0] == 0 || safeRel(rel, isS ? SAMPLE_DEPTH : 0);   // leer = Wurzel
    if (!strcmp(method, "GET") && !strcmp(target, "/api/dir")) {
        if (!relOk || !isS) { replyText(fd, 400, "Pfad?"); return false; }
        handleDir(fd, dir, rel, ext);
        return false;
    }
    if (!strcmp(method, "GET") && !strcmp(target, "/api/search")) {
        char qq[100] = ""; if (q) queryGet(q, "q", qq, sizeof qq);
        Buf b = {nullptr, 0, 0}; int hits = 0; long long total = 0;
        bstr(b, "{\"hits\":[");
        if (qq[0]) searchDir(b, dir, "", qq, 0, hits, total);
        char num[80]; snprintf(num, sizeof num, "],\"more\":%s}", hits > 400 ? "true" : "false"); bstr(b, num);
        reply(fd, 200, "application/json", b.p, b.n);
        free(b.p);
        return false;
    }
    if (!strcmp(method, "GET") && !strcmp(target, "/api/info")) {
        if (!rel[0] || !relOk || !hasExt(rel, ".wav")) { replyText(fd, 400, "Pfad?"); return false; }
        handleInfo(fd, full);
        return false;
    }
    if (!strcmp(method, "POST") && !strcmp(target, "/api/mkdir")) {
        if (!isS || !rel[0] || !relOk || hasExt(rel, ".wav")) { replyText(fd, 400, "Ungueltiger Ordnername"); return false; }
        mkParents(dir, rel);
        if (mkdir(full, 0755) && errno != EEXIST) { replyText(fd, 400, "Ordner nicht angelegt"); return false; }
        g_changedAt = SDL_GetTicks(); g_changed = 1;
        replyText(fd, 200, "ok");
        return false;
    }
    if (!strcmp(method, "POST") && !strcmp(target, "/api/rmdir")) {
        struct stat st;
        if (!isS || !rel[0] || !relOk || stat(full, &st) || !S_ISDIR(st.st_mode)) { replyText(fd, 400, "Ordner?"); return false; }
        bool ok = rmTree(full, 0);
        sync();
        g_changedAt = SDL_GetTicks(); g_changed = 1;
        note(g_lang ? "DELETED: %.60s" : "GELÖSCHT: %.60s", rel);
        replyText(fd, ok ? 200 : 400, ok ? "ok" : "nicht alles geloescht");
        return false;
    }
    if (!strcmp(method, "POST") && !strcmp(target, "/api/move")) {   // Datei oder Ordner verschieben/umbenennen
        char to[400] = "", tfull[1200]; struct stat st;
        if (q) queryGet(q, "to", to, sizeof to);
        if (!rel[0] || !relOk || !to[0] || !safeRel(to, isS ? SAMPLE_DEPTH : 0) || stat(full, &st)) { replyText(fd, 400, "Pfad?"); return false; }
        bool isDir = S_ISDIR(st.st_mode);
        if (!isS && isDir) { replyText(fd, 400, "Pfad?"); return false; }
        if (!isDir && !hasExt(to, ext)) { replyText(fd, 400, isS ? "Ziel muss auf .wav enden" : "Ziel muss auf .gbx enden"); return false; }
        size_t rl = strlen(rel);
        if (isDir && !strncmp(to, rel, rl) && (to[rl] == '/' || !to[rl])) { replyText(fd, 400, "Ordner kann nicht in sich selbst"); return false; }
        snprintf(tfull, sizeof tfull, "%s/%s", dir, to);
        if (!stat(tfull, &st)) { replyText(fd, 409, "Ziel existiert schon"); return false; }
        mkParents(dir, to);
        if (rename(full, tfull)) { replyText(fd, 400, "Verschieben fehlgeschlagen"); return false; }
        sync();
        if (isS) { g_changedAt = SDL_GetTicks(); g_changed = 1; }
        replyText(fd, 200, "ok");
        return false;
    }

    if (!strcmp(method, "GET") && !strcmp(target, "/api/file")) {
        if (!safeRel(rel, isS ? SAMPLE_DEPTH : 0) || !hasExt(rel, ext)) { replyText(fd, 400, "Pfad?"); return false; }
        FILE* f = fopen(full, "rb");
        if (!f) { replyText(fd, 404, "nicht gefunden"); return false; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        char h[300];
        int l = snprintf(h, sizeof h, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n",
                         isS ? "audio/wav" : "application/octet-stream", n);
        sendAll(fd, h, (size_t)l);
        static char buf[65536];
        size_t r;
        while ((r = fread(buf, 1, sizeof buf, f)) > 0) if (!sendAll(fd, buf, r)) break;
        fclose(f);
        return false;
    }
    if (!strcmp(method, "PUT") && !strcmp(target, "/api/upload")) {
        if (!safeRel(rel, isS ? SAMPLE_DEPTH : 0) || !hasExt(rel, ext)) { replyText(fd, 400, "Ungueltiger Dateiname"); return false; }
        if (clen <= 0 || clen > 200LL * 1024 * 1024) { replyText(fd, 413, "Datei zu gross (max. 200 MB)"); return false; }
        mkParents(dir, rel);
        char tmp[1300];
        snprintf(tmp, sizeof tmp, "%s.upload", full);
        FILE* f = fopen(tmp, "wb");
        if (!f) { replyText(fd, 400, "Kann Datei nicht anlegen"); return false; }
        long long got = 0;
        int pre = hl - (int)(bodyStart - hb);
        if (pre > 0) { if (pre > clen) pre = (int)clen; fwrite(bodyStart, 1, (size_t)pre, f); got = pre; }
        static char buf[65536];
        bool ok = true;
        while (got < clen) {
            ssize_t r = recv(fd, buf, (size_t)(clen - got < (long long)sizeof buf ? clen - got : (long long)sizeof buf), 0);
            if (r <= 0) { ok = false; break; }
            if (fwrite(buf, 1, (size_t)r, f) != (size_t)r) { ok = false; break; }
            got += r;
        }
        if (fclose(f) != 0) ok = false;
        if (!ok) { unlink(tmp); replyText(fd, 400, "Uebertragung abgebrochen oder Karte voll"); return false; }
        sync();
        if (rename(tmp, full)) { unlink(tmp); replyText(fd, 400, "Umbenennen fehlgeschlagen"); return false; }
        if (isS) { g_changedAt = SDL_GetTicks(); g_changed = 1; }
        note(isS ? "SAMPLE: %.60s" : (g_lang ? "PROJECT: %.60s" : "PROJEKT: %.60s"), rel);
        replyText(fd, 200, "ok");
        return false;
    }
    if (!strcmp(method, "POST") && !strcmp(target, "/api/delete")) {
        if (!safeRel(rel, isS ? SAMPLE_DEPTH : 0) || !hasExt(rel, ext)) { replyText(fd, 400, "Pfad?"); return false; }
        if (unlink(full)) { replyText(fd, 404, "nicht gefunden"); return false; }
        sync();   // Ordner bleibt stehen (der Explorer zeigt ihn leer; loeschen ueber /api/rmdir)
        if (isS) { g_changedAt = SDL_GetTicks(); g_changed = 1; }
        note(g_lang ? "DELETED: %.60s" : "GELÖSCHT: %.60s", rel);
        replyText(fd, 200, "ok");
        return false;
    }
    replyText(fd, 404, "unbekannt");
    return false;
}

static int webThread(void* arg) {
    int ls = (int)(intptr_t)arg;
    for (;;) {
        int fd = accept(ls, nullptr, nullptr);
        if (fd < 0) { if (errno == EINTR) continue; SDL_Delay(200); continue; }
        if (handle(fd)) continue;   // WebSocket: eigener Thread hat fd uebernommen
        shutdown(fd, SHUT_WR);
        char d[256];
        struct timeval tv = {1, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        while (recv(fd, d, sizeof d, 0) > 0) {}
        close(fd);
    }
    return 0;
}

bool webStart(const char* basePath) {
    snprintf(g_base, sizeof g_base, "%s", basePath);
    static const int ports[2] = {80, 8080};
    for (int i = 0; i < 2; i++) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return false;
        int one = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        struct sockaddr_in a;
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET;
        a.sin_port = htons((uint16_t)ports[i]);
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, (struct sockaddr*)&a, sizeof a) == 0 && listen(s, 4) == 0) {
            g_port = ports[i];
            SDL_Thread* t = SDL_CreateThread(webThread, "web", (void*)(intptr_t)s);
            if (t) SDL_DetachThread(t);
            printf("Web-Upload auf Port %d\n", g_port);
            return true;
        }
        close(s);
    }
    printf("Web-Upload: kein Port frei\n");
    return false;
}
