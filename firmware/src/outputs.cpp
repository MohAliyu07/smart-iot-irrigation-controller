#include "outputs.h"
#include "config.h"

static const uint8_t valvePins[4] = {PIN_VALVE1, PIN_VALVE2, PIN_VALVE3, PIN_VALVE4};
static bool vState[4] = {false, false, false, false};
static bool pumpOn = false;
static const int PUMP_MAX = (1 << PUMP_PWM_BITS) - 1;

void outputsBegin() {
  for (auto p : valvePins) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  ledcAttach(PIN_PUMP_EN, PUMP_PWM_HZ, PUMP_PWM_BITS);
  ledcWrite(PIN_PUMP_EN, 0);
  pinMode(PIN_BUZZER, OUTPUT); digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_LED_WIFI, OUTPUT); digitalWrite(PIN_LED_WIFI, LOW);
}

void pumpStart() {
  if (pumpOn) return;
  const int steps = 50;
  for (int i = 1; i <= steps; i++) {             // 500 ms linear ramp limits inrush (sim: 3.0 A -> 1.65 A)
    ledcWrite(PIN_PUMP_EN, PUMP_MAX * i / steps);
    delay(SOFTSTART_MS / steps);
  }
  pumpOn = true;
}
void pumpStop()     { ledcWrite(PIN_PUMP_EN, 0); pumpOn = false; }
bool pumpRunning()  { return pumpOn; }

void valveSet(int z, bool open) {
  if (z < 0 || z >= NUM_ZONES) return;
  digitalWrite(valvePins[z], open ? HIGH : LOW);
  vState[z] = open;
}
bool valveOpen(int z) { return z >= 0 && z < NUM_ZONES && vState[z]; }
int valvesOpenCount() { int n = 0; for (bool s : vState) n += s; return n; }

void allOff() { pumpStop(); delay(50); for (int z = 0; z < NUM_ZONES; z++) valveSet(z, false); }

void buzz(int ms, int times) {
  for (int i = 0; i < times; i++) { digitalWrite(PIN_BUZZER, HIGH); delay(ms); digitalWrite(PIN_BUZZER, LOW); delay(ms); }
}
void wifiLed(bool on) { digitalWrite(PIN_LED_WIFI, on); }
