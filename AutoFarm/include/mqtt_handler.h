#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

using MqttCallback = std::function<void(const char* topic, const char* payload)>;

class MqttHandler {
public:
    MqttHandler() : _client(_wifi) {}
    
    void begin(const char* server, uint16_t port, MqttCallback cb) {
        _callback = cb;
        _client.setServer(server, port);
        _client.setBufferSize(512);
        _client.setCallback([this](char* topic, byte* payload, unsigned int len) {
            char buf[256];
            memcpy(buf, payload, min(len, sizeof(buf)-1));
            buf[min(len, sizeof(buf)-1)] = '\0';
            if (_callback) _callback(topic, buf);
        });
    }
    
    bool loop() {
        if (!_client.connected()) {
            if (millis() - _lastReconnect > MQTT_RECONNECT_INTERVAL) {
                _lastReconnect = millis();
                reconnect();
            }
            return false;
        }
        _client.loop();
        return true;
    }
    
    template<typename T>
    void publish(const char* subtopic, T& doc) {
        char topic[64], payload[256];
        snprintf(topic, sizeof(topic), "hydro/%s/%s", GROW_ID, subtopic);
        serializeJson(doc, payload);
        _client.publish(topic, payload);
    }
    
    bool connected() { return _client.connected(); }

private:
    WiFiClient _wifi;
    PubSubClient _client;
    MqttCallback _callback;
    unsigned long _lastReconnect = 0;
    
    void reconnect() {
        char clientId[32];
        snprintf(clientId, sizeof(clientId), "hydro-%s-%04x", GROW_ID, (uint16_t)random(0xFFFF));
        
        if (_client.connect(clientId)) {
            char topic[64];
            snprintf(topic, sizeof(topic), "hydro/%s/control/#", GROW_ID);
            _client.subscribe(topic);
            Serial.println("[MQTT] Connected");
        }
    }
};