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

## addState()

```cpp
int addState(const char* name,
             Action        update,
             Action        entry,
             Action        exit,
             unsigned long timeoutMs,
             int           timeoutNext,
             EventCb       onEvent,
             int           parent = -1);
```

Registers a new state. Returns the state's index (0, 1, 2, …) on success, or
`-1` on failure.

**Failures:**
- The state table is full (`stateCount >= PULSEHSM_MAX_STATES`).
- The depth of `parent`'s ancestry chain exceeds `PULSEHSM_MAX_DEPTH`.

**Rules:**
- A parent must be added **before** its children — you pass the parent's index,
  which only exists after `addState()` returns it.
- Pass `parent = -1` for a root-level state.
- Any parameter may be `nullptr` / `0` / `-1` to opt out of that feature.

**Parameters:**

| Parameter | Description |
|---|---|
| `name` | Human-readable label for debugging (stored as pointer, not copied) |
| `update` | Called every `update()` tick while in this state or any descendant |
| `entry` | Called once when entering this state |
| `exit` | Called once when leaving this state |
| `timeoutMs` | Milliseconds before an automatic transition; `0` = no timeout |
| `timeoutNext` | State to transition to on timeout; `-1` = no timeout target |
| `onEvent` | Event handler; return `true` to consume, `false` to bubble |
| `parent` | Parent state index; `-1` for a root state |

---

## setInitial()

```cpp
bool setInitial(int parent, int child);
```

Marks `child` as the default substate entered when `parent` is targeted.  
`child` must be a **direct** child of `parent`.

Returns `false` if either index is out of range, or if `child` is not a direct
child of `parent`. Call after all relevant states have been added.

See [Initial Substates](../guide/initial-substates.md) for full details.

---

## begin()

```cpp
bool begin(int startState);
```

Starts the machine in `startState`. Calls the full entry chain from the root down
to the resolved leaf. Resets the event queue.

`startState` may be:
- A **leaf** — entered directly.
- A **composite with `setInitial` configured** — resolved recursively to the
  deepest initial leaf.

Returns `false` if `startState` is out of range, or is a composite with no
`setInitial` set (a programming error).

Must be called once before `update()`.

---

## update()

```cpp
void update();
```

The main scheduler tick. Call once per `loop()`.

Order of operations each tick:
1. Drain the event queue — dispatch each queued event via `onEvent` bubbling.
2. Check the current state's timeout — if elapsed, set the pending transition.
3. Run `update()` callbacks from the root down to the current leaf.
4. Apply any pending transition (from `transitionTo()`, a timeout, or an event
   handler that called `transitionTo()`).

---

## transitionTo()

```cpp
void transitionTo(int newState);
```

Requests a transition to `newState`. The transition is **deferred** — it is
applied at the end of the current `update()` tick, after `_runUpdates()`.

Safe to call from inside `entry()`, `exit()`, `update()`, or `onEvent()`. If
called from inside `entry()` or `exit()` (i.e., while a transition is already
executing), the request is recorded and applied on the **next** `update()`.

`newState` may be a leaf or a composite with `setInitial` configured.

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
(returns `""` for an unnamed state).

---

## getStateName()

```cpp
const char* getStateName(int idx) const;
```

Returns the `name` string of the state at index `idx`. Returns `""` for an
out-of-range or unnamed state.

---

## getStateElapsed()

```cpp
unsigned long getStateElapsed() const;
```

Returns the number of milliseconds since the machine last entered the current
state (i.e., since `entryTime` was recorded). Useful in `update()` for
time-based behaviour that needs finer control than `timeoutMs`.

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
transition.

---

## getEventData()

```cpp
int32_t getEventData() const;
```

Returns the `int32_t` payload of the event currently being dispatched. Valid only
inside an `onEvent` callback (or any callback synchronously invoked from one).

---

## isInHierarchy()

```cpp
bool isInHierarchy(int state) const;
```

Returns `true` if `state` is the current leaf **or** any active ancestor of it.

```cpp
// While in state STARTING (child of RUNNING):
fsm.isInHierarchy(STARTING) == true
fsm.isInHierarchy(RUNNING)  == true   // active ancestor
fsm.isInHierarchy(FAULT)    == false  // not in the active chain
```

Useful for status displays and conditional logic outside the state machine.
