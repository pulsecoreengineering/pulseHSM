# Serial Protocol Parser

**Demonstrates:** ISR-safe `sendEvent()` from a UART interrupt, byte-by-byte
frame assembly, timeout reset on stale frames, event payloads for frame data.

## The problem

A device receives binary frames over UART at any time, including mid-loop. Each
frame has a start byte, a length byte, N data bytes, and a checksum. The parser
must be ISR-safe (bytes arrive in an interrupt), reset if a frame is not completed
within a timeout, and hand completed frames to the application without blocking the
ISR.

## State diagram

```
IDLE          (waiting for start byte 0xAA)
RECEIVING     (collecting length + data bytes)
VALIDATING    (checksum pass → COMPLETE; fail → IDLE)
COMPLETE      (frame ready; application reads it, then → IDLE)
```

**Events:**
- `EVT_BYTE` — byte received (payload = byte value); from UART ISR
- `EVT_FRAME_OK` — checksum passed; generated internally after last byte
- `EVT_FRAME_BAD` — checksum failed
- `EVT_TIMEOUT` — no byte received within 50 ms while in RECEIVING

## Full example

```cpp
#define PULSEHSM_MAX_STATES 16
#define PULSEHSM_MAX_EVENTS 32   // larger queue: bytes arrive fast
#include "PulseHSM.h"

PulseHSM parser;

// ---- State indices -------------------------------------------------------
int ST_IDLE, ST_RECEIVING, ST_VALIDATING, ST_COMPLETE;

// ---- Events -------------------------------------------------------------
enum Events : uint8_t {
  EVT_BYTE      = 1,   // payload: received byte (0-255)
  EVT_FRAME_OK,        // internal: checksum passed
  EVT_FRAME_BAD,       // internal: checksum failed
  EVT_TIMEOUT,         // internal: stale frame timeout
};

// ---- Frame buffer -------------------------------------------------------
static const uint8_t START_BYTE  = 0xAA;
static const uint8_t MAX_PAYLOAD = 64;
static const unsigned long FRAME_TIMEOUT_MS = 50;

static uint8_t frameBuf[MAX_PAYLOAD];
static uint8_t frameLen   = 0;
static uint8_t bytesLeft  = 0;
static uint8_t checksum   = 0;

// ---- Application callback -----------------------------------------------
// Called when a complete, valid frame is available.
void onFrameReceived(const uint8_t* data, uint8_t len) {
  Serial.print("Frame OK, ");
  Serial.print(len);
  Serial.println(" bytes");
}

// ---- IDLE ---------------------------------------------------------------
void idle_entry() {
  frameLen  = 0;
  bytesLeft = 0;
  checksum  = 0;
}

bool idle_event(uint8_t e) {
  if (e == EVT_BYTE) {
    uint8_t b = (uint8_t)parser.getEventData();
    if (b == START_BYTE) {
      parser.transitionTo(ST_RECEIVING);
    }
    // Any other byte while idle is silently discarded
    return true;
  }
  return false;
}

// ---- RECEIVING ----------------------------------------------------------
// On entry we have just seen the start byte; next byte is the length.
// We count expected bytes in bytesLeft and accumulate a XOR checksum.

void receiving_entry() {
  frameLen  = 0;
  bytesLeft = 1;   // first incoming byte = length byte
  checksum  = 0;
}

bool receiving_event(uint8_t e) {
  if (e == EVT_BYTE) {
    uint8_t b = (uint8_t)parser.getEventData();

    if (frameLen == 0 && bytesLeft == 1) {
      // This is the length byte
      frameLen  = b;
      bytesLeft = frameLen + 1;   // N data bytes + 1 checksum byte
      if (frameLen == 0 || frameLen > MAX_PAYLOAD) {
        // Bad length — abort
        parser.transitionTo(ST_IDLE);
        return true;
      }
      checksum = b;
    } else if (bytesLeft > 1) {
      // Data byte
      frameBuf[frameLen - (bytesLeft - 1)] = b;
      checksum ^= b;
      bytesLeft--;
    } else {
      // Last byte = received checksum
      if (b == checksum) {
        parser.sendEvent(EVT_FRAME_OK);
      } else {
        parser.sendEvent(EVT_FRAME_BAD);
      }
      parser.transitionTo(ST_VALIDATING);
    }
    return true;
  }
  if (e == EVT_TIMEOUT) {
    // Stale frame — go back to idle
    Serial.println("Frame timeout — resync.");
    parser.transitionTo(ST_IDLE);
    return true;
  }
  return false;
}

// ---- VALIDATING ---------------------------------------------------------
// Entered immediately after the checksum byte. EVT_FRAME_OK/BAD was
// already queued in receiving_event; it fires here on the next tick.

bool validating_event(uint8_t e) {
  if (e == EVT_FRAME_OK) {
    parser.transitionTo(ST_COMPLETE);
    return true;
  }
  if (e == EVT_FRAME_BAD) {
    Serial.println("Checksum error.");
    parser.transitionTo(ST_IDLE);
    return true;
  }
  return false;
}

// ---- COMPLETE -----------------------------------------------------------
// Application consumes the frame here (entry callback), then the machine
// immediately goes back to IDLE to be ready for the next frame.

void complete_entry() {
  onFrameReceived(frameBuf, frameLen);
  // Auto-return on the same tick
  parser.transitionTo(ST_IDLE);
}

// ---- Timeout watchdog (runs in update) -----------------------------------
// RECEIVING has a fixed timeoutMs, but we use an update() callback here
// to demonstrate the pattern for dynamic timeouts.

void receiving_update() {
  if (parser.getStateElapsed() > FRAME_TIMEOUT_MS) {
    parser.sendEvent(EVT_TIMEOUT);
  }
}

// ---- UART ISR (platform-specific) ----------------------------------------
// On AVR:
//   ISR(USART_RX_vect) { parser.sendEvent(EVT_BYTE, UDR0); }
//
// On ESP32 (HardwareSerial callback):
//   serialEvent() { while (Serial.available()) parser.sendEvent(EVT_BYTE, Serial.read()); }
//
// On RP2040 (irq callback registered via Serial1.setFIFOSize / irq_set_exclusive_handler):
//   void uart_irq() { while (uart_is_readable(uart0)) parser.sendEvent(EVT_BYTE, uart_getc(uart0)); }

// ---- setup / loop -------------------------------------------------------
void setup() {
  Serial.begin(115200);

  ST_IDLE       = parser.addState("IDLE",       nullptr,          idle_entry,      nullptr, 0,     -1, idle_event,       -1);
  ST_RECEIVING  = parser.addState("RECEIVING",  receiving_update, receiving_entry, nullptr, 0,     -1, receiving_event,  -1);
  ST_VALIDATING = parser.addState("VALIDATING", nullptr,          nullptr,         nullptr, 0,     -1, validating_event, -1);
  ST_COMPLETE   = parser.addState("COMPLETE",   nullptr,          complete_entry,  nullptr, 0,     -1, nullptr,          -1);

  parser.begin(ST_IDLE);
}

void loop() {
  // In a real sketch the ISR (or serialEvent) feeds bytes automatically.
  // This simulation lets you type hex bytes manually via Serial Monitor.
  if (Serial.available()) {
    uint8_t b = (uint8_t)Serial.read();
    parser.sendEvent(EVT_BYTE, b);
  }

  parser.update();
}
```

## ISR safety notes

`sendEvent()` writes into a lock-free ring buffer and is safe to call from an
ISR on all supported platforms. The main loop's `update()` drains the queue
without disabling interrupts.

**Queue depth**  
At 115200 baud, bytes arrive every ~87 µs. If `loop()` takes longer than
`87 µs × PULSEHSM_MAX_EVENTS`, bytes can be dropped. Two mitigations:
1. Increase `PULSEHSM_MAX_EVENTS` (define it before `#include "PulseHSM.h"`).
2. Use a platform UART FIFO or DMA to buffer bytes in hardware.

**Dual-core (RP2040)**  
If the ISR runs on core 0 and the state machine runs on core 1, you need an
additional cross-core FIFO or mutex. PulseHSM's queue is only ISR-safe on a
**single core**. See [Platforms](../guide/platforms.md) for details.

## Key design decisions

**Why `EVT_TIMEOUT` via `receiving_update()`?**  
`receiving_update()` reads `getStateElapsed()` so the timeout starts fresh every
time `RECEIVING` is (re)entered — even mid-stream if the length is invalid and
we re-enter. A fixed `timeoutMs` in `addState()` would work equally well here;
the `update()` approach is shown to demonstrate dynamic timeout logic.

**Queuing `EVT_FRAME_OK` / `EVT_FRAME_BAD` inside an event handler**  
`receiving_event` calls `sendEvent()` and then `transitionTo(ST_VALIDATING)`.
Both are deferred: the queued event fires after the transition completes, so
`validating_event` receives it on the next tick. No race condition.

**`COMPLETE` with immediate self-exit**  
`complete_entry()` calls the application callback, then immediately queues a
transition back to `IDLE`. The machine never pauses at `COMPLETE` — it is just a
named transition point that makes the sequence explicit in the state diagram.

**No hierarchy needed**  
All four states are siblings. Hierarchy is not required to benefit from PulseHSM's
deferred transitions, ISR-safe queue, and timeout support.
