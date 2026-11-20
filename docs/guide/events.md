# Events & payloads

## Sending an event

```cpp
bool sendEvent(uint8_t event, int32_t data = 0);
```

Puts an event in the ring buffer. `data` is an optional 32-bit payload. It returns `true` if the event was queued and `false` if the queue was full and the event was dropped. Existing callers that ignore the return value still compile unchanged.

```cpp
fsm.sendEvent(EVT_BUTTON_PRESS);         // no payload
fsm.sendEvent(EVT_TEMPERATURE, 2350);    // payload: 23.50 °C, scaled by 100
fsm.sendEvent(EVT_RX_BYTE, Serial.read());
```

## Receiving an event

Implement `EventCb`, a plain function with the signature `bool(uint8_t event)`:

```cpp
bool myHandler(uint8_t e) {
  switch (e) {
    case EVT_CONNECT:
      fsm.transitionTo(ST_CONNECTING);
      return true;      // consumed, stop bubbling
    case EVT_TIMEOUT:
      return false;     // not handled here, let the parent try
    default:
      return false;
  }
}
```

Assign it to the `onEvent` field in the state's `StaticState` row. A state can leave its handler as `nullptr`, and the event bubbles up automatically.

## Reading the payload

`getEventData()` returns the `int32_t` payload of the event being dispatched right now. Call it only inside an `onEvent` handler, or inside a callback invoked during event dispatch (`entry()`, `exit()` or `update()`) when you know an event triggered it. In general you can't assume that.

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

PulseHSM dispatches an event to the current leaf first, then to its parent, then to the grandparent, and so on up to the root. The first `onEvent` that returns `true` stops the walk. So a shared handler, such as an E-stop or a global "menu back" button, can live in a superstate and fire for all its descendants.

```
RUNNING        (onEvent handles EVT_ESTOP for the whole subgraph)
  STARTING
  OPERATING
  PAUSED
```

## ISR safety

`sendEvent()` is safe to call from an interrupt service routine. On AVR and ARM Cortex-M it uses a save/restore critical section that is safe to nest. On RP2040 it uses `save_and_disable_interrupts()` and `restore_interrupts()`.

```cpp
void IRAM_ATTR onButtonFall() {      // ESP32 ISR attribute
  fsm.sendEvent(EVT_BUTTON_PRESS);   // safe, the ring buffer is guarded
}
```

On dual-core chips (ESP32, RP2040), `sendEvent()` is safe from ISRs and tasks on the same core as `update()`. A producer on the other core needs its own synchronisation, such as a mutex or a core-safe queue wrapper.

## Queue depth and overflow

The queue depth is `PULSEHSM_MAX_EVENTS`. The default is 8, and it must be a power of two. When the queue is full, `sendEvent()` returns `false` and the event is silently dropped.

```cpp
if (!fsm.sendEvent(EVT_ALARM)) {
  // queue full: log it or take emergency action
}
```

To make the queue deeper:

```cpp
#define PULSEHSM_MAX_EVENTS 16   // before #include "PulseHSM.h"
```

## Event ordering

Events are dispatched first in, first out, in the order they were enqueued. Within a single `update()` call, PulseHSM drains the whole queue, dispatching once per event, before it checks timeouts and runs `update()` callbacks.

## Defining event constants

Use an `enum` or `#define`, with values from 1 to 255 (`uint8_t`). `0` is legal, but it is easy to mistake for a "no event" marker.

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
