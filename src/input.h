// Eingabe: Gamepad (frei belegbar per Lernmodus), SDL-GameController oder Tastatur
#pragma once
#include <SDL.h>

enum Btn {
    BT_UP, BT_DOWN, BT_LEFT, BT_RIGHT, BT_A, BT_B, BT_X, BT_Y,
    BT_L1, BT_R1, BT_L2, BT_R2, BT_SELECT, BT_START, BT_COUNT
};
extern const char* const BTN_NAMES[BT_COUNT];

struct Binding { int type = 0; int idx = 0; int val = 0; }; // 1=Button 2=Hat(val=Maske) 3=Achse(val=+1/-1)

struct Input {
    SDL_Joystick* joy = nullptr;
    SDL_GameController* gc = nullptr;
    Binding map[BT_COUNT];
    int axLY = -1, axRY = -1, sgnLY = -1, sgnRY = -1; // sgn: Vorzeichen fuer "oben"
    int axLX = -1, axRX = -1, sgnLX = 1, sgnRX = 1;   // sgn: Vorzeichen fuer "rechts"
    bool haveLX = false, haveRX = false;
    bool haveMap = false;

    bool down[BT_COUNT] = {false}, prev[BT_COUNT] = {false};
    Uint32 downSince[BT_COUNT] = {0}, lastRep[BT_COUNT] = {0};
    float ly = 0, ry = 0; // oben = positiv
    float lx = 0, rx = 0; // rechts = positiv

    void open();
    bool loadMap(const char* path);
    bool saveMap(const char* path);
    void poll(); // einmal pro Frame nach SDL_PollEvent
    bool pressed(int b) const { return down[b] && !prev[b]; }
    bool released(int b) const { return !down[b] && prev[b]; }
    bool repeat(int b, Uint32 now, Uint32 delay = 280, Uint32 rate = 55);
    Uint32 heldMs(int b, Uint32 now) const { return down[b] ? now - downSince[b] : 0; }
};
