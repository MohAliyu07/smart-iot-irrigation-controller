// zones.cpp : watering logic, operating modes and fault handling
//
// Rules this file enforces:
//   1. A valve is always open before the pump starts, and the pump always stops
//      before the last valve closes.
//   2. No more than MAX_ZONES_OPEN valves are open at once.
//   3. Any fault switches everything off and stays latched until it is cleared
//      by a long press of the MODE button or from the app.
#include "zones.h"
#include "outputs.h"
#include "config.h"

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
