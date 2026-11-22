// sensors.cpp : soil moisture, DHT11, supply voltage and pump current
#include "sensors.h"
#include "config.h"
#include <DHT.h>

static DHT dht(PIN_DHT, DHT11);
static const uint8_t MOIST_PINS[NUM_ZONES] = {PIN_MOIST1, PIN_MOIST2, PIN_MOIST3, PIN_MOIST4};

void sensorsBegin() {
  pinMode(PIN_SENS_PWR_N, OUTPUT);
  digitalWrite(PIN_SENS_PWR_N, HIGH);        // sensor rail off until a reading is needed (slows probe corrosion)
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);            // full 0 to 3.1 V range on every ADC pin
  dht.begin();
}

static int rawToPercent(int raw) {
  return constrain(map(raw, MOIST_RAW_DRY, MOIST_RAW_WET, 0, 100), 0, 100);
}

float readVin() {
  return analogReadMilliVolts(PIN_VIN_SENSE) / 1000.0f * VIN_DIV_RATIO;
}

// The pump is switched at 20 kHz during soft start. Averaging over about ten PWM
// periods removes the ripple that is left after the 1 k + 100 nF filter.
float readPumpCurrent() {
  uint32_t sumMilliVolts = 0;
  for (int i = 0; i < ISENSE_SAMPLES; i++) {
    sumMilliVolts += analogReadMilliVolts(PIN_PUMP_ISENSE);
    delayMicroseconds(ISENSE_SAMPLE_US);
  }
  return (sumMilliVolts / (float)ISENSE_SAMPLES) / 1000.0f / ISENSE_V_PER_A;
}

void sensorsRead(SensorData &d) {
  digitalWrite(PIN_SENS_PWR_N, LOW);         // power the sensor rail
  delay(SENSOR_SETTLE_MS);

  for (int z = 0; z < NUM_ZONES; z++) {
    long sum = 0;
    for (int s = 0; s < MOIST_SAMPLES; s++) sum += analogRead(MOIST_PINS[z]);
    int raw = sum / MOIST_SAMPLES;
    d.moistureOk[z]  = (raw >= MOIST_RAW_MIN && raw <= MOIST_RAW_MAX);
    d.moisturePct[z] = rawToPercent(raw);
  }

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  d.dhtOk    = !(isnan(t) || isnan(h));
  d.tempC    = d.dhtOk ? t : NAN;
  d.humidity = d.dhtOk ? h : NAN;

  digitalWrite(PIN_SENS_PWR_N, HIGH);        // rail off again

  d.vin   = readVin();
  d.pumpA = readPumpCurrent();
}
