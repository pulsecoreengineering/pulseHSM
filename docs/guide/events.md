# Events & Payloads

## Sending an event

```cpp
bool sendEvent(uint8_t event, int32_t data = 0);
```

Enqueues an event into the ring buffer. `data` is an optional 32-bit payload.
Returns `true` if the event was queued, `false` if the queue was full (event
dropped). Existing callers that ignore the return value are source-compatible.

```cpp
fsm.sendEvent(EVT_BUTTON_PRESS);         // no payload
fsm.sendEvent(EVT_TEMPERATURE, 2350);    // payload: 23.50 °C × 100
fsm.sendEvent(EVT_RX_BYTE, Serial.read());
```

## Receiving an event

Implement `EventCb` — a plain function with the signature `bool(uint8_t event)`:

```cpp
bool myHandler(uint8_t e) {
  switch (e) {
    case EVT_CONNECT:
      fsm.transitionTo(ST_CONNECTING);
      return true;      // consumed — stop bubbling
    case EVT_TIMEOUT:
      return false;     // not handled here — let the parent try
    default:
      return false;
  }
}
```

Pass it to `addState()` as the `onEvent` parameter. A state may have `nullptr`
for its handler — the event bubbles up automatically.

## Reading the payload

`getEventData()` returns the `int32_t` payload of the **currently dispatching**
event. Call it only inside an `onEvent` handler (or any callback invoked during
event dispatch — `entry()`, `exit()`, `update()` — as long as you know an event
triggered them, which is not guaranteed in the general case).

```cpp
bool tempHandler(uint8_t e) {
  if (e == EVT_TEMPERATURE) {
    int32_t hundredths = fsm.getEventData();   // e.g. 2350 = 23.50 °C
    if (hundredths > 8000) fsm.transitionTo(ST_OVERHEAT);
    return true;
  }
  return false;
}
```

## Event bubbling

PulseHSM dispatches an event starting at the current leaf and walking up:

```
current leaf → parent → grandparent → … → root
```

The first `onEvent` that returns `true` stops the walk. This lets you put
shared handlers (e.g., an E-stop or a global "menu back" button) in a
superstate and have them fire for all descendants.

```
RUNNING  ← onEvent handles EVT_ESTOP for the whole subgraph
├── STARTING
├── OPERATING
└── PAUSED
```

## ISR safety

`sendEvent()` is safe to call from an interrupt service routine. On AVR and ARM
Cortex-M it uses a save/restore critical section (nesting-safe). On RP2040 it
uses `save_and_disable_interrupts()` / `restore_interrupts()`.

```cpp
void IRAM_ATTR onButtonFall() {      // ESP32 ISR attribute
  fsm.sendEvent(EVT_BUTTON_PRESS);   // safe — ring buffer is guarded
}
```

> **Dual-core note (ESP32, RP2040):** `sendEvent()` is safe from ISRs and tasks
> on the **same core** as `update()`. A producer on the **other core** needs its
> own synchronisation (a mutex or a core-safe queue wrapper).

## Queue depth and overflow

The queue depth is `PULSEHSM_MAX_EVENTS` (default 8, must be a power of two).
When it's full, `sendEvent()` returns `false` and the event is silently dropped.

```cpp
if (!fsm.sendEvent(EVT_ALARM)) {
  // queue full — log or take emergency action
}
```

To deepen the queue:

```cpp
#define PULSEHSM_MAX_EVENTS 16   // before #include "PulseHSM.h"
```

## Event ordering

Events are dispatched **FIFO** — in the order they were enqueued. Within a single
`update()` call, PulseHSM drains the entire queue (one dispatch per event) before
checking timeouts and running `update()` callbacks.

## Defining event constants

Use an `enum` or `#define`. Keep values in the range 1–255 (`uint8_t`). Value `0`
is legal but can be confusing as a "no event" sentinel.

```cpp
enum Events : uint8_t {
  EVT_CONNECT    = 1,
  EVT_DISCONNECT,
  EVT_AUTH_OK,
  EVT_AUTH_FAIL,
  EVT_TIMEOUT,
  EVT_ERROR,
};
```
