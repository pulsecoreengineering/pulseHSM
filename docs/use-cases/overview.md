# Use Cases

These examples cover real-world embedded patterns. Each one is self-contained and
designed to be adapted directly to a project.

## Which example to read first?

| If you want to see… | Read |
|---|---|
| Hierarchy + shared event handling + initial substates | [Vending Machine](vending-machine.md) |
| Timed retries, event payloads, state-dependent logic | [Device Connection Manager](device-manager.md) |
| Safety-critical E-stop at superstate + complex hierarchy | [Industrial Machine Controller](machine-controller.md) |
| Deep nesting, back-navigation, composite entry via setInitial | [UI Menu System](menu-system.md) |
| ISR-safe byte stream parsing, timeout reset | [Serial Protocol Parser](protocol-parser.md) |

## PulseHSM features by example

| Feature | Vending | Device Mgr | Machine Ctrl | Menu | Protocol |
|---|:---:|:---:|:---:|:---:|:---:|
| Hierarchy / superstates | ✓ | ✓ | ✓ | ✓ | — |
| `setInitial` | ✓ | ✓ | ✓ | ✓ | — |
| Event payloads | ✓ | ✓ | — | ✓ | ✓ |
| Event bubbling | ✓ | — | ✓ | ✓ | — |
| Timed transitions | — | ✓ | ✓ | — | ✓ |
| ISR `sendEvent` | — | — | — | — | ✓ |
| `isInHierarchy` | ✓ | ✓ | ✓ | — | — |
| `getPreviousState` | — | ✓ | — | ✓ | — |
| Self-transition | — | ✓ | — | — | — |

## General patterns

### Shared event handling

Put the handler in a **superstate** and let bubbling do the work:

```cpp
bool sharedHandler(uint8_t e) {
  if (e == EVT_FAULT) { fsm.transitionTo(ST_FAULT); return true; }
  return false;
}
// Add this as the onEvent for the top-level superstate.
// All children that don't consume EVT_FAULT will bubble it here.
```

### Entry action as a fire-and-forget command

Use `entry()` to issue a command when a state is entered, and an event to know
when it is done:

```cpp
void connectingEntry() {
  wifi.connect(SSID, PASSWORD);   // non-blocking kick-off
}
bool connectingEvent(uint8_t e) {
  if (e == EVT_WIFI_CONNECTED) { fsm.transitionTo(ST_CONNECTED); return true; }
  if (e == EVT_WIFI_FAILED)    { fsm.transitionTo(ST_BACKING_OFF); return true; }
  return false;
}
```

### Polling in update()

For hardware that must be polled (a button, an ADC reading), use `update()`:

```cpp
void monitorUpdate() {
  if (analogRead(TEMP_PIN) > THRESHOLD)
    fsm.transitionTo(ST_OVERHEAT);
}
```

### Timeout as a watchdog

Use `timeoutMs` + `timeoutNext` as a lightweight watchdog — if the state hasn't
received its expected event in time, automatically recover:

```cpp
// WAITING_FOR_ACK times out after 2 s → RETRY
fsm.addState("WAITING_ACK", nullptr, sendPacket, nullptr, 2000, ST_RETRY, nullptr, parent);
```
