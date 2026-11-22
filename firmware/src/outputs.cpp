// outputs.cpp : pump, valves, buzzer and status LED
#include "outputs.h"
#include "sensors.h"
#include "config.h"

static const uint8_t VALVE_PINS[NUM_ZONES] = {PIN_VALVE1, PIN_VALVE2, PIN_VALVE3, PIN_VALVE4};
static const int     PUMP_DUTY_MAX = (1 << PUMP_PWM_BITS) - 1;

static bool valveState[NUM_ZONES] = {false, false, false, false};
static bool pumpOn = false;

void outputsBegin() {
  for (uint8_t pin : VALVE_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  ledcAttach(PIN_PUMP_EN, PUMP_PWM_HZ, PUMP_PWM_BITS);
  ledcWrite(PIN_PUMP_EN, 0);
  pinMode(PIN_BUZZER, OUTPUT);   digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_LED_WIFI, OUTPUT); digitalWrite(PIN_LED_WIFI, LOW);
}

// Ramps the PWM duty from 0 to 100 % to limit the inrush current, and checks the
// current on every step so a stalled pump is stopped before the ramp finishes.
//
// The shunt is in the MOSFET source, so it only carries current while the MOSFET
// is on. The filtered reading is (duty x motor current); dividing by the duty
// gives the motor current.
bool pumpStart() {
  if (pumpOn) return true;
  for (int step = 1; step <= SOFTSTART_STEPS; step++) {
    ledcWrite(PIN_PUMP_EN, PUMP_DUTY_MAX * step / SOFTSTART_STEPS);
    delay(SOFTSTART_MS / SOFTSTART_STEPS);
    float duty = (float)step / SOFTSTART_STEPS;
    if (duty >= SOFTSTART_CHECK_FROM_DUTY && readPumpCurrent() / duty > PUMP_MAX_CURRENT_A) {
      ledcWrite(PIN_PUMP_EN, 0);
      return false;
    }
  }
  pumpOn = true;
  return true;
}

void pumpStop()    { ledcWrite(PIN_PUMP_EN, 0); pumpOn = false; }
bool pumpRunning() { return pumpOn; }

void valveSet(int z, bool open) {
  if (z < 0 || z >= NUM_ZONES) return;
  digitalWrite(VALVE_PINS[z], open ? HIGH : LOW);
  valveState[z] = open;
}

bool valveOpen(int z) { return z >= 0 && z < NUM_ZONES && valveState[z]; }

int valvesOpenCount() {
  int n = 0;
  for (bool open : valveState) n += open;
  return n;
}

// Pump first, so it never pushes against closed valves.
void allOff() {
  pumpStop();
  delay(PUMP_LAG_MS);
  for (int z = 0; z < NUM_ZONES; z++) valveSet(z, false);
}

void buzz(int ms, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_BUZZER, HIGH); delay(ms);
    digitalWrite(PIN_BUZZER, LOW);  delay(ms);
  }
}

void wifiLed(bool on) { digitalWrite(PIN_LED_WIFI, on); }
