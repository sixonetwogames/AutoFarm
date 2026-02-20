#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct SensorReading {
    float value;
    bool valid;
    const char* name;
    const char* unit;
};

struct SensorInfo {
    const char* name;
    bool online;
    bool (*probeFn)();
};

class SensorRegistry {
public:
    void add(const char* name, bool (*probeFn)()) {
        if (_count >= 8) return;
        _sensors[_count++] = {name, false, probeFn};
    }

    void probeAll() {
        Serial.println("\n[Sensors] Probing...");
        for (size_t i = 0; i < _count; i++) {
            _sensors[i].online = _sensors[i].probeFn();
            Serial.printf("  %-12s: %s\n", _sensors[i].name, 
                _sensors[i].online ? "ONLINE" : "OFFLINE");
        }
        Serial.printf("[Sensors] %d/%d online\n\n", onlineCount(), _count);
    }

    bool isOnline(const char* name) const {
        for (size_t i = 0; i < _count; i++)
            if (strcmp(_sensors[i].name, name) == 0) return _sensors[i].online;
        return false;
    }

    size_t onlineCount() const {
        size_t c = 0;
        for (size_t i = 0; i < _count; i++) if (_sensors[i].online) c++;
        return c;
    }

    void getStatus(JsonDocument& doc) const {
        for (size_t i = 0; i < _count; i++)
            doc[_sensors[i].name] = _sensors[i].online ? "online" : "offline";
    }

private:
    SensorInfo _sensors[8];
    size_t _count = 0;
};