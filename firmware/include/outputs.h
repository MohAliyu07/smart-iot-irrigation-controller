// outputs.h : pump, valves, buzzer and status LED
#pragma once
#include <Arduino.h>

void outputsBegin();                  // everything off
bool pumpStart();                     // soft start; false = stopped on overcurrent during the ramp
void pumpStop();
bool pumpRunning();
void valveSet(int zone, bool open);
bool valveOpen(int zone);
int  valvesOpenCount();
void allOff();                        // pump first, then every valve
void buzz(int ms, int times = 1);
void wifiLed(bool on);
