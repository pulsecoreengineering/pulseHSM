/*
 * AppStateMachine.h
 * Compile-time configuration for the Hierarchical State Machine.
 *
 * The table is `constexpr` so it is built entirely by the compiler, and marked
 * PULSEHSM_TABLE so it is *stored* in flash rather than copied into SRAM at
 * startup on Harvard-architecture parts (AVR). See the note in PulseHSM.h.
 */
#pragma once
#include "PulseHSM.h"

// 1. Strongly typed state indices (instead of random runtime int variables)
enum StateID : int8_t {
    ST_SYSTEM = 0,
    ST_STANDBY,
    ST_RUNNING,
    ST_WARMING_UP,
    ST_OPERATING,
    STATE_COUNT // Automatically holds total number of states (5)
};

// 2. Strongly typed event IDs
enum Evt : uint8_t {
    EVT_START = 1,
    EVT_STOP,
    EVT_ESTOP
};

// 3. Forward declarations so our table array can point to them seamlessly
bool onEvent_System(uint8_t evt);
void onEntry_Standby();
bool onEvent_Standby(uint8_t evt);
void onEntry_WarmingUp();
void onEntry_Operating();
bool onEvent_Operating(uint8_t evt);

// 4. THE FLASH LAYOUT MATRIX
// The `[ST_x] = {...}` form is a GNU extension (C++20 added designated
// initialisers for members, never for array indices). Both avr-g++ and the
// ESP32 toolchain accept it under their default -std=gnu++11/gnu++17; it is
// the one thing here that would fail under a strict -std=c++17 -pedantic.
constexpr PulseHSM::StaticState HSM_STATE_TABLE[STATE_COUNT] PULSEHSM_TABLE = {
    // [ID] = { Name, Update, Entry, Exit, TimeoutMs, TimeoutNext, EventCallback, Parent, InitialChild }

    [ST_SYSTEM] = {
        PULSEHSM_NAME("SYSTEM"),
        nullptr, nullptr, nullptr,
        0, -1,
        onEvent_System,
        -1,
        ST_STANDBY   // <-- SYSTEM is composite, so it names where it resolves to.
                     //     Nothing transitions to SYSTEM today, but declaring it
                     //     makes every state a legal target and lets the
                     //     validator prove _resolveEntry() always lands on a leaf.
    },

    [ST_STANDBY] = {
        PULSEHSM_NAME("STANDBY"),
        nullptr, onEntry_Standby, nullptr,
        0, -1,
        onEvent_Standby,
        ST_SYSTEM, -1
    },

    [ST_RUNNING] = {
        PULSEHSM_NAME("RUNNING"),
        nullptr, nullptr, nullptr,
        0, -1,
        nullptr,
        ST_SYSTEM,
        ST_WARMING_UP // <-- Resolves setInitial()! Landing on RUNNING automatically deepens to WARMING_UP
    },

    [ST_WARMING_UP] = {
        PULSEHSM_NAME("WARMING_UP"),
        nullptr, onEntry_WarmingUp, nullptr,
        3000, ST_OPERATING, // <-- Automatic 3-second timeout rule wired straight to Flash!
        nullptr,
        ST_RUNNING, -1
    },

    [ST_OPERATING] = {
        PULSEHSM_NAME("OPERATING"),
        nullptr, onEntry_Operating, nullptr,
        0, -1,
        onEvent_Operating,
        ST_RUNNING, -1
    }
};

// 5. Prove the topology on the desktop. Bad parents, cycles, chains deeper than
//    PULSEHSM_MAX_DEPTH, composites with no initialChild and half-wired
//    timeouts are all compiler errors now, not runtime mysteries.
PULSEHSM_VALIDATE_TABLE(HSM_STATE_TABLE, STATE_COUNT);
