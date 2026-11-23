// =============================================================================
//  Smart IoT Irrigation Controller v2 - board Rev B
//  config.h : pin map, electrical scaling, limits and defaults
//
//  MCU   : ESP32-S3-WROOM-1-N8
//  Board : Rev B (pump MOSFET AO4404B, INA181A2 output filtered by 1 k + 100 nF)
//  Every number that depends on the hardware lives in this file.
// =============================================================================
#pragma once

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
#define VIN_FAULT_TICKS      2        // supply must be out of range on 2 safety ticks in a row (rejects short dips)
#define MAX_WATER_MS        (20UL * 60UL * 1000UL)   // hard cap per zone: 20 minutes
#define VALVE_LEAD_MS       100      // valve opens this long before the pump starts
#define PUMP_LAG_MS         50       // pump stops this long before the last valve closes

// ---- Watering thresholds (changeable from Blynk) ------------------------------
#define DEFAULT_DRY_PCT     35       // start watering below this
#define DEFAULT_WET_PCT     60       // stop watering at or above this
#define MIN_HYSTERESIS_PCT  5        // wet must stay at least this far above dry
#define SENSOR_PERIOD_MS    10000UL  // soil is read every 10 s

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
#include "secrets.h"
