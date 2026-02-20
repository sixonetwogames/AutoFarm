#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensors.h"
#include "sensor_impl.h"
#include "relay.h"
#include "mqtt_handler.h"

// ── State ────────────────────────────────────────────────────────────
enum AppState { STATE_PROVISIONING, STATE_RUNNING };
AppState appState = STATE_PROVISIONING;

// ── Captive Portal ───────────────────────────────────────────────────
WebServer webServer(WEB_PORT);
DNSServer dnsServer;
Preferences prefs;

// Runtime MQTT server (loaded from NVS)
String mqttServer;

const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
  *{box-sizing:border-box}
  body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 20px;background:#1a1a2e;color:#eee}
  h2{text-align:center}
  label{display:block;margin-top:10px;font-size:14px;color:#aaa}
  select,input,button{width:100%;padding:12px;margin:4px 0 6px;border-radius:6px;border:1px solid #444;font-size:16px;background:#16213e;color:#eee}
  button{background:#2ecc71;border:none;cursor:pointer;font-weight:bold;margin-top:14px}
  button:hover{background:#27ae60}
  .scan{font-size:13px;text-align:center;color:#888;margin:4px 0}
  #status{text-align:center;margin:12px 0;color:#0f0}
  hr{border:none;border-top:1px solid #333;margin:16px 0}
</style>
</head><body>
<h2>&#127793; AutoFarm Setup</h2>
<div id="status"></div>
<div class="scan">Scanning networks...</div>
<label>WiFi Network</label>
<select id="ssid"><option>Loading...</option></select>
<label>WiFi Password</label>
<input id="pass" type="password" placeholder="Leave empty if open">
<hr>
<label>MQTT Server IP</label>
<input id="mqtt" type="text" placeholder="e.g. 192.168.0.124" value=")rawliteral" DEFAULT_MQTT_SERVER R"rawliteral(">
<button onclick="save()">Connect</button>
<script>
fetch('/scan').then(r=>r.json()).then(nets=>{
  let s=document.getElementById('ssid');
  s.innerHTML=nets.map(n=>'<option value="'+n.ssid+'">'+n.ssid+' ('+n.rssi+'dBm)</option>').join('');
  document.querySelector('.scan').textContent=nets.length+' networks found';
});
function save(){
  let s=document.getElementById('ssid').value;
  let p=document.getElementById('pass').value;
  let m=document.getElementById('mqtt').value;
  document.getElementById('status').textContent='Saving & connecting...';
  fetch('/save?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p)+'&mqtt='+encodeURIComponent(m))
    .then(r=>r.text()).then(t=>document.getElementById('status').textContent=t);
}
</script>
</body></html>
)rawliteral";

void handleScan() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    webServer.send(200, "application/json", json);
}

void handleSave() {
    String ssid = webServer.arg("ssid");
    String pass = webServer.arg("pass");
    String mqtt = webServer.arg("mqtt");

    if (ssid.isEmpty()) {
        webServer.send(400, "text/plain", "SSID required");
        return;
    }

    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    prefs.begin("mqtt", false);
    prefs.putString("server", mqtt.isEmpty() ? DEFAULT_MQTT_SERVER : mqtt);
    prefs.end();

    webServer.send(200, "text/plain", "Saved! Rebooting...");
    delay(500);
    ESP.restart();
}

void startCaptivePortal() {
    WiFi.softAP(AP_SSID);
    dnsServer.start(53, "*", WiFi.softAPIP());

    webServer.onNotFound([]() { webServer.send(200, "text/html", PORTAL_HTML); });
    webServer.on("/generate_204", []() { webServer.send(200, "text/html", PORTAL_HTML); });
    webServer.on("/connecttest.txt", []() { webServer.send(200, "text/html", PORTAL_HTML); });
    webServer.on("/scan", handleScan);
    webServer.on("/save", handleSave);
    webServer.begin();

    Serial.printf("[WiFi] Captive portal on AP: %s\n", AP_SSID);
    Serial.printf("[WiFi] Portal IP: %s\n", WiFi.softAPIP().toString().c_str());
}

bool connectSavedWiFi() {
    prefs.begin("wifi", true);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    prefs.end();

    if (ssid.isEmpty()) return false;

    Serial.printf("[WiFi] Connecting to '%s'", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
        return true;
    }

    Serial.println("[WiFi] Connection failed.");
    return false;
}

String loadMqttServer() {
    prefs.begin("mqtt", true);
    String server = prefs.getString("server", DEFAULT_MQTT_SERVER);
    prefs.end();
    return server;
}

void clearAllCredentials() {
    prefs.begin("wifi", false);
    prefs.clear();
    prefs.end();
    prefs.begin("mqtt", false);
    prefs.clear();
    prefs.end();
}

// ── Sensors & Relays (unchanged) ─────────────────────────────────────
DhtSensor dhtSensor(PIN_DHT);
WaterLevelSensor waterLevelSensor(PIN_WATER_LEVEL);
PhSensor phSensor(PIN_PH);
SensorRegistry sensorRegistry;

RelayController relays;
MqttHandler mqtt;

unsigned long lastRead = 0;
unsigned long lastStatusPrint = 0;
constexpr unsigned long STATUS_PRINT_INTERVAL = 5000;

void publishSensorStatus() {
    JsonDocument doc;
    sensorRegistry.getStatus(doc);
    mqtt.publish("status/sensors", doc);
    Serial.println("[MQTT] Published sensor status");
}

void probeSensors() {
    sensorRegistry.probeAll();
    if (mqtt.connected()) publishSensorStatus();
}

void publishSensor(const SensorReading& r) {
    if (!r.valid) return;
    JsonDocument doc;
    doc["value"] = r.value;
    doc["unit"] = r.unit;
    doc["online"] = sensorRegistry.isOnline(r.name);

    char subtopic[48];
    snprintf(subtopic, sizeof(subtopic), "sensors/%s", r.name);
    mqtt.publish(subtopic, doc);
}

void publishRelayStates() {
    JsonDocument doc;
    relays.getStates(doc);
    mqtt.publish("status/relays", doc);
}

void printSensorReadings() {
    Serial.println("─────────────────────────────────");

    auto temp = dhtSensor.readTemp();
    auto hum = dhtSensor.readHumidity();
    auto level = waterLevelSensor.read();
    auto ph = phSensor.read();

    Serial.printf("  Temp:        %6.1f °F  %s\n", temp.value,
        temp.valid ? "" : "[INVALID]");
    Serial.printf("  Humidity:    %6.1f %%   %s\n", hum.value,
        hum.valid ? "" : "[INVALID]");
    Serial.printf("  Water Level: %6.1f %%   (raw: %d)\n", level.value,
        waterLevelSensor.readRaw());
    Serial.printf("  pH:          %6.2f     (%.2fV)\n", ph.value,
        phSensor.readVoltage());
    Serial.println("─────────────────────────────────");
}

void onMqttMessage(const char* topic, const char* payload) {
    Serial.printf("[MQTT] << %s: %s\n", topic, payload);

    const char* p = strrchr(topic, '/');
    if (!p) return;
    const char* device = p + 1;

    if (strcmp(device, "probe") == 0) {
        Serial.println("[CMD] Re-probing sensors...");
        probeSensors();
        return;
    }

    if (strcmp(device, "reset_wifi") == 0) {
        Serial.println("[CMD] WiFi/MQTT reset requested via MQTT");
        clearAllCredentials();
        delay(500);
        ESP.restart();
        return;
    }

    if (relays.handleCommand(device, payload)) {
        Serial.printf("[Relay] %s -> %s\n", device, relays.get(device) ? "ON" : "OFF");
        publishRelayStates();
    }
}

// ── Setup & Loop ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n════════════════════════════════════");
    Serial.printf("  HYDRO CONTROLLER - %s\n", GROW_ID);
    Serial.println("════════════════════════════════════\n");

    // Init sensors
    Serial.println("[Init] Sensors...");
    dhtSensor.begin();
    waterLevelSensor.begin();
    phSensor.begin();

    sensorRegistry.add("air_temp", []() { return dhtSensor.probe(); });
    sensorRegistry.add("humidity", []() { return dhtSensor.probe(); });
    sensorRegistry.add("water_level", []() { return waterLevelSensor.probe(); });
    sensorRegistry.add("ph", []() { return phSensor.probe(); });

    probeSensors();

    // Init relays
    Serial.println("[Init] Relays...");
    relays.add("pump", PIN_RELAY_PUMP);
    relays.add("light", PIN_RELAY_LIGHT);
    relays.add("fan", PIN_RELAY_FAN);
    Serial.println("  pump, light, fan registered");

    // Network — try saved creds, fallback to captive portal
    if (connectSavedWiFi()) {
        appState = STATE_RUNNING;
        mqttServer = loadMqttServer();
        Serial.printf("[MQTT] Server: %s\n", mqttServer.c_str());
        mqtt.begin(mqttServer.c_str(), MQTT_PORT, onMqttMessage);
        Serial.println("\n[Init] Complete!\n");
    } else {
        appState = STATE_PROVISIONING;
        WiFi.mode(WIFI_AP_STA);
        startCaptivePortal();
        Serial.println("\n[Init] Waiting for WiFi provisioning...\n");
    }
}

void loop() {
    if (appState == STATE_PROVISIONING) {
        dnsServer.processNextRequest();
        webServer.handleClient();
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (!connectSavedWiFi()) {
            Serial.println("[WiFi] Lost connection, entering setup mode");
            appState = STATE_PROVISIONING;
            WiFi.mode(WIFI_AP_STA);
            startCaptivePortal();
            return;
        }
    }

    bool wasConnected = mqtt.connected();
    mqtt.loop();

    if (!wasConnected && mqtt.connected()) {
        publishSensorStatus();
        publishRelayStates();
    }

    if (mqtt.connected() && millis() - lastRead > SENSOR_INTERVAL) {
        lastRead = millis();
        publishSensor(dhtSensor.readTemp());
        publishSensor(dhtSensor.readHumidity());
        publishSensor(waterLevelSensor.read());
        publishSensor(phSensor.read());
    }

    if (millis() - lastStatusPrint > STATUS_PRINT_INTERVAL) {
        lastStatusPrint = millis();
        printSensorReadings();
    }
}