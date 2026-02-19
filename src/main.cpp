#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "config.h"

// ─── Server ──────────────────────────────────────────────────────────────────

AsyncWebServer server(80);

// ─── State ───────────────────────────────────────────────────────────────────

enum class MachineState { IDLE, POURING };

static MachineState  machineState = MachineState::IDLE;
static int           lastPourMl   = 0;
static unsigned long pourEndMs    = 0;  // millis() when current pour should stop

// ─── HC-SR04 ultrasonic distance sensor ──────────────────────────────────────
// Sends a 10µs trigger pulse and measures the echo return time.

int readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000);  // 30ms timeout (~5m max)
  if (duration == 0) return 999;                    // no echo = nothing in range
  return (int)(duration * 0.034f / 2.0f);
}

bool readGlassPresent() {
  int distance = readDistanceCm();
  return distance < GLASS_PRESENT_CM;
}

// ─── Pump control via relay ──────────────────────────────────────────────────

void pumpOn() {
  digitalWrite(PIN_PUMP, HIGH);
  Serial.println("[pump] ON");
}

void pumpOff() {
  digitalWrite(PIN_PUMP, LOW);
  Serial.println("[pump] OFF");
}

// ─── Pour logic ──────────────────────────────────────────────────────────────

void startPour(int ml) {
  unsigned long durationMs = (unsigned long)((ml / FLOW_RATE_ML_PER_S) * 1000.0f);
  pourEndMs    = millis() + durationMs;
  lastPourMl   = ml;
  machineState = MachineState::POURING;
  pumpOn();
  Serial.printf("[dispense] Starting %dml pour, duration %lums\n", ml, durationMs);
}

void stopPour() {
  pumpOff();
  machineState = MachineState::IDLE;
  Serial.println("[dispense] Pour complete");
}

// ─── Route handlers ──────────────────────────────────────────────────────────

// GET /status
void handleStatus(AsyncWebServerRequest *request) {
  JsonDocument doc;

  int distanceCm = readDistanceCm();

  doc["state"]         = (machineState == MachineState::POURING) ? "pouring" : "idle";
  doc["glass_present"] = (distanceCm < GLASS_PRESENT_CM);
  doc["uptime"]        = millis() / 1000;
  doc["last_pour_ml"]  = lastPourMl;
  doc["distance_cm"]   = distanceCm;  // extra field for debugging

  String body;
  serializeJson(doc, body);
  request->send(200, "application/json", body);
}

// POST /dispense
void handleDispense(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                    size_t index, size_t total) {
  // Reject if already pouring
  if (machineState == MachineState::POURING) {
    request->send(409, "application/json", "{\"error\":\"Already pouring\"}");
    return;
  }

  // Parse JSON body
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  int amountMl = doc["amount_ml"] | 0;
  if (amountMl <= 0 || amountMl > MAX_DISPENSE_ML) {
    request->send(400, "application/json", "{\"error\":\"Invalid amount\"}");
    return;
  }

  startPour(amountMl);
  request->send(200, "application/json", "{\"ok\":true}");
}

// ─── WiFi ────────────────────────────────────────────────────────────────────

void connectWifi() {
  Serial.printf("[wifi] Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[wifi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
}

// ─── Setup / loop ────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Pin modes — HC-SR04 + relay
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_PUMP, OUTPUT);
  digitalWrite(PIN_PUMP, LOW);  // ensure pump is off on boot

  connectWifi();

  // mDNS: device becomes reachable at http://esp32.local
  if (MDNS.begin(MDNS_HOSTNAME)) {
    Serial.printf("[mdns] Hostname: http://%s.local\n", MDNS_HOSTNAME);
  } else {
    Serial.println("[mdns] Failed to start mDNS");
  }

  // Routes
  server.on("/status", HTTP_GET, handleStatus);
  server.on(
      "/dispense", HTTP_POST,
      [](AsyncWebServerRequest *request) {},  // headers-only handler (required by AsyncWebServer)
      nullptr,
      handleDispense
  );

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "application/json", "{\"error\":\"Not found\"}");
  });

  server.begin();
  Serial.println("[server] HTTP server started");
}

void loop() {
  // Non-blocking pour timer: stop pump when duration has elapsed
  if (machineState == MachineState::POURING && millis() >= pourEndMs) {
    stopPour();
  }
}
