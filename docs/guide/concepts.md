# Concepts

## What is a state machine?

A finite state machine (FSM) is a model where a system is always in exactly one state. Events or timeouts move it from one state to another, and each transition can run a callback, such as opening a valve, sending a packet or lighting an LED.

Embedded firmware is full of them: button debouncers, communication protocols, motor sequencers, UI navigators.

## Why hierarchical?

A flat FSM gets messy once states share behaviour. Take a motor controller with states `STARTING`, `RUNNING` and `COASTING`. All three must respond to an E-stop event by going to `FAULT`, so in a flat FSM you write the E-stop handler three times. When you add `BRAKING` later, you write it a fourth time.

An HSM fixes this with superstates (composite states). Group `STARTING`, `RUNNING` and `COASTING` under a `MOTOR_ON` superstate and put the E-stop handler there once. Any event the active leaf doesn't handle bubbles up to its parent.

```
MOTOR_ON       (E-stop handler lives here)
  STARTING     (3-second ramp-up, then RUNNING)
  RUNNING      (steady operation)
  COASTING     (power cut, decelerating)
FAULT          (safe stop, requires service)
```

## PulseHSM's model

PulseHSM is always in exactly one leaf state, and in all of that leaf's ancestors at the same time. The machine above is in `STARTING`, `RUNNING` or `COASTING`, and whichever it is, it is also in `MOTOR_ON`.

### State callbacks

| Callback | When it fires |
|---|---|
| `entry()` | Once, when the machine enters the state. For a superstate it fires on the way in, before the child's entry. |
| `exit()` | Once, when the machine leaves the state. For a superstate it fires on the way out, after the child's exit. |
| `update()` | Every `loop()` tick while the machine is in the state or any descendant. |

### Transition mechanics

Say you call `transitionTo(B)` from inside any callback while in state `A`. Then:

1. PulseHSM finds the lowest common ancestor (LCA) of `A` and `B`.
2. It calls `exit()` on `A`, then walks up toward the LCA, calling `exit()` on each ancestor. It stops at the LCA, and the LCA itself is not exited.
3. It walks down from the LCA toward `B`, calling `entry()` on each state on the way and ending with `B`'s `entry()`.

States between the LCA and the target are entered from the outside in. Shared superstates are never exited or re-entered, so they stay active the whole time.

### Event bubbling

An event from `sendEvent()` goes to the current leaf's `onEvent` handler first. If that handler returns `false` or is `nullptr`, PulseHSM moves to the parent and tries its `onEvent`, and keeps going up to the root. The first handler that returns `true` consumes the event.

## Glossary

| Term | Meaning |
|---|---|
| Composite / superstate | A state that has children |
| Leaf state | A state with no children. The machine is always in a leaf |
| LCA | Lowest common ancestor: the deepest state that is an ancestor of both the source and the target of a transition |
| Initial substate | The default child entered when a composite state is targeted directly |
| Event bubbling | An unhandled event goes to the parent's handler |
| Deferred transition | `transitionTo()` sets a pending state, and the transition runs at the end of the current `update()` tick |
