#pragma once
#include "sensors.h"
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
