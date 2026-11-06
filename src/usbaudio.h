// USB-Audio zum PC als zweiter Ausgang (UAC2-Gadget im Groovebox-OS), siehe usbaudio.cpp
#pragma once
bool usbAudioStart();                         // Gadget-Soundkarte suchen und Schreib-Thread starten
void usbAudioStop();
bool usbAudioActive();
int usbAudioStatus();                         // 0 aus, 1 wartet auf PC, 2 PC liest mit
int usbAudioRate();                           // vom PC gewaehlte Abtastrate (44100/48000), 0 = kein Strom offen
void usbAudioSetGain(float g);                // USB-Lautstaerke 0..1
void usbAudioTestTone(int ms);                // 1-kHz-Testton statt der Mischung
void usbAudioPush(const float* stereo, int frames);   // aus dem Audio-Callback (nur bei 2 Kanaelen)
void usbAudioSetChannels(int ch);             // Kanaele zum PC (2, 8, 18, 24), nur vor usbAudioStart wirksam
int usbAudioChannels();
void usbAudioPushMulti(const float* buf, int frames);   // Mehrkanal: frames x Kanaele (0-1 = Master)
void usbAudioSetEngineRate(int sr);           // tatsaechliche Rate der Engine (nach dem Oeffnen des Audio-Geraets)

// PC -> R36S: was der PC auf "Synth City" abspielt, in den Ausgang mischen und als Sample aufnehmen
#include <stdint.h>
bool pcInStart();
void pcInStop();
bool pcInActive();
int pcInStatus();                             // 0 aus, 1 PC spielt nichts, 2 PC spielt
void pcInMix(float* stereo, int frames, float gain);   // aus dem Audio-Callback
bool pcRecStart();                            // wartet auf das erste Signal, dann max. 60 s
void pcRecStop();
int pcRecState();                             // 0 aus, 1 wartet, 2 nimmt auf, 3 fertig
float pcRecSeconds();
int pcRecTake(float** data);                  // Frames (stereo, Float) der fertigen Aufnahme
int pcRecRate();                              // Abtastrate der Aufnahme (= Engine-Rate)
void pcRecDone();
