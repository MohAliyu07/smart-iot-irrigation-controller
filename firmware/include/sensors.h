#pragma once
#include <Arduino.h>
struct SensorData { int moisturePct[4]; float tempC; float humidity; float vin; float pumpA; bool dhtOk; };
void sensorsBegin();
void sensorsRead(SensorData &d);
float readPumpCurrent();
float readVin();
