# Performance model

`sim.cpp` is a plant model (pump, valves, soil, supply). It is compiled together with the real
firmware sources in `../../firmware/src`, so the graphs come from the code that runs on the board.

    F=../../firmware
    g++ -O2 -std=c++17 -DREVA -o simA sim.cpp && ./simA     # Rev A reference curves (old board and old code, fw_core.inc)
    g++ -O2 -std=c++17 -Istub -I$F/include -o sim sim.cpp $F/src/sensors.cpp $F/src/outputs.cpp $F/src/zones.cpp && ./sim
    python3 plot.py

`stub/` stands in for the Arduino, DHT and secrets headers on a PC.
