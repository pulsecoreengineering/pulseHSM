# Device Connection Manager

**Demonstrates:** multi-level hierarchy, `initialChild`, timed retry with
exponential backoff, event payloads (error code), `getPreviousState()` for
logging, `isInHierarchy()` for status display.

## State diagram

```
DISCONNECTED  (superstate — manages retry logic)
├── CONNECTING     (initial; kicks off connection attempt on entry)
└── BACKING_OFF    (waiting before retry; timeout → CONNECTING)
CONNECTED     (superstate — manages authenticated session)
├── AUTHENTICATING (initial; sends credentials on entry)
└── ONLINE         (normal operation)
FAULT          (unrecoverable error; requires user intervention)
```

**Transitions:**
- `CONNECTING` → `CONNECTED` on `EVT_LINK_UP`
- `CONNECTING` → `BACKING_OFF` on `EVT_LINK_FAIL` (payload = retry count)
- `BACKING_OFF` timeout → `CONNECTING` (timeout grows with retry count)
- `AUTHENTICATING` → `ONLINE` on `EVT_AUTH_OK`
- `AUTHENTICATING` → `DISCONNECTED` on `EVT_AUTH_FAIL` (retry from scratch)
- `ONLINE` → `DISCONNECTED` on `EVT_DISCONNECTED`
- Any state → `FAULT` on `EVT_FATAL`

## Full example

```cpp
#define PULSEHSM_MAX_EVENTS 16
#include "PulseHSM.h"

// ---- State indices -------------------------------------------------------
enum StateID : int8_t {
    ST_DISCONNECTED = 0, ST_CONNECTED, ST_FAULT,
    ST_CONNECTING, ST_BACKING_OFF,
    ST_AUTHENTICATING, ST_ONLINE,
    ST_COUNT
};

// ---- Events -------------------------------------------------------------
enum Events : uint8_t {
  EVT_LINK_UP       = 1,
  EVT_LINK_FAIL,       // payload: failure reason code
  EVT_AUTH_OK,
  EVT_AUTH_FAIL,       // payload: HTTP status or 0
  EVT_DISCONNECTED,
  EVT_FATAL,           // payload: error code
};

// ---- Retry state --------------------------------------------------------
static int retryCount  = 0;
static const unsigned long BACKOFF_BASE_MS = 2000;
static const int           MAX_RETRIES     = 5;

// ---- Hardware stubs (replace with your WiFi/BLE/Ethernet calls) ---------
void hw_connect()       { /* WiFi.begin(SSID, PASS); */ Serial.println("Connecting…"); }
void hw_authenticate()  { /* send credentials */ Serial.println("Authenticating…"); }
void hw_disconnect()    { /* WiFi.disconnect(); */ }

// ---- DISCONNECTED superstate -------------------------------------------
void disconnected_entry() {
  // Nothing to do — child states handle the work
}
void disconnected_exit() {
  // Clean up any pending connection attempt
  hw_disconnect();
}

// ---- CONNECTING ---------------------------------------------------------
void connecting_entry() {
  Serial.print("[Attempt ");
  Serial.print(retryCount + 1);
  Serial.println("] Initiating connection…");
  hw_connect();
}

bool connecting_event(uint8_t e) {
  if (e == EVT_LINK_UP) {
    retryCount = 0;
    fsm.transitionTo(ST_CONNECTED);   // → AUTHENTICATING (initial)
    return true;
  }
  if (e == EVT_LINK_FAIL) {
    retryCount++;
    if (retryCount >= MAX_RETRIES) {
      fsm.transitionTo(ST_FAULT);
    } else {
      fsm.transitionTo(ST_BACKING_OFF);
    }
    return true;
  }
  return false;
}

// ---- BACKING_OFF --------------------------------------------------------
// Timeout is set dynamically in entry() by calling transitionTo() is not right
// here — instead we use the update() to watch elapsed time, or we pre-size
// several backing-off states. The cleanest approach on PulseHSM:
// a single BACKING_OFF state with update() checking getStateElapsed().

void backingOff_update() {
  unsigned long backoff = BACKOFF_BASE_MS * (1UL << (retryCount - 1));  // 2s, 4s, 8s…
  backoff = min(backoff, 30000UL);   // cap at 30 s
  if (fsm.getStateElapsed() >= backoff) {
    fsm.transitionTo(ST_CONNECTING);
  }
}

void backingOff_entry() {
  unsigned long backoff = BACKOFF_BASE_MS * (1UL << (retryCount - 1));
  backoff = min(backoff, 30000UL);
  Serial.print("Backing off for ");
  Serial.print(backoff);
  Serial.println(" ms…");
}

// ---- CONNECTED superstate ----------------------------------------------
void connected_entry() {
  Serial.println("Link up — authenticating…");
}
void connected_exit() {
  Serial.println("Connection lost.");
}

bool connected_event(uint8_t e) {
  if (e == EVT_DISCONNECTED) {
    fsm.transitionTo(ST_DISCONNECTED);   // → CONNECTING (initial)
    return true;
  }
  return false;
}

// ---- AUTHENTICATING -----------------------------------------------------
void authenticating_entry() { hw_authenticate(); }

bool authenticating_event(uint8_t e) {
  if (e == EVT_AUTH_OK) {
    Serial.println("Authenticated — online.");
    fsm.transitionTo(ST_ONLINE);
    return true;
  }
  if (e == EVT_AUTH_FAIL) {
    int32_t code = fsm.getEventData();
    Serial.print("Auth failed (code ");
    Serial.print(code);
    Serial.println(") — retrying from scratch.");
    retryCount++;
    fsm.transitionTo(ST_DISCONNECTED);   // → CONNECTING (initial)
    return true;
  }
  return false;
}

// ---- ONLINE -------------------------------------------------------------
bool online_event(uint8_t e) {
  // Individual application events handled here
  (void)e;
  return false;
}

// ---- FAULT --------------------------------------------------------------
void fault_entry() {
  Serial.print("FATAL error — previous state: ");
  Serial.println(fsm.getPreviousName());
  Serial.println("Manual intervention required.");
  hw_disconnect();
}

// ---- Global fault handler (bubbles from any state) ----------------------
// We attach this to both superstates by using a shared function pointer:
bool fault_event(uint8_t e) {
  if (e == EVT_FATAL) {
    fsm.transitionTo(ST_FAULT);
    return true;
  }
  return false;
}

// ---- Forward declarations (callbacks reference fsm, defined after the table)
void disconnected_entry(); void disconnected_exit();
void connecting_entry();   bool connecting_event(uint8_t e);
void backingOff_update();  void backingOff_entry();
void connected_entry();    void connected_exit();    bool connected_event(uint8_t e);
void authenticating_entry(); bool authenticating_event(uint8_t e);
bool online_event(uint8_t e);
void fault_entry();        bool fault_event(uint8_t e);

// ---- State table ---------------------------------------------------------
//                                         name           upd              entry               exit                ms  next  event              parent           initChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_DISCONNECTED]   = { PULSEHSM_NAME("DISCONNECTED"),   nullptr,         disconnected_entry,  disconnected_exit,  0,  -1, fault_event,         -1,              ST_CONNECTING    },
    [ST_CONNECTED]      = { PULSEHSM_NAME("CONNECTED"),      nullptr,         connected_entry,     connected_exit,     0,  -1, connected_event,     -1,              ST_AUTHENTICATING},
    [ST_FAULT]          = { PULSEHSM_NAME("FAULT"),          nullptr,         fault_entry,         nullptr,            0,  -1, nullptr,             -1,              -1               },
    [ST_CONNECTING]     = { PULSEHSM_NAME("CONNECTING"),     nullptr,         connecting_entry,    nullptr,            0,  -1, connecting_event,    ST_DISCONNECTED, -1               },
    [ST_BACKING_OFF]    = { PULSEHSM_NAME("BACKING_OFF"),    backingOff_update,backingOff_entry,   nullptr,            0,  -1, nullptr,             ST_DISCONNECTED, -1               },
    [ST_AUTHENTICATING] = { PULSEHSM_NAME("AUTHENTICATING"), nullptr,         authenticating_entry,nullptr,            0,  -1, authenticating_event,ST_CONNECTED,    -1               },
    [ST_ONLINE]         = { PULSEHSM_NAME("ONLINE"),         nullptr,         nullptr,             nullptr,            0,  -1, online_event,        ST_CONNECTED,    -1               },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

// ---- setup / loop -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  fsm.begin(ST_DISCONNECTED);   // → CONNECTING (initialChild)
}

void loop() {
  // Simulate external events from Serial
  if (Serial.available()) {
    switch (Serial.read()) {
      case 'u': fsm.sendEvent(EVT_LINK_UP);       break;
      case 'f': fsm.sendEvent(EVT_LINK_FAIL, 1);  break;
      case 'a': fsm.sendEvent(EVT_AUTH_OK);        break;
      case 'x': fsm.sendEvent(EVT_AUTH_FAIL, 403);break;
      case 'd': fsm.sendEvent(EVT_DISCONNECTED);   break;
      case '!': fsm.sendEvent(EVT_FATAL, -1);      break;
    }
  }

  // Dashboard line every 3 s
  static unsigned long last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.print("State: ");
    Serial.print(fsm.getCurrentName());
    Serial.print("  Online: ");
    Serial.print(fsm.isInHierarchy(ST_CONNECTED) ? "YES" : "NO");
    Serial.print("  Retries: ");
    Serial.println(retryCount);
  }

  fsm.update();
}
```

## Key design decisions

**Why a superstate for `DISCONNECTED`?**  
`CONNECTING` and `BACKING_OFF` share the connection-teardown logic in
`disconnected_exit()`. Without the superstate, both states would need their own
`exit()` callbacks calling `hw_disconnect()`.

**Exponential backoff without multiple states**  
`backingOff_update()` reads `getStateElapsed()` and computes the backoff
dynamically. This avoids pre-defining five `BACKING_OFF_1`, `BACKING_OFF_2`, …
states with different `timeoutMs` values.

**`getPreviousName()` in the fault entry**  
Knowing which state faulted from is invaluable during debugging on a device with
only a serial log.

**`fault_event` on both superstates**  
Because `ST_FAULT` is a sibling of both `ST_DISCONNECTED` and `ST_CONNECTED` (not
a child of either), there is no single root superstate to put the fault handler on.
Using the same function pointer for both superstates' `onEvent` is the clean
solution — one function, two registrations.
