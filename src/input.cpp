#include "input.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

const char* const BTN_NAMES[BT_COUNT] = {
    "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "SELECT", "START"
};

void Input::open() {
    int n = SDL_NumJoysticks();
    // Joystick mit den meisten Tasten nehmen
    int best = -1, bestB = -1;
    for (int i = 0; i < n; i++) {
        SDL_Joystick* j = SDL_JoystickOpen(i);
        if (!j) continue;
        int b = SDL_JoystickNumButtons(j);
        if (b > bestB) { bestB = b; best = i; }
        SDL_JoystickClose(j);
    }
    if (best < 0) return;
    if (!haveMap && SDL_IsGameController(best)) {
        gc = SDL_GameControllerOpen(best);
        if (gc) joy = SDL_GameControllerGetJoystick(gc);
    }
    if (!joy) joy = SDL_JoystickOpen(best);
    if (joy) printf("Gamepad: %s (%d Tasten, %d Achsen, %d Hats)%s\n", SDL_JoystickName(joy),
                    SDL_JoystickNumButtons(joy), SDL_JoystickNumAxes(joy), SDL_JoystickNumHats(joy),
                    gc ? " [GameController-Mapping]" : "");
}

bool Input::loadMap(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char key[32], type[16]; int idx = 0, val = 0;
        int k = sscanf(line, "%31s %15s %d %d", key, type, &idx, &val);
        if (k < 3 || key[0] == '#') continue;
        if (!strcmp(key, "LSTICK_Y")) { axLY = idx; sgnLY = val ? val : -1; continue; }
        if (!strcmp(key, "RSTICK_Y")) { axRY = idx; sgnRY = val ? val : -1; continue; }
        if (!strcmp(key, "LSTICK_X")) { axLX = idx; sgnLX = val ? val : 1; haveLX = true; continue; }
        if (!strcmp(key, "RSTICK_X")) { axRX = idx; sgnRX = val ? val : 1; haveRX = true; continue; }
        for (int b = 0; b < BT_COUNT; b++) {
            if (strcmp(key, BTN_NAMES[b])) continue;
            Binding& m = map[b];
            m.idx = idx; m.val = val;
            m.type = !strcmp(type, "button") ? 1 : !strcmp(type, "hat") ? 2 : !strcmp(type, "axis") ? 3 : 0;
        }
    }
    fclose(f);
    // aeltere Belegungen ohne X-Achse: X liegt meist direkt neben Y (0/1, 2/3)
    if (!haveLX && axLY >= 0) { axLX = axLY ^ 1; sgnLX = 1; }
    if (!haveRX && axRY >= 0) { axRX = axRY ^ 1; sgnRX = 1; }
    haveMap = true;
    return true;
}

bool Input::saveMap(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    fprintf(f, "# Groovebox Tastenbelegung: NAME typ index wert\n# typ: button | hat (wert=Richtungsmaske) | axis (wert=+1/-1)\n");
    static const char* tn[4] = {"none", "button", "hat", "axis"};
    for (int b = 0; b < BT_COUNT; b++) fprintf(f, "%s %s %d %d\n", BTN_NAMES[b], tn[map[b].type], map[b].idx, map[b].val);
    fprintf(f, "LSTICK_Y axis %d %d\n", axLY, sgnLY);
    fprintf(f, "RSTICK_Y axis %d %d\n", axRY, sgnRY);
    fprintf(f, "LSTICK_X axis %d %d\n", axLX, sgnLX);
    fprintf(f, "RSTICK_X axis %d %d\n", axRX, sgnRX);
    fclose(f);
    return true;
}

static float axisVal(SDL_Joystick* j, int a) {
    if (!j || a < 0 || a >= SDL_JoystickNumAxes(j)) return 0.f;
    return SDL_JoystickGetAxis(j, a) / 32767.f;
}

void Input::poll() {
    for (int b = 0; b < BT_COUNT; b++) prev[b] = down[b];
    bool d[BT_COUNT] = {false};

    const Uint8* ks = SDL_GetKeyboardState(nullptr);
    static const SDL_Scancode keys[BT_COUNT] = {
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_S, SDL_SCANCODE_A,
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_1, SDL_SCANCODE_2,
        SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_RETURN};
    for (int b = 0; b < BT_COUNT; b++) d[b] = ks[keys[b]] != 0;
    if (ks[SDL_SCANCODE_RSHIFT] || ks[SDL_SCANCODE_LSHIFT]) d[BT_SELECT] = true;

    float kly = (ks[SDL_SCANCODE_I] ? 1.f : 0.f) - (ks[SDL_SCANCODE_K] ? 1.f : 0.f);
    float kry = (ks[SDL_SCANCODE_O] ? 1.f : 0.f) - (ks[SDL_SCANCODE_L] ? 1.f : 0.f);
    ly = kly; ry = kry;
    lx = (ks[SDL_SCANCODE_SEMICOLON] ? 1.f : 0.f) - (ks[SDL_SCANCODE_J] ? 1.f : 0.f);
    rx = (ks[SDL_SCANCODE_PERIOD] ? 1.f : 0.f) - (ks[SDL_SCANCODE_COMMA] ? 1.f : 0.f);

    if (gc && !haveMap) {
        static const int gcb[BT_COUNT] = {
            SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
            SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
            SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, -1, -1, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START};
        for (int b = 0; b < BT_COUNT; b++)
            if (gcb[b] >= 0 && SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)gcb[b])) d[b] = true;
        if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000) d[BT_L2] = true;
        if (SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000) d[BT_R2] = true;
        float a = -SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f;
        float c = -SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.f;
        if (fabsf(a) > fabsf(ly)) ly = a;
        if (fabsf(c) > fabsf(ry)) ry = c;
        float gx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f;
        float hx = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.f;
        if (fabsf(gx) > fabsf(lx)) lx = gx;
        if (fabsf(hx) > fabsf(rx)) rx = hx;
    } else if (joy) {
        int nb = SDL_JoystickNumButtons(joy), nh = SDL_JoystickNumHats(joy), na = SDL_JoystickNumAxes(joy);
        for (int b = 0; b < BT_COUNT; b++) {
            const Binding& m = map[b];
            if (m.type == 1 && m.idx < nb) d[b] = d[b] || SDL_JoystickGetButton(joy, m.idx);
            else if (m.type == 2 && m.idx < nh) d[b] = d[b] || (SDL_JoystickGetHat(joy, m.idx) & m.val);
            else if (m.type == 3 && m.idx < na) d[b] = d[b] || axisVal(joy, m.idx) * m.val > 0.5f;
        }
        float a = axisVal(joy, axLY) * sgnLY, c = axisVal(joy, axRY) * sgnRY;
        if (fabsf(a) > fabsf(ly)) ly = a;
        if (fabsf(c) > fabsf(ry)) ry = c;
        float gx = axisVal(joy, axLX) * sgnLX, hx = axisVal(joy, axRX) * sgnRX;
        if (fabsf(gx) > fabsf(lx)) lx = gx;
        if (fabsf(hx) > fabsf(rx)) rx = hx;
    }
    Uint32 now = SDL_GetTicks();
    for (int b = 0; b < BT_COUNT; b++) {
        down[b] = d[b];
        if (down[b] && !prev[b]) { downSince[b] = now; lastRep[b] = now; }
    }
}

bool Input::repeat(int b, Uint32 now, Uint32 delay, Uint32 rate) {
    if (pressed(b)) return true;
    if (!down[b]) return false;
    if (now - downSince[b] < delay) return false;
    if (now - lastRep[b] >= rate) { lastRep[b] = now; return true; }
    return false;
}
