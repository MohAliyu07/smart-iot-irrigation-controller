# Rev B firmware: rewrite notes

The firmware was rewritten for the Rev B board. Pins, Blynk virtual pins and limits are the same as before.

## Structure
- `config.h`: every hardware number in one place (pins, scaling, limits, timings). No bare numbers are left in the code.
- `sensors`, `outputs`, `zones`, `main`: one job each, each header says what its functions do and how long they block.

## Carried over from the first Rev B edit
- Pump current is averaged over 16 samples (0.5 ms, 10 PWM periods).
- `pumpStart()` checks the current on every soft-start step and stops a stalled pump during the ramp.
  The shunt is in the MOSFET source, so the reading is duty x motor current; the code divides by duty.

## Behaviour that is new in this rewrite
1. Changing mode (button or app) stops the pump and closes every valve. Before, only OFF did this,
   so a valve opened in MANUAL stayed open after switching to AUTO.
2. A soil sensor that reads below 100 or above 4000 raw (shorted, open or unplugged) is marked bad
   and its zone is not watered. Before, an unplugged sensor could read as "dry" and water for 20 minutes.
3. Thresholds sent from the app are range-checked, and wet is kept at least 5 % above dry.
4. Only the first fault is latched, so a fault that stays present does not sound the buzzer every 10 s.
5. A new fault code is sent to the app within 1 s instead of on the next 10 s sensor read.

## Checked
- The performance model now compiles the real files in `firmware/src` (not a copy) with a plant model.
  Results: 250 ms trips, 1.62 A soft-start peak, max 2 valves, 0 faults in 8 h, stalled start stopped at 2.54 A after 429 ms.
- `main.cpp` and the Wokwi sketch pass a syntax check against stub headers.

## Not checked
- Not built with PlatformIO for the ESP32, and not run on a board.
- Wokwi and Proteus were not run again.
- Items 1 to 5 above are not exercised by the model's test scenarios.
