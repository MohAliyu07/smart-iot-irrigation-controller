// Smart IoT Irrigation Controller v2 — Mohammed Olamilekan Aliyu
#include "config.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "sensors.h"
#include "outputs.h"
#include "zones.h"

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
  outputsBegin();            // all outputs off first
  sensorsBegin();
  zonesBegin();
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Blynk.config(BLYNK_AUTH_TOKEN);
  timer.setInterval(SENSOR_PERIOD_MS, sensorTask);
  timer.setInterval(100L, zonesSafetyTick);
  timer.setInterval(20L, buttonTask);
  timer.setInterval(1000L, [] { wifiLed(WiFi.status() == WL_CONNECTED && (Blynk.connected() || (millis() / 500) % 2)); });
  buzz(50);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) Blynk.run();   // non-blocking: control continues offline
  timer.run();
}
