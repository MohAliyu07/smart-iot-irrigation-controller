# Smart IoT Irrigation Controller

ESP32-S3 based irrigation controller with soil-moisture sensing, environment monitoring and remote control through the Blynk IoT app. Designed in EasyEDA Pro as a 100 mm round, 2-layer PCB.

## Features
- **Automatic mode:** soil moisture triggers irrigation per zone
- **Manual mode:** control the pump and each valve from the Blynk app or a physical button
- 4 capacitive soil-moisture sensors, DHT11 temperature/humidity, buzzer alerts
- 12 V pump + 4 solenoid valves switched by logic-level MOSFETs

## Hardware
| Block | Part |
|---|---|
| MCU | Seeed XIAO ESP32-S3 (Wi-Fi) |
| I/O expander | MCP23017 (I²C, 0x20), outputs are off at power-up |
| Load switches | 5 × AO3400A + SS34 flyback diodes + status LEDs |
| Power | 12 V DC jack / terminal → 3 A PTC → SS34 reverse protection → K7805-1000R3 (5 V) |
| Connectors | 5.08 mm screw terminals (pump, valves, 12 V), JST-XH (sensors, DHT11, button) |

## Pin map
| XIAO pin | Function |
|---|---|
| D0-D3 | Soil moisture 1-4 (ADC1) |
| D4 / D5 | I²C SDA / SCL |
| D6 | Buzzer |
| D7 | Mode button (active low) |
| D8 | DHT11 |
| D9 | MCP23017 INTA |
| D10 | Spare (header J10) |

MCP23017: GPA0 = pump, GPA1-GPA4 = valves 1-4.

## Status
- [x] Schematic
- [x] PCB layout (DRC clean)
- [ ] Gerbers & BOM
- [ ] Firmware (Arduino + Blynk)
- [ ] Assembly & testing
