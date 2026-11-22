#pragma once
#include <Arduino.h>
void outputsBegin();
void pumpStart();          // soft-start ramp
void pumpStop();
bool pumpRunning();
void valveSet(int zone, bool open);
bool valveOpen(int zone);
int  valvesOpenCount();
void allOff();
void buzz(int ms, int times = 1);
void wifiLed(bool on);
