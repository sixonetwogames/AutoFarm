# 🌱 AutoFarm

A mini automated hydroponic farm powered by an ESP32 microcontroller. Monitors environmental and water conditions in real-time, controls grow equipment via relays, and streams everything to an InfluxDB + Node-RED dashboard.

![ESP32](https://img.shields.io/badge/ESP32-PlatformIO-blue)
![MQTT](https://img.shields.io/badge/MQTT-Mosquitto-purple)
![InfluxDB](https://img.shields.io/badge/InfluxDB-2.x-green)
![Node-RED](https://img.shields.io/badge/Node--RED-Dashboard-red)

## Architecture

```
ESP32 (Sensors + Relays)
    │
    │  MQTT
    ▼
Mosquitto Broker
    │
    ├──► InfluxDB (time-series storage)
    │
    └──► Node-RED (dashboard + automation rules)
```

## Features

### Sensors
- **Temperature & Humidity** — DHT11 with calibration offsets
- **Water Level** — Capacitive sensor mapped to 0–100%
- **pH** — Analog pH module with voltage-based calibration

### Controls
- **Pump** — Water circulation
- **Grow Light** — Lighting schedule
- **Fan** — Air circulation / temperature control

All relays are controllable via MQTT commands from the Node-RED dashboard.

### WiFi Provisioning
The ESP32 stores WiFi and MQTT server credentials in non-volatile storage (NVS) rather than in firmware. On first boot (or if the saved network is unavailable), it automatically creates a WiFi access point called **AUTOFARM** with a captive portal that:

- Scans and lists available networks
- Lets you select SSID + enter password
- Lets you configure the MQTT server IP
- Saves everything to flash and reboots

Credentials can be reset remotely via MQTT or by reflashing.

## Hardware

| Component | Pin | Notes |
|-----------|-----|------------------------------------|
| DHT11     | 32  | Temperature + humidity             |
| H20 Level | GPIO 34 | ADC1 only (capacitive)         |
| pH Sensor | GPIO 35 | ADC1, HiLetgo module           |
| Pump      | GPIO 16 |                                |
| Light     | GPIO 17 |                                |
| Fan       | GPIO 18 |                                |

> ADC1 pins (32–39) are required for analog reads since ADC2 conflicts with WiFi on the ESP32.

## Getting Started

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- MQTT broker (e.g. [Mosquitto](https://mosquitto.org/))
- [InfluxDB 2.x](https://www.influxdata.com/)
- [Node-RED](https://nodered.org/) with `node-red-dashboard` and `node-red-contrib-influxdb`

### 1. Flash the ESP32

```bash
git clone https://github.com/yourusername/autofarm.git
cd autofarm
pio run --target upload
pio device monitor
```

### 2. Configure WiFi + MQTT

On first boot, connect to the **AUTOFARM** WiFi network from your phone or laptop. The captive portal will open automatically — select your WiFi network, enter the password, and set your MQTT server IP. The device will reboot and connect.

### 3. Set Up Mosquitto

Install and run with default settings. The ESP32 publishes to and subscribes under `hydro/grow1/`.

```bash
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable mosquitto
```

### 4. Set Up InfluxDB

Create a bucket for sensor data. Use Telegraf or a Node-RED flow to subscribe to MQTT topics and write to InfluxDB:

```
hydro/grow1/sensors/air_temp
hydro/grow1/sensors/humidity
hydro/grow1/sensors/water_level
hydro/grow1/sensors/ph
```

Each message is JSON:
```json
{
  "value": 72.5,
  "unit": "°F",
  "online": true
}
```

### 5. Set Up Node-RED Dashboard

Import or build flows that:
- Subscribe to `hydro/grow1/sensors/#` for live readings
- Subscribe to `hydro/grow1/status/#` for device status
- Publish to `hydro/grow1/control/<device>` to toggle relays
- Query InfluxDB for historical charts

#### MQTT Control Topics

| Topic | Payload | Action |
|-------|---------|--------|
| `hydro/grow1/control/pump` | `1` / `0` | Toggle pump |
| `hydro/grow1/control/light` | `1` / `0` | Toggle grow light |
| `hydro/grow1/control/fan` | `1` / `0` | Toggle fan |
| `hydro/grow1/control/probe` | any | Re-probe all sensors |
| `hydro/grow1/control/reset_wifi` | any | Clear WiFi/MQTT creds & reboot into setup |

## Calibration

Calibration values are in `config.h`:

```cpp
// DHT offsets (adjust to match a reference thermometer)
#define TEMP_OFFSET_F -6.0f
#define HUMID_OFFSET 7.0

// Water level: measure raw ADC with sensor in air vs submerged
#define WATER_LEVEL_EMPTY 2500
#define WATER_LEVEL_FULL 975

// pH: use pH 7.0 buffer solution, measure voltage
#define PH_V_NEUTRAL 3.46f
#define PH_SLOPE 0.175f
```

## Project Structure

```
autofarm/
├── src/
│   ├── main.cpp           # Setup, loop, WiFi provisioning, captive portal
│   ├── config.h           # Pins, calibration, timing, network defaults
│   ├── sensors.h          # Sensor base classes & registry
│   ├── sensor_impl.h      # DHT, water level, pH implementations
│   ├── relay.h            # Relay controller
│   └── mqtt_handler.h     # MQTT client wrapper
└── platformio.ini
```

## License

MIT
