// Smart IoT Irrigation Controller v2 (board Rev B) - Wokwi simulation build (ESP32-S3)
// Mohammed Olamilekan Aliyu
// The PlatformIO firmware merged into one file. Simulation-only changes:
//   * Wi-Fi and Blynk are not started (the control logic runs offline, as on the board with no network)
//   * DHT22 with a small built-in reader instead of DHT11 (Wokwi has no DHT11)
//   * sensors are read every 2 s instead of 10 s
//   * the "ipump" pot is a steady level, so the soft-start check does not divide by the duty
#define WOKWI_SIM 1
#define BLYNK_TEMPLATE_ID   "TMPLsim"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation"
#define BLYNK_AUTH_TOKEN    "sim"
#include <Arduino.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ===== config.h =====
// =============================================================================
//  Smart IoT Irrigation Controller v2 - board Rev B
//  config.h : pin map, electrical scaling, limits and defaults
//
//  MCU   : ESP32-S3-WROOM-1-N8
//  Board : Rev B (pump MOSFET AO4404B, INA181A2 output filtered by 1 k + 100 nF)
//  Every number that depends on the hardware lives in this file.
// =============================================================================

// ---- Pin map (matches the Rev B schematic, sheet 3_MCU) ---------------------
#define PIN_MOIST1      1     // ADC1: soil moisture, zone 1
#define PIN_MOIST2      2     // ADC1: soil moisture, zone 2
#define PIN_MOIST3      3     // ADC1: soil moisture, zone 3
#define PIN_MOIST4      4     // ADC1: soil moisture, zone 4
#define PIN_DHT         5     // DHT11 data
#define PIN_BTN_MODE    6     // MODE button, active low
#define PIN_BUZZER      7     // buzzer driver, active high
#define PIN_VIN_SENSE   8     // ADC1: 12 V rail through a 100 k / 22 k divider
#define PIN_PUMP_ISENSE 9     // ADC1: pump current, 1 V per amp
#define PIN_PUMP_EN     10    // pump MOSFET gate, 20 kHz PWM
#define PIN_VALVE1      11    // valve MOSFET gates, active high
#define PIN_VALVE2      12
#define PIN_VALVE3      13
#define PIN_VALVE4      14
#define PIN_LED_WIFI    15    // status LED, active high
#define PIN_SENS_PWR_N  16    // sensor rail P-FET gate: LOW = sensors powered
#define PIN_SDA         17    // I2C header (spare)
#define PIN_SCL         18

#define NUM_ZONES 4

// ---- Analog scaling -----------------------------------------------------------
#define VIN_DIV_RATIO    ((100.0f + 22.0f) / 22.0f)   // 100 k over 22 k
#define ISENSE_V_PER_A   1.0f                         // 20 mOhm shunt x INA181A2 gain of 50
#define ISENSE_SAMPLES   16                           // ADC samples averaged per current reading
#define ISENSE_SAMPLE_US 31                           // 16 x 31 us = 0.5 ms = 10 PWM periods

// ---- Pump drive ---------------------------------------------------------------
#define PUMP_PWM_HZ      20000   // above the audible range
#define PUMP_PWM_BITS    10
#define SOFTSTART_MS     500     // length of the start-up ramp
#define SOFTSTART_STEPS  50      // one step every 10 ms
#define SOFTSTART_CHECK_FROM_DUTY 0.2f   // below this the duty-corrected reading is too noisy to use

// ---- Protection limits --------------------------------------------------------
#define MAX_ZONES_OPEN      2        // keeps the total inside a 12 V 3 A adapter
#define PUMP_MAX_CURRENT_A  2.5f     // overcurrent trip (stalled or shorted pump)
#define PUMP_DRY_CURRENT_A  0.25f    // below this while running means no water
#define DRY_RUN_GRACE_MS    5000UL   // ignore low current while the pump primes
#define VIN_MIN_V           10.5f
#define VIN_MAX_V           13.8f
#define MAX_WATER_MS        (20UL * 60UL * 1000UL)   // hard cap per zone: 20 minutes
#define VALVE_LEAD_MS       100      // valve opens this long before the pump starts
#define PUMP_LAG_MS         50       // pump stops this long before the last valve closes

// ---- Watering thresholds (changeable from Blynk) ------------------------------
#define DEFAULT_DRY_PCT     35       // start watering below this
#define DEFAULT_WET_PCT     60       // stop watering at or above this
#define MIN_HYSTERESIS_PCT  5        // wet must stay at least this far above dry
#define SENSOR_PERIOD_MS    2000UL   // sim: 2 s (board: 10 s)

// ---- Moisture sensor calibration (12-bit raw ADC) ------------------------------
#define MOIST_RAW_DRY       3000     // reading in dry air
#define MOIST_RAW_WET       1200     // reading in water
#define MOIST_RAW_MIN       100      // below this the sensor is shorted or unplugged
#define MOIST_RAW_MAX       4000     // above this the sensor is open circuit
#define MOIST_SAMPLES       8
#define SENSOR_SETTLE_MS    150      // wait after powering the sensor rail

// ---- Task periods ---------------------------------------------------------------
#define SAFETY_PERIOD_MS    100UL
#define BUTTON_PERIOD_MS    20UL
#define STATUS_PERIOD_MS    1000UL

// ---- Wi-Fi and Blynk credentials -------------------------------------------------
// Copy secrets.example.h to secrets.h and fill it in. secrets.h is git-ignored.

// ===== sensors.h =====
// sensors.h : soil moisture, DHT11, supply voltage and pump current

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

// ===== outputs.h =====
// outputs.h : pump, valves, buzzer and status LED

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

// ===== zones.h =====
// zones.h : watering logic, operating modes and fault handling

enum Mode  { MODE_AUTO = 0, MODE_MANUAL = 1, MODE_OFF = 2 };
enum Fault { FAULT_NONE = 0, FAULT_OVERCURRENT, FAULT_DRY_RUN, FAULT_VIN, FAULT_TIMEOUT };

struct ZoneCfg { int dryPct; int wetPct; bool enabled; };

void     zonesBegin();
void     zonesUpdate(const SensorData &d);   // call after every sensor read
void     zonesSafetyTick();                  // call every 100 ms
void     zonesManual(int zone, bool on);     // only acts in MODE_MANUAL
void     zonesSetMode(Mode m);
void     zonesSetThresholds(int dryPct, int wetPct);   // applied to every zone, range-checked
Mode     zonesMode();
Fault    zonesFault();
void     zonesClearFault();
ZoneCfg &zoneCfg(int z);

// ===== sensors.cpp =====
// sensors.cpp : soil moisture, DHT11, supply voltage and pump current

// sim: small DHT22 reader (Wokwi's ESP32-S3 times out with the usual DHT libraries; board uses Adafruit DHT with DHT11)
static int32_t dhtWait(int level, uint32_t timeoutUs) {
  uint32_t s = micros();
  while (digitalRead(PIN_DHT) != level) if (micros() - s > timeoutUs) return -1;
  return (int32_t)(micros() - s);
}
static bool dhtRead22(float &t, float &h) {
  // Times every high pulse after the start signal; the last 40 are the data bits (short = 0, long = 1).
  int32_t hi[42]; int n = 0;
  pinMode(PIN_DHT, OUTPUT); digitalWrite(PIN_DHT, LOW); delay(2);
  pinMode(PIN_DHT, INPUT_PULLUP);
  noInterrupts();
  dhtWait(LOW, 1000);                                // sensor answers low
  while (n < 42) {
    if (dhtWait(HIGH, 300) < 0) break;
    int32_t d = dhtWait(LOW, 300);
    if (d < 0) break;                                // line left high = end of frame
    hi[n++] = d;
  }
  interrupts();
  if (n < 40) { Serial.printf("DHT: only %d pulses\n", n); return false; }
  uint8_t b[5] = {0};
  for (int i = 0; i < 40; i++) {
    int k = n - 40 + i;                              // the last 40 pulses are the data bits
    b[i / 8] = (b[i / 8] << 1) | (hi[k] > 30 ? 1 : 0);
  }
  if (((b[0] + b[1] + b[2] + b[3]) & 0xFF) != b[4]) { Serial.printf("DHT checksum %02X %02X %02X %02X %02X\n", b[0], b[1], b[2], b[3], b[4]); return false; }
  h = ((b[0] << 8) | b[1]) * 0.1f;
  t = (((b[2] & 0x7F) << 8) | b[3]) * 0.1f * ((b[2] & 0x80) ? -1 : 1);
  return true;
}
static const uint8_t MOIST_PINS[NUM_ZONES] = {PIN_MOIST1, PIN_MOIST2, PIN_MOIST3, PIN_MOIST4};

void sensorsBegin() {
  pinMode(PIN_SENS_PWR_N, OUTPUT);
  digitalWrite(PIN_SENS_PWR_N, HIGH);        // sensor rail off until a reading is needed (slows probe corrosion)
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);            // full 0 to 3.1 V range on every ADC pin
  pinMode(PIN_DHT, INPUT_PULLUP);
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

  float t = NAN, h = NAN;
  if (!dhtRead22(t, h)) { t = NAN; h = NAN; }
  d.dhtOk    = !(isnan(t) || isnan(h));
  d.tempC    = d.dhtOk ? t : NAN;
  d.humidity = d.dhtOk ? h : NAN;

  digitalWrite(PIN_SENS_PWR_N, HIGH);        // rail off again

  d.vin   = readVin();
  d.pumpA = readPumpCurrent();
}

// ===== outputs.cpp =====
// outputs.cpp : pump, valves, buzzer and status LED

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
#ifdef WOKWI_SIM
    float amps = readPumpCurrent();            // sim: the pot is the motor current directly
#else
    float amps = readPumpCurrent() / duty;
#endif
    if (duty >= SOFTSTART_CHECK_FROM_DUTY && amps > PUMP_MAX_CURRENT_A) {
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

// ===== zones.cpp =====
// zones.cpp : watering logic, operating modes and fault handling
//
// Rules this file enforces:
//   1. A valve is always open before the pump starts, and the pump always stops
//      before the last valve closes.
//   2. No more than MAX_ZONES_OPEN valves are open at once.
//   3. Any fault switches everything off and stays latched until it is cleared
//      by a long press of the MODE button or from the app.

static ZoneCfg       cfg[NUM_ZONES];
static unsigned long openedAt[NUM_ZONES] = {0};
static unsigned long pumpStartedAt = 0;
static Mode          mode  = MODE_AUTO;
static Fault         fault = FAULT_NONE;

ZoneCfg &zoneCfg(int z) { return cfg[z]; }
Mode     zonesMode()    { return mode; }
Fault    zonesFault()   { return fault; }

void zonesBegin() {
  for (ZoneCfg &c : cfg) { c.dryPct = DEFAULT_DRY_PCT; c.wetPct = DEFAULT_WET_PCT; c.enabled = true; }
}

// Latches the first fault only, so a fault that persists does not sound the buzzer again.
static void trip(Fault f) {
  allOff();
  if (fault != FAULT_NONE) return;
  fault = f;
  buzz(150, 3);
}

void zonesClearFault() { fault = FAULT_NONE; }

static void openZone(int z) {
  if (fault != FAULT_NONE || valveOpen(z)) return;
  if (valvesOpenCount() >= MAX_ZONES_OPEN) return;      // rule 2

  valveSet(z, true);
  openedAt[z] = millis();
  delay(VALVE_LEAD_MS);                                  // rule 1: valve first

  if (!pumpRunning()) {
    if (!pumpStart()) { trip(FAULT_OVERCURRENT); return; }   // stalled pump caught during the ramp
    pumpStartedAt = millis();
  }
}

static void closeZone(int z) {
  if (!valveOpen(z)) return;
  if (valvesOpenCount() == 1) pumpStop();                // rule 1: pump first
  delay(PUMP_LAG_MS);
  valveSet(z, false);
}

// Changing mode always stops watering, so nothing is left running by the old mode.
void zonesSetMode(Mode m) {
  if (m != MODE_AUTO && m != MODE_MANUAL && m != MODE_OFF) return;
  if (m == mode) return;
  allOff();
  mode = m;
}

void zonesManual(int z, bool on) {
  if (mode != MODE_MANUAL) return;
  if (on) openZone(z); else closeZone(z);
}

void zonesSetThresholds(int dryPct, int wetPct) {
  dryPct = constrain(dryPct, 5, 90);
  wetPct = constrain(wetPct, dryPct + MIN_HYSTERESIS_PCT, 95);
  for (ZoneCfg &c : cfg) { c.dryPct = dryPct; c.wetPct = wetPct; }
}

void zonesUpdate(const SensorData &d) {
  if (d.vin < VIN_MIN_V || d.vin > VIN_MAX_V) { trip(FAULT_VIN); return; }
  if (mode != MODE_AUTO || fault != FAULT_NONE) return;

  for (int z = 0; z < NUM_ZONES; z++) {
    if (!cfg[z].enabled || !d.moistureOk[z]) { closeZone(z); continue; }   // never water on a bad reading
    if (d.moisturePct[z] >= cfg[z].wetPct)      closeZone(z);
    else if (d.moisturePct[z] < cfg[z].dryPct)  openZone(z);
  }
}

// Fast protection, independent of the 10 s sensor loop.
void zonesSafetyTick() {
  if (!pumpRunning()) return;

  float amps = readPumpCurrent();
  if (amps > PUMP_MAX_CURRENT_A) { trip(FAULT_OVERCURRENT); return; }
  if (millis() - pumpStartedAt > DRY_RUN_GRACE_MS && amps < PUMP_DRY_CURRENT_A) { trip(FAULT_DRY_RUN); return; }

  for (int z = 0; z < NUM_ZONES; z++) {
    if (valveOpen(z) && millis() - openedAt[z] > MAX_WATER_MS) { trip(FAULT_TIMEOUT); return; }
  }
}

// ===== main.cpp =====
// =============================================================================
//  Smart IoT Irrigation Controller v2 - board Rev B
//  Mohammed Olamilekan Aliyu
//
//  main.cpp : start-up, Blynk link, MODE button and the task schedule
//
//  Watering and protection run from timers and do not depend on Wi-Fi, so the
//  controller keeps working when the network is down.
//
//  Blynk virtual pins
//    V0-V3   soil moisture %, zones 1-4        (read)
//    V4, V5  temperature C, humidity %         (read)
//    V6      supply voltage V                  (read)
//    V7      pump current A                    (read)
//    V8      mode: 0 auto, 1 manual, 2 off     (read/write)
//    V9      fault code, see zones.h           (read)
//    V10-V13 manual valve switches, zones 1-4  (write, manual mode only)
//    V14     dry threshold %                   (write)
//    V15     wet threshold %                   (write)
//    V16     clear fault                       (write)
// =============================================================================

static BlynkTimer timer;
static SensorData data;

// ---- Tasks ------------------------------------------------------------------

static void sensorTask() {
  sensorsRead(data);
  zonesUpdate(data);

  if (Blynk.connected()) {
    for (int z = 0; z < NUM_ZONES; z++) Blynk.virtualWrite(V0 + z, data.moisturePct[z]);
    if (data.dhtOk) { Blynk.virtualWrite(V4, data.tempC); Blynk.virtualWrite(V5, data.humidity); }
    Blynk.virtualWrite(V6, data.vin);
    Blynk.virtualWrite(V7, data.pumpA);
  }

  Serial.printf("M %d %d %d %d | T %.1f H %.0f | VIN %.2f | I %.2f | mode %d fault %d\n",
                data.moisturePct[0], data.moisturePct[1], data.moisturePct[2], data.moisturePct[3],
                data.tempC, data.humidity, data.vin, data.pumpA, zonesMode(), zonesFault());
}

// Short press: AUTO -> MANUAL -> OFF. Long press (2 s): clear a latched fault.
static void buttonTask() {
  static bool          wasDown = false;
  static unsigned long downAt  = 0;

  bool down = (digitalRead(PIN_BTN_MODE) == LOW);
  if (down && !wasDown) downAt = millis();
  if (!down && wasDown) {
    unsigned long held = millis() - downAt;
    if (held > 2000) {
      zonesClearFault();
      buzz(80, 2);
    } else if (held > 40) {                                  // ignore contact bounce
      zonesSetMode((Mode)((zonesMode() + 1) % 3));
      buzz(60, zonesMode() + 1);                             // 1, 2 or 3 beeps
      if (Blynk.connected()) Blynk.virtualWrite(V8, (int)zonesMode());
    }
  }
  wasDown = down;
}

// LED: solid when online, blinking when Wi-Fi is up but Blynk is not, off when offline.
// A new fault is pushed to the app here, so it shows within a second.
static void statusTask() {
  static Fault lastSent = FAULT_NONE;

  bool wifiUp = (WiFi.status() == WL_CONNECTED);
  wifiLed(wifiUp && (Blynk.connected() || (millis() / 500) % 2));

  if (Blynk.connected() && zonesFault() != lastSent) {
    lastSent = zonesFault();
    Blynk.virtualWrite(V9, (int)lastSent);
  }
}

// ---- Blynk handlers -----------------------------------------------------------

BLYNK_WRITE(V8)  { zonesSetMode((Mode)param.asInt()); }
BLYNK_WRITE(V10) { zonesManual(0, param.asInt()); }
BLYNK_WRITE(V11) { zonesManual(1, param.asInt()); }
BLYNK_WRITE(V12) { zonesManual(2, param.asInt()); }
BLYNK_WRITE(V13) { zonesManual(3, param.asInt()); }
BLYNK_WRITE(V14) { zonesSetThresholds(param.asInt(), zoneCfg(0).wetPct); }
BLYNK_WRITE(V15) { zonesSetThresholds(zoneCfg(0).dryPct, param.asInt()); }
BLYNK_WRITE(V16) { if (param.asInt()) zonesClearFault(); }
BLYNK_CONNECTED() { Blynk.syncVirtual(V8, V14, V15); }

// ---- Arduino entry points -------------------------------------------------------

void setup() {
  Serial.begin(115200);
  outputsBegin();                       // outputs off before anything else
  sensorsBegin();
  zonesBegin();
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);

  Serial.println("Smart IoT Irrigation Controller v2 Rev B - Wokwi simulation");
  // sim: Wi-Fi and Blynk are not started; control runs offline exactly as on the board

  timer.setInterval(SENSOR_PERIOD_MS, sensorTask);
  timer.setInterval(SAFETY_PERIOD_MS, zonesSafetyTick);
  timer.setInterval(BUTTON_PERIOD_MS, buttonTask);
  timer.setInterval(STATUS_PERIOD_MS, statusTask);

  buzz(50);                             // power-on beep
}

void loop() {
  timer.run();
}
