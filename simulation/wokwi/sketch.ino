// Smart IoT Irrigation Controller v2 — Wokwi simulation build (ESP32-S3)
// Mohammed Olamilekan Aliyu
// Same code as the PlatformIO firmware, merged into one file. Sim-only changes:
//   * WiFi/Blynk not started (control logic runs offline, as on the board when WiFi is down)
//   * DHT22 (small built-in reader) instead of DHT11 (Wokwi has no DHT11), sensor period 2 s instead of 10 s
#define BLYNK_TEMPLATE_ID   "TMPLsim"
#define BLYNK_TEMPLATE_NAME "Smart Irrigation"
#define BLYNK_AUTH_TOKEN    "sim"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ===== config.h =====
// ---- Smart IoT Irrigation Controller v2 : pin map (ESP32-S3-WROOM-1-N8) ----
#define PIN_MOIST1      1
#define PIN_MOIST2      2
#define PIN_MOIST3      3
#define PIN_MOIST4      4
#define PIN_DHT         5
#define PIN_BTN_MODE    6
#define PIN_BUZZER      7
#define PIN_VIN_SENSE   8
#define PIN_PUMP_ISENSE 9
#define PIN_PUMP_EN     10
#define PIN_VALVE1      11
#define PIN_VALVE2      12
#define PIN_VALVE3      13
#define PIN_VALVE4      14
#define PIN_LED_WIFI    15
#define PIN_SENS_PWR_N  16   // P-FET gate, LOW = sensors powered
#define PIN_SDA         17
#define PIN_SCL         18

#define NUM_ZONES 4

// ---- Analog scaling ----
#define VIN_DIV_RATIO   ((100.0f + 22.0f) / 22.0f)   // 100k / 22k divider
#define ISENSE_V_PER_A  1.0f                         // 20 mOhm x INA181A2 (gain 50)

// ---- Safety limits ----
#define MAX_ZONES_OPEN      2       // 12 V 3 A adapter budget
#define PUMP_MAX_CURRENT_A  2.5f    // trip above this
#define PUMP_DRY_CURRENT_A  0.25f   // below this while running = dry run / no water
#define VIN_MIN_V           10.5f
#define VIN_MAX_V           13.8f
#define MAX_WATER_MS        (20UL * 60UL * 1000UL)  // 20 min hard cap per zone
#define SOFTSTART_MS        500
#define PUMP_PWM_HZ         20000
#define PUMP_PWM_BITS       10

// ---- Defaults (overridable from Blynk) ----
#define DEFAULT_DRY_PCT     35      // start watering below this
#define DEFAULT_WET_PCT     60      // stop watering above this
#define SENSOR_PERIOD_MS    2000    // sim: 2 s (board: 10 s)

// ---- Moisture calibration (raw ADC, 12-bit) ----
#define MOIST_RAW_DRY       3000
#define MOIST_RAW_WET       1200

// ---- Blynk (fill in secrets.h, not committed) ----


// ===== sensors.h =====
struct SensorData { int moisturePct[4]; float tempC; float humidity; float vin; float pumpA; bool dhtOk; };
void sensorsBegin();
void sensorsRead(SensorData &d);
float readPumpCurrent();
float readVin();

// ===== outputs.h =====
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

// ===== zones.h =====
enum Mode { MODE_AUTO = 0, MODE_MANUAL = 1, MODE_OFF = 2 };
enum Fault { FAULT_NONE = 0, FAULT_OVERCURRENT, FAULT_DRY_RUN, FAULT_VIN, FAULT_TIMEOUT };
struct ZoneCfg { int dryPct; int wetPct; bool enabled; };
void zonesBegin();
void zonesUpdate(const SensorData &d);
void zonesManual(int zone, bool on);
void zonesSetMode(Mode m);
Mode zonesMode();
Fault zonesFault();
void zonesClearFault();
ZoneCfg &zoneCfg(int z);
void zonesSafetyTick();

// ===== sensors.cpp =====

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
static const uint8_t moistPins[4] = {PIN_MOIST1, PIN_MOIST2, PIN_MOIST3, PIN_MOIST4};

void sensorsBegin() {
  pinMode(PIN_SENS_PWR_N, OUTPUT);
  digitalWrite(PIN_SENS_PWR_N, HIGH);           // sensors off until needed (stops probe corrosion)
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(PIN_DHT, INPUT_PULLUP);
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
  float t = NAN, h = NAN;
  if (!dhtRead22(t, h)) { t = NAN; h = NAN; }
  d.dhtOk = !(isnan(t) || isnan(h));
  d.tempC = d.dhtOk ? t : NAN;
  d.humidity = d.dhtOk ? h : NAN;
  digitalWrite(PIN_SENS_PWR_N, HIGH);           // power off
  d.vin = readVin();
  d.pumpA = readPumpCurrent();
}

// ===== outputs.cpp =====

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

// ===== zones.cpp =====

static ZoneCfg cfg[NUM_ZONES];
static unsigned long openedAt[NUM_ZONES] = {0};
static Mode mode = MODE_AUTO;
static Fault fault = FAULT_NONE;
static unsigned long pumpStartedAt = 0;

ZoneCfg &zoneCfg(int z) { return cfg[z]; }
Mode zonesMode() { return mode; }
Fault zonesFault() { return fault; }

void zonesBegin() {
  for (auto &c : cfg) { c.dryPct = DEFAULT_DRY_PCT; c.wetPct = DEFAULT_WET_PCT; c.enabled = true; }
}

static void trip(Fault f) { allOff(); fault = f; buzz(150, 3); }
void zonesClearFault() { fault = FAULT_NONE; }

static void openZone(int z) {
  if (fault != FAULT_NONE || valveOpen(z)) return;
  if (valvesOpenCount() >= MAX_ZONES_OPEN) return;   // rule: never exceed adapter budget
  valveSet(z, true); openedAt[z] = millis();
  delay(100);                                          // valve opens before pump builds pressure
  if (!pumpRunning()) { pumpStart(); pumpStartedAt = millis(); }
}
static void closeZone(int z) {
  if (!valveOpen(z)) return;
  if (valvesOpenCount() == 1) pumpStop();              // rule: pump off before last valve closes
  delay(50);
  valveSet(z, false);
}

void zonesSetMode(Mode m) { mode = m; if (m == MODE_OFF) allOff(); }
void zonesManual(int z, bool on) { if (mode == MODE_MANUAL) on ? openZone(z) : closeZone(z); }

void zonesUpdate(const SensorData &d) {
  if (d.vin < VIN_MIN_V || d.vin > VIN_MAX_V) { trip(FAULT_VIN); return; }
  if (mode != MODE_AUTO || fault != FAULT_NONE) return;
  for (int z = 0; z < NUM_ZONES; z++) {
    if (!cfg[z].enabled) { closeZone(z); continue; }
    if (d.moisturePct[z] >= cfg[z].wetPct) closeZone(z);
    else if (d.moisturePct[z] < cfg[z].dryPct) openZone(z);
  }
}

// Called every ~100 ms: fast protection independent of the sensor loop
void zonesSafetyTick() {
  if (!pumpRunning()) return;
  float a = readPumpCurrent();
  if (a > PUMP_MAX_CURRENT_A) { trip(FAULT_OVERCURRENT); return; }
  if (millis() - pumpStartedAt > 5000 && a < PUMP_DRY_CURRENT_A) { trip(FAULT_DRY_RUN); return; }
  for (int z = 0; z < NUM_ZONES; z++)
    if (valveOpen(z) && millis() - openedAt[z] > MAX_WATER_MS) { trip(FAULT_TIMEOUT); return; }
}

// ===== main.cpp =====
// Smart IoT Irrigation Controller v2 — Mohammed Olamilekan Aliyu

// Blynk virtual pins
// V0-V3 moisture %, V4 temp, V5 humidity, V6 VIN, V7 pump A, V8 mode, V9 fault,
// V10-V13 manual zone switches, V14 dry threshold, V15 wet threshold, V16 clear fault
BlynkTimer timer;
SensorData data;

void sensorTask() {
  sensorsRead(data);
  zonesUpdate(data);
  if (Blynk.connected()) {
    for (int i = 0; i < 4; i++) Blynk.virtualWrite(V0 + i, data.moisturePct[i]);
    if (data.dhtOk) { Blynk.virtualWrite(V4, data.tempC); Blynk.virtualWrite(V5, data.humidity); }
    Blynk.virtualWrite(V6, data.vin);
    Blynk.virtualWrite(V7, data.pumpA);
    Blynk.virtualWrite(V9, (int)zonesFault());
  }
  Serial.printf("M %d %d %d %d | T %.1f H %.0f | VIN %.2f | I %.2f | mode %d fault %d\n",
    data.moisturePct[0], data.moisturePct[1], data.moisturePct[2], data.moisturePct[3],
    data.tempC, data.humidity, data.vin, data.pumpA, zonesMode(), zonesFault());
}

BLYNK_WRITE(V8)  { zonesSetMode((Mode)param.asInt()); }
BLYNK_WRITE(V10) { zonesManual(0, param.asInt()); }
BLYNK_WRITE(V11) { zonesManual(1, param.asInt()); }
BLYNK_WRITE(V12) { zonesManual(2, param.asInt()); }
BLYNK_WRITE(V13) { zonesManual(3, param.asInt()); }
BLYNK_WRITE(V14) { for (int z = 0; z < NUM_ZONES; z++) zoneCfg(z).dryPct = param.asInt(); }
BLYNK_WRITE(V15) { for (int z = 0; z < NUM_ZONES; z++) zoneCfg(z).wetPct = param.asInt(); }
BLYNK_WRITE(V16) { if (param.asInt()) zonesClearFault(); }
BLYNK_CONNECTED() { Blynk.syncVirtual(V8, V14, V15); }

// Mode button: short press cycles AUTO -> MANUAL -> OFF; long press (2 s) clears fault
void buttonTask() {
  static bool last = HIGH; static unsigned long down = 0;
  bool b = digitalRead(PIN_BTN_MODE);
  if (last == HIGH && b == LOW) down = millis();
  if (last == LOW && b == HIGH) {
    unsigned long held = millis() - down;
    if (held > 2000) { zonesClearFault(); buzz(80, 2); }
    else if (held > 40) { zonesSetMode((Mode)((zonesMode() + 1) % 3)); buzz(60, zonesMode() + 1);
                          if (Blynk.connected()) Blynk.virtualWrite(V8, (int)zonesMode()); }
  }
  last = b;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Smart IoT Irrigation Controller v2 - Wokwi simulation");
  outputsBegin();            // all outputs off first
  sensorsBegin();
  zonesBegin();
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  // sim: WiFi/Blynk skipped, control runs offline exactly as on the board
  timer.setInterval(SENSOR_PERIOD_MS, sensorTask);
  timer.setInterval(100L, zonesSafetyTick);
  timer.setInterval(20L, buttonTask);
  timer.setInterval(1000L, [] { wifiLed(false); });
  buzz(50);
}

void loop() {
  timer.run();
}
