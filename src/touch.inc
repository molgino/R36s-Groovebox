// Touch aus der Browser-Fernbedienung (/screen): Finger auf dem Bild -> Flaechen aus hitAdd() (zuletzt gezeichnetes Bild)
//   Antippen: Spur / Seite / Kachel / Reiter / Kartenfeld waehlen, Step = wie A (antippen = Trig an/aus, halten = P-Lock)
//   Ziehen auf einer Kachel (hoch/rechts = mehr): Wert aendern; mit einem zweiten Finger auf einem Step = P-Lock
//   Kachel nochmal antippen: Auswahlliste (Sample, LFO-Ziel, Engine, Zeichnen) wie X
struct TouchPtr { bool on; int kind, a, b, x0, y0, lx, ly; Uint32 t0; bool moved, wasSel, emuA, emuBtn; int btn; float acc; };
static TouchPtr tch[REMOTE_TOUCH_IDS];

static const Hit* hitAt(int x, int y) {
    for (int i = nHits - 1; i >= hitModalAt; i--) {
        const Hit& h = hits[i];
        if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) return &h;
    }
    return nullptr;
}
static void touchEmuDown(int b) { remoteDown[b] = true; remoteHit[b] = true; }
static void touchEmuUp(int b) {   // nur loslassen, wenn kein anderer Finger dieselbe Taste haelt
    for (int i = 0; i < REMOTE_TOUCH_IDS; i++) if (tch[i].on && ((b == BT_A && tch[i].emuA) || (tch[i].emuBtn && tch[i].btn == b))) return;
    remoteDown[b] = false;
}
static void touchDrag(TouchPtr& t, int x, int y) {
    int dx = x - t.lx, dy = y - t.ly;
    t.lx = x; t.ly = y;
    if (abs(x - t.x0) + abs(y - t.y0) > 8) t.moved = true;
    if (!t.moved) return;
    if (t.kind == HK_SLOT || t.kind == HK_MIXCH) {
        if (t.kind == HK_SLOT) slot = t.a;
        float per = curMax() >= 100 ? 3.f : 16.f;   // grosse Bereiche fein, kleine (Typen, Oktaven) grob
        t.acc += (dx - dy) / per;
        int d = (int)t.acc;
        if (d) { t.acc -= d; editValue(d); if (aHeld) aEdited = true; }
    } else if (t.kind == HK_FBROW && fbOpen && fbN > 15) {   // Liste mit dem Finger schieben (Auswahl bleibt sichtbar)
        fbScroll -= dy / 22.f;
        if (fbScroll > fbN - 15) fbScroll = (float)(fbN - 15);
        if (fbScroll < 0) fbScroll = 0;
        int lo = (int)ceilf(fbScroll) + 2, hi = (int)fbScroll + 12;   // ceil: sonst zieht drawBrowser die Liste zurueck
        if (fbSel < lo) fbSel = lo; if (fbSel > hi) fbSel = hi;
        if (fbSel >= fbN) fbSel = fbN - 1;
    } else if (t.kind == HK_SET && !setIsAction(t.a)) {
        t.acc += dx / 22.f;
        int d = (int)t.acc;
        if (d) { t.acc -= d; setSel = t.a; setChange(t.a, d > 0 ? 1 : -1); }
    }
}
static void touchDown(TouchPtr& t, int x, int y, Uint32 now) {
    const Hit* h = hitAt(x, y);
    t = TouchPtr{};
    t.on = true; t.x0 = t.lx = x; t.y0 = t.ly = y; t.t0 = now; t.kind = h ? h->k : -1;
    if (!h) return;
    t.a = h->a; t.b = h->b;
    switch (h->k) {
    case HK_TRACK: track = h->a; break;
    case HK_PAGE: if (pageAvail(h->a)) page = h->a; break;
    case HK_MENU: if (pageAvail(h->a)) { page = h->a; menuSel = h->a; } menuOpen = false; break;
    case HK_SLOT: t.wasSel = slot == h->a; slot = h->a; break;
    case HK_MIXCH: track = h->a; break;
    case HK_MUTE: eng.toggleMute(h->a); break;
    case HK_SOLO: eng.solo[h->a] = !eng.solo[h->a]; break;
    case HK_LIVE: eng.toggleMute(h->a); break;
    case HK_STEP: {
        bool other = false;
        for (int i = 0; i < REMOTE_TOUCH_IDS; i++) if (&tch[i] != &t && tch[i].on && tch[i].emuA) other = true;
        if (lockLatch) {   // Lock-Modus: gleicher Step = fertig, anderer Step derselben Spur = dorthin springen
            if (h->b >= 0 && h->b != holdTrack) break;
            if (h->a == holdStep) remoteHit[BT_A] = true; else lockJumpTo(h->a);
            break;
        }
        if (other) break;   // nur ein gehaltener Step
        if (h->b >= 0) track = h->b;
        cursor = h->a;
        t.emuA = true; touchEmuDown(BT_A);
    } break;
    case HK_BTN: t.emuBtn = true; t.btn = h->a; touchEmuDown(h->a); break;
    case HK_YES: remoteHit[BT_A] = true; break;
    case HK_NO: remoteHit[BT_B] = true; break;
    case HK_PRJ: t.wasSel = slot == h->a; slot = h->a; break;
    case HK_SET: t.wasSel = setSel == h->a && setIn; setSel = h->a; setIn = true; break;
    case HK_SONG: t.wasSel = songSel == h->a && songCol == h->b; songSel = h->a; songCol = h->b; break;
    case HK_BLOCK: if (menuOpen) menuOpen = false; break;   // neben die Karte getippt: schliessen
    case HK_LOCKEND: if (lockLatch) remoteHit[BT_A] = true; break;
    case HK_FBROW: t.wasSel = fbSel == h->a; fbSel = h->a; break;
    case HK_FBBACK: if (fbDir[0]) fbUp(); else fbClose(true); break;
    case HK_FBCANCEL: fbClose(true); break;
    case HK_FBPLAY: fbPreview(); break;
    }
}
static void touchUp(TouchPtr& t, Uint32 now) {
    bool tap = !t.moved && now - t.t0 < 500;
    if (tap) {
        if (t.kind == HK_PRJ) prjTrigger(t.a);
        if (t.kind == HK_FBROW && fbOpen && t.a < fbN) {   // Liste: nochmal antippen = waehlen/oeffnen, erstes Antippen bei Samples = vorhoeren
            if (t.wasSel) fbPick();
            else if (fbMode == FB_SAMPLE && !fbEnt[t.a].dir) fbPreview();
        }
        if (t.kind == HK_SLOT && t.wasSel && trackPage(page)) {   // nochmal antippen: Liste wie X (nur Sonderregler)
            ParamDef d; int v, bv; bool lk; char lb[24];
            if (knobInfo(t.a, d, v, lk, bv, lb, sizeof lb) && (d.kind == K_SMPL || d.kind == K_LDEST || d.kind == K_EDEST || d.kind == K_MACH || d.kind == K_WDRAW)) remoteHit[BT_X] = true;
        }
        if ((t.kind == HK_SET && t.wasSel && setIsAction(t.a)) || (t.kind == HK_SONG && t.wasSel)) remoteHit[BT_A] = true;
    }
    t.on = false;
    if (t.emuA) { t.emuA = false; touchEmuUp(BT_A); }
    if (t.emuBtn) { t.emuBtn = false; touchEmuUp(t.btn); }
}
static void touchProcess(Uint32 now) {
    RemoteTouch e;
    while (remoteTouchPop(e)) {
        if (e.id < 0 || e.id >= REMOTE_TOUCH_IDS) continue;
        TouchPtr& t = tch[e.id];
        if (e.type == 'c') { for (int i = 0; i < REMOTE_TOUCH_IDS; i++) if (tch[i].on) touchUp(tch[i], now); continue; }   // Verbindung weg
        if (e.type == 't') { if (t.on) touchUp(t, now); touchDown(t, e.x, e.y, now); }
        else if (e.type == 'm' && t.on) touchDrag(t, e.x, e.y);
        else if (e.type == 'e' && t.on) { touchDrag(t, e.x, e.y); touchUp(t, now); }
    }
}
