#pragma once

// ===== GROW IDENTITY =====
#define GROW_ID "grow1"

// ===== NETWORK =====
#define AP_SSID "AUTOFARM"
#define DEFAULT_MQTT_SERVER "192.168.0.124"
#define MQTT_PORT 1883
#define WIFI_CONNECT_TIMEOUT 15000
#define WEB_PORT 80

// ===== TIMING (ms) =====
#define SENSOR_INTERVAL 1000
#define MQTT_RECONNECT_INTERVAL 5000

// ===== PINS =====
#define PIN_DHT 32           // DHT11 DATA pin
#define PIN_WATER_LEVEL 34   // Capacitive sensor AOUT (ADC1 only: 32-39)
#define PIN_PH 35            // pH module Po pin (ADC1 only)

// Relays
#define PIN_RELAY_PUMP 16
#define PIN_RELAY_LIGHT 17
#define PIN_RELAY_FAN 18

// ===== CALIBRATION =====
// DHT calibration
#define TEMP_OFFSET_F -6.0f  // Adjustment in Fahrenheit
#define HUMID_OFFSET 7.0

// Water Level: measure raw ADC in air (empty) and water (full)
#define WATER_LEVEL_EMPTY 2500
#define WATER_LEVEL_FULL 975

// pH: use buffer solutions, measure voltage at pH 7.0
#define PH_V_NEUTRAL 3.46f
#define PH_SLOPE 0.175f
#define PH_VOLTAGE_DIVIDER_RATIO 5.0f  // HiLetgo module divides by 5 (25V→5V range)