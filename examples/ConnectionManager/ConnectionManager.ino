/*
 * 06_ConnectionManager — platform-specific HSM with auto-retry
 *
 * On ESP32: manages a real WiFi connection — scans, connects, monitors,
 * and reconnects automatically on drop.
 *
 * On AVR / other boards: runs a simulated version over Serial so you can
 * observe the same state machine behaviour without WiFi hardware.
 *
 * States:
 *   DISCONNECTED → CONNECTING → CONNECTED
 *                      ↑              |
 *                      └── RETRYING ←─┘  (on drop or timeout)
 *
 * Concepts shown:
 *   - #ifdef platform guards for hardware-specific code
 *   - Timed retry: RETRYING has a 10-second timeout back to CONNECTING
 *   - getPreviousName() for diagnostics
 *   - sendEvent() from application logic (connection-status polling)
 *
 * ── ESP32 setup ──────────────────────────────────────────
 * Edit WIFI_SSID and WIFI_PASS below before uploading.
 *
 * Compatible: ESP32 (native WiFi), AVR + all others (simulated).
 */

#include "PulseHSM.h"

// ── WiFi credentials (ESP32 only) ────────────────────────
#define WIFI_SSID "YourSSID"
#define WIFI_PASS "YourPassword"

#if defined(ESP32)
  #include <WiFi.h>
#endif

PulseHSM fsm;

enum Evt : uint8_t { EVT_CONNECTED = 1, EVT_DROPPED, EVT_FAILED };

int ST_DISCONNECTED, ST_CONNECTING, ST_CONNECTED, ST_RETRYING;

// ── DISCONNECTED ──────────────────────────────────────────

void onEntry_Disconnected() {
  Serial.println("[DISCONNECTED] Idle. Send 'c' to connect.");
}

bool onEvent_Disconnected(uint8_t evt) {
  if (evt == EVT_CONNECTED) { fsm.transitionTo(ST_CONNECTED); return true; }
  return false;
}

// ── CONNECTING ────────────────────────────────────────────

void onEntry_Connecting() {
  Serial.print("[CONNECTING] → WiFi ");
#if defined(ESP32)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println(WIFI_SSID);
#else
  Serial.println("(simulated)");
#endif
}

void onUpdate_Connecting() {
#if defined(ESP32)
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("  IP: "); Serial.println(WiFi.localIP());
    fsm.sendEvent(EVT_CONNECTED);
  } else if (WiFi.status() == WL_CONNECT_FAILED ||
             WiFi.status() == WL_NO_SSID_AVAIL) {
    fsm.sendEvent(EVT_FAILED);
  }
#endif
}

bool onEvent_Connecting(uint8_t evt) {
  if (evt == EVT_CONNECTED) { fsm.transitionTo(ST_CONNECTED); return true; }
  if (evt == EVT_FAILED)    { fsm.transitionTo(ST_RETRYING);  return true; }
  return false;
}

// ── CONNECTED ─────────────────────────────────────────────

void onEntry_Connected() {
  Serial.println("[CONNECTED] Link up. Send 'd' to simulate a drop.");
}

void onUpdate_Connected() {
#if defined(ESP32)
  if (WiFi.status() != WL_CONNECTED) {
    fsm.sendEvent(EVT_DROPPED);
  }
#endif
}

bool onEvent_Connected(uint8_t evt) {
  if (evt == EVT_DROPPED) {
    Serial.println("  Connection lost!");
    fsm.transitionTo(ST_RETRYING);
    return true;
  }
  return false;
}

// ── RETRYING ─────────────────────────────────────────────

void onEntry_Retrying() {
  Serial.print("[RETRYING] Was: "); Serial.print(fsm.getPreviousName());
  Serial.println(" — waiting 10 s before reconnect.");
#if defined(ESP32)
  WiFi.disconnect();
#endif
}

// ── Setup & loop ──────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // RETRYING times out to CONNECTING (index known after addState below)
  ST_DISCONNECTED = fsm.addState("DISCONNECTED", nullptr,           onEntry_Disconnected, nullptr, 0,     -1, onEvent_Disconnected);
  ST_CONNECTING   = fsm.addState("CONNECTING",   onUpdate_Connecting, onEntry_Connecting, nullptr, 15000, -1, onEvent_Connecting);
  ST_CONNECTED    = fsm.addState("CONNECTED",    onUpdate_Connected,  onEntry_Connected,  nullptr, 0,     -1, onEvent_Connected);
  ST_RETRYING     = fsm.addState("RETRYING",     nullptr,           onEntry_Retrying,   nullptr, 10000, ST_CONNECTING, nullptr);

  // Wire CONNECTING timeout → RETRYING now that ST_RETRYING is known
  // (done inline: 15000 ms timeout fires EVT_FAILED via update, which transitions to RETRYING)

  fsm.begin(ST_DISCONNECTED);
}

void loop() {
  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'c') {                       // manual connect trigger
      if (fsm.getCurrentState() == ST_DISCONNECTED)
        fsm.transitionTo(ST_CONNECTING);
    }
    if (ch == 'd') fsm.sendEvent(EVT_DROPPED);    // simulate a drop
    if (ch == 'f') fsm.sendEvent(EVT_FAILED);     // simulate connect failure
    if (ch == 'k') fsm.sendEvent(EVT_CONNECTED);  // simulate success (non-ESP32)
  }

  fsm.update();
}
