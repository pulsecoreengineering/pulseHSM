# Concepts

## What is a state machine?

A finite state machine (FSM) is a model where a system is always in exactly one
**state**, and **events** or **timeouts** cause it to move between states. Each
transition can run a callback — open a valve, send a packet, light an LED.

FSMs are everywhere in embedded firmware: button debouncers, communication protocols,
motor sequencers, UI navigators.

## Why hierarchical?

A flat FSM breaks down once states share behaviour. Suppose you have a motor
controller with states `STARTING`, `RUNNING`, and `COASTING`. All three need to
respond to an E-stop event by going to `FAULT`. In a flat FSM you add the E-stop
handler to each state. When you later add `BRAKING`, you add it again.

An **HSM** solves this with **superstates** (composite states): group
`STARTING`, `RUNNING`, and `COASTING` under a `MOTOR_ON` superstate and put the
E-stop handler there once. Any event that the active leaf doesn't handle
automatically **bubbles up** to the parent.

```
MOTOR_ON  ← E-stop handler lives here
├── STARTING   (3-second ramp-up, then → RUNNING)
├── RUNNING    (steady operation)
└── COASTING   (power cut, decelerating)
FAULT          (safe stop, requires service)
```

## PulseHSM's model

PulseHSM is always **in exactly one leaf state**, but simultaneously in all of that
leaf's ancestors. The machine above is either in `STARTING`, `RUNNING`, or
`COASTING` — and whichever it is, it is also in `MOTOR_ON`.

### State callbacks

| Callback | When it fires |
|---|---|
| `entry()` | Once, when the machine enters the state. For superstates, fires on the way **in** before the child's entry. |
| `exit()` | Once, when the machine leaves the state. For superstates, fires on the way **out** after the child's exit. |
| `update()` | Every `loop()` tick while the machine is in the state or any descendant. |

### Transition mechanics

When you call `transitionTo(B)` from inside any callback while in state `A`:

1. PulseHSM finds the **lowest common ancestor** (LCA) of `A` and `B`.
2. It calls `exit()` on `A`, then walks up toward the LCA (calling `exit()` on each
   ancestor). It stops at the LCA — the LCA is **not** exited.
3. It walks down from the LCA toward `B`, calling `entry()` on each state along the
   way, ending with `B`'s `entry()`.

**States between the LCA and the target are entered in outer-to-inner order.**
Shared superstates are never exited or re-entered — they stay active throughout.

### Event bubbling

Events dispatched by `sendEvent()` are delivered to the current leaf's `onEvent`
handler first. If that handler returns `false` (or is `nullptr`), PulseHSM walks to
the parent and tries its `onEvent`, continuing up to the root. The first handler
that returns `true` consumes the event.

## Glossary

| Term | Meaning |
|---|---|
| **Composite / superstate** | A state that has children |
| **Leaf state** | A state with no children; the machine is always in a leaf |
| **LCA** | Lowest Common Ancestor — the deepest state that is an ancestor of both the source and target of a transition |
| **Initial substate** | The default child entered when a composite state is targeted directly |
| **Event bubbling** | Unhandled events propagate to the parent's handler |
| **Deferred transition** | `transitionTo()` sets a pending state; the actual transition runs at the end of the current `update()` tick |
