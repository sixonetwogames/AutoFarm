#pragma once
#include "sensors.h"
#include "config.h"
#include <DHT.h>

class DhtSensor {
public:
    DhtSensor(uint8_t pin) : _dht(pin, DHT11) {}
    void begin() { _dht.begin(); delay(1000); }
    
    bool probe() {
        float t = _dht.readTemperature();
        float h = _dht.readHumidity();
        return !isnan(t) && !isnan(h);
    }
    
    SensorReading readTemp() {
        float c = _dht.readTemperature();
        if (isnan(c)) return {0, false, "air_temp", "F"};
        float f = c * 9.0f / 5.0f + 32.0f + TEMP_OFFSET_F;
        return {f, true, "air_temp", "F"};
    }
    
    SensorReading readHumidity() {
        float h = _dht.readHumidity() + HUMID_OFFSET;
        return {h, !isnan(h), "humidity", "%"};
    }
private:
    DHT _dht;
};

class WaterLevelSensor {
public:
    WaterLevelSensor(uint8_t pin) : _pin(pin) {}
    void begin() { analogReadResolution(12); }
    
    bool probe() {
        uint32_t sum = 0;
        for (int i = 0; i < 5; i++) { sum += analogRead(_pin); delay(2); }
        uint16_t raw = sum / 5;
        return (raw > 200 && raw < 3800);
    }
    
    SensorReading read() {
        uint32_t sum = 0;
        for (int i = 0; i < 10; i++) { sum += analogRead(_pin); delay(5); }
        float raw = sum / 10.0f;
        float pct = map(raw, WATER_LEVEL_EMPTY, WATER_LEVEL_FULL, 0, 100);
        pct = constrain(pct, 0, 100);
        return {pct, true, "water_level", "%"};
    }
    
    uint16_t readRaw() { return analogRead(_pin); }
private:
    uint8_t _pin;
};

class PhSensor {
public:
    PhSensor(uint8_t pin) : _pin(pin) {}
    void begin() { analogReadResolution(12); }
    
    bool probe() {
        uint32_t sum = 0;
        for (int i = 0; i < 5; i++) { sum += analogRead(_pin); delay(2); }
        uint16_t raw = sum / 5;
        // Voltage divider output range: 0-1V → ADC 0-1241, valid mid-range
        return (raw > 80 && raw < 1200);
    }
    
    SensorReading read() {
        uint32_t sum = 0;
        for (int i = 0; i < 10; i++) { sum += analogRead(_pin); delay(10); }
        
        float measuredV = (sum / 10.0f) * 3.3f / 4095.0f;  // Voltage at ESP32 (after divider)
        float actualV = measuredV * PH_VOLTAGE_DIVIDER_RATIO;  // Scale back to actual pH sensor voltage
        float ph = 7.0f + (PH_V_NEUTRAL - actualV) / PH_SLOPE;
        
        return {ph, (ph >= 0 && ph <= 14), "ph", "pH"};
    }
    
    float readVoltage() { 
        float measured = analogRead(_pin) * 3.3f / 4095.0f;
        return measured * PH_VOLTAGE_DIVIDER_RATIO;  // Return actual pH sensor voltage
    }
    
private:
    uint8_t _pin;
};