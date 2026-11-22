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
#include "config.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "sensors.h"
#include "outputs.h"
#include "zones.h"

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

  WiFi.begin(WIFI_SSID, WIFI_PASS);     // does not block: control starts straight away
  Blynk.config(BLYNK_AUTH_TOKEN);

  timer.setInterval(SENSOR_PERIOD_MS, sensorTask);
  timer.setInterval(SAFETY_PERIOD_MS, zonesSafetyTick);
  timer.setInterval(BUTTON_PERIOD_MS, buttonTask);
  timer.setInterval(STATUS_PERIOD_MS, statusTask);

  buzz(50);                             // power-on beep
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) Blynk.run();
  timer.run();
}
