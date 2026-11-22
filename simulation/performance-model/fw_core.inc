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
#define ISENSE_SAMPLES 16
#define ISENSE_SAMPLE_US 31
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
#define SENSOR_PERIOD_MS    10000   // board value

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
bool pumpStart();          // soft-start ramp
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
extern bool g_revA;   // model only: true = old board + old code (no RC filter, single sample, no ramp check)
float readPumpCurrent() {
  if (g_revA) return analogReadMilliVolts(PIN_PUMP_ISENSE) / 1000.0f / ISENSE_V_PER_A;
  uint32_t acc = 0;
  for (int i = 0; i < ISENSE_SAMPLES; i++) { acc += analogReadMilliVolts(PIN_PUMP_ISENSE); delayMicroseconds(ISENSE_SAMPLE_US); }
  return (acc / (float)ISENSE_SAMPLES) / 1000.0f / ISENSE_V_PER_A;
}

void sensorsRead(SensorData &d) {
  digitalWrite(PIN_SENS_PWR_N, LOW);            // power sensors
  delay(150);                                   // settle
  for (int i = 0; i < 4; i++) {
    long acc = 0;
    for (int s = 0; s < 8; s++) acc += analogRead(moistPins[i]);
    d.moisturePct[i] = rawToPct(acc / 8);
  }
  float t = NAN, h = NAN;
  t = g_temp; h = g_hum;
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

void simRampLog(float duty, float reading);
bool pumpStart() {
  if (pumpOn) return true;
  const int steps = 50;
  for (int i = 1; i <= steps; i++) {
    ledcWrite(PIN_PUMP_EN, PUMP_MAX * i / steps);
    delay(SOFTSTART_MS / steps);
    float duty = (float)i / steps;
    float rd = readPumpCurrent(); simRampLog(duty, rd);
    if (!g_revA && duty >= 0.2f && rd / duty > PUMP_MAX_CURRENT_A) { ledcWrite(PIN_PUMP_EN, 0); return false; }
  }
  pumpOn = true;
  return true;
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
  if (!pumpRunning()) { if (!pumpStart()) { trip(FAULT_OVERCURRENT); return; } pumpStartedAt = millis(); }
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
