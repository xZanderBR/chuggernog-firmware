#pragma once

// ─── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID "your_ssid"
#define WIFI_PASSWORD "your_password"

// mDNS hostname — device will be reachable at http://esp32.local
// Must match ESP32_URL in backend/config.py (http://esp32.local)
#define MDNS_HOSTNAME "esp32"

// ─── Hardware pins ───────────────────────────────────────────────────────────
// HC-SR04 ultrasonic sensor (glass detection)
#define PIN_TRIG 26          // Trigger pin (output)
#define PIN_ECHO 27          // Echo pin (input) — needs 5V→3.3V level shifter!

// Pump relay
#define PIN_PUMP 25          // Relay control pin (output, active HIGH)

// ─── Glass detection thresholds ──────────────────────────────────────────────
// HC-SR04 distance thresholds (in cm) — calibrate to your enclosure
#define GLASS_PRESENT_CM 4   // distance < this = glass detected  (from old code)
#define GLASS_REMOVED_CM 8   // distance > this = glass removed   (from old code)

// ─── Behaviour ───────────────────────────────────────────────────────────────
// Maximum dispense volume accepted (should match backend settings)
#define MAX_DISPENSE_ML 60

// Flow rate used to convert ml → pump-on duration (calibrate after assembly)
// Old code used 2000ms for a full shot — adjust once you measure actual flow.
// e.g. 10 ml/s means 30ml takes 3000ms
#define FLOW_RATE_ML_PER_S 10.0f
