// Samples: Bibliothek (alle WAV-Dateien unter samples/, nur Namen, beliebig viele) und
// Pool (die vom Projekt benutzten Samples im Speicher; Spur-Parameter SMPL = Pool-Platz).
#pragma once
#include <stdint.h>

enum {
    MAX_SAMPLES = 254,              // Pool-Plaetze (Parameterwert 0..253; 255 = kein Lock)
    PREVIEW_SLOT = MAX_SAMPLES - 1, // Platz zum Vorhoeren im Explorer
    MAX_LIB = 8192,                 // Dateien in der Bibliothek
    SAMPLE_DEPTH = 8                // Unterordner-Ebenen
};

struct Sample {
    char name[96];   // Bibliotheksname ohne .wav, z.B. "drums/808/kick"
    float* data;     // mono
    int len;
    int rate;
    long mtime, fsize;   // zum Erkennen geaenderter Dateien
};

// Bibliothek
int libScan(const char* dir);       // Liste neu einlesen (nur Namen), liefert Anzahl
int libCount();
const char* libName(int i);         // sortiert, ohne .wav
int libFind(const char* name);      // -1 wenn nicht vorhanden
int libSkipped();                   // Audiodateien in anderen Formaten (MP3, AIFF, FLAC ...): werden nicht gelesen
const char* libDir();

// Pool
int sampleCount();                  // hoechster belegter Platz + 1 (ohne Vorhoer-Platz)
const Sample* sampleGet(int slot);  // nullptr, wenn leer
const char* sampleName(int slot);   // "" wenn leer
int poolFind(const char* name);     // Platz mit diesem Sample oder -1
int poolFreeSlot();                 // erster freier Platz (ohne Vorhoer-Platz) oder -1
bool sampleRead(const char* name, Sample& out);   // Datei lesen (ohne Audio-Sperre); out.data mit malloc
void poolSwap(int slot, Sample& s); // UNTER AUDIO-SPERRE: Platz <-> s tauschen (danach sampleFree(s))
void sampleFree(Sample& s);
const char* sampleLastError();      // Grund des letzten Lesefehlers

// Tests/Benchmark: Bibliothek lesen und die ersten Dateien der Reihe nach in den Pool laden
int samplesLoad(const char* dir);
// Zuordnung alter Projekte (Nummer = Position): bis 11/2026 (1 Ebene) max 128, depth 1, cap 256;
// danach (3 bzw. 8 Ebenen) max 250, depth SAMPLE_DEPTH, cap 500
int libLegacyList(char (*out)[96], int max, int depth, int cap);
