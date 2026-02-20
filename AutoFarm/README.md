# Hydroponic Monitoring System

## Architecture
```
ESP32 (per grow) ──MQTT──▶ Mosquitto ──▶ Node-RED ──▶ InfluxDB ──▶ Grafana
                              │              │
                              └──── control ◀┘
```

## Quick Start

### Pi Server
```bash
cd hydro-server
mkdir -p mosquitto/{config,data,log} nodered/data influxdb/{data,config} grafana/data
chmod -R 777 mosquitto nodered influxdb grafana  # Fix permissions
docker-compose up -d
```

**Endpoints:**
- Node-RED Dashboard: `http://<pi-ip>:1880/ui`
- Node-RED Editor: `http://<pi-ip>:1880`
- Grafana: `http://<pi-ip>:3000` (admin/admin)
- InfluxDB: `http://<pi-ip>:8086` (admin/changeme123)

### Node-RED Setup
1. Open `http://<pi-ip>:1880`
2. Install palettes: `node-red-dashboard`, `node-red-contrib-influxdb`
3. Import `flows.json` (Menu → Import)
4. Update InfluxDB node config with your token
5. Deploy

### ESP32 Firmware
```bash
cd hydro-esp32
# Edit include/config.h: WIFI_SSID, WIFI_PASS, MQTT_SERVER, GROW_ID
pio run -t upload
```

**Per-unit config:**
- Change `GROW_ID` for each ESP32
- Comment out unused `ENABLE_*` for sensors not connected
- Adjust `PIN_*` for your wiring

## MQTT Topics
| Topic | Direction | Payload |
|-------|-----------|---------|
| `hydro/{grow_id}/sensors/{type}` | ESP→Server | `{"value":6.2,"unit":"pH","ts":12345}` |
| `hydro/{grow_id}/control/{device}` | Server→ESP | `{"state":"on"}` or `{"state":true}` |
| `hydro/{grow_id}/status` | ESP→Server | `{"online":"true"}` |
| `hydro/{grow_id}/status/relays` | ESP→Server | `{"pump":"on","light":"off",...}` |

## Sensor Calibration
Edit `config.h`:
```c
#define PH_OFFSET 0.0f   // Add to reading
#define PH_SLOPE 1.0f    // Multiply reading
#define EC_OFFSET 0.0f
#define EC_SLOPE 1.0f
```

For pH: Use 4.0 and 7.0 buffer solutions, measure raw voltages, calculate slope/offset.

## Adding a Grow
1. Flash new ESP32 with unique `GROW_ID`
2. Add option in Node-RED `grow-selector` dropdown
3. Grafana auto-detects via template variable

## Automation Examples (Node-RED)

**Auto pH dosing** (add function node after alert check):
```js
if (msg.sensorType === 'ph' && msg.sensorValue < 5.5) {
    return { topic: 'ph_up', payload: 'on' };
}
if (msg.sensorType === 'ph' && msg.sensorValue > 6.5) {
    return { topic: 'ph_down', payload: 'on' };
}
return null;
```

**Timed pump cycles** (inject nodes):
- Inject `{"payload":"on","topic":"pump"}` every 15 min
- Delay node → 5 min → `{"payload":"off","topic":"pump"}`

## File Structure
```
hydro-esp32/
├── platformio.ini
├── include/
│   ├── config.h        # ← Edit per unit
│   ├── sensors.h
│   ├── sensor_impl.h
│   ├── relay.h
│   └── mqtt_handler.h
└── src/
    └── main.cpp

hydro-server/
├── docker-compose.yml
├── mosquitto/config/mosquitto.conf
└── grafana/provisioning/
    ├── datasources/influxdb.yml
    └── dashboards/
        ├── dashboard.yml
        └── hydroponics.json

hydro-nodered/
└── flows.json          # Import into Node-RED
```
