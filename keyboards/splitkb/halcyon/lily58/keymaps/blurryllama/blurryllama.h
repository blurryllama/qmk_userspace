#pragma once

#include QMK_KEYBOARD_H

// Custom keycodes. The order must match "customKeycodes" in vial.json,
// which Vial maps onto QK_KB_0, QK_KB_1, ...
enum blurryllama_keycodes {
    OSK_INSPECT = QK_KB_0,
    OSK_SCREENSHOT,
    OSK_LOCK,
    OSK_FORCE_QUIT,
    OSK_CALC,
    OSK_VOLUME_MIXER,
    OSK_POWER_MENU,
    OSK_LAST
};

// State the master half shares with the other half, for the displays
typedef struct {
    bool    space_held;
    uint8_t key_presses; // lets the other half notice typing, to keep its display awake
} blurryllama_state_t;

extern blurryllama_state_t bl_state;

// True when the Ctrl/GUI swap is on, which is how this keymap represents Mac mode
bool is_mac_mode(void);
