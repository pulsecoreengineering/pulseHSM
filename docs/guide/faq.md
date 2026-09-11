# FAQ

## General

### Can I use PulseHSM without the Arduino framework?

Not yet out of the box — `PulseHSM.h` includes `<Arduino.h>` for `millis()`, `noInterrupts()`, and `interrupts()`. A bare-metal port would need those three symbols stubbed. A framework-agnostic build is on the roadmap; open an issue if you need it sooner.

### Can I have more than 8 states?

Yes. Override the default before including the header:

```cpp
#define PULSEHSM_MAX_STATES 24   // must be ≤ 127
#include "PulseHSM.h"
```

In PlatformIO add it to `build_flags`:

```ini
build_flags = -DPULSEHSM_MAX_STATES=24
```

### Can I run multiple HSMs simultaneously?

Yes — each `PulseHSM` instance is fully independent. Call `update()` for each one in `loop()`. There is no shared state between instances.

```cpp
PulseHSM motorFsm;
PulseHSM uiFsm;

void loop() {
  motorFsm.update();
  uiFsm.update();
}
```

### Does it support orthogonal (parallel) regions?

No. PulseHSM models a single active leaf at a time. For parallel regions, run two separate `PulseHSM` instances and coordinate via shared variables or events.

---

## Events

### What happens if the event queue fills up?

`sendEvent()` returns `false` and the event is silently dropped. The queue is never overwritten. Raise `PULSEHSM_MAX_EVENTS` (must remain a power of two) if you need a larger buffer:

```cpp
#define PULSEHSM_MAX_EVENTS 32
#include "PulseHSM.h"
```

### Can I send events from an ISR?

Yes — `sendEvent()` disables interrupts (save/restore, not a global clear) while it enqueues, making it safe to call from a hardware interrupt. See [Events & Payloads](events.md) for per-platform interrupt notes.

### Can I send events from `entry()`, `exit()`, or `update()`?

Yes. Events sent during a callback are queued and dispatched on the **next** `update()` tick, not immediately.

### Can I send events from `onEvent()`?

Yes, with the same caveat — they queue up and dispatch on the next tick.

---

## Transitions

### Can I call `transitionTo()` from inside a callback?

Yes, from any callback. Transitions are deferred to the end of `update()`. Calling `transitionTo()` from inside `entry()` or `exit()` (during an already-executing transition) is safe — the new target is recorded and applied on the **next** `update()`.

### What is a self-transition?

`transitionTo(currentState)` — a transition to the state you are already in.

- **Default (lightweight):** only `entryTime` resets; `entry()` / `exit()` do not run.
- **Full reinit:** `exit()` then `entry()` run for that state only. Enable with:
  ```cpp
  #define PULSEHSM_SELF_TRANSITION_FULL_REINIT 1
  ```

### What fires during a transition?

PulseHSM walks up from the source leaf to the LCA (Lowest Common Ancestor), calls `exit()` on each state, then walks down from the LCA to the target leaf, calling `entry()` on each. States above the LCA stay active and their `exit()`/`entry()` do not fire.

---

## Memory and performance

### How much RAM does PulseHSM use?

See the [landing page](/) for a footprint table. The short answer: ~160 B on AVR at defaults. Each extra state slot costs ~16 B; each extra event slot costs ~5 B.

### Is PulseHSM safe to use in a tight `loop()` with no delay?

Yes — `update()` runs in O(depth) time with no heap access and no blocking calls. On a 16 MHz ATmega328P the typical `update()` call with no pending events completes in under 10 µs.

### Does `millis()` rollover break timeouts?

No. All time comparisons use unsigned subtraction (`millis() - entryTime`), which wraps correctly at the 49-day boundary.

---

## Platform-specific

### RP2040 dual-core: is cross-core `sendEvent()` safe?

Only if you add your own synchronisation. `sendEvent()` disables interrupts on the **calling core**, which provides no protection against a concurrent call from the other core. Use a `mutex_t` (Pico SDK) or `portMUX_TYPE` (FreeRTOS) around `sendEvent()` on dual-core projects.

### Does PulseHSM work on ESP32 with FreeRTOS tasks?

If `update()` and all `sendEvent()` callers run on the same FreeRTOS task or core, no extra locking is needed. For multi-task use, wrap `sendEvent()` in a mutex.

### Can I use it on non-Arduino ARM bare-metal (no `<Arduino.h>`)?

Not directly — but `PulseHSM.h` only calls `millis()`, `noInterrupts()`, and `interrupts()`. If you provide those three as macros or inline functions, the rest compiles cleanly.
