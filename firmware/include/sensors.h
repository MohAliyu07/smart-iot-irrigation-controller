// sensors.h : soil moisture, DHT11, supply voltage and pump current
#pragma once
#include <Arduino.h>

struct SensorData {
  int   moisturePct[4];   // 0 = dry, 100 = wet
  bool  moistureOk[4];    // false when a probe is shorted, open or unplugged
  float tempC;            // NAN when the DHT11 did not answer
  float humidity;
  float vin;              // 12 V rail, volts
  float pumpA;            // pump current, amps
  bool  dhtOk;
};

void  sensorsBegin();
void  sensorsRead(SensorData &d);   // blocks for about 150 ms while the sensor rail settles
float readPumpCurrent();            // averaged; blocks for about 0.5 ms
float readVin();
