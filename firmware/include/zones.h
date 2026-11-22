// zones.h : watering logic, operating modes and fault handling
#pragma once
#include "sensors.h"

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
