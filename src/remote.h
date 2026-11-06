// Bildschirm-Spiegelung + Fernbedienung ueber das Webinterface (/screen)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "input.h"

// Eingaben aus dem Browser (vom Web-Thread gesetzt, in Input::poll zusammengefuehrt)
extern volatile bool remoteDown[BT_COUNT];
extern volatile bool remoteHit[BT_COUNT];      // kurzer Druck: mindestens ein Frame lang "unten"
extern volatile float remoteAxis[4];           // lx, ly, rx, ry (oben/rechts = positiv)
void remoteInput(const char* name, int value); // Name wie "A", "UP", "LX"; value 1/0 bzw. -1/0/1 fuer Achsen

// Bildschirm
bool remoteWanted();                                   // hat in den letzten 2 s jemand zugeschaut?
void remoteSnapshot(const uint32_t* argb, int pitchPx, int w, int h);   // aus dem Hauptthread
// Web-Thread: Bild seit Nummer "since" kodieren; liefert Laenge (0 = nichts Neues), out wird (re)alloziert
size_t remoteEncode(unsigned since, uint8_t** out, size_t* cap);
// Web-Thread: Anfrage "GET /ws" mit Upgrade -> Handshake, eigener Thread uebernimmt fd (true = fd nicht schliessen)
bool remoteWsAccept(int fd, const char* headers);
// Touch auf dem Bild (Koordinaten 640x480): 't' Finger auf, 'm' bewegt, 'e' Finger weg, 'c' alle weg (Verbindung zu)
enum { REMOTE_TOUCH_IDS = 10 };
struct RemoteTouch { char type; int id, x, y; };
bool remoteTouchPop(RemoteTouch& e);   // Hauptthread
