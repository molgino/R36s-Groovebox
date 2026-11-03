// Sample-Bibliothek: WAV-Dateien aus dem Ordner samples/ (inkl. eine Unterordner-Ebene)
#pragma once
#include <stdint.h>

enum { MAX_SAMPLES = 128 };

struct Sample {
    char name[48];   // z.B. "drums/kick-808"
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
