# AutoFarm

A mini automated hydroponic farm powered by an ESP32. It monitors temperature, humidity, water level, and pH, and controls grow equipment (pump, light, fan) via relays.

**The Raspberry Pi / server stack is completely optional.** You choose at setup:

- **Local mode** — the ESP32 hosts its own live web dashboard. No server, no broker, nothing else to run. Just flash and go.
- **MQTT mode** — the ESP32 streams readings to an MQTT broker for logging and dashboards (InfluxDB + Node-RED), for multi-device setups or long-term history.

You pick the mode in the WiFi setup portal — no recompiling to switch.

![ESP32](https://img.shields.io/badge/ESP32-PlatformIO-blue) ![MQTT](https://img.shields.io/badge/MQTT-optional-purple) ![InfluxDB](https://img.shields.io/badge/InfluxDB-optional-green) ![Node-RED](https://img.shields.io/badge/Node--RED-optional-red)

## Operating modes

### Local mode (no server required)

```
ESP32 ──► hosts dashboard at http://<grow-id>.local/
          (sensors read on demand, nothing leaves the device)
```

The ESP32 joins your WiFi and serves a self-contained dashboard showing live sensor cards and relay toggle buttons. Sensors are read on demand when the page refreshes; no data is sent anywhere. This is the simplest setup and all most users need.

### MQTT mode (optional server stack)

```
ESP32 ──MQTT──► Mosquitto ──► InfluxDB  (time-series storage)
                          └─► Node-RED  (dashboard + automation)
```

Use this only if you want historical logging, automation rules, or a single dashboard across multiple grows. It requires the server stack below.

## Features

- **Temperature & Humidity** — DHT11 with calibration offsets
- **Water Level** — capacitive sensor mapped to 0–100%
- **pH** — analog pH module with voltage-based calibration
- **Relays** — pump, grow light, fan, controllable from the local dashboard *or* via MQTT
- **On-device dashboard** — live stats + relay control, zero external dependencies
- **WiFi provisioning** — captive portal stores WiFi/mode/server settings in flash (NVS)

### WiFi Provisioning

The ESP32 stores its settings in non-volatile storage (NVS) rather than in firmware. On first boot (or if the saved network is unavailable), it creates a WiFi access point called **AUTOFARM** with a captive portal that:

- Scans and lists available networks
- Lets you select SSID + enter password
- Lets you choose **Data Mode**: *Host dashboard on ESP32* (local) or *Send to MQTT server*
- For MQTT mode, lets you set the broker IP/hostname
- Saves everything to flash and reboots

In local mode the MQTT field is hidden — it isn't needed. Settings can be reset remotely via MQTT (MQTT mode) or by reflashing.

## Hardware

### Pin map

| Component | ESP32 pin | Type | Power | Pull-up / notes |
|---|---|---|---|---|
| DHT11 `DATA` | GPIO 32 | digital | 3.3V | 4.7–10kΩ → 3.3V (skip if using a 3-pin module — pull-up is onboard) |
| Water level `AOUT` | GPIO 34 | ADC1 in (input-only) | **3.3V** | none |
| pH `Po` (**via 5:1 divider**) | GPIO 35 | ADC1 in (input-only) | module = **5V** | 39kΩ top + 10kΩ bottom |
| Pump relay `IN1` | GPIO 16 | digital out | — | 10kΩ → 3.3V (boot hold-off; relays are active-LOW) |
| Light relay `IN2` | GPIO 17 | digital out | — | 10kΩ → 3.3V |
| Fan relay `IN3` | GPIO 18 | digital out | — | 10kΩ → 3.3V |

> Analog reads must use ADC1 pins (GPIO 32–39) — ADC2 conflicts with WiFi on the ESP32. GPIO 34/35 are input-only and have no internal pull-ups, which is correct for analog inputs.

### pH voltage divider

The firmware reconstructs the true pH-module voltage by multiplying the measured pin voltage by `PH_VOLTAGE_DIVIDER_RATIO` (5.0), so a **5:1 divider** between the module's `Po` output and GPIO 35 is required:

```
pH Po ──[ 39–40kΩ ]──┬── GPIO 35
                     │
                 [ 10kΩ ]
                     │
                    GND
```

Ratio = 1 + Rtop/Rbottom = 5. At pH 7 the module outputs ~2.5V, so the pin sees ~0.5V — safely within ADC range. A standard 39kΩ gives a ratio of 4.9 (set `PH_VOLTAGE_DIVIDER_RATIO 4.9f` to match); use 40.2kΩ for exactly 5.0.

### Power & ground

| Rail | Feeds | Source |
|---|---|---|
| **5V** | ESP32 5V/VIN, pH module VCC, relay board VCC | 5V ≥2A USB charger into the ESP32; tap the 5V/VIN pin for the pH module + relay coils (~210mA total) |
| **3.3V** | DHT11, water-level sensor | ESP32 onboard regulator |
| **GND (common)** | ESP32, all sensors, relay logic side, 5V source | All tied to a single ground bus |

All grounds must be common — the analog reads on GPIO 34/35 are meaningless without a shared reference. The relay's **switched side** (COM/NO → pump/light/fan) stays electrically isolated and runs on its own load supply.

### Relay logic

Relays are **active-LOW**: the firmware (`relay.h`) initializes each pin HIGH (off) and drives it LOW to switch on. Because GPIO 16/17/18 float at power-on before `setup()` runs, the 10kΩ pull-ups to 3.3V keep the pump, light, and fan off during the boot window.

### Bill of materials

| Item | Suggested part | Notes |
|---|---|---|
| ESP32 WROOM-32 DevKitC, 38-pin | [HiLetgo 3-pack](https://www.amazon.com/HiLetgo-ESP-WROOM-32-Bluetooth-ESP32-DevKitC-32-Development/dp/B0CNYK7WT2) | Use WROOM, not WROVER (WROVER ties up GPIO 16/17 for PSRAM) |
| DHT11, 3-pin module | [HiLetgo 5-pack](https://www.amazon.com/HiLetgo-Temperature-Humidity-Digital-3-3V-5V/dp/B01DKC2GQ0) | Onboard pull-up — no external DHT resistor needed |
| Water-level probe (capacitive) | [Stemedu capacitive v2.0, 5-pack](https://www.amazon.com/Stemedu-Capacitive-Corrosion-Resistant-Electronic/dp/B0BTHL6M19) | Power at 3.3V; matches the EMPTY/FULL calibration. Choose a version with the onboard 3.3V regulator |
| pH module + BNC probe | [Teyleten PH0-14](https://www.amazon.com/Teyleten-Robot-Acquisition-Alkalinity-Monitoring/dp/B09H1MJS4S) | 5V power; analog output via the 5:1 divider |
| 4-channel relay, 5V active-LOW | [ELEGOO 4-channel](https://www.amazon.com/ELEGOO-Channel-Optocoupler-Compatible-Raspberry/dp/B09ZQS2JRD) | Matches firmware logic; 4th channel spare |
| Divider + relay pull-ups (resistors) | [Aniann 1280pc 1% kit](https://www.amazon.com/Resistor-Assorted-Resistors-Assortment-Experiments/dp/B07L851T3V) | Contains 39kΩ + 10kΩ (pH divider) and 3× 10kΩ (relay pull-ups) |
| Divider (small-part alternative) | [3296W multiturn trimpot kit](https://www.amazon.com/3296W-Multiturn-Trimmer-Potentiometer-Kit/dp/B0FDK65H47) | Wire a 20kΩ trimpot as an adjustable divider, wiper → GPIO 35 |
| Power | Any 5V ≥2A USB charger + USB cable | No separate PSU required |

> Not listed (your choice of hardware and supply): the pump, grow light, and fan, which connect to the relay COM/NO terminals on the isolated switched side.

## Getting Started

### 1. Flash the ESP32

```
git clone https://github.com/yourusername/autofarm.git
cd autofarm
pio run --target upload
pio device monitor
```

Requires [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

### 2. Provision over WiFi

On first boot, connect to the **AUTOFARM** WiFi network from your phone or laptop. The captive portal opens automatically:

1. Select your WiFi network and enter the password.
2. Choose a **Data Mode**:
   - **Host dashboard on ESP32** — done after this, skip to step 3.
   - **Send to MQTT server** — enter your broker IP/hostname, then set up the server stack (below).
3. Enter a **Grow ID** (unique device name, e.g. `grow1`).

The device saves and reboots.

### 3a. Local mode — open the dashboard

That's it. Open **`http://<grow-id>.local/`** (e.g. `http://grow1.local/`) on any device on the same WiFi. The serial monitor also prints the raw IP as a fallback if `.local` doesn't resolve on your network.

The dashboard shows live sensor cards and pump/light/fan toggle buttons. No server needed.

### 3b. MQTT mode — optional server stack

Only needed if you chose MQTT mode and want logging/automation.

<details>
<summary>Server setup (Mosquitto + InfluxDB + Node-RED)</summary>

**Prerequisites:** an MQTT broker ([Mosquitto](https://mosquitto.org/)), [InfluxDB 2.x](https://www.influxdata.com/), and [Node-RED](https://nodered.org/) with `node-red-dashboard` and `node-red-contrib-influxdb`.

**Mosquitto**
```
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable mosquitto
```
The ESP32 publishes to and subscribes under `hydro/<grow-id>/`.

**InfluxDB** — create a bucket and write the MQTT topics to it (via Telegraf or a Node-RED flow):
```
hydro/<grow-id>/sensors/air_temp
hydro/<grow-id>/sensors/humidity
hydro/<grow-id>/sensors/water_level
hydro/<grow-id>/sensors/ph
```
Each message is JSON:
```json
{ "value": 72.5, "unit": "°F", "online": true }
```

**Node-RED** — paste `flows.json` into the flow editor at `http://<server-ip>:1880`.

**MQTT control topics**

| Topic | Payload | Action |
|---|---|---|
| `hydro/<grow-id>/control/pump` | `1` / `0` | Toggle pump |
| `hydro/<grow-id>/control/light` | `1` / `0` | Toggle grow light |
| `hydro/<grow-id>/control/fan` | `1` / `0` | Toggle fan |
| `hydro/<grow-id>/control/probe` | any | Re-probe all sensors |
| `hydro/<grow-id>/control/reset_wifi` | any | Clear settings & reboot into setup |

</details>

## Calibration

Calibration values are in `config.h`:

```c
// DHT offsets (adjust to match a reference thermometer)
#define TEMP_OFFSET_F -6.0f
#define HUMID_OFFSET 7.0

// Water level: measure raw ADC with sensor in air vs submerged
#define WATER_LEVEL_EMPTY 2500
#define WATER_LEVEL_FULL 975

// pH: use pH 7.0 buffer solution, measure voltage
#define PH_V_NEUTRAL 3.46f
#define PH_SLOPE 0.175f
#define PH_VOLTAGE_DIVIDER_RATIO 5.0f   // 5:1 hardware divider on GPIO 35
```

## Project Structure

```
autofarm/
├── src/
│   ├── main.cpp           # Setup, loop, provisioning, mode routing
│   ├── config.h           # Pins, calibration, timing, network defaults
│   ├── credentials.h      # NVS storage + mode (local / mqtt)
│   ├── wifi_manager.h     # STA WiFi connection
│   ├── portal.h           # Captive setup portal (mode selector)
│   ├── sensors.h          # Sensor registry & reading struct
│   ├── sensor_impl.h      # DHT, water level, pH implementations
│   ├── relay.h            # Relay controller (active-LOW)
│   ├── local_server.h     # On-device web dashboard (local mode)
│   └── mqtt_handler.h     # MQTT client wrapper (MQTT mode)
└── platformio.ini
```

## License

MIT
