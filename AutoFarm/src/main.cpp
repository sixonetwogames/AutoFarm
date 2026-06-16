#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "credentials.h"
#include "wifi_manager.h"
#include "portal.h"
#include "sensors.h"
#include "sensor_impl.h"
#include "relay.h"
#include "mqtt_handler.h"
#include "local_server.h"

// ── App state ────────────────────────────────────────────────────────
enum AppState { STATE_PROVISIONING, STATE_RUNNING };
AppState appState = STATE_PROVISIONING;

Credentials creds;
CaptivePortal portal;

// ── Hardware ─────────────────────────────────────────────────────────
DhtSensor        dhtSensor(PIN_DHT);
WaterLevelSensor waterLevelSensor(PIN_WATER_LEVEL);
PhSensor         phSensor(PIN_PH);
SensorRegistry   sensorRegistry;
RelayController  relays;
MqttHandler      mqtt;
LocalServer      localServer;   // serves the on-device dashboard in local mode

unsigned long lastRead = 0, lastStatusPrint = 0;

// ── Forward declarations ─────────────────────────────────────────────
void initSensors();
void initRelays();
void enterRunning();
void enterProvisioning();
void probeSensors();
void publishSensorStatus();
void publishRelayStates();
void publishSensor(const SensorReading& r);
void printSensorReadings();
void buildSnapshot(JsonDocument& doc);
void onMqttMessage(const char* topic, const char* payload);

// ── Setup & Loop ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n════════════════════════════════════");
    Serial.println("  AUTOFARM HYDRO CONTROLLER");
    Serial.println("════════════════════════════════════\n");

    initSensors();
    initRelays();

    creds = CredentialStore::load();
    if (creds.complete() && WifiManager::connect(creds)) {
        enterRunning();
    } else {
        Serial.println("[Init] No valid credentials — entering provisioning.");
        enterProvisioning();
    }
}

void loop() {
    if (appState == STATE_PROVISIONING) {
        portal.loop();
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Lost connection, retrying...");
        if (!WifiManager::connect(creds)) {
            enterProvisioning();
            return;
        }
    }

    unsigned long now = millis();

    if (creds.isLocal()) {
        // Local mode: just service the web dashboard. Sensors are read
        // on demand when the browser hits /data — nothing is sent out.
        localServer.loop();
    } else {
        bool wasConnected = mqtt.connected();
        mqtt.loop();

        if (!wasConnected && mqtt.connected()) {
            publishSensorStatus();
            publishRelayStates();
        }

        if (mqtt.connected() && now - lastRead > SENSOR_INTERVAL) {
            lastRead = now;
            publishSensor(dhtSensor.readTemp());
            publishSensor(dhtSensor.readHumidity());
            publishSensor(waterLevelSensor.read());
            publishSensor(phSensor.read());
        }
    }

    if (now - lastStatusPrint > STATUS_PRINT_INTERVAL) {
        lastStatusPrint = now;
        printSensorReadings();
    }
}


// ── Publishers (MQTT mode) ───────────────────────────────────────────
void publishSensorStatus() {
    JsonDocument doc;
    sensorRegistry.getStatus(doc);
    mqtt.publish("status/sensors", doc);
}

void publishRelayStates() {
    JsonDocument doc;
    relays.getStates(doc);
    mqtt.publish("status/relays", doc);
}

void publishSensor(const SensorReading& r) {
    if (!r.valid) return;
    JsonDocument doc;
    doc["value"]  = r.value;
    doc["unit"]   = r.unit;
    doc["online"] = sensorRegistry.isOnline(r.name);

    char subtopic[48];
    snprintf(subtopic, sizeof(subtopic), "sensors/%s", r.name);
    mqtt.publish(subtopic, doc);
}

void probeSensors() {
    sensorRegistry.probeAll();
    if (!creds.isLocal() && mqtt.connected()) publishSensorStatus();
}

void printSensorReadings() {
    auto temp  = dhtSensor.readTemp();
    auto hum   = dhtSensor.readHumidity();
    auto level = waterLevelSensor.read();
    auto ph    = phSensor.read();

    Serial.println("─────────────────────────────────");
    Serial.printf("  Temp:        %6.1f °F  %s\n",   temp.value,  temp.valid ? "" : "[INVALID]");
    Serial.printf("  Humidity:    %6.1f %%   %s\n",  hum.value,   hum.valid  ? "" : "[INVALID]");
    Serial.printf("  Water Level: %6.1f %%   (raw: %d)\n", level.value, waterLevelSensor.readRaw());
    Serial.printf("  pH:          %6.2f     (%.2fV)\n",   ph.value,    phSensor.readVoltage());
    Serial.println("─────────────────────────────────");
}

// ── Local dashboard snapshot (local mode) ────────────────────────────
static void addReading(JsonObject obj, const SensorReading& r) {
    JsonObject o = obj[r.name].to<JsonObject>();
    o["value"]  = r.value;
    o["unit"]   = r.unit;
    o["online"] = r.valid;
}

// Fills the JSON served at /data. Add a sensor here and it shows up on
// the dashboard automatically — no HTML changes needed.
void buildSnapshot(JsonDocument& doc) {
    JsonObject s = doc["sensors"].to<JsonObject>();
    addReading(s, dhtSensor.readTemp());
    addReading(s, dhtSensor.readHumidity());
    addReading(s, waterLevelSensor.read());
    addReading(s, phSensor.read());

    JsonDocument rdoc;
    relays.getStates(rdoc);
    doc["relays"] = rdoc;

    doc["grow"]   = creds.growId;
    doc["uptime"] = millis() / 1000;
}

// ── MQTT command routing (MQTT mode) ─────────────────────────────────
void onMqttMessage(const char* topic, const char* payload) {
    Serial.printf("[MQTT] << %s: %s\n", topic, payload);

    const char* slash = strrchr(topic, '/');
    if (!slash) return;
    const char* device = slash + 1;

    if (strcmp(device, "probe") == 0) { probeSensors(); return; }

    if (strcmp(device, "reset_wifi") == 0) {
        Serial.println("[CMD] Clearing credentials, restarting...");
        CredentialStore::clear();
        delay(500);
        ESP.restart();
        return;
    }

    if (relays.handleCommand(device, payload)) {
        Serial.printf("[Relay] %s -> %s\n", device, relays.get(device) ? "ON" : "OFF");
        publishRelayStates();
    }
}

// ── Init helpers ─────────────────────────────────────────────────────
void initSensors() {
    dhtSensor.begin();
    waterLevelSensor.begin();
    phSensor.begin();
    sensorRegistry.add("air_temp",    []() { return dhtSensor.probe(); });
    sensorRegistry.add("humidity",    []() { return dhtSensor.probe(); });
    sensorRegistry.add("water_level", []() { return waterLevelSensor.probe(); });
    sensorRegistry.add("ph",          []() { return phSensor.probe(); });
    probeSensors();
}

void initRelays() {
    relays.add("pump",  PIN_RELAY_PUMP);
    relays.add("light", PIN_RELAY_LIGHT);
    relays.add("fan",   PIN_RELAY_FAN);
}

void enterProvisioning() {
    appState = STATE_PROVISIONING;
    portal.begin();
}

void enterRunning() {
    appState = STATE_RUNNING;

    if (creds.isLocal()) {
        Serial.println("[Mode] Local hosting — MQTT disabled");
        if (MDNS.begin(creds.growId.c_str()))
            Serial.printf("[Local] http://%s.local/\n", creds.growId.c_str());
        localServer.begin(buildSnapshot,
            [](const char* id, bool state) { return relays.set(id, state); });
    } else {
        Serial.printf("[MQTT] Server: %s  Grow: %s\n", creds.mqttServer.c_str(), creds.growId.c_str());
        mqtt.begin(creds.mqttServer.c_str(), MQTT_PORT, creds.growId, onMqttMessage);
    }
}
