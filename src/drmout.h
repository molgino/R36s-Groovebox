// Direkte DRM/KMS-Ausgabe ohne GPU (Groovebox-OS), siehe drmout.cpp
#pragma once
#include <SDL.h>
bool drmOutOpen();                  // erste angeschlossene Anzeige uebernehmen
bool drmOutActive();
void drmOutPresent(SDL_Surface* s); // Surface (ARGB8888) anzeigen
void drmOutClose();
// Ausgleich fuer Displays, die den Datenstrom um einige Bytes versetzt lesen (Bild verschoben + Farben rotiert)
void drmOutSetByteOffset(int bytes);
void drmOutStats(double* copyMs, double* flipMs);   // Mittelwerte seit dem letzten Aufruf
bool drmOutSingleBuffer();
