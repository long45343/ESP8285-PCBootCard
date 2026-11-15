# ESP8285 PC Boot Card

A boot card firmware for the ESP8285, compatible with both Bemfa Cloud and Home Assistant (HA). The Home Assistant mode has not been fully tested.

## Live Demo

Not interested in the code? There is a browser-based demo that walks through the setup, login and status pages:

**[Open the live demo](https://long45343.github.io/ESP8285-PCBootCard/)**

The demo is a pure front-end simulation and does not connect to any real device. The panel in the bottom-right corner lets you trigger power on/off and watch the status and logs change.

## Features

- **Remote power on/off and reset**: Controls Power_SW and Reset_SW remotely, and still works when the PC is frozen
- **Status reporting**: The PC power state is reported back to Bemfa Cloud / HA
- **Web-based WiFi setup**: Connect to the board's hotspot, open the web page and enter your WiFi password — no code changes needed
- **Web maintenance**: On the same LAN, open the board's web page to view logs, update firmware, switch WiFi, and change the access password
- **Status LED**: Slow blink during setup, fast blink while connecting, off when normal, solid on when there is a problem

## Hardware

- MCU: ESP8285 (1MB Flash)
- Slot: PCIe x1
- Power: 3.3V standby rail from the PCIe slot
- Peripherals: Two or four optocouplers (for the chassis power button and reset button; four optocouplers can coexist with the motherboard's power button), one status LED, one onboard button

## Pinout

| Function | GPIO | Description |
|---|---|---|
| Power button optocoupler | 12 | Outputs high for about 500ms, simulating a short press |
| Reset button optocoupler | 13 | Outputs high for about 500ms, simulating a short press |
| Status LED | 2 | Active low |
| Power state detection | 14 | Connected to PCIe A11 (PERST#), pulled-up input |
| Onboard button | 4 | Hold for 3 seconds to clear WiFi config and reboot into setup mode |

## Building

Use Arduino IDE or arduino-cli with the **Generic ESP8285 Module** board:

```
Flash Size: 1MB (FS:64KB OTA:~470KB)
Flash Mode: DOUT
CPU Frequency: 80MHz
```

Required libraries:

- PubSubClient (MQTT)
- ArduinoJson (parsing web form data and Bemfa Cloud API responses)

Build with arduino-cli:

```bash
arduino-cli compile \
  --fqbn "esp8266:esp8266:esp8285:eesz=1M64" \
  --output-dir build \
  .
```

The first flash must be done over USB-to-serial. After that, the firmware supports OTA updates through the web page — just upload a new `.bin` file, no need to open the case again.

## Compatibility

The code is not tied to the ESP8285. How much work it takes to move to another chip depends a lot on which one.

### ESP8266 family: almost no changes

The ESP8285 is essentially an ESP8266 with built-in Flash. Same instruction set, same WiFi stack, same Arduino libraries.

| Module | Flash | Notes |
|---|---|---|
| ESP-12F / ESP-12S | 4MB | The most common module, more pins than the 8285 |
| NodeMCU | 4MB | Dev board with onboard USB-to-serial |
| Wemos D1 mini | 4MB | Same as above, smaller footprint |
| ESP-01 / ESP-01S | 1MB | Not recommended — only 2 usable pins, not enough for two optocouplers |

**No code changes needed, only the Flash size in the build settings.** For example, on an ESP-12F:

```
Flash Size: 4MB (FS:1MB OTA:~1019KB)
Flash Mode: DOUT
CPU Frequency: 80MHz
```

The matching arduino-cli command:

```bash
arduino-cli compile \
  --fqbn "esp8266:esp8266:generic:eesz=4M1M" \
  --output-dir build \
  .
```

OTA space grows from 470KB to about 1MB. Note that ESP8266 OTA splits Flash into two firmware slots, so roughly 1MB is the ceiling no matter how large the chip is.

### ESP32 family: a dozen or so changes

The ESP32, ESP32-S2, ESP32-S3, ESP32-C3 and ESP32-C6 all work, but the underlying libraries have different names. These parts need changing:

| Current | On ESP32 |
|---|---|
| `ESP8266WiFi.h` | `WiFi.h` |
| `ESP8266WebServer.h` | `WebServer.h` |
| `ESP8266mDNS.h` | `ESPmDNS.h` |
| `Updater.h` | `Update.h` |
| `EEPROM.h` | `Preferences.h` |
| `ESP.getChipId()` | `ESP.getEfuseMac()` |
| `WiFi.scanComplete()` | Not available on ESP32; scanning uses event callbacks |

**Why move to ESP32**: at least 4MB Flash, 520KB RAM (vs 80KB now), plenty of OTA space, and room for features like a buzzer or POST code reading.

**What it costs**: the board has to be redesigned — different pin voltages and packages — and the code takes half a day to port.

### Chips to avoid

- **Other WiFi chips** (BK7231, RTL8710, etc.): Arduino support is incomplete, many libraries won't work, porting means rewriting
- **Chips without WiFi** (Arduino UNO, STM32): you would need an ESP-01 as a WiFi module, plus a serial protocol between the two chips — several times the work and less reliable

## WiFi Setup

1. On first power-up, or after holding the onboard button for 3 seconds, the board opens a hotspot named `PCBoot-XXXX`
2. Connect to this hotspot; the setup page should pop up automatically (if not, open `192.168.4.1` manually)
3. Select your WiFi, enter the password, then choose the control mode:
   - **Bemfa Cloud**: Enter your 32-character private key; the two switch topics are created automatically
   - **Home Assistant**: Enter the MQTT server address and port; three entities are registered automatically
4. Click Save; the board reboots and connects

## Web Maintenance

When the board and your phone are on the same WiFi, open `http://pcboot.local` or the board's IP address in a browser.

From this page you can:

- View device status (WiFi signal, MQTT connection, PC power state, free memory)
- View runtime logs (roughly the last 25 minutes)
- Upload a `.bin` file to update the firmware
- Scan for nearby WiFi networks and switch
- Change the web access password

## About Bemfa Cloud

- Your private key is shown on the home page of the [Bemfa Cloud MQTT console](https://cloud.bemfa.com/mdevice.php)
- The two topic names are derived from the last four characters of the board's MAC address, e.g. `pcpwswb2c3006` and `pcpwrstb2c3006`
- The last three characters of the topic name determine the device type; `006` means a switch device

## About Home Assistant

The board automatically publishes MQTT Discovery messages, and three entities appear in HA:

- `button.pc_power`: Press to power on or off
- `button.pc_reset`: Press to reset
- `binary_sensor.pc_status`: Reflects the actual PC power state

Power is a button rather than a switch because physically it is a "press", not a "hold state". Whether the PC is currently on or off is shown by the status sensor.

## Notes

- The board is powered from the PCIe slot's standby rail, so it keeps working even when the PC is off
- If you enter the wrong WiFi password when switching, the board will not connect — hold the onboard button for 3 seconds to re-enter setup mode
- Do not cut power during a firmware update; if it gets corrupted, you will have to pull the board out and recover it over USB
- If you forget the web password, hold the onboard button for 3 seconds to enter setup mode, clear the password field, and save

## File Overview

| File | Contents |
|---|---|
| `LongBootCard.ino` | Main program: WiFi setup, MQTT, button control, state detection, web server |
| `web_page.h` | WiFi setup page |
| `status_page.h` | Runtime status page (logs, OTA, WiFi switching, password change) |
| `login_page.h` | Login page |
