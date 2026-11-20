# FAQ

## General

### Can I use PulseHSM without the Arduino framework?

Not yet. `PulseHSM.h` includes `<Arduino.h>` for `millis()`, `noInterrupts()` and `interrupts()`, so a bare-metal port needs those three symbols stubbed. A framework-agnostic build is on the roadmap. Open an issue if you need it sooner.

### How many states can I have?

Up to 127. The state count is the number of elements in your `StaticState TABLE[N]` array, and there is no `PULSEHSM_MAX_STATES` macro to set. State indices are stored as `int8_t`, which is where the limit of 127 comes from.

```cpp
enum StateID : int8_t { ST_A, ST_B, ..., ST_COUNT };   // ST_COUNT is the limit
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = { ... };
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);
PulseHSM fsm(TABLE, ST_COUNT);
```

### Can I run multiple HSMs simultaneously?

Yes. Each `PulseHSM` instance is independent and shares no state with the others. Call `update()` on each one in `loop()`.

```cpp
PulseHSM motorFsm(MOTOR_TABLE, MOTOR_COUNT);
PulseHSM uiFsm(UI_TABLE, UI_COUNT);

void loop() {
  motorFsm.update();
  uiFsm.update();
}
```

### Does it support orthogonal (parallel) regions?

No. PulseHSM has one active leaf at a time. For parallel regions, run two separate `PulseHSM` instances and coordinate them with shared variables or events.

## Events

### What happens if the event queue fills up?

`sendEvent()` returns `false` and the event is silently dropped. The queue is never overwritten. `getDroppedEvents()` tells you how many events have been dropped since `begin()`. If you need a bigger buffer, raise `PULSEHSM_MAX_EVENTS`, which must stay a power of two:

```cpp
#define PULSEHSM_MAX_EVENTS 32
#include "PulseHSM.h"
```

### Can I send events from an ISR?

Yes. While `sendEvent()` enqueues, it disables interrupts with a save and restore, not a global clear, so it is safe to call from a hardware interrupt. [Events & Payloads](events.md) has the per-platform interrupt notes.

### Can I send events from `entry()`, `exit()` or `update()`?

Yes. An event sent during a callback is queued and dispatched on the next `update()` tick, not immediately.

### Can I send events from `onEvent()`?

Yes, and the same rule applies: the event queues and dispatches on the next tick.

## Transitions

### Can I call `transitionTo()` from inside a callback?

Yes, from any callback. Transitions are deferred to the end of `update()`. Calling `transitionTo()` from inside `entry()` or `exit()`, while a transition is already running, is safe. The new target is recorded and applied on the next `update()`.

### What is a self-transition?

It is `transitionTo(currentState)`, a transition to the state you are already in. By default it is lightweight: only `entryTime` resets, and `entry()` and `exit()` don't run. With full reinit, `exit()` then `entry()` run for that state only. To turn that on:

```cpp
#define PULSEHSM_SELF_TRANSITION_FULL_REINIT 1
```

### What fires during a transition?

PulseHSM walks up from the source leaf to the LCA (lowest common ancestor) and calls `exit()` on each state along the way. Then it walks down from the LCA to the target leaf, calling `entry()` on each. States above the LCA stay active, and their `exit()` and `entry()` don't fire.

## Memory and performance

### How much RAM does PulseHSM use?

The [landing page](/) has a footprint table. In short, about 160 B on AVR at the defaults. Each extra state slot costs about 16 B and each extra event slot about 5 B.

### Is PulseHSM safe to use in a tight `loop()` with no delay?

Yes. `update()` runs in O(depth) time with no heap access and no blocking calls. On a 16 MHz ATmega328P, a typical `update()` with no pending events finishes in under 10 µs.

### Does `millis()` rollover break timeouts?

No. Every time comparison uses unsigned subtraction (`millis() - entryTime`), which wraps correctly at the 49-day boundary.

## Platform-specific

### RP2040 dual-core: is cross-core `sendEvent()` safe?

Only if you add your own synchronisation. `sendEvent()` disables interrupts on the calling core, which does nothing against a concurrent call from the other core. On dual-core projects, put a `mutex_t` (Pico SDK) or a `portMUX_TYPE` (FreeRTOS) around `sendEvent()`.

### Does PulseHSM work on ESP32 with FreeRTOS tasks?

If `update()` and every `sendEvent()` caller run on the same FreeRTOS task or core, you need no extra locking. If several tasks send events, wrap `sendEvent()` in a mutex.

### Can I use it on non-Arduino ARM bare-metal (no `<Arduino.h>`)?

Not directly, but `PulseHSM.h` only calls `millis()`, `noInterrupts()` and `interrupts()`. If you provide those three as macros or inline functions, the rest compiles cleanly.
