# ecodan-ha-local
ESP32 compatible program for local monitoring of Mitsubishi Ecodan Air to Water heat pumps.

Uses the CN105 connector on the Cased Flow Temp Controller (FTC6 in my setup) to control + monitor the heat pump.

There are a number of existing solutions for connecting to Mitsubish heat pump models via the CN105 connector, though I couldn't find any which were tailored towards air-to-water heat pumps rather than the more common air-to-air versions.

## Hardware Dependencies
- ESP32-compatible development board (I'm using a Wemos S2 Mini)
- CN105 female connector + pigtails, as described [here](https://github.com/SwiCago/HeatPump#Demo-Circuit).

## Library Dependencies
- ArduinoJson v6.21.2
- Seeed_Arduino_mdedtls v3.0.1

## Software Configuration