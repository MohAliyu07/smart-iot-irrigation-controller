#include "sensors.h"
#include "config.h"
#include <DHT.h>

static DHT dht(PIN_DHT, DHT11);
static const uint8_t moistPins[4] = {PIN_MOIST1, PIN_MOIST2, PIN_MOIST3, PIN_MOIST4};

void sensorsBegin() {
  pinMode(PIN_SENS_PWR_N, OUTPUT);
  digitalWrite(PIN_SENS_PWR_N, HIGH);           // sensors off until needed (stops probe corrosion)
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  dht.begin();
}

static int rawToPct(int raw) {
  int p = map(raw, MOIST_RAW_DRY, MOIST_RAW_WET, 0, 100);
  return constrain(p, 0, 100);
}

float readVin()          { return analogReadMilliVolts(PIN_VIN_SENSE) / 1000.0f * VIN_DIV_RATIO; }
float readPumpCurrent()  { return analogReadMilliVolts(PIN_PUMP_ISENSE) / 1000.0f / ISENSE_V_PER_A; }

void sensorsRead(SensorData &d) {
  digitalWrite(PIN_SENS_PWR_N, LOW);            // power sensors
  delay(150);                                   // settle
  for (int i = 0; i < 4; i++) {
    long acc = 0;
    for (int s = 0; s < 8; s++) acc += analogRead(moistPins[i]);
    d.moisturePct[i] = rawToPct(acc / 8);
  }
  float t = dht.readTemperature(), h = dht.readHumidity();
  d.dhtOk = !(isnan(t) || isnan(h));
  d.tempC = d.dhtOk ? t : NAN;
  d.humidity = d.dhtOk ? h : NAN;
  digitalWrite(PIN_SENS_PWR_N, HIGH);           // power off
  d.vin = readVin();
  d.pumpA = readPumpCurrent();
}
