#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct Relay {
    const char* id;
    uint8_t pin;
    bool state;
};

class RelayController {
public:
    void add(const char* id, uint8_t pin) {
        if (_count >= 8) return;
        _relays[_count++] = {id, pin, false};
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);  // Active LOW - start OFF
    }
    
    bool set(const char* id, bool state) {
        for (size_t i = 0; i < _count; i++) {
            if (strcmp(_relays[i].id, id) == 0) {
                _relays[i].state = state;
                digitalWrite(_relays[i].pin, state ? LOW : HIGH);
                return true;
            }
        }
        return false;
    }
    
    bool get(const char* id) const {
        for (size_t i = 0; i < _count; i++)
            if (strcmp(_relays[i].id, id) == 0) return _relays[i].state;
        return false;
    }
    
    void getStates(JsonDocument& doc) const {
        for (size_t i = 0; i < _count; i++)
            doc[_relays[i].id] = _relays[i].state ? "on" : "off";
    }
    
    bool handleCommand(const char* id, const char* payload) {
        JsonDocument doc;
        if (deserializeJson(doc, payload)) return false;
        
        bool state;
        if (doc["state"].is<bool>()) state = doc["state"].as<bool>();
        else if (doc["state"].is<const char*>()) {
            const char* s = doc["state"];
            state = (strcmp(s, "on") == 0 || strcmp(s, "1") == 0);
        } else return false;
        
        return set(id, state);
    }

private:
    Relay _relays[8];
    size_t _count = 0;
};