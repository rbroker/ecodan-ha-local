# ecodan-ha-local
ESP32 compatible program for local monitoring of Mitsubishi Ecodan Air to Water heat pumps with automatic discovery in HomeAssistant.

Uses the CN105 connector on the Cased Flow Temp Controller (FTC6 in my setup) to do very basic control (zone 1 temperature set point, DHW temperature set point, boost DHW) + retrieve basic sensor information from the heat pump.

<p float="left">
  <img src="img/config_page.png" height="640" />
  <img src="img/hp_state.png" height="640" />
</p>

![HomeAssistant MQTT Auto Discovery](img/ha_discover.png)

## Hardware Dependencies
- ESP32-compatible development board (I'm using a LOLIN S2 Mini)
  - Will almost certainly exceed the memory budgets on an ESP8266, as I've not been super careful about memory usage.
- CN105 female connector + pigtails, as described [here](https://github.com/SwiCago/HeatPump#Demo-Circuit).
  - Note: The software is configured to use Serial UART1 on the ESP32, explicitly labelled TX/RX pins on a pinout will usually be pre-assigned to UART0.

## Library Dependencies
- ArduinoJson v6.21.2
- Seeed_Arduino_mbedtls v3.0.1
- MQTT v2.5.1

## First Time Setup
- Clone this repository and build with the Arduino IDE
- Flash the image to the ESP32 board
- Take the board out of firmware update mode, and it should broadcast a wireless network called `ecodan_ha_local`, connect to this network
- Configure the device to match your setup (see [Software Configuration](#Software-Configuration))

## Deploying Updates
After initial flashing + configuration is completed, it's possible to update the firmware over your WiFi network by:
- Building the firmware binary with: `Sketch > Export Compiled Binary`
- Visit the configuration page for your device, by default this will be: [http://ecodan-ha-local/configuration](http://ecodan-ha-local/configuration)
- Scroll down to the Firmware Update section at the bottom of the configuration page
- Select the firmware binary which should be inside a folder such as `build\esp32.esp32.esp32s2\ecodan-ha-local.ino.bin` under `Sketch > Show Sketch Folder`
- Hit the "Update" button, the firmware update should proceed, and load back to the home page when completed

## Software Configuration

### Device Password
Setting a device password will cause the web interface to require the password to be specified each time the board is booted, or the client's browser cookies are cleared.

It's strongly recommended to enable this setting in case the device falls back to broadcasting an open access point, as it will retain other configuration values (MQTT passwords, server, Wifi SSID/Password) which may then be readable by anyone.

| Default | Required |
| ------- | -------- |
| `""`    | No       |

### Serial Rx Port
The GPIO pin number which should be used for Serial data receive.

| Default | Required |
| ------- | -------- |
| 27      | Yes      |

### Serial Tx Port
The GPIO pin number which should be used for Serial data transmit.

| Default | Required |
| ------- | -------- |
| 26      | Yes      |

### Status LED Port
The GPIO pin number which should be used for the status LED. The following flashing patterns are possible:

- 2 short flashes, followed by one long flash: Serial connection to the Heat Pump has not been established.
- Steady long flashes: Network connection to the MQTT server has not been established
- Constantly lit: Status is OK

| Default     | Required |
| ----------- | -------- |
| LED_BUILTIN | No       |

### Heat Pump Configuration
Some parameters of your Mitsubishi Ecodan HVAC
| Parameter   | Description | Default  |
| ----------- | ------------| -------- |
| `Cool enabled` | Check this option if your ecodan has cool working mode. Enable setting cool mode from Home Assistant | False |

### Dump Serial Packets
Dump packets sent to/received from the heat pump to the diagnostic log window on the Diagnostics page.

| Default | Required |
| ------- | -------- |
| Off     | No       |

### WiFi SSID
The SSID of the WiFi network which you'd like the device to connect to. When the diagnostics page is loaded, the device will automatically initiate a scan for available WiFi networks and populate the menu when it completes.

Note: If this setting or "WiFi Password" are unset, the device will continue to boot into a captive portal access point.

| Default | Required |
| ------- | -------- |
| `""`    | Yes      |

### WiFi Password
The passphrase/password which should be used when connecting to the previously specified WiFi SSID.

Note: If this setting or "WiFi SSID" are unset, the device will continue to boot into a captive portal access point.

| Default | Required |
| ------- | -------- |
| `""`    | Yes      |

### Hostname
The network hostname the device will use to identify itself. 

| Default           | Required |
| ----------------- | -------- |
| `ecodan_ha_local` | No       |

### MQTT Server
The IP address or hostname of the MQTT server on your local network.

| Default | Required |
| ------- | -------- |
| `""`    | No       |

### MQTT Port
The port on which your MQTT server is listening. 

| Default | Required |
| ------- | -------- |
| 1883    | No       |

### MQTT User
The username which should be used when connecting to the specified MQTT Server.

| Default | Required |
| ------- | -------- |
| `""`    | No       |

### MQTT Password
The password for the given MQTT User.

| Default | Required |
| ------- | -------- |
| `""`    | No       |

### MQTT Topic
The topic value which the server should use to filter messages related to this heat pump.

| Default     | Required |
| ----------- | -------- |
| `ecodan_hp` | Yes      |


## See Also
There are a number of existing solutions for connecting to Mitsubish heat pump models via the CN105 connector, I wouldn't have been able to put this together without work already done here:
- https://github.com/m000c400/Mitsubishi-CN105-Protocol-Decode
- https://github.com/BartGijsbers/CN105Gateway
- https://github.com/gysmo38/mitsubishi2MQTT

