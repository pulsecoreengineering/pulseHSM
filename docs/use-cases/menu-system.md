# UI Menu System

**Demonstrates:** deep nesting, back-navigation with `getPreviousState()`, composite
entry via `setInitial`, `isInHierarchy()` for breadcrumb display, event payloads
for encoder input.

## The problem

An LCD/OLED menu system has a root menu, sub-menus, and settings screens.
"Back" must always return to the correct parent, regardless of how deeply
nested the user is. A flat FSM requires a back-target variable per screen; an
HSM derives it from the state hierarchy automatically.

## State diagram

```
ROOT_MENU             (initial screen)
├── MAIN_MENU         (initial; top-level items)
│   ├── SETTINGS_MENU (initial: DISPLAY_SETTINGS)
│   │   ├── DISPLAY_SETTINGS  (initial leaf)
│   │   └── AUDIO_SETTINGS
│   └── INFO_SCREEN   (leaf)
└── SCREENSAVER       (auto-activated; any input → MAIN_MENU)
```

**Events:**
- `EVT_UP` / `EVT_DOWN` — scroll cursor (payload: steps)
- `EVT_ENTER` — activate current item
- `EVT_BACK` — go to parent menu
- `EVT_TIMEOUT` — no activity → SCREENSAVER
- `EVT_WAKE` — any input while screensaver → MAIN_MENU

## Full example

```cpp
#define PULSEHSM_MAX_STATES 16
#define PULSEHSM_MAX_EVENTS 16
#include "PulseHSM.h"

PulseHSM fsm;

// ---- State indices -------------------------------------------------------
int ST_ROOT, ST_MAIN_MENU, ST_SETTINGS_MENU;
int ST_DISPLAY_SETTINGS, ST_AUDIO_SETTINGS;
int ST_INFO_SCREEN, ST_SCREENSAVER;

// ---- Events -------------------------------------------------------------
enum Events : uint8_t {
  EVT_UP      = 1,
  EVT_DOWN,
  EVT_ENTER,
  EVT_BACK,
  EVT_TIMEOUT,  // no activity for N seconds → screensaver
  EVT_WAKE,     // any wake event while screensaver is active
};

// ---- UI state -----------------------------------------------------------
static int  cursorPos   = 0;
static bool settingChanged = false;

// ---- Display helpers (implement for your hardware) ----------------------
void lcd_clear()                   { Serial.println("--- [CLEAR] ---"); }
void lcd_print(const char* s)      { Serial.println(s); }
void lcd_cursor(int pos)           { Serial.print("  cursor -> "); Serial.println(pos); }
void lcd_screensaver()             { Serial.println("[screensaver]"); }

// ---- ROOT_MENU (root superstate) ----------------------------------------
// Timeout fires here if no child handles EVT_TIMEOUT.
bool rootEvent(uint8_t e) {
  if (e == EVT_TIMEOUT) {
    fsm.transitionTo(ST_SCREENSAVER);
    return true;
  }
  return false;
}

// ---- MAIN_MENU ----------------------------------------------------------
static const char* MAIN_ITEMS[] = { "Settings", "Info", "Back" };
static const int   N_MAIN = 3;

void mainMenu_entry() {
  cursorPos = 0;
  lcd_clear();
  lcd_print("=== Main Menu ===");
  for (int i = 0; i < N_MAIN; i++) lcd_print(MAIN_ITEMS[i]);
  lcd_cursor(cursorPos);
}

bool mainMenu_event(uint8_t e) {
  if (e == EVT_UP) {
    cursorPos = (cursorPos - 1 + N_MAIN) % N_MAIN;
    lcd_cursor(cursorPos);
    return true;
  }
  if (e == EVT_DOWN) {
    cursorPos = (cursorPos + 1) % N_MAIN;
    lcd_cursor(cursorPos);
    return true;
  }
  if (e == EVT_ENTER) {
    switch (cursorPos) {
      case 0: fsm.transitionTo(ST_SETTINGS_MENU); break;  // → DISPLAY_SETTINGS
      case 1: fsm.transitionTo(ST_INFO_SCREEN);   break;
      case 2: /* root has no parent; do nothing */ break;
    }
    return true;
  }
  return false;
}

// ---- SETTINGS_MENU (superstate) -----------------------------------------
void settingsMenu_entry() {
  lcd_clear();
  lcd_print("=== Settings ===");
}

bool settingsMenu_event(uint8_t e) {
  if (e == EVT_BACK) {
    fsm.transitionTo(ST_MAIN_MENU);
    return true;
  }
  return false;
}

// ---- DISPLAY_SETTINGS ---------------------------------------------------
static int brightness = 8;

void displaySettings_entry() {
  lcd_clear();
  lcd_print("Brightness:");
  Serial.println(brightness);
}

bool displaySettings_event(uint8_t e) {
  if (e == EVT_UP) {
    brightness = min(brightness + 1, 10);
    Serial.print("Brightness: "); Serial.println(brightness);
    return true;
  }
  if (e == EVT_DOWN) {
    brightness = max(brightness - 1, 0);
    Serial.print("Brightness: "); Serial.println(brightness);
    return true;
  }
  if (e == EVT_ENTER) {
    // Navigate to audio settings
    fsm.transitionTo(ST_AUDIO_SETTINGS);
    return true;
  }
  // EVT_BACK bubbles to settingsMenu_event → MAIN_MENU
  return false;
}

// ---- AUDIO_SETTINGS -----------------------------------------------------
static int volume = 5;

void audioSettings_entry() {
  lcd_clear();
  lcd_print("Volume:");
  Serial.println(volume);
}

bool audioSettings_event(uint8_t e) {
  if (e == EVT_UP) {
    volume = min(volume + 1, 10);
    Serial.print("Volume: "); Serial.println(volume);
    return true;
  }
  if (e == EVT_DOWN) {
    volume = max(volume - 1, 0);
    Serial.print("Volume: "); Serial.println(volume);
    return true;
  }
  // EVT_BACK bubbles to settingsMenu_event → MAIN_MENU
  return false;
}

// ---- INFO_SCREEN --------------------------------------------------------
void infoScreen_entry() {
  lcd_clear();
  lcd_print("FW v1.2.0  |  SN: 0042");
  lcd_print("Press BACK");
}

bool infoScreen_event(uint8_t e) {
  if (e == EVT_BACK) {
    fsm.transitionTo(ST_MAIN_MENU);
    return true;
  }
  return false;
}

// ---- SCREENSAVER --------------------------------------------------------
void screensaver_entry() { lcd_screensaver(); }

bool screensaver_event(uint8_t e) {
  if (e == EVT_WAKE || e == EVT_UP || e == EVT_DOWN ||
      e == EVT_ENTER || e == EVT_BACK) {
    fsm.transitionTo(ST_MAIN_MENU);   // → MAIN_MENU (initial of ROOT_MENU)
    return true;
  }
  return false;
}

// ---- Breadcrumb helper (uses isInHierarchy) ----------------------------
void printBreadcrumb() {
  Serial.print("Path: ROOT");
  if (fsm.isInHierarchy(ST_SETTINGS_MENU)) Serial.print(" > Settings");
  if (fsm.getCurrentState() == ST_DISPLAY_SETTINGS) Serial.print(" > Display");
  if (fsm.getCurrentState() == ST_AUDIO_SETTINGS)   Serial.print(" > Audio");
  if (fsm.getCurrentState() == ST_INFO_SCREEN)       Serial.print(" > Info");
  if (fsm.getCurrentState() == ST_SCREENSAVER)       Serial.print(" [screensaver]");
  Serial.println();
}

// ---- setup / loop -------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Parents must be added before children
  ST_ROOT          = fsm.addState("ROOT",     nullptr, nullptr,               nullptr, 30000, -1,          rootEvent,            -1);
  ST_SCREENSAVER   = fsm.addState("SCR",      nullptr, screensaver_entry,     nullptr, 0,     -1,          screensaver_event,    ST_ROOT);
  ST_MAIN_MENU     = fsm.addState("MAIN",     nullptr, mainMenu_entry,        nullptr, 0,     -1,          mainMenu_event,       ST_ROOT);
  ST_INFO_SCREEN   = fsm.addState("INFO",     nullptr, infoScreen_entry,      nullptr, 0,     -1,          infoScreen_event,     ST_MAIN_MENU);
  ST_SETTINGS_MENU = fsm.addState("SETTINGS", nullptr, settingsMenu_entry,    nullptr, 0,     -1,          settingsMenu_event,   ST_MAIN_MENU);
  ST_DISPLAY_SETTINGS = fsm.addState("DISPLAY", nullptr, displaySettings_entry, nullptr, 0,   -1, displaySettings_event, ST_SETTINGS_MENU);
  ST_AUDIO_SETTINGS   = fsm.addState("AUDIO",   nullptr, audioSettings_entry,   nullptr, 0,   -1, audioSettings_event,   ST_SETTINGS_MENU);

  // Wire initial substates
  fsm.setInitial(ST_ROOT,          ST_MAIN_MENU);
  fsm.setInitial(ST_MAIN_MENU,     ST_SETTINGS_MENU);  // not used directly, but safe to set
  fsm.setInitial(ST_SETTINGS_MENU, ST_DISPLAY_SETTINGS);

  fsm.begin(ST_ROOT);   // → MAIN_MENU (ROOT's initial) → leaf
}

void loop() {
  if (Serial.available()) {
    switch (Serial.read()) {
      case 'u': fsm.sendEvent(EVT_UP);      break;
      case 'd': fsm.sendEvent(EVT_DOWN);    break;
      case 'e': fsm.sendEvent(EVT_ENTER);   break;
      case 'b': fsm.sendEvent(EVT_BACK);    break;
      case 'w': fsm.sendEvent(EVT_WAKE);    break;
    }
    printBreadcrumb();
  }

  fsm.update();
}
```

## Key design decisions

**`EVT_TIMEOUT` at the root**  
`ROOT`'s `timeoutMs = 30000` is set but `timeoutNext = -1` — the timeout fires
`EVT_TIMEOUT` into `rootEvent`, which then calls `transitionTo(ST_SCREENSAVER)`.
Alternatively, set `timeoutNext = ST_SCREENSAVER` directly and skip the handler.

**`EVT_BACK` bubbling**  
`DISPLAY_SETTINGS` and `AUDIO_SETTINGS` do not handle `EVT_BACK`. The event
bubbles to `SETTINGS_MENU`, which handles it and transitions to `MAIN_MENU`.
No per-leaf back-target tracking needed.

**`setInitial` on `SETTINGS_MENU`**  
`transitionTo(ST_SETTINGS_MENU)` resolves to `DISPLAY_SETTINGS` automatically.
Every entry into Settings always starts at the first item — consistent UX with
zero extra code.

**Breadcrumbs with `isInHierarchy`**  
`isInHierarchy(ST_SETTINGS_MENU)` is `true` for both `DISPLAY_SETTINGS` and
`AUDIO_SETTINGS`. The breadcrumb code never needs to know which leaf is active;
it just asks whether Settings is in the active ancestor chain.

**Re-entering `MAIN_MENU` from the screensaver**  
`transitionTo(ST_MAIN_MENU)` from the screensaver runs `MAIN_MENU`'s `entry()`,
resetting `cursorPos = 0`. The LCA is `ST_ROOT`, so `ROOT`'s timeout counter
also resets.
