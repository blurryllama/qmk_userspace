// Custom content for the two Halcyon TFT displays.
//  - Status display (USB half): layer name, OS logo and held modifiers
//  - Second display: typing speed, or lighting info on the RGB layer, with Luna the keyboard pet

#include "blurryllama.h"
#include "hlc_tft_display/hlc_tft_display.h"
#include "graphics/russo_one_35.qff.h"
#include "graphics/russo_one_23.qff.h"
#include "graphics/russo_one_20.qff.h"
#include "graphics/russo_one_14.qff.h"
#include "graphics/russo_one_52_digits.qff.h"
#include "os_detection.h"
#include "graphics/logo_apple.qgf.h"
#include "graphics/logo_windows.qgf.h"
#include "graphics/logo_tux.qgf.h"
#include "graphics/luna_sit_0.qgf.h"
#include "graphics/luna_sit_1.qgf.h"
#include "graphics/luna_walk_0.qgf.h"
#include "graphics/luna_walk_1.qgf.h"
#include "graphics/luna_run_0.qgf.h"
#include "graphics/luna_run_1.qgf.h"
#include "graphics/luna_bark_0.qgf.h"
#include "graphics/luna_bark_1.qgf.h"
#include "graphics/luna_sneak_0.qgf.h"
#include "graphics/luna_sneak_1.qgf.h"

// Colours are QP HSV with every channel 0-255: a synthwave palette of neon on deep indigo
typedef struct {
    uint8_t h, s, v;
} bl_color_t;

#define HSV(c) (c).h, (c).s, (c).v

static const bl_color_t BG         = {185, 240, 33};   // #0d0221 deep indigo background
static const bl_color_t WHITE      = {0, 0, 255};
static const bl_color_t MUTED      = {193, 175, 134};  // #5b2a86 dusky purple, for things that are off
static const bl_color_t HOT_PINK   = {228, 255, 255};  // #ff00a0
static const bl_color_t LIGHT_PINK = {212, 138, 254};  // #fe75fe
static const bl_color_t CYAN       = {140, 255, 254};  // #00b3fe
static const bl_color_t ACID       = {50, 184, 254};   // #defe47 acid yellow
static const bl_color_t VIOLET     = {192, 251, 235};  // #7a04eb

// Russo One in several sizes, with where the capitals sit in each line (measured from the converted fonts),
// so text can be centred on the capitals rather than on the whole line.
// The fonts are made from graphics/fonts/RussoOne-Regular.ttf in QMK MSYS, from the graphics folder:
//   qmk painter-make-font-image -f fonts/RussoOne-Regular.ttf -s 35 -o russo_one_35.png
//   qmk painter-convert-font-image -f mono4 -i russo_one_35.png
// (and the same for 23, 20 and 14; the 52px one adds "-n -u 0123456789" to both commands for digits only)
typedef struct {
    painter_font_handle_t font;
    uint8_t               cap_top;
    uint8_t               cap_height;
} bl_font_t;

static bl_font_t              font_bar  = {.cap_top = 4, .cap_height = 25}; // 35px: layer bar
static bl_font_t              font_mod  = {.cap_top = 3, .cap_height = 17}; // 23px: modifiers, "WPM"
static bl_font_t              font_name = {.cap_top = 2, .cap_height = 16}; // 20px: lighting effect name
static bl_font_t              font_lbl  = {.cap_top = 2, .cap_height = 10}; // 14px: lighting labels
static bl_font_t              font_num  = {.cap_top = 0, .cap_height = 38}; // 52px, digits only: WPM
static painter_image_handle_t logo_apple;
static painter_image_handle_t logo_windows;
static painter_image_handle_t logo_tux;

bool module_post_init_user(void) {
    font_bar.font = qp_load_font_mem(font_russo_one_35);
    font_mod.font = qp_load_font_mem(font_russo_one_23);
    font_name.font = qp_load_font_mem(font_russo_one_20);
    font_lbl.font  = qp_load_font_mem(font_russo_one_14);
    font_num.font  = qp_load_font_mem(font_russo_one_52_digits);
    logo_apple   = qp_load_image_mem(gfx_logo_apple);
    logo_windows = qp_load_image_mem(gfx_logo_windows);
    logo_tux     = qp_load_image_mem(gfx_logo_tux);
    return true;
}

//// Drawing helpers

// Draws text centred in a box, with the capitals centred vertically
static void draw_text_centered(const bl_font_t *font, int16_t left, int16_t top, int16_t width, int16_t height, const char *text, bl_color_t fg, bl_color_t bg) {
    int16_t x = left + (width - qp_textwidth(font->font, text)) / 2;
    int16_t y = top + (height - font->cap_height) / 2 - font->cap_top;
    qp_drawtext_recolor(lcd_surface, x, y, font->font, text, HSV(fg), HSV(bg));
}

static void fill_clipped(int16_t left, int16_t top, int16_t right, int16_t bottom, bl_color_t color) {
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > LCD_WIDTH - 1) right = LCD_WIDTH - 1;
    if (bottom > LCD_HEIGHT - 1) bottom = LCD_HEIGHT - 1;
    if (left <= right && top <= bottom) {
        qp_rect(lcd_surface, left, top, right, bottom, HSV(color), true);
    }
}

//// Status display

static const char *layer_names[] = {NULL, "Fun", "RGB"};

#define LAYER_TOP 4
#define LAYER_HEIGHT 40 // tall enough to hold the whole 37px line of the bar font
#define MOD_GAP 5
#define MOD_HEIGHT 36

static int16_t layer_bar_bottom(void) {
    return LAYER_TOP + LAYER_HEIGHT;
}

static int16_t mods_top(void) {
    return LCD_HEIGHT - 2 * (MOD_GAP + MOD_HEIGHT);
}

static void draw_layer(uint8_t layer, bool caps) {
    fill_clipped(0, 0, LCD_WIDTH - 1, layer_bar_bottom(), BG);

    char        number[4];
    const char *name  = number;
    bl_color_t  color = WHITE;
    if (layer < ARRAY_SIZE(layer_names) && layer_names[layer] == NULL) {
        if (!caps) {
            return; // base layer: show nothing
        }
        name  = "CAPS"; // only shown on the base layer, other layer names take priority
        color = ACID;
    } else if (layer < ARRAY_SIZE(layer_names)) {
        name  = layer_names[layer];
        color = layer == 1 ? HOT_PINK : CYAN;
    } else {
        snprintf(number, sizeof(number), "%d", (int)layer);
    }

    // Dark text on a solid neon bar, so the active layer is obvious at a glance
    fill_clipped(4, LAYER_TOP, LCD_WIDTH - 5, LAYER_TOP + LAYER_HEIGHT - 1, color);
    draw_text_centered(&font_bar, 4, LAYER_TOP, LCD_WIDTH - 8, LAYER_HEIGHT, name, BG, color);
}

typedef enum { OS_LOGO_WINDOWS, OS_LOGO_APPLE, OS_LOGO_TUX } os_logo_t;

// Linux uses PC mode, so it only changes the picture
static os_logo_t current_os_logo(void) {
    if (is_mac_mode()) {
        return OS_LOGO_APPLE;
    }
    return detected_host_os() == OS_LINUX ? OS_LOGO_TUX : OS_LOGO_WINDOWS;
}

// OS logo, centred between the layer bar and the modifiers
static void draw_os(os_logo_t logo, int16_t top, int16_t bottom) {
    fill_clipped(0, top, LCD_WIDTH - 1, bottom, BG);

    painter_image_handle_t image = logo == OS_LOGO_APPLE ? logo_apple : logo == OS_LOGO_TUX ? logo_tux : logo_windows;
    int16_t                x     = (LCD_WIDTH - image->width) / 2;
    int16_t                y     = top + (bottom - top + 1 - image->height) / 2;
    qp_drawimage(lcd_surface, x, y, image);
}

// On: solid neon with contrasting text. Off: muted purple outline and text.
static void draw_mod_pill(int16_t left, int16_t top, int16_t width, int16_t height, const char *label, bool on, bl_color_t color, bl_color_t text) {
    if (on) {
        fill_clipped(left, top, left + width - 1, top + height - 1, color);
        draw_text_centered(&font_mod, left, top, width, height, label, text, color);
    } else {
        fill_clipped(left, top, left + width - 1, top + height - 1, BG);
        qp_rect(lcd_surface, left, top, left + width - 1, top + height - 1, HSV(MUTED), false);
        draw_text_centered(&font_mod, left, top, width, height, label, MUTED, BG);
    }
}

static void draw_mods(uint8_t mods, bool mac) {
    int16_t height = MOD_HEIGHT;
    int16_t width  = (LCD_WIDTH - 3 * MOD_GAP) / 2;
    int16_t col1 = MOD_GAP, col2 = MOD_GAP * 2 + width;
    int16_t row2 = LCD_HEIGHT - MOD_GAP - height, row1 = row2 - MOD_GAP - height;

    draw_mod_pill(col1, row1, width, height, "SHF", mods & MOD_MASK_SHIFT, LIGHT_PINK, BG);
    draw_mod_pill(col2, row1, width, height, "CTL", mods & MOD_MASK_CTRL, ACID, BG);
    draw_mod_pill(col1, row2, width, height, mac ? "OPT" : "ALT", mods & MOD_MASK_ALT, CYAN, BG);
    draw_mod_pill(col2, row2, width, height, mac ? "CMD" : "WIN", mods & MOD_MASK_GUI, VIOLET, WHITE);
}

static bool status_redraw = true;

static void update_status_display(void) {
    static uint8_t   last_layer;
    static bool      last_caps;
    static bool      last_mac;
    static os_logo_t last_logo;
    static uint8_t   last_mods;

    uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    bool    mac   = is_mac_mode();
    uint8_t   mods  = get_mods() | get_oneshot_mods();
    os_logo_t logo  = current_os_logo();

    bool caps = host_keyboard_led_state().caps_lock;
    if (status_redraw || layer != last_layer || caps != last_caps) {
        draw_layer(layer, caps);
    }
    if (status_redraw || logo != last_logo) {
        draw_os(logo, layer_bar_bottom() + 1, mods_top() - 1);
    }
    if (status_redraw || mac != last_mac || mods != last_mods) {
        draw_mods(mods, mac);
    }

    last_layer    = layer;
    last_caps     = caps;
    last_mac      = mac;
    last_logo     = logo;
    last_mods     = mods;
    status_redraw = false;
}

//// Second display: typing stats, or lighting info on the RGB layer, with neon Luna at the bottom

#define RGB_LAYER 2

// Luna: pre-rendered neon images, 108x78 including the glow
#define LUNA_WIDTH 108
#define LUNA_HEIGHT 78
#define LUNA_X ((LCD_WIDTH - LUNA_WIDTH) / 2)
#define LUNA_Y (LCD_HEIGHT - LUNA_HEIGHT - 2)
#define LUNA_JUMP 12
#define LUNA_AREA_TOP (LUNA_Y - LUNA_JUMP) // everything above this belongs to the stats or lighting info
#define LUNA_FRAME_MS 300
#define MIN_WALK_SPEED 10
#define MIN_RUN_SPEED 40

enum { LUNA_SIT, LUNA_WALK, LUNA_RUN, LUNA_BARK, LUNA_SNEAK };

static const uint8_t *const luna_images[][2] = {
    [LUNA_SIT]   = {gfx_luna_sit_0, gfx_luna_sit_1},
    [LUNA_WALK]  = {gfx_luna_walk_0, gfx_luna_walk_1},
    [LUNA_RUN]   = {gfx_luna_run_0, gfx_luna_run_1},
    [LUNA_BARK]  = {gfx_luna_bark_0, gfx_luna_bark_1},
    [LUNA_SNEAK] = {gfx_luna_sneak_0, gfx_luna_sneak_1},
};

typedef struct {
    uint8_t sprite;
    uint8_t frame;
    bool    jumping;
} luna_pose_t;

static luna_pose_t luna_pose(uint8_t frame) {
    uint8_t     mods = get_mods();
    uint8_t     wpm  = get_current_wpm();
    luna_pose_t pose = {.frame = frame, .jumping = bl_state.space_held};

    if (host_keyboard_led_state().caps_lock || (mods & MOD_MASK_SHIFT)) {
        pose.sprite = LUNA_BARK;
    } else if (mods & (MOD_MASK_CTRL | MOD_MASK_GUI)) {
        pose.sprite = LUNA_SNEAK;
    } else if (wpm <= MIN_WALK_SPEED) {
        pose.sprite = LUNA_SIT;
    } else if (wpm <= MIN_RUN_SPEED) {
        pose.sprite = LUNA_WALK;
    } else {
        pose.sprite = LUNA_RUN;
    }
    return pose;
}

// Each image includes the background around Luna, so it covers the previous frame by itself.
// Her area only needs clearing when she jumps or lands.
static void luna_draw(luna_pose_t pose, bool clear) {
    if (clear) {
        fill_clipped(0, LUNA_AREA_TOP, LCD_WIDTH - 1, LCD_HEIGHT - 1, BG);
    }
    painter_image_handle_t image = qp_load_image_mem(luna_images[pose.sprite][pose.frame]);
    if (image) {
        qp_drawimage(lcd_surface, LUNA_X, LUNA_Y - (pose.jumping ? LUNA_JUMP : 0), image);
        qp_close_image(image);
    }
}

// Typing stats: current WPM and a graph of the last minute

#define WPM_BASELINE 56
#define GRAPH_LEFT 4
#define GRAPH_TOP 66
#define GRAPH_RIGHT (LCD_WIDTH - 5)
#define GRAPH_BOTTOM 146
#define GRAPH_SAMPLES 60
#define GRAPH_SAMPLE_MS 1000
#define GRAPH_MIN_SCALE 60 // the graph's top is at least this many WPM

static const bl_color_t GRID       = {187, 205, 82};  // #2a1052
static const bl_color_t GRAPH_FILL = {187, 228, 94};  // #2c0a5e
static const bl_color_t GRAPH_GLOW = {161, 224, 255}; // #1f4dff
static const bl_color_t GRAPH_CORE = {139, 71, 255};  // #b8ecff

static uint8_t wpm_history[GRAPH_SAMPLES]; // oldest first

static void draw_wpm(uint8_t wpm) {
    fill_clipped(0, 0, LCD_WIDTH - 1, GRAPH_TOP - 1, BG);

    char number[4];
    snprintf(number, sizeof(number), "%u", (unsigned)wpm);
    int16_t number_width = qp_textwidth(font_num.font, number);
    int16_t x            = (LCD_WIDTH - (number_width + 6 + qp_textwidth(font_mod.font, "WPM"))) / 2;

    // The number and the label share a baseline
    qp_drawtext_recolor(lcd_surface, x, WPM_BASELINE - font_num.cap_top - font_num.cap_height, font_num.font, number, HSV(HOT_PINK), HSV(BG));
    qp_drawtext_recolor(lcd_surface, x + number_width + 6, WPM_BASELINE - font_mod.cap_top - font_mod.cap_height, font_mod.font, "WPM", HSV(LIGHT_PINK), HSV(BG));
}

static void vertical_line(int16_t x, int16_t top, int16_t bottom, bl_color_t color) {
    top    = MAX(top, GRAPH_TOP);
    bottom = MIN(bottom, GRAPH_BOTTOM);
    if (top <= bottom) {
        qp_line(lcd_surface, x, top, x, bottom, HSV(color));
    }
}

static void draw_graph(void) {
    fill_clipped(GRAPH_LEFT, GRAPH_TOP, GRAPH_RIGHT, GRAPH_BOTTOM, BG);
    for (int16_t y = GRAPH_TOP; y <= GRAPH_BOTTOM; y += 16) {
        qp_line(lcd_surface, GRAPH_LEFT, y, GRAPH_RIGHT, y, HSV(GRID));
    }
    for (int16_t x = GRAPH_LEFT; x <= GRAPH_RIGHT; x += 21) {
        qp_line(lcd_surface, x, GRAPH_TOP, x, GRAPH_BOTTOM, HSV(GRID));
    }

    uint8_t top = GRAPH_MIN_SCALE;
    for (int i = 0; i < GRAPH_SAMPLES; i++) {
        top = MAX(top, wpm_history[i]);
    }

    // One point per pixel column, interpolated between samples
    int16_t width = GRAPH_RIGHT - GRAPH_LEFT;
    int16_t prev_y = 0;
    for (int16_t col = 0; col <= width; col++) {
        int32_t pos   = (int32_t)col * (GRAPH_SAMPLES - 1) * 256 / width;
        int     i     = MIN(pos >> 8, GRAPH_SAMPLES - 2);
        int32_t frac  = pos - (i << 8);
        int32_t value = (wpm_history[i] * (256 - frac) + wpm_history[i + 1] * frac) >> 8;
        int16_t y     = GRAPH_BOTTOM - value * (GRAPH_BOTTOM - GRAPH_TOP - 4) / top;
        int16_t x     = GRAPH_LEFT + col;
        if (col == 0) prev_y = y;

        // Fill under the line, then a glowing line: wide blue glow, cyan tube and a bright core
        int16_t high = MIN(y, prev_y), low = MAX(y, prev_y);
        vertical_line(x, y + 2, GRAPH_BOTTOM, GRAPH_FILL);
        vertical_line(x, high - 2, low + 2, GRAPH_GLOW);
        vertical_line(x, high - 1, low + 1, CYAN);
        vertical_line(x, high, low, GRAPH_CORE);
        prev_y = y;
    }
}

// Lighting info for the RGB layer

typedef struct {
    bool    on;
    uint8_t mode;
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    uint8_t speed;
} rgb_info_t;

static rgb_info_t current_rgb_info(void) {
    return (rgb_info_t){rgb_matrix_is_enabled(), rgb_matrix_get_mode(), rgb_matrix_get_hue(), rgb_matrix_get_sat(), rgb_matrix_get_val(), rgb_matrix_get_speed()};
}

static bool rgb_info_equal(rgb_info_t a, rgb_info_t b) {
    return a.on == b.on && a.mode == b.mode && a.hue == b.hue && a.sat == b.sat && a.val == b.val && a.speed == b.speed;
}

// QMK's own names for the enabled effects, like "CYCLE_PINWHEEL", indexed by effect number
static const char *const effect_ids[RGB_MATRIX_EFFECT_MAX] = {
#define RGB_MATRIX_EFFECT(name, ...) [RGB_MATRIX_##name] = #name,
#include "rgb_matrix_effects.inc"
#undef RGB_MATRIX_EFFECT
};

// Shorter names for effects whose own name doesn't fit on two lines
static const struct {
    const char *id;
    const char *line1;
    const char *line2;
} effect_short_names[] = {
    {"BAND_PINWHEEL_SAT", "PINWHEEL", "BAND SAT"},
    {"BAND_PINWHEEL_VAL", "PINWHEEL", "BAND VAL"},
    {"BAND_SPIRAL_SAT", "SPIRAL", "BAND SAT"},
    {"BAND_SPIRAL_VAL", "SPIRAL", "BAND VAL"},
    {"CYCLE_OUT_IN_DUAL", "OUT IN", "DUAL"},
    {"MULTISPLASH", "MULTI", "SPLASH"},
    {"RAINBOW_MOVING_CHEVRON", "RAINBOW", "CHEVRON"},
    {"SOLID_MULTISPLASH", "SOLID", "M-SPLASH"},
    {"SOLID_REACTIVE_CROSS", "REACTIVE", "CROSS"},
    {"SOLID_REACTIVE_MULTICROSS", "REACTIVE", "M-CROSS"},
    {"SOLID_REACTIVE_MULTINEXUS", "REACTIVE", "M-NEXUS"},
    {"SOLID_REACTIVE_MULTIWIDE", "REACTIVE", "M-WIDE"},
    {"SOLID_REACTIVE_NEXUS", "REACTIVE", "NEXUS"},
    {"SOLID_REACTIVE_SIMPLE", "REACTIVE", "SIMPLE"},
    {"SOLID_REACTIVE_WIDE", "REACTIVE", "WIDE"},
};

#define NAME_WIDTH (LCD_WIDTH - 10)
#define NAME_LEN 32

// Splits an effect's name over at most two lines that fit the screen
static void effect_name_lines(uint8_t mode, char *line1, char *line2) {
    const char *id = mode < RGB_MATRIX_EFFECT_MAX ? effect_ids[mode] : NULL;
    line1[0] = line2[0] = '\0';
    if (!id) {
        snprintf(line1, NAME_LEN, "EFFECT %u", (unsigned)mode);
        return;
    }
    for (size_t i = 0; i < ARRAY_SIZE(effect_short_names); i++) {
        if (strcmp(id, effect_short_names[i].id) == 0) {
            strlcpy(line1, effect_short_names[i].line1, NAME_LEN);
            strlcpy(line2, effect_short_names[i].line2, NAME_LEN);
            return;
        }
    }

    char name[NAME_LEN];
    strlcpy(name, id, sizeof(name));
    for (char *c = name; *c; c++) {
        if (*c == '_') *c = ' ';
    }
    if (qp_textwidth(font_name.font, name) <= NAME_WIDTH) {
        strlcpy(line1, name, NAME_LEN);
        return;
    }
    // Break at the first space that makes both lines fit, or failing that the first space
    char *first_space = strchr(name, ' ');
    for (char *space = first_space; space; space = strchr(space + 1, ' ')) {
        *space = '\0';
        bool fits = qp_textwidth(font_name.font, name) <= NAME_WIDTH && qp_textwidth(font_name.font, space + 1) <= NAME_WIDTH;
        *space = ' ';
        if (fits) {
            first_space = space;
            break;
        }
    }
    if (first_space) {
        *first_space = '\0';
        strlcpy(line2, first_space + 1, NAME_LEN);
    }
    strlcpy(line1, name, NAME_LEN);
}

#define ROWS_TOP 62
#define ROW_STEP 22
#define ROW_HEIGHT 14
#define CONTROL_LEFT 64
#define CONTROL_RIGHT (LCD_WIDTH - 6)
#define SEGMENTS 10
#define SEGMENT_GAP 2
#define BADGE_HEIGHT 20 // taller than a row so the whole 16px line of the label font fits inside the badge

// A bar of 10 segments with the first `lit` filled in
static void draw_segments(int16_t top, uint8_t lit, bl_color_t color) {
    int16_t width = CONTROL_RIGHT - CONTROL_LEFT + 1;
    for (int i = 0; i < SEGMENTS; i++) {
        int16_t left  = CONTROL_LEFT + i * (width + SEGMENT_GAP) / SEGMENTS;
        int16_t right = CONTROL_LEFT + (i + 1) * (width + SEGMENT_GAP) / SEGMENTS - SEGMENT_GAP - 1;
        if (i < lit) {
            fill_clipped(left, top, right, top + ROW_HEIGHT - 1, color);
        } else {
            qp_rect(lcd_surface, left, top, right, top + ROW_HEIGHT - 1, HSV(MUTED), false);
        }
    }
}

static uint8_t segments_lit(uint8_t value, uint8_t max) {
    return MIN(SEGMENTS, ((uint16_t)value * SEGMENTS + max / 2) / max);
}

static void draw_rgb_info(rgb_info_t info) {
    fill_clipped(0, 0, LCD_WIDTH - 1, LUNA_AREA_TOP - 1, BG);

    // Effect name, one or two lines
    char line1[NAME_LEN], line2[NAME_LEN];
    effect_name_lines(info.mode, line1, line2);
    bl_color_t name_color = info.on ? WHITE : MUTED;
    if (line2[0]) {
        draw_text_centered(&font_name, 0, 4, LCD_WIDTH, 26, line1, name_color, BG);
        draw_text_centered(&font_name, 0, 30, LCD_WIDTH, 26, line2, name_color, BG);
    } else {
        draw_text_centered(&font_name, 0, 4, LCD_WIDTH, 52, line1, name_color, BG);
    }

    // Settings: a label on the left and its control on the right of each row
    static const char *const labels[] = {"POWER", "COLOR", "BRIGHT", "SPEED"};
    bl_color_t               label_color = info.on ? LIGHT_PINK : MUTED;
    for (int row = 0; row < 4; row++) {
        int16_t top = ROWS_TOP + row * ROW_STEP;
        qp_drawtext_recolor(lcd_surface, 5, top + (ROW_HEIGHT - font_lbl.cap_height) / 2 - font_lbl.cap_top, font_lbl.font, labels[row], HSV(label_color), HSV(BG));
    }

    int16_t    power_top = ROWS_TOP - (BADGE_HEIGHT - ROW_HEIGHT) / 2;
    bl_color_t swatch    = {info.hue, info.sat, 255}; // at full brightness, brightness has its own bar
    if (info.on) {
        fill_clipped(CONTROL_LEFT, power_top, CONTROL_LEFT + 40, power_top + BADGE_HEIGHT - 1, ACID);
        draw_text_centered(&font_lbl, CONTROL_LEFT, power_top, 41, BADGE_HEIGHT, "ON", BG, ACID);
        fill_clipped(CONTROL_LEFT, ROWS_TOP + ROW_STEP, CONTROL_RIGHT, ROWS_TOP + ROW_STEP + ROW_HEIGHT - 1, swatch);
    } else {
        qp_rect(lcd_surface, CONTROL_LEFT, power_top, CONTROL_LEFT + 40, power_top + BADGE_HEIGHT - 1, HSV(MUTED), false);
        draw_text_centered(&font_lbl, CONTROL_LEFT, power_top, 41, BADGE_HEIGHT, "OFF", MUTED, BG);
        qp_rect(lcd_surface, CONTROL_LEFT, ROWS_TOP + ROW_STEP, CONTROL_RIGHT, ROWS_TOP + ROW_STEP + ROW_HEIGHT - 1, HSV(MUTED), false);
    }
    draw_segments(ROWS_TOP + 2 * ROW_STEP, segments_lit(info.val, RGB_MATRIX_MAXIMUM_BRIGHTNESS), info.on ? CYAN : MUTED);
    draw_segments(ROWS_TOP + 3 * ROW_STEP, segments_lit(info.speed, 255), info.on ? ACID : MUTED);
}

static bool second_redraw = true;

// Drawing blocks the key scan on this half, so do at most one drawing job per pass,
// with a gap between jobs for the keys to be scanned
#define SECOND_JOB_GAP_MS 25

static void update_second_display(void) {
    static uint32_t    last_job          = 0;
    static uint32_t    last_sample       = 0;
    static uint32_t    last_frame_change = 0;
    static uint8_t     frame             = 0;
    static bool        last_rgb_screen;
    static uint8_t     last_wpm;
    static rgb_info_t  last_rgb;
    static luna_pose_t last_pose;
    static bool        luna_pending, wpm_pending, graph_pending, rgb_pending;

    // Keep recording typing speed even while the lighting info is showing.
    // The number and graph are refreshed with each sample, once a second.
    if (timer_elapsed32(last_sample) >= GRAPH_SAMPLE_MS) {
        memmove(wpm_history, wpm_history + 1, GRAPH_SAMPLES - 1);
        wpm_history[GRAPH_SAMPLES - 1] = get_current_wpm();
        last_sample                    = timer_read32();
        wpm_pending                    = true;
        graph_pending                  = true;
    }
    if (timer_elapsed32(last_frame_change) >= LUNA_FRAME_MS) {
        frame ^= 1;
        last_frame_change = timer_read32();
    }

    if (!second_redraw && timer_elapsed32(last_job) < SECOND_JOB_GAP_MS) {
        return;
    }

    // Switching screens: clear the top, then fill it in over the next passes
    bool rgb_screen = get_highest_layer(layer_state | default_layer_state) == RGB_LAYER;
    if (second_redraw || rgb_screen != last_rgb_screen) {
        fill_clipped(0, 0, LCD_WIDTH - 1, LUNA_AREA_TOP - 1, BG);
        luna_pending    = luna_pending || second_redraw;
        wpm_pending     = true;
        graph_pending   = true;
        rgb_pending     = true;
        last_wpm        = 0xFF;
        last_rgb_screen = rgb_screen;
        second_redraw   = false;
        last_job        = timer_read32();
        return;
    }

    // Luna first, as she's what moves
    luna_pose_t pose = luna_pose(frame);
    if (luna_pending || pose.sprite != last_pose.sprite || pose.frame != last_pose.frame || pose.jumping != last_pose.jumping) {
        luna_draw(pose, luna_pending || pose.jumping != last_pose.jumping);
        last_pose    = pose;
        luna_pending = false;
        last_job     = timer_read32();
        return;
    }

    if (rgb_screen) {
        rgb_info_t info = current_rgb_info();
        if (rgb_pending || !rgb_info_equal(info, last_rgb)) {
            draw_rgb_info(info);
            last_rgb    = info;
            rgb_pending = false;
            last_job    = timer_read32();
        }
        return;
    }

    if (wpm_pending) {
        wpm_pending = false;
        uint8_t wpm = wpm_history[GRAPH_SAMPLES - 1];
        if (wpm != last_wpm) {
            draw_wpm(wpm);
            last_wpm = wpm;
            last_job = timer_read32();
            return;
        }
    }
    if (graph_pending) {
        draw_graph();
        graph_pending = false;
        last_job      = timer_read32();
    }
}

//// Hook from hlc_tft_display.c, replaces its default drawing

bool display_module_housekeeping_task_user(bool second_display) {
    // The other half can take a moment to learn it is the second display,
    // so start from a blank screen whenever the role changes
    static int8_t last_role = -1;
    if (last_role != second_display) {
        fill_clipped(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, BG);
        status_redraw = true;
        second_redraw = true;
        last_role     = second_display;
    }

    if (second_display) {
        update_second_display();
    } else {
        update_status_display();
    }

    qp_surface_draw(lcd_surface, lcd, 0, 0, 0);
    qp_flush(lcd);
    return false;
}
