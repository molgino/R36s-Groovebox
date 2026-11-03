// Mini-Webserver fuer Sample-/Projekt-Upload per Browser
#pragma once
#include <SDL.h>
bool webStart(const char* basePath);           // basePath endet mit '/'
int webPort();                                 // 0 = laeuft nicht
bool webTakeChange(Uint32 now, Uint32 quietMs); // true, wenn Samples geaendert wurden und es seit quietMs ruhig ist
int webLastEvent(char* out, int n);            // laufende Nummer + Text des letzten Uploads
