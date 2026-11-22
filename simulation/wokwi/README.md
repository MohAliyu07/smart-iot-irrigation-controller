# Smart IoT Irrigation Controller v2 – Wokwi simulation (ESP32-S3)

Open https://wokwi.com/projects/new/esp32-s3, then replace:
- `sketch.ino`  -> this sketch.ino
- `diagram.json` -> this diagram.json
- add a file `libraries.txt` -> this libraries.txt
Press the green Play button. Serial monitor prints one status line every 2 s.

Same pin connections as the PCB (ESP32-S3-WROOM-1, firmware config.h):
| GPIO | PCB net | In the simulation |
|---|---|---|
| 1–4 | Soil moisture 1–4 | Pots soil1–soil4 (turn clockwise = drier) |
| 5 | DHT data | DHT22 (Wokwi has no DHT11) |
| 6 | MODE button | Blue MODE button |
| 7 | Buzzer | Buzzer |
| 8 | VIN sense (100k/22k) | Pot "vin" (591 ≈ 12 V) |
| 9 | Pump current (INA181) | Pot "ipump" (300 ≈ 1.1 A) |
| 10 | Pump PWM | Blue LED PUMP |
| 11–14 | Valves 1–4 | Green LEDs VALVE 1–4 |
| 15 | WiFi LED | Yellow LED WIFI |

Serial line: `M <moisture % x4> | T <°C> H <%> | VIN <V> | I <A> | mode <0 auto,1 manual,2 off> | fault <0 none,1 overcurrent,2 dry run,3 supply,4 timeout>`
