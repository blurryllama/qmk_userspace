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
typedef struct {
    uint16_t pc;
    uint16_t mac;
} os_shortcut_t;

static const os_shortcut_t os_shortcuts[OSK_LAST - QK_KB_0] = {
    [OSK_INSPECT - QK_KB_0]      = {C(S(KC_I)), G(A(KC_I))},   // browser dev tools
    [OSK_SCREENSHOT - QK_KB_0]   = {KC_PSCR, G(S(KC_5))},      // Snipping Tool / screenshot toolbar
    [OSK_LOCK - QK_KB_0]         = {G(KC_L), C(G(KC_Q))},      // lock screen
    [OSK_FORCE_QUIT - QK_KB_0]   = {A(KC_F4), G(A(KC_ESC))},   // close window / Force Quit dialog
    [OSK_CALC - QK_KB_0]         = {KC_CALC, KC_NO},           // Mac: see mac_open_calculator()
    [OSK_VOLUME_MIXER - QK_KB_0] = {C(A(KC_V)), A(KC_VOLU)},   // Option + a volume key opens Sound settings
    [OSK_POWER_MENU - QK_KB_0]   = {G(KC_X), KC_NO},           // Windows power user menu
};

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
            held[i] = is_mac_mode() ? os_shortcuts[i].mac : os_shortcuts[i].pc;
            if (keycode == OSK_CALC && is_mac_mode()) {
                mac_open_calculator();
            } else if (held[i] != KC_NO) {
                register_code16(held[i]);
            }
        } else if (held[i] != KC_NO) {
            unregister_code16(held[i]);
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
