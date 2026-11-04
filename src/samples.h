// Sample-Bibliothek: WAV-Dateien aus dem Ordner samples/ (inkl. eine Unterordner-Ebene)
#pragma once
#include <stdint.h>

enum { MAX_SAMPLES = 250, SAMPLE_DEPTH = 3 };   // Unterordner bis 3 Ebenen tief

struct Sample {
    char name[96];   // z.B. "drums/808/kick"
    float* data;     // mono
    int len;
    int rate;
};

int samplesLoad(const char* dir);  // liefert Anzahl geladener Samples
int sampleCount();
// Neu laden ohne Aussetzer: samplesPrepare (ohne Audio-Sperre) -> samplesSwap (unter Sperre) -> samplesFreeBack
int samplesPrepare(const char* dir);
void samplesSwap();
void samplesFreeBack();
const Sample* sampleGet(int i);   // nullptr, wenn nicht vorhanden
const char* sampleName(int i);
