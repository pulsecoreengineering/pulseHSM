# API Reference

## Callback types

```cpp
using Action  = void (*)();
using EventCb = bool (*)(uint8_t event);
```

`Action` is used for `entry`, `exit`, and `update` callbacks.  
`EventCb` returns `true` if the event was consumed (stops bubbling), `false` to
pass it to the parent.

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

**Field order** mirrors the original `addState()` argument order, with `initialChild`
appended as the ninth field.

**Macro helpers:**

| Macro | Use |
|---|---|
| `PULSEHSM_NAME("label")` | Expands to `"label"` normally; elided when `PULSEHSM_NAMES=0` |
| `PULSEHSM_TABLE` | Placement attribute: `PROGMEM` on AVR, empty elsewhere — always apply this to your table declaration |

**Typical table definition:**

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

Compile-time macro that `static_assert`s the following on `table`:

- No state has an out-of-range `parent` or `timeoutNext`.
- No parent/child relationship forms a cycle.
- No state exceeds `PULSEHSM_MAX_DEPTH`.
- Every composite (state with an `initialChild != -1`) has a valid direct child.
- No half-wired timeouts (`timeoutMs > 0` with `timeoutNext == -1` is
  intentionally allowed; only clearly invalid index values are caught).

Place it immediately after the table, before defining the `PulseHSM` instance.
Nothing that used to be a runtime hang survives this check.

---

## PulseHSM constructor

```cpp
PulseHSM(const StaticState* stateTable, uint8_t count);
```

Constructs a state machine over the given table. `count` must equal the number of
elements in `stateTable` (i.e., `ST_COUNT`).

---

## begin()

```cpp
bool begin(int startState);
```

Starts the machine in `startState`. Calls the full entry chain from the root down
to the resolved leaf. Resets the event queue and the dropped-events counter.

`startState` may be:
- A **leaf** — entered directly.
- A **composite with `initialChild` configured** — resolved recursively to the
  deepest initial leaf.

Returns `false` if `startState` is out of range, or is a composite with
`initialChild == -1` (a programming error caught by `PULSEHSM_VALIDATE_TABLE`).

Must be called once before `update()`.

---

## update()

```cpp
void update();
```

The main scheduler tick. Call once per `loop()`.

Order of operations each tick:
1. Drain the event queue — dispatch each queued event via `onEvent` bubbling.
2. Check the current state's timeout — if elapsed, apply the pending transition.
3. Run `update()` callbacks from the root down to the current leaf.
4. Apply any pending transition (from `transitionTo()`, a timeout, or an event
   handler that called `transitionTo()`).

---

## transitionTo()

```cpp
void transitionTo(int newState);
```

Requests a transition to `newState`. The transition is **deferred** — it is
applied at the end of the current `update()` tick, after `update()` callbacks run.

Safe to call from inside `entry()`, `exit()`, `update()`, or `onEvent()`. If
called from inside `entry()` or `exit()` (i.e., while a transition is already
executing), the request is recorded and applied on the **next** `update()`.

`newState` may be a leaf or a composite with `initialChild` configured.

---

## sendEvent()

```cpp
bool sendEvent(uint8_t event, int32_t data = 0);
```

Enqueues an event into the interrupt-safe ring buffer. `data` is an optional
32-bit payload readable via `getEventData()` inside the handler.

Returns `true` if the event was queued, `false` if the queue was full (event
dropped). The queue capacity is `PULSEHSM_MAX_EVENTS`.

Safe to call from an ISR. See [Events & Payloads](../guide/events.md) for
interrupt-safety details per platform.

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

Returns the `name` string of the current leaf state. Never returns `nullptr`
(returns `""` for an unnamed state or when `PULSEHSM_NAMES=0`).

---

## getStateName()

```cpp
const char* getStateName(int idx) const;
```

Returns the `name` string of the state at index `idx`. Returns `""` for an
out-of-range or unnamed state, or when `PULSEHSM_NAMES=0`.

---

## getStateElapsed()

```cpp
unsigned long getStateElapsed() const;
```

Returns the number of milliseconds since the machine last entered the current
state. Useful in `update()` for time-based behaviour that needs finer control than
`timeoutMs`.

---

## getPreviousState()

```cpp
int getPreviousState() const;
```

Returns the index of the state the machine was in **before** the last transition.
Returns `-1` before the first transition.

---

## getPreviousName()

```cpp
const char* getPreviousName() const;
```

Returns the `name` string of the previous state. Returns `""` before the first
transition, or when `PULSEHSM_NAMES=0`.

---

## getEventData()

```cpp
int32_t getEventData() const;
```

Returns the `int32_t` payload of the event currently being dispatched. Valid only
inside an `onEvent` callback (or any callback synchronously invoked from one).

---

## getDroppedEvents()

```cpp
uint8_t getDroppedEvents() const;
```

Returns the number of `sendEvent()` calls that were rejected because the queue was
full since the last `begin()`. Saturates at 255. Use this to detect queue
overflows at runtime.

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

Returns `true` if `state` is the current leaf **or** any active ancestor of it.

```cpp
// While in state STARTING (child of RUNNING):
fsm.isInHierarchy(ST_STARTING) == true
fsm.isInHierarchy(ST_RUNNING)  == true   // active ancestor
fsm.isInHierarchy(ST_FAULT)    == false  // not in the active chain
```

Useful for status displays and conditional logic outside the state machine.
