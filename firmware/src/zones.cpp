#include "zones.h"
#include "outputs.h"
#include "config.h"

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
