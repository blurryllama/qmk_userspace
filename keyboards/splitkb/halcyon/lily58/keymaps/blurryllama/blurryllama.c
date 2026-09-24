#include "blurryllama.h"
#include "os_detection.h"
#include "transactions.h"
#include "split_util.h"

blurryllama_state_t bl_state;

bool is_mac_mode(void) {
    return keymap_config.swap_lctl_lgui;
}

// Switch Mac/PC mode automatically once the host OS has been detected.
// CG_TOGG still works as a manual override until the next replug.
bool process_detected_host_os_user(os_variant_t os) {
    if (os != OS_UNSURE) {
        bool mac                     = (os == OS_MACOS || os == OS_IOS);
        keymap_config.swap_lctl_lgui = mac;
        keymap_config.swap_rctl_rgui = mac;
    }
    return true;
}

// Shortcuts per OS. These are sent as-is: the Ctrl/GUI swap is not applied to them.
// KC_NO means the key does nothing on that OS.
//
// QMK reports anything that isn't Windows, macOS or iOS as Linux: desktop Linux, SteamOS, Raspberry Pi,
// Android, and even consoles. So the Linux column sticks to standard keys that mean the same thing
// everywhere, and leaves out anything that could trigger an unrelated shortcut.
typedef struct {
    uint16_t pc;
    uint16_t mac;
    uint16_t linux_os; // not "linux", which some compilers predefine as a macro
} os_shortcut_t;

// Stands for the "AL Terminal Lock" media key, the standard lock-screen key, which isn't a QMK keycode
#define LOCK_SCREEN_KEY 0xFFFF

static const os_shortcut_t os_shortcuts[OSK_LAST - QK_KB_0] = {
    //                             PC           Mac            Linux
    [OSK_INSPECT - QK_KB_0]      = {C(S(KC_I)), G(A(KC_I)),    C(S(KC_I))},      // browser dev tools
    [OSK_SCREENSHOT - QK_KB_0]   = {KC_PSCR,    G(S(KC_5)),    KC_PSCR},         // screenshot tool
    [OSK_LOCK - QK_KB_0]         = {G(KC_L),    C(G(KC_Q)),    LOCK_SCREEN_KEY}, // lock screen
    [OSK_FORCE_QUIT - QK_KB_0]   = {A(KC_F4),   G(A(KC_ESC)),  A(KC_F4)},        // close window / Force Quit dialog
    [OSK_CALC - QK_KB_0]         = {KC_CALC,    KC_NO,         KC_CALC},         // Mac: see mac_open_calculator()
    [OSK_VOLUME_MIXER - QK_KB_0] = {C(A(KC_V)), A(KC_VOLU),    KC_NO},           // Mac: Option + a volume key opens Sound settings
    [OSK_POWER_MENU - QK_KB_0]   = {G(KC_X),    KC_NO,         KC_NO},           // Windows power user menu
};

static uint16_t os_shortcut(uint8_t i) {
    if (is_mac_mode()) {
        return os_shortcuts[i].mac;
    }
    return detected_host_os() == OS_LINUX ? os_shortcuts[i].linux_os : os_shortcuts[i].pc;
}

static void press_shortcut(uint16_t shortcut) {
    if (shortcut == LOCK_SCREEN_KEY) {
        host_consumer_send(AL_LOCK);
    } else {
        register_code16(shortcut);
    }
}

static void release_shortcut(uint16_t shortcut) {
    if (shortcut == LOCK_SCREEN_KEY) {
        host_consumer_send(0);
    } else {
        unregister_code16(shortcut);
    }
}

// macOS has no built-in Calculator shortcut, so open it through Spotlight
static void mac_open_calculator(void) {
    tap_code16(G(KC_SPC));
    wait_ms(300);
    SEND_STRING("calculator");
    wait_ms(300);
    tap_code(KC_ENT);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        bl_state.key_presses++;
    }
    if (keycode == KC_SPC) {
        bl_state.space_held = record->event.pressed;
    }

    if (keycode >= QK_KB_0 && keycode < OSK_LAST) {
        // Remember what was pressed so the release matches, even if the mode changes in between
        static uint16_t held[OSK_LAST - QK_KB_0];
        uint8_t         i = keycode - QK_KB_0;

        if (record->event.pressed) {
            held[i] = os_shortcut(i);
            if (keycode == OSK_CALC && is_mac_mode()) {
                mac_open_calculator();
            } else if (held[i] != KC_NO) {
                press_shortcut(held[i]);
            }
        } else if (held[i] != KC_NO) {
            release_shortcut(held[i]);
            held[i] = KC_NO;
        }
        return false;
    }
    return true;
}

// Split sync: the master half sends bl_state to the other half

static void bl_state_sync_handler(uint8_t in_buflen, const void *in_data, uint8_t out_buflen, void *out_data) {
    if (in_buflen == sizeof(bl_state)) {
        memcpy(&bl_state, in_data, sizeof(bl_state));
    }
}

void keyboard_post_init_user(void) {
    transaction_register_rpc(USER_SYNC_STATE, bl_state_sync_handler);
}

void housekeeping_task_user(void) {
    // Typing on the other half counts as activity here too, so this half's display and backlight
    // stay on. The time is taken from this half's own clock: QMK's SPLIT_ACTIVITY_ENABLE copies the
    // other half's timestamp instead, which can be slightly in the future here and briefly makes
    // the display think it has timed out.
    if (!is_keyboard_master()) {
        static uint8_t last_key_presses;
        if (bl_state.key_presses != last_key_presses) {
            last_key_presses = bl_state.key_presses;
            set_activity_timestamps(sync_timer_read32(), last_encoder_activity_time(), last_pointing_device_activity_time());
        }
        return;
    }
    if (!is_transport_connected()) {
        return;
    }

    static blurryllama_state_t last_sent;
    static uint32_t            last_sync = 0;

    // Send on change, and every half second in case the other half missed an update
    if (memcmp(&bl_state, &last_sent, sizeof(bl_state)) != 0 || timer_elapsed32(last_sync) > 500) {
        if (transaction_rpc_send(USER_SYNC_STATE, sizeof(bl_state), &bl_state)) {
            last_sent = bl_state;
            last_sync = timer_read32();
        }
    }
}
