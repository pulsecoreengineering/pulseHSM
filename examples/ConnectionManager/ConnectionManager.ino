/*
 * 06_ConnectionManager — platform-specific HSM with auto-retry
 *
 * On ESP32: manages a real WiFi connection — connects, monitors,
 * and reconnects automatically on drop.
 *
 * On AVR / other boards: runs a simulated version over Serial so you can
 * observe the same state machine behaviour without WiFi hardware.
 *
 * States:
 *   DISCONNECTED → CONNECTING → CONNECTED
 *                      ↑              |
 *                      └── RETRYING ←─┘  (on drop or 15-s connect timeout)
 *
 * Concepts shown:
 *   - #ifdef platform guards for hardware-specific code
 *   - Timed retry: RETRYING has a 10-second timeout back to CONNECTING
 *   - CONNECTING has a 15-second fallback timeout to RETRYING
 *   - getPreviousName() for diagnostics
 *   - sendEvent() from application logic (connection-status polling)
 *
 * Edit WIFI_SSID and WIFI_PASS before uploading to an ESP32.
 * Compatible: ESP32 (native WiFi), AVR + all others (simulated).
 */

#include "PulseHSM.h"

#define WIFI_SSID "YourSSID"
#define WIFI_PASS "YourPassword"

#if defined(ESP32)
  #include <WiFi.h>
#endif

enum StateID : int8_t { ST_DISCONNECTED = 0, ST_CONNECTING, ST_CONNECTED, ST_RETRYING, ST_COUNT };
enum Evt     : uint8_t { EVT_CONNECTED = 1, EVT_DROPPED, EVT_FAILED };

// Forward declarations
void onEntry_Disconnected();
bool onEvent_Disconnected(uint8_t evt);
void onEntry_Connecting();
void onUpdate_Connecting();
bool onEvent_Connecting(uint8_t evt);
void onEntry_Connected();
void onUpdate_Connected();
bool onEvent_Connected(uint8_t evt);
void onEntry_Retrying();

//                                name               update             entry                exit     ms     next          event                parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_DISCONNECTED] = { PULSEHSM_NAME("DISCONNECTED"), nullptr,           onEntry_Disconnected, nullptr, 0,     -1,           onEvent_Disconnected, -1, -1 },
    [ST_CONNECTING]   = { PULSEHSM_NAME("CONNECTING"),   onUpdate_Connecting, onEntry_Connecting, nullptr, 15000, ST_RETRYING,  onEvent_Connecting,   -1, -1 },
    [ST_CONNECTED]    = { PULSEHSM_NAME("CONNECTED"),    onUpdate_Connected,  onEntry_Connected,  nullptr, 0,     -1,           onEvent_Connected,    -1, -1 },
    [ST_RETRYING]     = { PULSEHSM_NAME("RETRYING"),     nullptr,           onEntry_Retrying,   nullptr, 10000, ST_CONNECTING, nullptr,              -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

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
    } else if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
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
    if (WiFi.status() != WL_CONNECTED) fsm.sendEvent(EVT_DROPPED);
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

// ── RETRYING ──────────────────────────────────────────────

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
    fsm.begin(ST_DISCONNECTED);
}

void loop() {
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch == 'c' && fsm.getCurrentState() == ST_DISCONNECTED)
            fsm.transitionTo(ST_CONNECTING);
        if (ch == 'd') fsm.sendEvent(EVT_DROPPED);
        if (ch == 'f') fsm.sendEvent(EVT_FAILED);
        if (ch == 'k') fsm.sendEvent(EVT_CONNECTED);
    }
    fsm.update();
}
