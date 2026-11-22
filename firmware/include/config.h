#pragma once
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
#define SENSOR_PERIOD_MS    10000

// ---- Moisture calibration (raw ADC, 12-bit) ----
#define MOIST_RAW_DRY       3000
#define MOIST_RAW_WET       1200

// ---- Blynk (fill in secrets.h, not committed) ----
#include "secrets.h"
