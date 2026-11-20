# API reference

## Callback types

```cpp
using Action  = void (*)();
using EventCb = bool (*)(uint8_t event);
```

`Action` is the type for `entry`, `exit` and `update` callbacks. An `EventCb` returns `true` when it consumed the event, which stops it bubbling. It returns `false` to pass the event to the parent.

---

## StaticState struct

```cpp
struct StaticState {
    const char*   name;          // human-readable label (or nullptr / PULSEHSM_NAME)
    Action        update;        // called every tick; nullptr to opt out
    Action        entry;         // called on entry; nullptr to opt out
    Action        exit;          // called on exit; nullptr to opt out
    unsigned long timeoutMs;     // 0 = no timeout
    int8_t        timeoutNext;   // state index to go to on timeout; -1 = none
    EventCb       onEvent;       // event handler; nullptr to opt out (event bubbles)
    int8_t        parent;        // parent state index; -1 for a root state
    int8_t        initialChild;  // default substate index; -1 for a leaf
};
```

The fields follow the argument order of the original `addState()`, with `initialChild` added as the ninth.

Two macros help with tables:

| Macro | Use |
|---|---|
| `PULSEHSM_NAME("label")` | Expands to `"label"`. Removed when `PULSEHSM_NAMES=0` |
| `PULSEHSM_TABLE` | Placement attribute: `PROGMEM` on AVR, empty elsewhere. Put it on every table declaration |

A typical table:

```cpp
enum StateID : int8_t { ST_A = 0, ST_B, ST_COUNT };

constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_A] = { PULSEHSM_NAME("A"), nullptr, entryA, nullptr, 0,    -1,   nullptr, -1, -1 },
    [ST_B] = { PULSEHSM_NAME("B"), nullptr, entryB, nullptr, 500, ST_A,  nullptr, -1, -1 },
};
```

---

## PULSEHSM_VALIDATE_TABLE()

```cpp
PULSEHSM_VALIDATE_TABLE(table, count)
```

This macro runs `static_assert` checks on `table` at compile time. It fails the build if:

- a state has an out-of-range `parent` or `timeoutNext`
- the parent/child links form a cycle
- a state is deeper than `PULSEHSM_MAX_DEPTH`
- a composite state (one with `initialChild != -1`) has no valid direct child

A timeout with `timeoutMs > 0` and `timeoutNext == -1` is allowed on purpose. Only clearly invalid index values are caught.

Put it right after the table and before the `PulseHSM` instance. Anything that used to hang at runtime is now a compile error.

---

## PulseHSM constructor

```cpp
PulseHSM(const StaticState* stateTable, uint8_t count);
```

Builds a state machine over the given table. `count` must equal the number of elements in `stateTable`, which is `ST_COUNT` in the example above.

---

## begin()

```cpp
bool begin(int startState);
```

Starts the machine in `startState`. It runs the entry callbacks from the root down to the resolved leaf, and it resets the event queue and the dropped-events counter.

`startState` can be a leaf, which is entered directly, or a composite with `initialChild` set, which is resolved recursively down to the deepest initial leaf.

It returns `false` if `startState` is out of range, or if it is a composite with `initialChild == -1`. The second case is a programming error that `PULSEHSM_VALIDATE_TABLE` catches.

Call it once before the first `update()`.

---

## update()

```cpp
void update();
```

The scheduler tick. Call it once per `loop()`. Each tick does four things in this order:

1. Drains the event queue and dispatches each event through `onEvent` bubbling.
2. Checks the current state's timeout. If it has elapsed, the transition becomes pending.
3. Runs the `update()` callbacks from the root down to the current leaf.
4. Applies the pending transition, whether it came from `transitionTo()`, a timeout, or an event handler that called `transitionTo()`.

---

## transitionTo()

```cpp
void transitionTo(int newState);
```

Requests a transition to `newState`. The transition is deferred: it is applied at the end of the current `update()` tick, after the `update()` callbacks have run.

You can call it from `entry()`, `exit()`, `update()` or `onEvent()`. If you call it from `entry()` or `exit()`, while another transition is already running, the request is recorded and applied on the next `update()`.

`newState` can be a leaf or a composite with `initialChild` set.

---

## sendEvent()

```cpp
bool sendEvent(uint8_t event, int32_t data = 0);
```

Puts an event in the interrupt-safe ring buffer. `data` is an optional 32-bit payload that the handler reads with `getEventData()`.

Returns `true` if the event was queued and `false` if the queue was full and the event was dropped. The queue holds `PULSEHSM_MAX_EVENTS` events.

It is safe to call from an ISR. Interrupt-safety details for each platform are in [Events & Payloads](../guide/events.md).

---

## getCurrentState()

```cpp
int getCurrentState() const;
```

Returns the index of the current leaf state.

---

## getCurrentName()

```cpp
const char* getCurrentName() const;
```

Returns the `name` of the current leaf state. It never returns `nullptr`. For an unnamed state, or when `PULSEHSM_NAMES=0`, it returns `""`.

---

## getStateName()

```cpp
const char* getStateName(int idx) const;
```

Returns the `name` of the state at index `idx`. It returns `""` if the index is out of range, the state is unnamed, or `PULSEHSM_NAMES=0`.

---

## getStateElapsed()

```cpp
unsigned long getStateElapsed() const;
```

Returns the milliseconds since the machine last entered the current state. Use it inside `update()` when you need finer timing control than `timeoutMs` gives you.

---

## getPreviousState()

```cpp
int getPreviousState() const;
```

Returns the index of the state the machine was in before the last transition, or `-1` if no transition has happened yet.

---

## getPreviousName()

```cpp
const char* getPreviousName() const;
```

Returns the `name` of the previous state. It returns `""` before the first transition or when `PULSEHSM_NAMES=0`.

---

## getEventData()

```cpp
int32_t getEventData() const;
```

Returns the `int32_t` payload of the event being dispatched. It is only valid inside an `onEvent` callback, or inside any callback that an `onEvent` callback calls synchronously.

---

## getDroppedEvents()

```cpp
uint8_t getDroppedEvents() const;
```

Returns how many `sendEvent()` calls were rejected because the queue was full since the last `begin()`. The count stops at 255. Check it to detect queue overflow at runtime.

```cpp
if (fsm.getDroppedEvents() > 0) {
    Serial.print("Events dropped: ");
    Serial.println(fsm.getDroppedEvents());
}
```

---

## isInHierarchy()

```cpp
bool isInHierarchy(int state) const;
```

Returns `true` if `state` is the current leaf or any active ancestor of it.

```cpp
// While in state STARTING (child of RUNNING):
fsm.isInHierarchy(ST_STARTING) == true
fsm.isInHierarchy(ST_RUNNING)  == true   // active ancestor
fsm.isInHierarchy(ST_FAULT)    == false  // not in the active chain
```

Handy for status displays and for logic outside the state machine.
