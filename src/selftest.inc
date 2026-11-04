// Leistungstest, Stresstest und UI-Tests (wird am Ende von main.cpp eingebunden, nutzt dessen Zustand).
//   groovebox --bench [datei]      Audio-Last je Engine + Worst Case, Zeichenzeit je Seite, Speicher
//   groovebox --selftest [sek]     UI-Szenarien, Bedienweg-Report, UI-Monkey, Engine-Stress (Rueckgabe 1 bei Fehler)
// Auf dem Geraet: SET -> "Leistungstest" (schreibt logs/bench.txt, im Browser unter Logs).

#include <sys/resource.h>

static FILE* benchOut = nullptr;
static void bout(const char* fmt, ...) {
    char b[400];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    fputs(b, stdout);
    if (benchOut) fputs(b, benchOut);
}
static double nowMs() { return SDL_GetPerformanceCounter() * 1000.0 / SDL_GetPerformanceFrequency(); }
static void memInfo(long& rssKb, long& peakKb) {
    rssKb = peakKb = -1;
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return;
    char l[200];
    while (fgets(l, sizeof l, f)) {
        if (!strncmp(l, "VmRSS:", 6)) rssKb = atol(l + 6);
        if (!strncmp(l, "VmHWM:", 6)) peakKb = atol(l + 6);
    }
    fclose(f);
}
static void cpuInfo(char* b, int n) {
    snprintf(b, n, "unbekannt");
    FILE* f = fopen("/proc/cpuinfo", "r");
    int cores = 0; char model[96] = "";
    if (f) {
        char l[256];
        while (fgets(l, sizeof l, f)) {
            if (!strncmp(l, "processor", 9)) cores++;
            if (!model[0] && (!strncmp(l, "model name", 10) || !strncmp(l, "CPU part", 8) || !strncmp(l, "Hardware", 8))) {
                char* c = strchr(l, ':'); if (c) { snprintf(model, sizeof model, "%s", c + 2); model[strcspn(model, "\n")] = 0; }
            }
        }
        fclose(f);
    }
    long mhz = 0;
    FILE* q = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (q) { if (fscanf(q, "%ld", &mhz) != 1) mhz = 0; fclose(q); }
    snprintf(b, n, "%d Kerne, %s%s%ld MHz", cores, model[0] ? model : "?", mhz ? ", " : "", mhz / 1000);
}

// ---------------------------------------------------------------- Audio-Last

static Engine benchEng;   // eigene Engine: die laufende Wiedergabe bleibt unberuehrt
struct BenchRes { double load, peak; float lvl; bool bad; double voices; };
static BenchRes benchRender(Engine& E, double secs) {
    static float blk[256 * 2];
    const int B = 256;
    int blocks = (int)(secs * E.sr / B);
    double budget = B * 1000.0 / E.sr, tot = 0, worst = 0;
    float pk = 0; bool bad = false; double vsum = 0;
    E.start();
    for (int i = 0; i < blocks; i++) {
        double t0 = nowMs();
        E.render(blk, B);
        double el = nowMs() - t0;
        tot += el; if (el > worst) worst = el;
        for (int t = 0; t < NTRACKS; t++) { if (E.v[t].active) vsum += 1; for (int k = 0; k < NCHORDV; k++) if (E.cv[t][k].active) vsum += 1; }
        for (int k = 0; k < B * 2; k++) { float x = blk[k]; if (!(x == x)) bad = true; else if (fabsf(x) > pk) pk = fabsf(x); }
    }
    E.stop();
    BenchRes r = {tot / (blocks * budget) * 100.0, worst / budget * 100.0, pk, bad, vsum / blocks};
    return r;
}
// alle 8 Spuren mit Engine m, jeder Step ein Trig, typische Einstellungen
static void benchSetupEngine(Engine& E, int m) {
    E.defaultProject(false);
    E.songOn = false; E.curPat = 0; E.nextPat = -1;
    E.proj.bpm = 128;
    Pattern& P = E.proj.pat[0];
    for (int t = 0; t < NTRACKS; t++) {
        Engine::initTrack(P, t, m);
        if (m == M_SAMPLE || m == M_CHOP || m == M_GRAN) P.p[t][P_S0] = (uint8_t)(t % (sampleCount() ? sampleCount() : 1));
        for (int s = 0; s < 16; s++) P.s[t][s].on = 1;
        P.p[t][P_LEN] = 60; P.p[t][P_DLY] = 40; P.p[t][P_REV] = 40;
        P.p[t][P_NOTE] = (uint8_t)(48 + t * 3);
    }
}
static void benchSetupWorst(Engine& E) {
    static const int ms[NTRACKS] = {M_SYNTH, M_WTBL, M_FM, M_GRAN, M_CHOP, M_SAMPLE, M_SYNTH, M_SNARE};
    E.defaultProject(false);
    E.songOn = false; E.curPat = 0; E.nextPat = -1;
    E.proj.bpm = 174;
    Pattern& P = E.proj.pat[0];
    for (int t = 0; t < NTRACKS; t++) {
        int m = ms[t];
        Engine::initTrack(P, t, m);
        if (m == M_SAMPLE || m == M_CHOP || m == M_GRAN) P.p[t][P_S0] = (uint8_t)(t % (sampleCount() ? sampleCount() : 1));
        if (m == M_GRAN) { P.p[t][P_S4] = 127; P.p[t][P_S3] = 40; }   // maximale Kornrate
        for (int s = 0; s < 16; s++) P.s[t][s].on = 1;
        P.p[t][P_LEN] = 100; P.p[t][P_DLY] = 90; P.p[t][P_REV] = 90; P.p[t][P_RESO] = 100;
        P.p[t][P_FTYP] = (uint8_t)(t == 0 ? FT_LADR : (t == 6 ? FT_CMBP : (t == 1 ? FT_VOWL : FT_LP24)));
        P.p[t][P_RTRG] = 6;
        P.p[t][P_ITYP] = (uint8_t)(1 + t % (INS_COUNT - 1)); P.p[t][P_IMIX] = 100; P.p[t][P_CRSH] = 40; P.p[t][P_SRR] = 30; P.p[t][P_DRIV] = 60;
        P.p[t][P_BASE] = 20; P.p[t][P_WDTH] = 90;
        static const int lb[3] = {P_L1, P_L2, P_L3};
        for (int k = 0; k < 3; k++) { P.p[t][lb[k] + LF_DEST] = (uint8_t)(k == 0 ? P_FREQ + 1 : (k == 1 ? P_PAN + 1 : P_S2 + 1)); P.p[t][lb[k] + LF_DEPTH] = 110; P.p[t][lb[k] + LF_RATE] = 100; P.p[t][lb[k] + LF_WAVE] = LW_SH; }
        if (!machineIsDrum(m)) { P.p[t][P_AMOD] = (t == 0 || t == 1 || t == 2) ? ARP_CHORD : 1; P.p[t][P_ACHD] = 5; P.p[t][P_AOCT] = 2; P.p[t][P_ARAT] = 0; }   // 3 Spuren mit 4-stimmigen Akkorden
    }
    E.proj.cmp[CM_THR] = 30; E.proj.cmp[CM_SRC] = 1; E.proj.cmp[CM_MIX] = 127;
    E.proj.mst[MS_SWNG] = 10;
}
static void benchAudio() {
    Engine& E = benchEng;
    E.init(44100);
    bout("\nAUDIO (44,1 kHz, Bloecke zu 256 Frames = %.1f ms Budget)\n", 256000.0 / 44100);
    bout("  %-26s %8s %10s %7s %8s %10s\n", "Szenario", "Last", "Spitze", "Pegel", "Stimmen", "je Stimme");
    E.defaultProject(false);
    BenchRes r = benchRender(E, 2.0);
    bout("  %-26s %7.1f%% %9.1f%% %7.2f\n", "Leerlauf (leeres Pattern)", r.load, r.peak, r.lvl);
    double idle = r.load;
    double worstEng = 0; int worstM = 0;
    E.audioThreads = 1;   // Engine-Tabelle mit einem Kern (vergleichbar mit frueheren Messungen)
    for (int i = 0; i < M_COUNT; i++) {
        int m = MACHINE_ORDER[i];
        benchSetupEngine(E, m);
        r = benchRender(E, 3.0);
        char nm[40]; snprintf(nm, sizeof nm, "8x %s", MACHINE_NAMES[m]);
        double per = r.voices > 0.1 ? (r.load - idle) / r.voices : 0;
        bout("  %-26s %7.1f%% %9.1f%% %7.2f %8.1f %9.2f%%%s\n", nm, r.load, r.peak, r.lvl, r.voices, per, r.bad ? "  NAN!" : "");
        if (per > worstEng) { worstEng = per; worstM = m; }
    }
    int thr = E.helperReady() ? 2 : 1;
    for (int k = 1; k <= thr; k++) {   // realistischer Song und Worst Case mit 1 und (falls vorhanden) 2 Kernen
        E.audioThreads = k;
        char nm[48];
        buildDemo(E); E.songOn = true; E.songStart(0);
        r = benchRender(E, 6.0);
        E.songOn = false;
        snprintf(nm, sizeof nm, "DEMO-SONG, %d KERN%s", k, k > 1 ? "E" : "");
        bout("  %-26s %7.1f%% %9.1f%% %7.2f %8.1f\n", nm, r.load, r.peak, r.lvl, r.voices);
        benchSetupWorst(E);
        r = benchRender(E, 4.0);
        snprintf(nm, sizeof nm, "WORST CASE, %d KERN%s", k, k > 1 ? "E" : "");
        bout("  %-26s %7.1f%% %9.1f%% %7.2f %8.1f%s\n", nm, r.load, r.peak, r.lvl, r.voices, r.bad ? "  NAN!" : "");
    }
    E.audioThreads = thr;
    bout("  -> teuerste Engine je Stimme: %s (%.2f%%). Reserve im Worst Case: %.0f%% eines Kerns\n", MACHINE_NAMES[worstM], worstEng, 100.0 - r.load);
    if (r.peak > 80) bout("  !! einzelne Bloecke brauchen >80%% des Budgets: Knackser bei kleinem Puffer moeglich\n");
}

// ---------------------------------------------------------------- Zeichenzeit

static void benchUi(int frames, bool withPresent) {
    int sp0 = page, st0 = track, ss0 = slot; bool mo0 = menuOpen;
    bout("\nBILDAUFBAU (%d Bilder je Seite%s, Ziel < 16,7 ms = 60 fps)\n", frames, withPresent ? ", inkl. Ausgabe aufs Display" : "");
    bout("  %-22s %8s %8s %9s\n", "Seite", "Mittel", "Max", withPresent ? "Ausgabe" : "");
    double sum = 0, worst = 0; int worstP = 0, n = 0;
    for (int p = 0; p <= NUI_PAGES + 1; p++) {
        menuOpen = false;
        if (p < NUI_PAGES) { page = p; if (!pageAvail(p)) continue; }
        else if (p == NUI_PAGES) { page = PG_FLT; menuOpen = true; menuSel = PG_FLT; }
        track = 4; slot = 0;
        double tot = 0, mx = 0, pres = 0;
        for (int i = 0; i < frames; i++) {
            double t0 = nowMs();
            drawAll(p == NUI_PAGES + 1);
            double t1 = nowMs();
            if (withPresent) present();
            double el = nowMs() - t0;
            pres += nowMs() - t1;
            tot += el; if (el > mx) mx = el;
        }
        const char* nm = p < NUI_PAGES ? PAGE_LONG[p] : (p == NUI_PAGES ? "SEITENMENUE" : "HILFE");
        if (withPresent) bout("  %-22s %6.2fms %6.2fms %7.2fms\n", nm, tot / frames, mx, pres / frames);
        else bout("  %-22s %6.2fms %6.2fms\n", nm, tot / frames, mx);
        sum += tot / frames; n++;
        if (tot / frames > worst) { worst = tot / frames; worstP = p; }
    }
    bout("  -> Mittel über alle Seiten %.2f ms, langsamste: %s (%.2f ms)\n", sum / n,
         worstP < NUI_PAGES ? PAGE_LONG[worstP] : (worstP == NUI_PAGES ? "SEITENMENUE" : "HILFE"), worst);
    page = sp0; track = st0; slot = ss0; menuOpen = mo0;
}

static void runBench(const char* outPath, bool onDevice) {
    benchOut = outPath ? fopen(outPath, "w") : nullptr;
    char ci[200]; cpuInfo(ci, sizeof ci);
    time_t tt = time(nullptr); char ts[40]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M", localtime(&tt));
    bout("SYNTH CITY Leistungstest %s  Version %s\n", ts, versionStr[0] ? versionStr : "dev");
    bout("CPU: %s\nSamples: %d   Zeichnen: %s\n", ci, sampleCount(), fbPix ? "eigene Pixel-Routinen" : "SDL-Software-Renderer");
    if (onDevice) bout("Hinweis: die laufende Wiedergabe rechnet nebenher weiter (CPU Audio jetzt %d%%)\n", (int)(cpuLoad * 100));
    benchAudio();
    benchUi(onDevice ? 30 : 60, onDevice && drmSurf);
    long rss, peak; memInfo(rss, peak);
    bout("\nSPEICHER: %ld MB belegt, Spitze %ld MB (Projekt %d KB, Engine %d KB)\n", rss / 1024, peak / 1024,
         (int)(sizeof(Project) / 1024), (int)(sizeof(Engine) / 1024));
    if (benchOut) { fclose(benchOut); benchOut = nullptr; printf("Bericht: %s\n", outPath); }
}

// ---------------------------------------------------------------- UI-Simulation

static Uint32 simNow = 1000000;
static bool simDown[BT_COUNT];
static float simLX = 0, simLY = 0, simRX = 0, simRY = 0;
static int simKeys = 0;   // gezaehlte Bedienhandlungen (Tastendruck, Stick-Antippen, Stick-Geste)
static bool simDraw = false;
static void simFrame() {
    for (int b = 0; b < BT_COUNT; b++) {
        in.prev[b] = in.down[b];
        in.down[b] = simDown[b];
        if (in.down[b] && !in.prev[b]) { in.downSince[b] = simNow; in.lastRep[b] = simNow; }
    }
    in.lx = simLX; in.ly = simLY; in.rx = simRX; in.ry = simRY;
    handleInput(simNow, 0.016f);
    undoTrack(simNow);
    if (simDraw) drawAll(false);
    simNow += 16;
}
static void simWait(int ms) { for (int t = 0; t < ms; t += 16) simFrame(); }
static void tap(int b) { simKeys++; simDown[b] = true; simFrame(); simFrame(); simDown[b] = false; simFrame(); simFrame(); }
static void holdDown(int b) { simKeys++; simDown[b] = true; simFrame(); }
static void release(int b) { simDown[b] = false; simFrame(); simFrame(); }
static void combo(int mod, int b) { holdDown(mod); tap(b); release(mod); }   // z.B. SELECT + A
static void flickL(int dx, int dy) { simKeys++; simLX = (float)dx; simLY = (float)dy; simFrame(); simFrame(); simLX = simLY = 0; simFrame(); simFrame(); simFrame(); }
static void stickR(float ry, int ms) { simKeys++; simRY = ry; simWait(ms); simRY = 0; simFrame(); simFrame(); }
static void simReset() {
    for (int b = 0; b < BT_COUNT; b++) simDown[b] = false;
    simLX = simLY = simRX = simRY = 0;
    for (int i = 0; i < 6; i++) simFrame();
    page = PG_TRG; track = 0; slot = 0; cursor = 0; menuOpen = false; confirmOpen = false; aHeld = false; lockLatch = false;
    eng.curPat = 0; eng.nextPat = -1;
    for (int i = 0; i < 4; i++) simFrame();
    simKeys = 0;
}

static int uiPass = 0, uiFail = 0;
static const char* uiCase = "";
#define UCHECK(c, ...) do { if (c) uiPass++; else { uiFail++; printf("  FEHLER [%s] Zeile %d: ", uiCase, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// Weg zur Seite wie in der Signalweg-Oberflaeche: Bloecke L1/R1, Modulation L2/R2, sonst SELECT-Menue
static int sigIndex(const int* list, int n, int p) { for (int i = 0; i < n; i++) if (list[i] == p) return i; return -1; }
static void goPage(int target) {
    for (int guard = 0; guard < 24 && page != target; guard++) {
        int ci = sigIndex(SIG_CHAIN, SIG_NCH, target), mi = sigIndex(SIG_MOD, SIG_NMOD, target);
        if (trackPage(page) && (ci >= 0 || mi >= 0)) {
            const int* list = ci >= 0 ? SIG_CHAIN : SIG_MOD;
            int n = ci >= 0 ? SIG_NCH : SIG_NMOD, tgt = ci >= 0 ? ci : mi;
            int cur = sigIndex(list, n, page);
            int dir = (cur < 0 || cur < tgt) ? 1 : -1;
            tap(ci >= 0 ? (dir > 0 ? BT_R1 : BT_L1) : (dir > 0 ? BT_R2 : BT_L2));
        } else {   // Menue: SELECT, Gruppe/Seite mit dem Steuerkreuz, A
            tap(BT_SELECT);
            for (int k = 0; k < 6 && PG_GROUP[menuSel] != PG_GROUP[target]; k++) tap(PG_GROUP[menuSel] < PG_GROUP[target] ? BT_DOWN : BT_UP);
            for (int k = 0; k < NUI_PAGES && menuSel != target; k++) tap(menuSel < target ? BT_RIGHT : BT_LEFT);
            tap(BT_A);
        }
    }
}
static void goSlot(int target) {   // linker Stick: je Antippen ein Feld
    while (slot / 4 != target / 4) flickL(0, slot / 4 < target / 4 ? -1 : 1);
    while (slot % 4 != target % 4) flickL(slot % 4 < target % 4 ? 1 : -1, 0);
}
static void goTrack(int t) { while (track != t) tap(track < t ? BT_DOWN : BT_UP); }
static void goStep(int s) { while (cursor != s) tap(cursor < s ? BT_RIGHT : BT_LEFT); }

static void uiScenarios() {
    Pattern& P = curP();
    uiCase = "Trig setzen/löschen";
    simReset(); P.s[0][0].on = 0;
    tap(BT_A); UCHECK(curP().s[0][0].on == 1, "A setzt keinen Trig");
    tap(BT_A); UCHECK(curP().s[0][0].on == 0, "A loescht den Trig nicht");

    uiCase = "Navigation";
    simReset();
    tap(BT_RIGHT); tap(BT_RIGHT); tap(BT_DOWN);
    UCHECK(cursor == 2 && track == 1, "Cursor %d Spur %d statt 2/1", cursor, track);
    tap(BT_R2); UCHECK(page == PG_SEQ, "R2: Modulationsreihe -> SEQ erwartet (Seite %d)", page);
    tap(BT_L2); UCHECK(page == PG_TRG, "L2 kehrt nicht zurueck (Seite %d)", page);
    tap(BT_R1); UCHECK(page == PG_SRC, "R1: Signalweg -> QUELLE erwartet (Seite %d)", page);
    tap(BT_R1); UCHECK(page == PG_FLT || page == PG_SRC2, "R1: naechster Block (Seite %d)", page);
    page = PG_TRG; simWait(32);
    flickL(1, 0); UCHECK(slot == 1, "Linker Stick: Kachel %d statt 1", slot);
    flickL(0, -1); UCHECK(slot == 5, "Linker Stick runter: Kachel %d statt 5", slot);

    uiCase = "Wert aendern";
    simReset(); goPage(PG_FLT); goSlot(0);
    int f0 = curP().p[0][P_FREQ];
    tap(BT_Y); UCHECK(curP().p[0][P_FREQ] == f0 - 1, "Y: FREQ %d statt %d", curP().p[0][P_FREQ], f0 - 1);
    stickR(1.f, 400); UCHECK(curP().p[0][P_FREQ] > f0 - 1, "rechter Stick hebt FREQ nicht");

    uiCase = "P-Lock";
    simReset(); goPage(PG_FLT); goSlot(0);
    goStep(4);
    curP().s[0][4].on = 0; memset(curP().s[0][4].lock, NO_LOCK, NPARAMS);
    holdDown(BT_A); tap(BT_Y); release(BT_A);
    UCHECK(curP().s[0][4].on == 1 && curP().s[0][4].lock[P_FREQ] != NO_LOCK, "A halten + Y: kein Lock (on=%d)", curP().s[0][4].on);
    tap(BT_A);
    UCHECK(curP().s[0][4].on == 2, "Trig mit Lock antippen -> trigless erwartet (on=%d)", curP().s[0][4].on);
    tap(BT_A);
    UCHECK(curP().s[0][4].on == 1, "trigless antippen -> Trig erwartet (on=%d)", curP().s[0][4].on);

    uiCase = "Lock-Modus";
    simReset(); goPage(PG_FLT);
    holdDown(BT_A); simWait(600); release(BT_A);
    UCHECK(lockLatch, "A lang halten rastet den Lock-Modus nicht ein");
    tap(BT_X); tap(BT_RIGHT);
    UCHECK(cursor == 1 && lockLatch, "im Lock-Modus: Rechts -> Step %d", cursor);
    tap(BT_A); UCHECK(!lockLatch, "A beendet den Lock-Modus nicht");

    uiCase = "Trig kopieren";
    simReset();
    curP().s[0][0].on = 1; curP().s[0][0].lock[P_VEL] = 33;
    holdDown(BT_A); tap(BT_L1); release(BT_A);
    goStep(3);
    holdDown(BT_A); tap(BT_R1); release(BT_A);
    UCHECK(curP().s[0][3].on == 1 && curP().s[0][3].lock[P_VEL] == 33, "eingefuegter Trig: on=%d VEL-Lock=%d", curP().s[0][3].on, curP().s[0][3].lock[P_VEL]);

    uiCase = "Mute/Play";
    simReset();
    bool m0 = eng.mute[0];
    combo(BT_SELECT, BT_A); UCHECK(eng.mute[0] != m0, "SELECT+A schaltet nicht stumm");
    combo(BT_SELECT, BT_A);
    tap(BT_START); UCHECK(eng.playing, "START spielt nicht");
    tap(BT_START); UCHECK(!eng.playing, "START stoppt nicht");

    uiCase = "Seitenmenue";
    simReset();
    tap(BT_SELECT); UCHECK(menuOpen, "SELECT antippen oeffnet kein Menue");
    tap(BT_B); tap(BT_SELECT);
    if (menuOpen) { tap(BT_B); }
    UCHECK(!menuOpen, "Menue laesst sich nicht schliessen");

    uiCase = "Song-Seite verlassen";
    simReset(); page = PG_SONG; simWait(50);
    tap(BT_R2); UCHECK(page != PG_SONG, "R2 verlaesst die Song-Seite nicht");
    page = PG_SONG; simWait(50);
    tap(BT_L2); UCHECK(page != PG_SONG, "L2 verlaesst die Song-Seite nicht");

    uiCase = "Einstellungen";
    simReset(); page = PG_SYS; simWait(50);
    tap(BT_DOWN); tap(BT_DOWN);
    tap(BT_B); UCHECK(page != PG_SYS, "B verlaesst SET nicht");

    uiCase = "Rückgängig";
    simReset(); curP().s[0][0].on = 0; simWait(900);
    tap(BT_A); simWait(900);
    UCHECK(curP().s[0][0].on == 1, "Trig nicht gesetzt");
    combo(BT_SELECT, BT_B);
    UCHECK(curP().s[0][0].on == 0, "SELECT+B macht den Trig nicht rückgängig");
    redoStep(); UCHECK(curP().s[0][0].on == 1, "Wiederholen stellt den Trig nicht her");
    curP().s[0][0].on = 0; simWait(900);

    uiCase = "Beat-Sprung";
    simReset(); combo(BT_SELECT, BT_R1); UCHECK(cursor == 4, "SELECT+R1: Step %d statt 5", cursor + 1);
    combo(BT_SELECT, BT_L1); UCHECK(cursor == 0, "SELECT+L1: Step %d statt 1", cursor + 1);

    uiCase = "Rhythmus-Vorlage";
    simReset(); track = 2; applyRhythm(0);
    UCHECK(curP().s[2][0].on == 1 && curP().s[2][4].on == 1 && curP().s[2][1].on == 0, "Four on the Floor falsch");
    applyRhythm(RHY_N - 1); simWait(900);

    uiCase = "LIVE-Seite";
    simReset(); goPage(PG_LIVE);
    UCHECK(page == PG_LIVE, "LIVE nicht erreichbar (Seite %d)", page);
    bool m5 = eng.mute[4];
    tap(BT_UP); UCHECK(eng.mute[0], "Steuerkreuz hoch schaltet Spur 1 nicht stumm");
    tap(BT_X); UCHECK(eng.mute[4] != m5, "X schaltet Spur 5 nicht");
    tap(BT_UP); tap(BT_X);
    holdDown(BT_L1); UCHECK(eng.fill, "L1 halten = FILL"); release(BT_L1); UCHECK(!eng.fill, "FILL bleibt nach Loslassen");
    eng.start();
    tap(BT_R1); UCHECK(recOn, "R1 startet keine Aufnahme");
    { static float tmpb[44100 * 2]; recPush(tmpb, 44100); }
    tap(BT_R1); UCHECK(!recOn, "R1 stoppt die Aufnahme nicht");
    for (int i = 0; i < 200 && recSaving; i++) SDL_Delay(10);
    UCHECK(recDoneNo > 0, "Aufnahme nicht gespeichert (%d)", recDoneNo);
    recDoneNo = 0; eng.stop();

    uiCase = "Speichern";
    simReset();
    curP().s[6][9].on = 1; curP().s[6][9].lock[P_VEL] = 77;
    combo(BT_SELECT, BT_START);
    {
        char tmp[720]; snprintf(tmp, sizeof tmp, "%s.tmp", projPath);
        struct stat st;
        UCHECK(stat(projPath, &st) == 0 && st.st_size >= (long)sizeof(Project) + 6, "Projektdatei fehlt oder hat falsche Groesse (ohne Sample-Liste?)");
        UCHECK(stat(tmp, &st) != 0, "Temp-Datei bleibt liegen");
        static Engine chk; chk.audioThreads = 1;
        UCHECK(chk.load(projPath) && chk.proj.pat[eng.curPat].s[6][9].lock[P_VEL] == 77, "gespeichertes Projekt nicht korrekt ladbar");
    }
    curP().s[6][9].on = 0; curP().s[6][9].lock[P_VEL] = NO_LOCK;

    uiCase = "Projekt speichern als";
    simReset(); goPage(PG_PRJ); goSlot(2); tap(BT_A);
    UCHECK(kbOpen, "SPEICH. ALS oeffnet keine Tastatur");
    for (int i = 0; i < 40; i++) tap(BT_B);
    tap(BT_A); tap(BT_RIGHT); tap(BT_A); tap(BT_DOWN); tap(BT_DOWN); tap(BT_A);   // "A", "b", dann Zeile 3 Spalte 2 = "1"
    UCHECK(!strcmp(kbText, "Ab1"), "Tastatur-Text '%s' statt 'Ab1'", kbText);
    tap(BT_START);
    UCHECK(!kbOpen && projectExists("Ab1") && !strcmp(cfg.project, "Ab1"), "Projekt 'Ab1' nicht gespeichert (aktiv: %s)", cfg.project);

    uiCase = "Projekt laden";
    curP().s[5][5].on = 1; curP().s[5][5].lock[P_VEL] = 11;   // ungespeicherte Aenderung
    simWait(900);
    goSlot(0); tap(BT_A);
    UCHECK(fbOpen && fbMode == FB_PROJECT, "LADEN oeffnet keinen Projekt-Explorer");
    for (int i = 0; i < fbN; i++) if (!strcmp(fbEnt[i].name, "Ab1")) fbSel = i;
    tap(BT_A);
    UCHECK(confirmOpen, "keine Rueckfrage trotz ungespeicherter Aenderung");
    tap(BT_A);
    UCHECK(curP().s[5][5].lock[P_VEL] == NO_LOCK && !strcmp(cfg.project, "Ab1"), "Projekt nicht neu geladen");

    uiCase = "Sound speichern/laden";
    simReset(); goTrack(4); goPage(PG_PRJ); goSlot(5); tap(BT_A);
    UCHECK(kbOpen && kbMode == KB_PSAVE, "SOUND SPEICH. oeffnet keine Tastatur");
    for (int i = 0; i < 40; i++) tap(BT_B);
    tap(BT_DOWN); tap(BT_LEFT); tap(BT_LEFT); tap(BT_LEFT); tap(BT_A);   // Zeile 2: von N dreimal links (Z, Y, X)
    int freq0 = curP().p[4][P_FREQ];
    tap(BT_START);
    { char pp[700]; presetFile(curP().machine[4], "X", pp, sizeof pp); UCHECK(fileExists(pp), "Sound-Datei X fehlt"); }
    curP().p[4][P_FREQ] = (uint8_t)(freq0 ^ 1);
    goSlot(4); tap(BT_A);
    UCHECK(fbOpen && fbMode == FB_PRESET, "SOUND LADEN oeffnet keinen Explorer");
    for (int i = 0; i < fbN; i++) if (fbEnt[i].dir && fbEnt[i].idx == curP().machine[4]) fbSel = i;
    tap(BT_A);
    for (int i = 0; i < fbN; i++) if (!strcmp(fbEnt[i].name, "X")) fbSel = i;
    tap(BT_A);
    UCHECK(!fbOpen && curP().p[4][P_FREQ] == freq0, "Sound nicht geladen (FREQ %d statt %d)", curP().p[4][P_FREQ], freq0);

    uiCase = "Sample-Explorer";
    if (libCount() > 1) {
        simReset(); goTrack(7); Engine::setMachine(curP(), 7, M_SAMPLE); goPage(PG_SRC); goSlot(0);
        tap(BT_X);
        UCHECK(fbOpen && fbMode == FB_SAMPLE, "X auf SMPL oeffnet keinen Explorer");
        while (fbDir[0]) tap(BT_B);   // zur Wurzel
        int dirs = 0; for (int i = 0; i < fbN; i++) dirs += fbEnt[i].dir;
        UCHECK(dirs > 0, "keine Unterordner sichtbar");
        fbSel = 0; tap(BT_A);   // in den ersten Ordner
        int want = -1; for (int i = 0; i < fbN; i++) if (!fbEnt[i].dir) { fbSel = i; want = fbEnt[i].idx; break; }
        tap(BT_A);
        int got = curP().p[7][P_S0];
        UCHECK(!fbOpen && want >= 0 && sampleGet(got) && !strcmp(sampleName(got), libName(want)), "Sample nicht gewaehlt ('%s' statt '%s')", sampleName(got), libName(want));
        tap(BT_X);   // oeffnet im Ordner des gewaehlten Samples
        for (int i = 0; i < fbN; i++) if (!fbEnt[i].dir) { fbSel = i; break; }
        tap(BT_X);   // Vorhoeren laedt in den Vorhoer-Platz (oder nutzt den Pool-Platz) ...
        tap(BT_SELECT);   // ... Abbruch stellt das alte Sample wieder her
        UCHECK(!fbOpen && curP().p[7][P_S0] == got && !sampleName(PREVIEW_SLOT)[0], "Abbruch nach Vorhoeren: Platz %d statt %d", curP().p[7][P_S0], got);
    }

    uiCase = "Sample-Pool";
    if (libCount() > 1) {
        simReset(); Engine::setMachine(curP(), 7, M_SAMPLE);
        char want[96]; snprintf(want, sizeof want, "%s", libName(libCount() - 1));
        int sl = poolAssign(want);
        UCHECK(sl >= 0 && !strcmp(sampleName(sl), want), "poolAssign laedt '%s' nicht", want);
        if (sl >= 0) {
            curP().p[7][P_S0] = (uint8_t)sl;
            doSaveProjectAs("PoolTest");
            poolSetSlot(sl, "");   // Pool leeren: Laden muss das Sample per Name wiederfinden
            doLoadProject("PoolTest");
            int s2 = curP().p[7][P_S0];
            UCHECK(sampleGet(s2) && !strcmp(sampleName(s2), want), "nach Projekt-Laden '%s' statt '%s'", sampleName(s2), want);
            char pp[700]; presetFile(M_SAMPLE, "PoolSound", pp, sizeof pp);
            UCHECK(savePresetFile(7, pp), "Sound mit Sample nicht gespeichert");
            poolSetSlot(s2, "");
            UCHECK(loadPresetPath(7, pp) == 1, "Sound mit Sample nicht ladbar");
            int s3 = curP().p[7][P_S0];
            UCHECK(sampleGet(s3) && !strcmp(sampleName(s3), want), "Sound: Sample '%s' statt '%s'", sampleName(s3), want);
            remove(pp);
        }
        static char l1[128][96], l2[250][96];   // altes Projekt ohne Namensliste: Nummer = Position in der frueheren Liste
        int n1 = libLegacyList(l1, 128, 1, 256), n2 = libLegacyList(l2, 250, SAMPLE_DEPTH, 500);
        if (n1 > 1 && n2 > 1) {
            for (int p = 0; p < NPATTERNS; p++) for (int t = 0; t < NTRACKS; t++) {
                if (machineUsesSample(eng.proj.pat[p].machine[t] % M_COUNT)) eng.proj.pat[p].p[t][P_S0] = 1;
                for (int s = 0; s < NSTEPS; s++) eng.proj.pat[p].s[t][s].lock[P_S0] = NO_LOCK;
            }
            for (int k = 0; k < 2; k++) {   // songNN = Zeit der 1-Ebenen-Liste, benannte Projekte = 250er-Liste
                const char* nm = k ? "PoolAlt" : "song99";
                char lp[700]; projectPath(nm, lp, sizeof lp);
                FILE* f = fopen(lp, "wb"); if (f) { fwrite(&eng.proj, sizeof(Project), 1, f); fclose(f); }
                doLoadProject(nm);
                const char* want = k ? l2[1] : l1[1];
                int s4 = curP().p[7][P_S0];
                UCHECK(poolLegacy == k && sampleGet(s4) && !strcmp(sampleName(s4), want), "altes Projekt %s: Liste %d, '%s' statt '%s'", nm, poolLegacy, sampleName(s4), want);
                setChange(SI_OLDSMP, 1);   // SET: andere Reihenfolge
                const char* other = k ? l1[1] : l2[1];
                UCHECK(poolLegacy == 1 - k && !strcmp(sampleName(curP().p[7][P_S0]), other), "SET Alte Sample-Nummern: '%s' statt '%s'", sampleName(curP().p[7][P_S0]), other);
                remove(lp);
            }
        }
        char p2[700]; projectPath("PoolTest", p2, sizeof p2); remove(p2);
        doLoadProject("Ab1");
    }

    uiCase = "Pattern wechseln";
    simReset();
    combo(BT_SELECT, BT_RIGHT); UCHECK(eng.curPat == 1, "SELECT+Rechts: Pattern %d statt 2", eng.curPat + 1);
    combo(BT_SELECT, BT_LEFT);
    for (int t = 0; t < NTRACKS; t++) for (int s = 0; s < NSTEPS; s++) curP().s[t][s].on = 0;
}

// Bedienweg: Tastenzahl je typischer Aufgabe (je kleiner, desto schneller). Startpunkt: TRIG-Seite, Spur 1, Step 1.
struct Task { const char* name; int keys; int steps; };
static void uiTasks() {
    Task T[16]; int n = 0;
    auto rec = [&](const char* nm, int steps) { T[n].name = nm; T[n].keys = simKeys; T[n].steps = steps; n++; };
    simReset(); for (int s = 0; s < 16; s += 4) { if (s) combo(BT_SELECT, BT_R1); tap(BT_A); } rec("Kick: 4 Trigs (1/5/9/13)", 4);
    UCHECK(curP().s[0][12].on == 1, "Aufgabe Kick");
    simReset(); goTrack(4); goPage(PG_FLT); goSlot(0); stickR(-1.f, 300); rec("Spur 5: Filter-Cutoff aendern", 4);
    simReset(); goTrack(4); goPage(PG_FLT); goSlot(0); goStep(6); holdDown(BT_A); stickR(-1.f, 300); release(BT_A); rec("Spur 5: Cutoff-Lock auf Step 7", 5);
    UCHECK(curP().s[4][6].lock[P_FREQ] != NO_LOCK, "Aufgabe Cutoff-Lock");
    simReset(); goTrack(2); combo(BT_SELECT, BT_X); rec("Spur 3: Engine wechseln", 2);
    simReset(); combo(BT_SELECT, BT_RIGHT); rec("Naechstes Pattern", 1);
    simReset(); goTrack(3); combo(BT_SELECT, BT_A); rec("Spur 4 stumm", 2);
    combo(BT_SELECT, BT_A);
    simReset(); goPage(PG_MST); goSlot(MS_BPM); stickR(1.f, 300); rec("Tempo aendern", 3);
    simReset(); goTrack(1); goPage(PG_AMP); goSlot(6); stickR(1.f, 300); rec("Spur 2: Delay-Send", 4);
    simReset(); goTrack(4); goPage(PG_LFO1); goSlot(LF_DEST); for (int i = 0; i < P_FREQ + 1; i++) tap(BT_X); goSlot(LF_DEPTH); stickR(1.f, 300); rec("Spur 5: LFO1 -> Filter", 5);
    simReset(); goTrack(4); goPage(PG_AMP); goSlot(0); stickR(1.f, 200); goSlot(3); stickR(1.f, 200); rec("Spur 5: Attack + Release", 5);
    simReset(); goTrack(4); goPage(PG_ARP); goSlot(0); tap(BT_X); rec("Spur 5: Arpeggiator an", 3);
    simReset(); goTrack(4); goPage(PG_SEQ); goSlot(1); stickR(1.f, 200); rec("Spur 5: Spurlänge", 3);
    simReset(); combo(BT_SELECT, BT_START); rec("Projekt speichern", 1);
    simReset(); tap(BT_SELECT); simWait(50); rec("Seitenmenue oeffnen", 1);
    tap(BT_B); if (menuOpen) tap(BT_SELECT);
    bout("\nBEDIENWEG - Signalweg-Oberflaeche (Bedienhandlungen je Aufgabe, Start: Spur 1, TRIG-Seite)\n");
    bout("  %-32s %6s %9s\n", "Aufgabe", "Tasten", "ca. Zeit");
    int sum = 0;
    for (int i = 0; i < n; i++) {
        double sec = T[i].keys * 0.28 + T[i].steps * 1.2;   // Keystroke-Level-Modell: Taste 0,28 s + Ueberlegen 1,2 s je Teilschritt
        bout("  %-32s %6d %7.1f s%s\n", T[i].name, T[i].keys, sec, T[i].keys >= 12 ? "  <- lang" : "");
        sum += T[i].keys;
    }
    bout("  Summe %d Tasten\n", sum);
}

// UI-Monkey: zufaellige Eingaben + Zeichnen, prueft Grenzen und Absturzfreiheit
static void uiMonkey(int frames) {
    uint32_t r = 12345;
    auto rnd = [&]() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return r; };
    simReset();
    simDraw = true;
    int bad = 0;
    for (int i = 0; i < frames; i++) {
        uint32_t x = rnd();
        if (x % 7 == 0) { int b = (int)(rnd() % BT_COUNT); if (b != BT_START || rnd() % 4 == 0) simDown[b] = !simDown[b]; }
        if (x % 11 == 0) { simLX = (float)((int)(rnd() % 3) - 1); simLY = (float)((int)(rnd() % 3) - 1); }
        if (x % 13 == 0) { simRX = ((int)(rnd() % 200) - 100) / 100.f; simRY = ((int)(rnd() % 200) - 100) / 100.f; }
        if (x % 97 == 0) for (int b = 0; b < BT_COUNT; b++) simDown[b] = false;
        if (confirmOpen && x % 3 == 0) { confirmOpen = false; }   // keine Abfragen bestaetigen (Laden/Loeschen)
        quitReq = false;
        simFrame();
        if (page < 0 || page >= NUI_PAGES || track < 0 || track >= NTRACKS || slot < 0 || slot > 7 || cursor < 0 || cursor >= NSTEPS) {
            if (bad++ < 5) printf("  FEHLER Monkey Bild %d: page %d track %d slot %d cursor %d\n", i, page, track, slot, cursor);
        }
    }
    simDraw = false;
    for (int b = 0; b < BT_COUNT; b++) simDown[b] = false;
    UCHECK(bad == 0, "Monkey: %d Zustandsfehler", bad);
    printf("  UI-Monkey: %d zufaellige Bilder ok\n", frames);
}

// Engine-Stress: zufaellige Parameter, Locks, Engines, Tempo, MIDI, Speichern/Laden waehrend der Wiedergabe
static void engineStress(double secs) {
    uint32_t r = 777;
    auto rnd = [&]() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return r; };
    static float blk[512 * 2];
    eng.defaultProject(true);
    eng.start();
    int blocks = (int)(secs * eng.sr / 256);
    float pk = 0; int nan = 0; double tot = 0, worst = 0;
    char tmpPath[700]; snprintf(tmpPath, sizeof tmpPath, "%sstress.gbx", basePath);
    for (int i = 0; i < blocks; i++) {
        for (int k = 0; k < 4; k++) {
            uint32_t x = rnd();
            Pattern& P = eng.proj.pat[eng.curPat];
            int t = (int)(x % NTRACKS), pi = (int)((x >> 3) % NPARAMS), s = (int)((x >> 10) % NSTEPS);
            ParamDef d = paramDef(P.machine[t], pi);
            int v = d.max > 0 ? (int)(rnd() % (d.max + 1)) : 0;
            if (pi == P_QLEN) { P.len[t] = (uint8_t)(1 + v % NSTEPS); continue; }
            if (pi == P_QSPD) { P.speed[t] = (uint8_t)(v % 7); continue; }
            switch ((x >> 20) % 8) {
            case 0: case 1: P.p[t][pi] = (uint8_t)v; break;
            case 2: P.s[t][s].on = (uint8_t)(rnd() % 3); P.s[t][s].lock[pi] = (uint8_t)v; break;
            case 3: P.s[t][s].on = (uint8_t)(rnd() % 2); break;
            case 4: if (rnd() % 50 == 0) Engine::setMachine(P, t, (int)(rnd() % M_COUNT)); break;
            case 5: if (rnd() % 200 == 0) eng.proj.bpm = (uint16_t)(30 + rnd() % 271); break;
            case 6: eng.midiIn((uint8_t)(0x90 | (rnd() % 8)), (uint8_t)(rnd() % 128), (uint8_t)(rnd() % 128)); break;
            case 7: if (rnd() % 300 == 0) eng.nextPat = (int)(rnd() % NPATTERNS); break;
            }
        }
        if (i % 2000 == 1999) { eng.save(tmpPath); eng.load(tmpPath); }
        int n = 64 + (int)(rnd() % 449);   // wechselnde Blockgroessen
        double t0 = nowMs();
        eng.render(blk, n);
        double el = nowMs() - t0; tot += el; if (el > worst) worst = el;
        for (int k = 0; k < n * 2; k++) { float y = blk[k]; if (!(y == y) || y > 1e6f || y < -1e6f) nan++; else if (fabsf(y) > pk) pk = fabsf(y); }
    }
    eng.stop();
    remove(tmpPath);
    UCHECK(nan == 0, "Stress: %d NaN/Inf-Werte", nan);
    UCHECK(pk <= 1.01f, "Stress: Ausgang %.2f über Vollaussteuerung", pk);
    printf("  Engine-Stress: %.0f s Audio, Spitze %.2f, Last %.1f%%, laengster Block %.2f ms\n", secs, pk, tot / (secs * 1000) * 100, worst);
}

// Sprachtest: alles auf Englisch zeichnen und angezeigte Texte auf deutsche Reste pruefen
static char langSeen[1500][100]; static int langN = 0;
static void langHook(const char* t) {
    if (strlen(t) < 2 || langN >= 1500) return;
    for (int i = 0; i < langN; i++) if (!strcmp(langSeen[i], t)) return;
    snprintf(langSeen[langN++], 100, "%s", t);
}
static bool looksGerman(const char* t) {
    if (strstr(t, "Sprache / Language") || !strcmp(t, "Deutsch")) return false;
    for (const unsigned char* c = (const unsigned char*)t; *c; c++) if (*c == 0xC3) return true;   // Umlaut/ß
    static const char* const W[] = {"SPUR", "SPUREN", "WAEHLEN", "LADEN", "SPEICHERN", "STUMM", "LOESCHEN", "ZURUECK", "KLANG", "AUS", "AN", "LEER",
        "SEITE", "QUELLE", "WERT", "REGLER", "NEU", "UND", "DER", "DIE", "DAS", "MIT", "VON", "IST", "EIN", "EINE", "NICHT", "ALLE", "PROJEKT",
        "EINSTELLUNGEN", "TASTEN", "ABBRUCH", "AKTIV", "GELADEN", "GESPEICHERT", "SCHLAG", "STEUERKREUZ", "AUFNAHME", "SCHRITTE", "AUSSETZER",
        "EFFEKTE", "KOMPRESSOR", "MEHR", "INTERN", "AUSGANG", "PUFFER", "OHNE", "HALTEN", "ZIEL", "SENDEN", "SUMME", "BEREIT", "KANAL", "ZEILE", "EINTRAG", 0};
    char tok[64]; int k = 0;
    for (const char* c = t;; c++) {
        if (isalpha((unsigned char)*c) && k < 63) { tok[k++] = (char)toupper((unsigned char)*c); continue; }
        tok[k] = 0;
        if (k >= 2) for (int i = 0; W[i]; i++) if (!strcmp(tok, W[i])) return true;
        k = 0;
        if (!*c) break;
    }
    return false;
}
static void langCheck() {
    int saveLang = g_lang;
    g_lang = 1; langN = 0; textHook = langHook; msgUntil = 0; msg[0] = 0;
    simReset();
    static const int engs[] = {M_KICK, M_SYNTH, M_FM, M_WTBL, M_SAMPLE, M_CHOP, M_GRAN};
    for (int e : engs) {
        Engine::setMachine(curP(), 4, e); track = 4;
        for (int p = 0; p < NUI_PAGES; p++) {
            if (!pageAvail(p)) continue;
            page = p;
            for (int sl = 0; sl < 8; sl++) { slot = sl; drawAll(false); }
        }
    }
    page = PG_SYS; for (int i = 0; i < SI_COUNT; i++) { setSel = i; setScroll = i - 7.5f; drawAll(false); }
    page = PG_TRG; drawAll(true);                                     // Hilfe
    menuOpen = true; drawAll(false); menuOpen = false;
    for (int m = KB_SAVEAS; m <= KB_PSAVE; m++) { openKeyboard(m, "x"); drawAll(false); kbOpen = false; }
    for (int m = FB_SAMPLE; m <= FB_PRESET; m++) { openBrowser(m); drawAll(false); fbClose(true); }
    eng.recording = true; drawAll(false); eng.recording = false;
    textHook = nullptr; g_lang = saveLang;
    int bad = 0;
    for (int i = 0; i < langN; i++) if (looksGerman(langSeen[i])) { if (bad++ < 40) printf("  DEUTSCH IN ENGLISCH: '%s'\n", langSeen[i]); }
    printf("  Sprachtest: %d verschiedene Texte, %d mit deutschen Resten\n", langN, bad);
    UCHECK(bad == 0, "Englische Oberflaeche enthaelt %d deutsche Texte", bad);
    Engine::setMachine(curP(), 4, M_SYNTH);
}

static int runSelftest(double stressSecs) {
    selfTesting = true;
    char tmp[600]; snprintf(tmp, sizeof tmp, "/tmp/gb-selftest-%d/", (int)getpid());
    mkdir(tmp, 0755);
    char sub[700]; snprintf(sub, sizeof sub, "%sprojects", tmp); mkdir(sub, 0755);
    char smp[700]; snprintf(smp, sizeof smp, "%ssamples", tmp);
    char src[700]; snprintf(src, sizeof src, "%ssamples", basePath);
    if (symlink(src, smp) != 0) {}
    snprintf(basePath, sizeof basePath, "%s", tmp);
    snprintf(cfgPath, sizeof cfgPath, "%sinput.cfg", tmp);
    snprintf(cfg.project, sizeof cfg.project, "Selbsttest");
    projectPath(cfg.project, projPath, sizeof projPath);
    printf("UI-Szenarien\n");
    uiScenarios();
    uiTasks();
    printf("Sprachtest\n");
    uiCase = "Sprache";
    langCheck();
    printf("UI-Monkey\n");
    uiMonkey(20000);
    printf("Engine-Stress\n");
    engineStress(stressSecs);
    printf("%s: %d UI-Pruefungen ok, %d Fehler\n", uiFail ? "FEHLGESCHLAGEN" : "OK", uiPass, uiFail);
    char cmd[700]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", tmp);
    if (system(cmd) != 0) {}
    return uiFail ? 1 : 0;
}
