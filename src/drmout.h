// Direkte DRM/KMS-Ausgabe ohne GPU (Groovebox-OS), siehe drmout.cpp
#pragma once
#include <SDL.h>
bool drmOutOpen();                  // erste angeschlossene Anzeige uebernehmen
bool drmOutActive();
void drmOutPresent(SDL_Surface* s); // Surface (ARGB8888) anzeigen
void drmOutClose();
