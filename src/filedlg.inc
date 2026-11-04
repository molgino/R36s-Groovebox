// Datei-Dialoge: Bildschirm-Tastatur (Namen eingeben) und Explorer (Samples mit Unterordnern, Projekte, Presets).
// Wird in main.cpp nach loadPreset eingebunden und nutzt dessen Zustand.

// ---------------------------------------------------------------- Namen und Pfade

// erlaubte Zeichen in Datei-Namen (wie die Weboberflaeche): Buchstaben, Ziffern, Leerzeichen, - _ . ( ) +
static void cleanName(const char* in, char* out, int n) {
    int k = 0;
    for (const char* c = in; *c && k < n - 1; c++) {
        unsigned char u = (unsigned char)*c;
        if (isalnum(u) || strchr(" -_.()+", u)) out[k++] = (char)u;
    }
    out[k] = 0;
    while (k > 0 && (out[k - 1] == ' ' || out[k - 1] == '.')) out[--k] = 0;   // keine Punkte/Leerzeichen am Ende
    int s = 0; while (out[s] == ' ' || out[s] == '.') s++;
    if (s) memmove(out, out + s, strlen(out + s) + 1);
}
static void projectPath(const char* name, char* out, int n) { snprintf(out, n, "%sprojects/%s.gbx", basePath, name); }
static bool projectExists(const char* name) { char p[700]; projectPath(name, p, sizeof p); return fileExists(p); }
static void presetFile(int m, const char* name, char* out, int n) { snprintf(out, n, "%spresets/%s/%s.gbp", basePath, MACHINE_NAMES[m], name); }
static bool anyProject() {
    char d[700]; snprintf(d, sizeof d, "%sprojects", basePath);
    DIR* dd = opendir(d); if (!dd) return false;
    struct dirent* e; bool any = false;
    while ((e = readdir(dd)) && !any) { size_t l = strlen(e->d_name); if (l > 4 && !strcasecmp(e->d_name + l - 4, ".gbx")) any = true; }
    closedir(dd);
    return any;
}
static void setCurrentProject(const char* name) {
    snprintf(cfg.project, sizeof cfg.project, "%s", name);
    projectPath(name, projPath, sizeof projPath);
    saveSettings();
}
// freier Name mit Zaehler: "Projekt 1", "Projekt 2", ...
static void freeProjectName(const char* base, char* out, int n) {
    if (!projectExists(base)) { snprintf(out, n, "%s", base); return; }
    for (int i = 2; i < 1000; i++) { snprintf(out, n, "%s %d", base, i); if (!projectExists(out)) return; }
}

static bool savePresetFile(int t, const char* path) {
    int m = curP().machine[t];
    char d[700];
    snprintf(d, sizeof d, "%spresets", basePath); mkdir(d, 0755);
    snprintf(d, sizeof d, "%spresets/%s", basePath, MACHINE_NAMES[m]); mkdir(d, 0755);
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    uint8_t hdr[8] = {'G', 'B', 'P', '1', (uint8_t)m, (uint8_t)NPARAMS, 2, 0};
    bool ok = fwrite(hdr, 8, 1, f) == 1 && fwrite(curP().p[t], NPARAMS, 1, f) == 1;
    int sl = curP().p[t][P_S0];
    if (ok) {   // Sample-Name dahinter (wie im Projekt), damit der Sound in jedem Projekt passt; Anzahl 0 = kein Sample
        bool has = machineUsesSample(m) && sampleGet(sl);
        const char* sn = has ? sampleName(sl) : "";
        uint16_t n = has ? 1 : 0; uint8_t eh[2] = {(uint8_t)sl, (uint8_t)strlen(sn)};
        ok = fwrite("SPL1", 4, 1, f) == 1 && fwrite(&n, 2, 1, f) == 1;
        if (ok && has) ok = fwrite(eh, 2, 1, f) == 1 && fwrite(sn, eh[1], 1, f) == 1;
    }
    fclose(f);
    return ok;
}

// ---------------------------------------------------------------- Projekt-Aktionen

static void doLoadProject(const char* name) {
    char p[700]; projectPath(name, p, sizeof p);
    if (!fileExists(p)) { toast(TR("PROJEKT '%s' NICHT GEFUNDEN", "PROJECT '%s' NOT FOUND"), name); return; }
    lockA(); eng.stop(); bool ok = eng.load(p); unlockA();
    if (!ok) { toast(TR("LADEN FEHLGESCHLAGEN", "LOADING FAILED")); return; }
    poolLoadForProject(p);
    memcpy(&lastSaved, &eng.proj, sizeof(Project));
    setCurrentProject(name);
    toast(TR("GELADEN: %s", "LOADED: %s"), name);
}
static void doSaveProjectAs(const char* name) {
    char d[700]; snprintf(d, sizeof d, "%sprojects", basePath); mkdir(d, 0755);
    setCurrentProject(name);
    toast(saveProject() ? TR("GESPEICHERT: %s", "SAVED: %s") : TR("SPEICHERN FEHLGESCHLAGEN", "SAVING FAILED"), name);
}
static void doNewProject(const char* name) {
    lockA(); eng.stop(); eng.defaultProject(false); eng.songOn = false; unlockA();
    poolDefaults();
    doSaveProjectAs(name);
    toast(TR("NEUES PROJEKT: %s", "NEW PROJECT: %s"), name);
}

// ---------------------------------------------------------------- Bildschirm-Tastatur

enum { KB_SAVEAS = 1, KB_NEW, KB_PSAVE };
static int kbMode = 0, kbRow = 0, kbCol = 0;
static bool kbUpper = true;
static char kbText[40];
static char pendingName[64];   // fuer Rueckfragen (Ueberschreiben, Laden)
static const char* const KB_ROWS[3] = {"ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "0123456789-_."};
enum { KB_COLS = 13, KB_MAXLEN = 32 };
static const char* const KB_SPECIAL[4] = {"a/A", "LEER", "<-", "OK"};

static void openKeyboard(int mode, const char* initial) {
    kbOpen = true; kbMode = mode; kbRow = 0; kbCol = 0; kbUpper = true;
    snprintf(kbText, sizeof kbText, "%s", initial);
}
static void kbSubmit() {
    char name[64]; cleanName(kbText, name, sizeof name);
    if (!name[0]) { toast(TR("BITTE EINEN NAMEN EINGEBEN", "PLEASE ENTER A NAME")); return; }
    kbOpen = false;
    snprintf(pendingName, sizeof pendingName, "%s", name);
    switch (kbMode) {
    case KB_SAVEAS:
        if (projectExists(name) && strcmp(name, cfg.project)) askConfirm(ACT_SAVEAS, 0, TR("'%s' ÜBERSCHREIBEN?", "OVERWRITE '%s'?"), name);
        else doSaveProjectAs(name);
        break;
    case KB_NEW:
        if (projectExists(name)) askConfirm(ACT_NEWNAME, 0, TR("'%s' GIBT ES SCHON - ÜBERSCHREIBEN?", "'%s' EXISTS - OVERWRITE?"), name);
        else if (memcmp(&lastSaved, &eng.proj, sizeof(Project))) askConfirm(ACT_NEWNAME, 0, TR("NEUES PROJEKT? UNGESPEICHERTES GEHT VERLOREN", "NEW PROJECT? UNSAVED CHANGES WILL BE LOST"));
        else doNewProject(name);
        break;
    case KB_PSAVE: {
        char p[700]; presetFile(curP().machine[track], name, p, sizeof p);
        if (fileExists(p)) askConfirm(ACT_PSAVENAME, track, TR("SOUND '%s' ÜBERSCHREIBEN?", "OVERWRITE SOUND '%s'?"), name);
        else toast(savePresetFile(track, p) ? TR("SOUND GESPEICHERT: %s", "SOUND SAVED: %s") : TR("SPEICHERN FEHLGESCHLAGEN", "SAVING FAILED"), name);
    } break;
    }
}
static void kbPress() {
    int len = (int)strlen(kbText);
    if (kbRow < 3) {
        if (len >= KB_MAXLEN) return;
        char c = KB_ROWS[kbRow][kbCol];
        if (!kbUpper && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        kbText[len] = c; kbText[len + 1] = 0;
        if (kbUpper && len == 0) kbUpper = false;   // nach dem ersten Grossbuchstaben klein weiter (wie am Handy)
        return;
    }
    switch (kbCol) {
    case 0: kbUpper = !kbUpper; break;
    case 1: if (len < KB_MAXLEN && len > 0) { kbText[len] = ' '; kbText[len + 1] = 0; } break;
    case 2: if (len) kbText[len - 1] = 0; break;
    case 3: kbSubmit(); break;
    }
}
static void kbInput(Input& I, Uint32 now) {
    int dx = (I.repeat(BT_RIGHT, now) ? 1 : 0) - (I.repeat(BT_LEFT, now) ? 1 : 0);
    int dy = (I.repeat(BT_DOWN, now) ? 1 : 0) - (I.repeat(BT_UP, now) ? 1 : 0);
    if (dy) {
        int oldRow = kbRow;
        kbRow = (kbRow + dy + 4) % 4;
        if (kbRow == 3 && oldRow != 3) kbCol = kbCol * 4 / KB_COLS;               // in die Sondertasten-Zeile
        else if (oldRow == 3 && kbRow != 3) kbCol = kbCol * KB_COLS / 4 + 1;
    }
    int cols = kbRow == 3 ? 4 : KB_COLS;
    if (dx) kbCol = (kbCol + dx + cols) % cols;
    if (kbCol >= cols) kbCol = cols - 1;
    if (I.pressed(BT_A)) kbPress();
    if (I.repeat(BT_B, now)) { int l = (int)strlen(kbText); if (l) kbText[l - 1] = 0; }
    if (I.pressed(BT_X)) { int l = (int)strlen(kbText); if (l && l < KB_MAXLEN) { kbText[l] = ' '; kbText[l + 1] = 0; } }
    if (I.pressed(BT_Y)) kbUpper = !kbUpper;
    if (I.pressed(BT_START)) kbSubmit();
    if (I.pressed(BT_SELECT)) { kbOpen = false; toast(TR("ABGEBROCHEN", "CANCELLED")); }
}
static void drawKeyboard() {
    int x0 = 40, y0 = 70, w = 560, h = 340;
    rrect(x0, y0, w, h, 14, C_ACC);
    rrect(x0 + 2, y0 + 2, w - 4, h - 4, 13, C_BG);
    const char* title = kbMode == KB_PSAVE ? TR("SOUND SPEICHERN ALS", "SAVE SOUND AS")
                      : (kbMode == KB_NEW ? TR("NEUES PROJEKT", "NEW PROJECT") : TR("PROJEKT SPEICHERN ALS", "SAVE PROJECT AS"));
    textC(320, y0 + 12, title, C_ACC, 2);
    // Eingabefeld
    rrect(x0 + 30, y0 + 52, w - 60, 34, 8, C_PANEL);
    text(x0 + 42, y0 + 61, kbText, C_PLAY, 1);
    if ((SDL_GetTicks() / 400) % 2) fill(x0 + 42 + textW(kbText), y0 + 59, 2, 20, C_ACC);
    char cnt[16]; snprintf(cnt, sizeof cnt, "%d/%d", (int)strlen(kbText), KB_MAXLEN);
    text(x0 + w - 36 - textW(cnt), y0 + 61, cnt, C_DIM);
    // Tasten
    int kw = 36, kh = 34, gx = x0 + (w - KB_COLS * (kw + 4) + 4) / 2, gy = y0 + 104;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < KB_COLS; c++) {
            bool sel = kbRow == r && kbCol == c;
            int x = gx + c * (kw + 4), y = gy + r * (kh + 6);
            rrect(x, y, kw, kh, 6, sel ? C_ACC : C_PANEL);
            char ch[2] = {KB_ROWS[r][c], 0};
            if (!kbUpper && ch[0] >= 'A' && ch[0] <= 'Z') ch[0] = (char)(ch[0] - 'A' + 'a');
            textC(x + kw / 2, y + 9, ch, sel ? C_BG : C_TEXT);
        }
    int sw = (KB_COLS * (kw + 4) - 4 - 3 * 8) / 4;
    for (int c = 0; c < 4; c++) {
        bool sel = kbRow == 3 && kbCol == c;
        int x = gx + c * (sw + 8), y = gy + 3 * (kh + 6);
        rrect(x, y, sw, kh, 6, sel ? C_ACC : (c == 3 ? C_SEL : C_PANEL));
        const char* lbl = c == 1 ? TR("LEER", "SPACE") : (c == 0 ? (kbUpper ? "ABC" : "abc") : KB_SPECIAL[c]);
        textC(x + sw / 2, y + 9, lbl, sel ? C_BG : (c == 3 ? C_GREEN : C_TEXT));
    }
    hint(x0 + 24, y0 + h - 26, TR("[A]TASTE [B]LÖSCHEN [X]LEER [Y]a/A [START]OK [SEL]ABBRUCH", "[A]KEY [B]DELETE [X]SPACE [Y]a/A [START]OK [SEL]CANCEL"), C_ACC, C_DIM);
}

// ---------------------------------------------------------------- Explorer

enum { FB_SAMPLE = 1, FB_PROJECT, FB_PRESET };
struct FbEnt { char name[96]; bool dir; int idx; };
static int fbMode = 0, fbSel = 0, fbN = 0, fbOrig = -1, fbOrigLib = -1;   // Samples: Pool-Platz bzw. Bibliotheksindex beim Oeffnen
static float fbScroll = 0;
static char fbDir[128];
static FbEnt fbEnt[MAX_LIB + 64];
static const char* FB_FACTORY = "WERK";   // Ordnername der Werksklaenge im Sound-Explorer (Anzeige uebersetzt)

static int fbCmp(const void* a, const void* b) {
    const FbEnt* x = (const FbEnt*)a; const FbEnt* y = (const FbEnt*)b;
    if (x->dir != y->dir) return x->dir ? -1 : 1;
    return strcasecmp(x->name, y->name);
}
static void fbAdd(const char* name, bool dir, int idx) {
    if (fbN >= (int)(sizeof fbEnt / sizeof *fbEnt)) return;
    if (dir && fbN && fbEnt[fbN - 1].dir && !strcmp(fbEnt[fbN - 1].name, name)) return;   // sortierte Bibliothek: Dateien eines Ordners folgen aufeinander
    snprintf(fbEnt[fbN].name, sizeof fbEnt[fbN].name, "%s", name);
    fbEnt[fbN].dir = dir; fbEnt[fbN].idx = idx; fbN++;
}
static void fbListFiles(const char* dir, const char* ext, int idx) {
    DIR* d = opendir(dir); if (!d) return;
    struct dirent* e;
    size_t el = strlen(ext);
    while ((e = readdir(d))) {
        size_t l = strlen(e->d_name);
        if (e->d_name[0] == '.' || l <= el || strcasecmp(e->d_name + l - el, ext)) continue;
        char nm[96]; snprintf(nm, sizeof nm, "%.*s", (int)(l - el), e->d_name);
        fbAdd(nm, false, idx);
    }
    closedir(d);
}
static void fbBuild() {
    fbN = 0;
    if (fbMode == FB_SAMPLE) {
        size_t pl = strlen(fbDir);
        for (int i = 0; i < libCount(); i++) {
            const char* n = libName(i);
            if (strncmp(n, fbDir, pl)) continue;
            const char* rest = n + pl;
            const char* sl = strchr(rest, '/');
            if (sl) { char dn[96]; snprintf(dn, sizeof dn, "%.*s", (int)(sl - rest), rest); fbAdd(dn, true, -1); }
            else fbAdd(rest, false, i);
        }
    } else if (fbMode == FB_PROJECT) {
        char d[700]; snprintf(d, sizeof d, "%sprojects", basePath);
        fbListFiles(d, ".gbx", -1);
    } else if (fbMode == FB_PRESET) {
        if (!fbDir[0]) {   // Wurzel: Werksklaenge + Ordner je Engine (eigene Sounds)
            fbAdd(FB_FACTORY, true, -1);
            for (int i = 0; i < M_COUNT; i++) {
                int m = MACHINE_ORDER[i];
                char d[700]; snprintf(d, sizeof d, "%spresets/%s", basePath, MACHINE_NAMES[m]);
                DIR* dd = opendir(d); if (!dd) continue;
                struct dirent* e; bool any = false;
                while ((e = readdir(dd)) && !any) { size_t l = strlen(e->d_name); if (l > 4 && !strcasecmp(e->d_name + l - 4, ".gbp")) any = true; }
                closedir(dd);
                if (any) fbAdd(MACHINE_NAMES[m], true, m);
            }
        } else if (!strncmp(fbDir, FB_FACTORY, strlen(FB_FACTORY))) {
            for (int i = 0; i < NFACTORY; i++) fbAdd(FACTORY[i].name, false, i);
        } else {
            char eng2[32]; snprintf(eng2, sizeof eng2, "%.*s", (int)strlen(fbDir) - 1, fbDir);
            int m = 0; for (int i = 0; i < M_COUNT; i++) if (!strcmp(MACHINE_NAMES[i], eng2)) m = i;
            char d[700]; snprintf(d, sizeof d, "%spresets/%s", basePath, eng2);
            fbListFiles(d, ".gbp", m);
        }
    }
    bool factoryDir = fbMode == FB_PRESET && fbDir[0] && !strncmp(fbDir, FB_FACTORY, strlen(FB_FACTORY));
    if (!factoryDir && !(fbMode == FB_PRESET && !fbDir[0])) qsort(fbEnt, fbN, sizeof(FbEnt), fbCmp);
    if (fbSel >= fbN) fbSel = fbN ? fbN - 1 : 0;
}
static void openBrowser(int mode) {
    fbOpen = true; fbMode = mode; fbSel = 0; fbScroll = 0; fbDir[0] = 0;
    fbOrig = -1; fbOrigLib = -1;
    if (mode == FB_SAMPLE) {   // im Ordner des aktuellen Samples starten und es markieren
        int cur = aHeld && holdS().lock[P_S0] != NO_LOCK ? holdS().lock[P_S0] : curP().p[track][P_S0];
        fbOrig = curP().p[track][P_S0];
        const char* n = sampleGet(cur) ? sampleName(cur) : nullptr;
        if (n) {
            fbOrigLib = libFind(n);
            const char* sl = strrchr(n, '/');
            if (sl && fbOrigLib >= 0) snprintf(fbDir, sizeof fbDir, "%.*s/", (int)(sl - n), n);
        }
        fbBuild();
        for (int i = 0; i < fbN; i++) if (!fbEnt[i].dir && fbEnt[i].idx == fbOrigLib) fbSel = i;
    } else if (mode == FB_PROJECT) {
        fbBuild();
        for (int i = 0; i < fbN; i++) if (!strcmp(fbEnt[i].name, cfg.project)) fbSel = i;
    } else {
        fbBuild();
        for (int i = 0; i < fbN; i++) if (fbEnt[i].dir && fbEnt[i].idx == curP().machine[track]) fbSel = i;   // Ordner der aktuellen Engine
    }
    fbScroll = fbSel > 6 ? fbSel - 6.f : 0.f;
}
static void fbClose(bool cancel) {
    if (cancel && fbMode == FB_SAMPLE && fbOrig >= 0) { lockA(); curP().p[track][P_S0] = (uint8_t)fbOrig; unlockA(); }   // Vorhoeren zuruecknehmen
    if (fbMode == FB_SAMPLE && sampleName(PREVIEW_SLOT)[0]) poolSetSlot(PREVIEW_SLOT, "");   // Speicher des Vorhoer-Platzes freigeben
    fbOpen = false;
}
static void fbUp() {
    int l = (int)strlen(fbDir);
    if (!l) return;
    char last[96]; int s = l - 2; while (s >= 0 && fbDir[s] != '/') s--;
    snprintf(last, sizeof last, "%.*s", l - 2 - s, fbDir + s + 1);
    fbDir[s + 1] = 0;
    fbBuild();
    fbSel = 0; for (int i = 0; i < fbN; i++) if (fbEnt[i].dir && !strcmp(fbEnt[i].name, last)) fbSel = i;
}
static void fbPick() {
    if (fbN == 0) return;
    FbEnt& e = fbEnt[fbSel];
    if (e.dir) {
        if (strlen(fbDir) + strlen(e.name) + 2 < sizeof fbDir) { strcat(fbDir, e.name); strcat(fbDir, "/"); }
        fbSel = 0; fbScroll = 0; fbBuild();
        return;
    }
    switch (fbMode) {
    case FB_SAMPLE: {
        lockA(); curP().p[track][P_S0] = (uint8_t)fbOrig; unlockA();   // Vorhoer-Platz nicht stehen lassen
        int sl = poolAssign(libName(e.idx));
        if (sl < 0) return;   // Pool voll oder Datei nicht lesbar (Meldung kommt von poolAssign)
        lockA();
        if (aHeld || lockLatch) { holdS().lock[P_S0] = (uint8_t)sl; aEdited = true; }
        else curP().p[track][P_S0] = (uint8_t)sl;
        unlockA();
        fbClose(false);
        toast("%s%s", aHeld || lockLatch ? "LOCK: " : "", libName(e.idx));
    } break;
    case FB_PROJECT:
        snprintf(pendingName, sizeof pendingName, "%s", e.name);
        fbOpen = false;
        if (memcmp(&lastSaved, &eng.proj, sizeof(Project))) askConfirm(ACT_LOADNAME, 0, TR("'%s' LADEN? UNGESPEICHERTES GEHT VERLOREN", "LOAD '%s'? UNSAVED CHANGES WILL BE LOST"), e.name);
        else doLoadProject(e.name);
        break;
    case FB_PRESET:
        fbOpen = false;
        if (!strncmp(fbDir, FB_FACTORY, strlen(FB_FACTORY))) {
            lockA(); applyFactory(curP(), track, e.idx); unlockA();
            toast(TR("WERKSKLANG: %s", "FACTORY SOUND: %s"), e.name);
        } else {
            char p[700]; presetFile(e.idx, e.name, p, sizeof p);
            int r = loadPresetPath(track, p);
            toast(r > 0 ? TR("SOUND GELADEN: %s", "SOUND LOADED: %s") : TR("SOUND DEFEKT: %s", "SOUND DAMAGED: %s"), e.name);
        }
        break;
    }
}
static void fbPreview() {
    if (fbN == 0 || fbEnt[fbSel].dir) return;
    if (fbMode == FB_SAMPLE) {   // schon im Pool: diesen Platz spielen, sonst in den Vorhoer-Platz laden
        const char* n = libName(fbEnt[fbSel].idx);
        int sl = poolFind(n);
        if (sl < 0) {
            if (strcmp(sampleName(PREVIEW_SLOT), n) && !poolSetSlot(PREVIEW_SLOT, n)) { toast(TR("NICHT LESBAR: %s", "CANNOT READ: %s"), sampleLastError()); return; }
            sl = PREVIEW_SLOT;
        }
        lockA(); curP().p[track][P_S0] = (uint8_t)sl; eng.preview(track, nullptr); unlockA();
    }
}
static void fbInput(Input& I, Uint32 now) {
    int d = (I.repeat(BT_DOWN, now, 250, 60) ? 1 : 0) - (I.repeat(BT_UP, now, 250, 60) ? 1 : 0);
    d += (I.repeat(BT_R1, now) ? 8 : 0) - (I.repeat(BT_L1, now) ? 8 : 0);
    if (d && fbN) { fbSel = clampi(fbSel + d, 0, fbN - 1); if (fbMode == FB_SAMPLE && aHeld == false && false) fbPreview(); }
    if (I.pressed(BT_A) || I.pressed(BT_RIGHT) || I.pressed(BT_START)) fbPick();
    else if (I.pressed(BT_X)) fbPreview();
    else if (I.pressed(BT_B) || I.pressed(BT_LEFT)) { if (fbDir[0]) fbUp(); else fbClose(true); }
    else if (I.pressed(BT_SELECT)) fbClose(true);
}
static void drawBrowser() {
    int x0 = 30, y0 = 34, w = 580, h = 412;
    rrect(x0, y0, w, h, 14, C_CYAN);
    rrect(x0 + 2, y0 + 2, w - 4, h - 4, 13, C_BG);
    const char* title = fbMode == FB_SAMPLE ? TR("SAMPLE WÄHLEN", "CHOOSE SAMPLE")
                      : (fbMode == FB_PROJECT ? TR("PROJEKT LADEN", "LOAD PROJECT") : TR("SOUND LADEN", "LOAD SOUND"));
    text(x0 + 16, y0 + 10, title, C_CYAN, 2);
    // Pfad
    char path[160];
    const char* root = fbMode == FB_SAMPLE ? "samples" : (fbMode == FB_PROJECT ? "projects" : TR("sounds", "sounds"));
    char shown[128]; snprintf(shown, sizeof shown, "%s", fbDir);
    if (fbMode == FB_PRESET && !strncmp(shown, FB_FACTORY, strlen(FB_FACTORY))) snprintf(shown, sizeof shown, "%s/", TR("WERKSKLÄNGE", "FACTORY SOUNDS"));
    snprintf(path, sizeof path, "/%s/%s", root, shown);
    int pw = textW(path); const char* pp = path;
    while (pw > w - 230 && *pp) { pp++; pw = textW(pp); }
    text(x0 + w - 16 - pw, y0 + 16, pp, C_DIM);
    // Liste
    int rows = 15, rh = 22, ly = y0 + 50;
    float tgt = fbSel < fbScroll + 2 ? fbSel - 2.f : (fbSel > fbScroll + rows - 3 ? fbSel - rows + 3.f : fbScroll);
    if (tgt > fbN - rows) tgt = (float)(fbN - rows);
    if (tgt < 0) tgt = 0;
    fbScroll = tgt;
    int first = (int)fbScroll;
    if (fbN == 0) textC(320, ly + 60, TR("(LEER)", "(EMPTY)"), C_DIM);
    for (int r = 0; r < rows && first + r < fbN; r++) {
        int i = first + r, y = ly + r * rh;
        FbEnt& e = fbEnt[i];
        bool sel = i == fbSel;
        bool cur = (fbMode == FB_PROJECT && !strcmp(e.name, cfg.project)) || (fbMode == FB_SAMPLE && !e.dir && e.idx == fbOrigLib);
        if (sel) rrect(x0 + 10, y - 2, w - 20, rh, 5, C_SEL);
        if (e.dir) {   // Ordnersymbol
            fill(x0 + 20, y + 4, 7, 3, C_LOCK); fill(x0 + 20, y + 6, 14, 10, C_LOCK);
            const char* dn = (fbMode == FB_PRESET && !strcmp(e.name, FB_FACTORY)) ? TR("WERKSKLÄNGE", "FACTORY SOUNDS") : e.name;
            text(x0 + 42, y + 1, dn, sel ? C_LOCK : C_TEXT);
            text(x0 + w - 30, y + 1, ">", C_DIM);
        } else {
            fill(x0 + 22, y + 4, 10, 13, sel ? C_CYAN : C_OFF2); fill(x0 + 24, y + 7, 6, 1, C_BG); fill(x0 + 24, y + 10, 6, 1, C_BG);
            text(x0 + 42, y + 1, e.name, sel ? C_PLAY : C_TEXT);
            if (cur) text(x0 + w - 24 - textW(TR("AKTIV", "ACTIVE")), y + 1, TR("AKTIV", "ACTIVE"), C_GREEN);
        }
    }
    if (fbN > rows) {   // Scrollbalken
        int bh = (h - 100) * rows / fbN, by = ly + (int)((h - 100 - bh) * fbScroll / (fbN - rows));
        fill(x0 + w - 8, ly, 3, h - 100, C_OFF2); fill(x0 + w - 8, by, 3, bh, C_CYAN);
    }
    char cnt[32]; snprintf(cnt, sizeof cnt, "%d/%d", fbN ? fbSel + 1 : 0, fbN);
    text(x0 + w - 16 - textW(cnt), y0 + h - 26, cnt, C_DIM);
    const char* h1 = fbMode == FB_SAMPLE ? TR("[A]WÄHLEN [X]HÖREN [B]ZURÜCK [L1/R1]SEITE [SEL]ABBRUCH", "[A]PICK [X]LISTEN [B]BACK [L1/R1]PAGE [SEL]CANCEL")
                                         : TR("[A]ÖFFNEN/LADEN [B]ZURÜCK [L1/R1]SEITE [SEL]ABBRUCH", "[A]OPEN/LOAD [B]BACK [L1/R1]PAGE [SEL]CANCEL");
    hint(x0 + 16, y0 + h - 26, h1, C_ACC, C_DIM);
}
