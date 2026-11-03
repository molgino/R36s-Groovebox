// MIDI ueber ALSA: Sequencer (automatisches Verbinden aller Geraete), Fallback rawmidi
#pragma once
#include <stdint.h>

struct MidiMsg { uint8_t st, d1, d2, pad; double due; };

bool midiStart();                       // startet den MIDI-Thread
void midiStop();
bool midiInPop(MidiMsg& m);             // nur vom Audio-Thread
bool midiOutPush(const MidiMsg& m);     // nur vom Audio-Thread
double midiNow();                       // Sekunden, monotone Uhr
int midiPortInfo(char* buf, int n);     // Namen der verbundenen Geraete, liefert Anzahl
const char* midiBackend();
int audioJackPoll();                    // Kopfhoerer-Buchse pruefen + Pfad umschalten (-1 = nicht vorhanden)              // "SEQ", "RAW" oder "AUS"
extern volatile uint32_t midiActivity;  // SDL-Ticks des letzten eingehenden Events
