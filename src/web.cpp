// Mini-Webserver: Samples und Projekte per Browser hochladen (PC-Modus: http://192.168.77.1)
// Ein Thread, eine Verbindung nach der anderen. Nur Dateien unter samples/ und projects/.
#include "web.h"
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

// Erlaubt: "name.wav" oder "ordner/name.wav" (eine Ebene), keine "..", nur harmlose Zeichen
static bool safeRel(const char* p, int maxDepth) {
    if (!p[0] || p[0] == '/' || p[0] == '.') return false;
    int slashes = 0;
    for (const char* c = p; *c; c++) {
        unsigned char u = (unsigned char)*c;
        if (*c == '/') { slashes++; if (c[1] == '.' || c[1] == '/' || c[1] == 0) return false; continue; }
        if (!(isalnum(u) || strchr(" _-.()+#", u))) return false;
    }
    return slashes <= maxDepth && !strstr(p, "..");
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
            if (depth < 1) {
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

static void handleList(int fd) {
    Buf b = {nullptr, 0, 0}, f = {nullptr, 0, 0};
    char sd[700], pd[700], num[160];
    rootDir("samples", sd, sizeof sd);
    rootDir("projects", pd, sizeof pd);
    bool first = true, ffirst = true, pf = true, dummy = true;
    bstr(f, "[");
    bstr(b, "{\"samples\":[");
    listDir(b, sd, "", ".wav", first, 0, &f, ffirst);
    bstr(f, "]");
    bstr(b, "],\"folders\":");
    bput(b, f.p, f.n);
    bstr(b, ",\"projects\":[");
    listDir(b, pd, "", ".gbx", pf, 1, nullptr, dummy);
    struct statvfs vs;
    long long freeB = statvfs(sd, &vs) == 0 ? (long long)vs.f_bavail * (long long)vs.f_frsize : -1;
    char ver[64] = "";
    { char vp[700]; snprintf(vp, sizeof vp, "%sVERSION", g_base); FILE* vf = fopen(vp, "r");
      if (vf) { if (fgets(ver, sizeof ver, vf)) ver[strcspn(ver, "\r\n\"\\")] = 0; fclose(vf); } }
    snprintf(num, sizeof num, "],\"free\":%lld,\"max\":%d,\"os\":%s,\"version\":\"%s\"}", freeB, 128, isOS() ? "true" : "false", ver);
    bstr(b, num);
    reply(fd, 200, "application/json", b.p, b.n);
    free(b.p); free(f.p);
}

// ---------------------------------------------------------------- Anfragen

static void handle(int fd) {
    struct timeval tv = {15, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    static char hb[8192];
    int hl = 0;
    char* bodyStart = nullptr;
    while (hl < (int)sizeof hb - 1) {
        ssize_t r = recv(fd, hb + hl, sizeof hb - 1 - hl, 0);
        if (r <= 0) return;
        hl += (int)r; hb[hl] = 0;
        if ((bodyStart = strstr(hb, "\r\n\r\n"))) { bodyStart += 4; break; }
    }
    if (!bodyStart) { replyText(fd, 400, "zu grosser Header"); return; }
    char method[8] = {0}, target[1024] = {0};
    if (sscanf(hb, "%7s %1023s", method, target) != 2) { replyText(fd, 400, "?"); return; }
    long long clen = 0;
    for (char* l = strstr(hb, "\r\n"); l && l < bodyStart; l = strstr(l + 2, "\r\n"))
        if (!strncasecmp(l + 2, "Content-Length:", 15)) clen = atoll(l + 17);
    char* q = strchr(target, '?');
    if (q) *q++ = 0;
    char root[16] = "samples", rel[400] = "", dir[700], full[1200];
    if (q) { queryGet(q, "root", root, sizeof root); queryGet(q, "path", rel, sizeof rel); }

    if (!strcmp(method, "GET") && (!strcmp(target, "/") || !strcmp(target, "/index.html"))) {
        reply(fd, 200, "text/html; charset=utf-8", WEB_PAGE, strlen(WEB_PAGE));
        return;
    }
    if (!strcmp(method, "GET") && !strcmp(target, "/api/list")) { handleList(fd); return; }
    if (!strcmp(method, "GET") && !strcmp(target, "/screen")) { reply(fd, 200, "text/html; charset=utf-8", WEB_SCREEN, strlen(WEB_SCREEN)); return; }
    if (!strcmp(method, "GET") && !strcmp(target, "/api/screen")) {   // Bild (Differenz seit "since")
        char sv[16] = "0";
        if (q) queryGet(q, "since", sv, sizeof sv);
        static uint8_t* sbuf = nullptr; static size_t scap = 0;
        size_t n = remoteEncode((unsigned)strtoul(sv, nullptr, 10), &sbuf, &scap);
        if (!n) { reply(fd, 204, "application/octet-stream", "", 0); return; }
        reply(fd, 200, "application/octet-stream", (const char*)sbuf, n);
        return;
    }
    if (!strcmp(method, "POST") && !strcmp(target, "/api/input")) {   // Fernbedienung
        char bn[12] = "", vv[8] = "0";
        if (q) { queryGet(q, "b", bn, sizeof bn); queryGet(q, "v", vv, sizeof vv); }
        remoteInput(bn, atoi(vv));
        reply(fd, 200, "text/plain", "ok", 2);
        return;
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
                             " echo UDC=$(cat $G/UDC 2>/dev/null); ls $G/configs/c.1 2>/dev/null; for f in $G/functions/uac2.usb0/*; do [ -f $f ] && echo \"$(basename $f)=$(cat $f 2>/dev/null)\"; done;"
                             " echo '=== USB-Geschwindigkeit'; cat /sys/class/udc/*/current_speed /sys/class/udc/*/state 2>/dev/null;"
                             " for c in $(ls /proc/asound | grep -o 'card[0-9]*' | tr -d card); do grep -qi uac /proc/asound/card$c/id 2>/dev/null && { echo \"=== Regler Karte $c\"; amixer -c $c contents; echo \"=== PCM Karte $c\"; cat /proc/asound/card$c/pcm0p/sub0/hw_params /proc/asound/card$c/pcm0p/sub0/status 2>/dev/null; }; done;"
                             " echo '=== Kernel (USB/Audio)'; dmesg | grep -iE 'dwc2|uac|gadget|u_audio|usb|configfs' | tail -80; } 2>&1", "r");
            if (!pp) { replyText(fd, 404, "(Diagnose nicht moeglich)"); return; }
            static char db[65536];
            size_t dn = fread(db, 1, sizeof db, pp);
            pclose(pp);
            reply(fd, 200, "text/plain; charset=utf-8", db, dn);
            return;
        }
        FILE* f = path[0] ? fopen(path, "rb") : nullptr;
        if (!f && !strcmp(nm, "groovebox.log")) { snprintf(path, sizeof path, "%sgroovebox.log", g_base); f = fopen(path, "rb"); }   // ArkOS
        if (!f) { replyText(fd, 404, "(Log nicht vorhanden)"); return; }
        fseek(f, 0, SEEK_END); long n = ftell(f);
        long start = n > 300000 ? n - 300000 : 0;   // nur das Ende grosser Logs
        fseek(f, start, SEEK_SET);
        char* buf = (char*)malloc((size_t)(n - start) + 1);
        size_t r = buf ? fread(buf, 1, (size_t)(n - start), f) : 0;
        fclose(f);
        reply(fd, 200, "text/plain; charset=utf-8", buf ? buf : "", r);
        free(buf);
        return;
    }
    if (!strcmp(method, "PUT") && !strcmp(target, "/api/update")) {   // Groovebox-OS: Programm-Update (.tgz)
        if (!isOS()) { replyText(fd, 400, "Updates per Browser nur im Groovebox-OS (ArkOS: deploy.ps1)"); return; }
        if (clen <= 1000 || clen > 64LL * 1024 * 1024) { replyText(fd, 413, "Update-Datei ungueltig"); return; }
        char udir[700], tmp[800], fin[800];
        snprintf(udir, sizeof udir, "%supdate", g_base);
        mkdir(udir, 0755);
        snprintf(tmp, sizeof tmp, "%s/groovebox-update.part", udir);
        snprintf(fin, sizeof fin, "%s/groovebox-update.tgz", udir);
        FILE* f = fopen(tmp, "wb");
        if (!f) { replyText(fd, 400, "Kann Update nicht speichern"); return; }
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
        if (!ok || mg[0] != 0x1f || mg[1] != 0x8b) { unlink(tmp); replyText(fd, 400, "Keine gueltige Update-Datei (.tgz)"); return; }
        sync();
        rename(tmp, fin);
        note("UPDATE: %.60s", "WIRD INSTALLIERT");
        replyText(fd, 200, "ok - Groovebox startet neu und installiert das Update");
        g_updateReady = 1;
        return;
    }

    if (!rootDir(root, dir, sizeof dir)) { replyText(fd, 400, "root?"); return; }
    bool isS = !strcmp(root, "samples");
    const char* ext = isS ? ".wav" : ".gbx";
    snprintf(full, sizeof full, "%s/%s", dir, rel);

    if (!strcmp(method, "GET") && !strcmp(target, "/api/file")) {
        if (!safeRel(rel, isS ? 1 : 0) || !hasExt(rel, ext)) { replyText(fd, 400, "Pfad?"); return; }
        FILE* f = fopen(full, "rb");
        if (!f) { replyText(fd, 404, "nicht gefunden"); return; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        char h[300];
        int l = snprintf(h, sizeof h, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n",
                         isS ? "audio/wav" : "application/octet-stream", n);
        sendAll(fd, h, (size_t)l);
        static char buf[65536];
        size_t r;
        while ((r = fread(buf, 1, sizeof buf, f)) > 0) if (!sendAll(fd, buf, r)) break;
        fclose(f);
        return;
    }
    if (!strcmp(method, "PUT") && !strcmp(target, "/api/upload")) {
        if (!safeRel(rel, isS ? 1 : 0) || !hasExt(rel, ext)) { replyText(fd, 400, "Ungueltiger Dateiname"); return; }
        int slotNo = 0;
        if (!isS && (sscanf(rel, "song%2d.gbx", &slotNo) != 1 || slotNo < 1 || slotNo > 32 || strlen(rel) != 10)) { replyText(fd, 400, "Projekte heissen songNN.gbx"); return; }
        if (clen <= 0 || clen > 200LL * 1024 * 1024) { replyText(fd, 413, "Datei zu gross (max. 200 MB)"); return; }
        const char* sl = strchr(rel, '/');
        if (sl) { char sub[700]; snprintf(sub, sizeof sub, "%s/%.*s", dir, (int)(sl - rel), rel); mkdir(sub, 0755); }
        char tmp[1300];
        snprintf(tmp, sizeof tmp, "%s.upload", full);
        FILE* f = fopen(tmp, "wb");
        if (!f) { replyText(fd, 400, "Kann Datei nicht anlegen"); return; }
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
        if (!ok) { unlink(tmp); replyText(fd, 400, "Uebertragung abgebrochen oder Karte voll"); return; }
        sync();
        if (rename(tmp, full)) { unlink(tmp); replyText(fd, 400, "Umbenennen fehlgeschlagen"); return; }
        if (isS) { g_changedAt = SDL_GetTicks(); g_changed = 1; }
        note(isS ? "SAMPLE: %.60s" : "PROJEKT: %.60s", rel);
        replyText(fd, 200, "ok");
        return;
    }
    if (!strcmp(method, "POST") && !strcmp(target, "/api/delete")) {
        if (!safeRel(rel, isS ? 1 : 0) || !hasExt(rel, ext)) { replyText(fd, 400, "Pfad?"); return; }
        if (unlink(full)) { replyText(fd, 404, "nicht gefunden"); return; }
        const char* sl = strchr(rel, '/');   // leeren Ordner mit entfernen
        if (sl) { char sub[700]; snprintf(sub, sizeof sub, "%s/%.*s", dir, (int)(sl - rel), rel); rmdir(sub); }
        sync();
        if (isS) { g_changedAt = SDL_GetTicks(); g_changed = 1; }
        note("GELOESCHT: %.60s", rel);
        replyText(fd, 200, "ok");
        return;
    }
    replyText(fd, 404, "unbekannt");
}

static int webThread(void* arg) {
    int ls = (int)(intptr_t)arg;
    for (;;) {
        int fd = accept(ls, nullptr, nullptr);
        if (fd < 0) { if (errno == EINTR) continue; SDL_Delay(200); continue; }
        handle(fd);
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
