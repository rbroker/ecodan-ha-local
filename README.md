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

### Device Password
Setting a device password will cause the web interface to require the password to be specified each time the board is booted, or the client's browser cookies are cleared.

It's strongly recommended to enable this setting in case the device falls back to broadcasting an open access point, as it will retain other configuration values (MQTT passwords, server, Wifi SSID/Password) which may then be readable by anyone.

*Default*: `""`

### Time Zone
This setting contains a timezone value specified in ["TZ" format](https://en.wikipedia.org/wiki/List_of_tz_database_time_zones).

*Default*: `Europe/London`

### WiFi SSID
The SSID of the WiFi network which you'd like the device to connect to. When the diagnostics page is loaded, the device will automatically initiate a scan for available WiFi networks and populate the menu when it completes.

Note: If this setting or "WiFi Password" are unset, the device will continue to boot into a captive portal access point.

*Default*: `""`

### WiFi Password
The passphrase/password which should be used when connecting to the previously specified WiFi SSID.

Note: If this setting or "WiFi SSID" are unset, the device will continue to boot into a captive portal access point.

*Default*: `""`

### Hostname
The network hostname the device will use to identify itself. 

*Default*: `ecodan_ha_local`

### MQTT Configuration


