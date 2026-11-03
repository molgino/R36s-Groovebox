// USB-Audio zum PC als zweiter Ausgang (UAC2-Gadget im Groovebox-OS), siehe usbaudio.cpp
#pragma once
bool usbAudioStart();                         // Gadget-Soundkarte suchen und Schreib-Thread starten
void usbAudioStop();
bool usbAudioActive();
int usbAudioStatus();                         // 0 aus, 1 wartet auf PC, 2 PC liest mit
void usbAudioTestTone(int ms);                // 1-kHz-Testton statt der Mischung
void usbAudioPush(const float* stereo, int frames);   // aus dem Audio-Callback
