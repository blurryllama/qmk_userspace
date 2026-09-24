# blurryllama: Halcyon Lily58 with two TFT displays

A Vial keymap for the splitkb Halcyon Lily58 (rev2) with a [Halcyon TFT display module](https://splitkb.com/products/halcyon-tft-lcd-display-module) on **both** halves, in a synthwave/cyberpunk style.

## What it does

**Mac/PC/Linux detection.** The keyboard detects the computer's OS when it's plugged in and sets itself up to match. Mac mode swaps Ctrl and Cmd. Anything that isn't Windows, macOS or iOS is reported as Linux, including Android, SteamOS, Raspberry Pi and consoles, and uses PC mode.

**OS-aware shortcut keys.** These are Vial custom keys, found in Vial's **User** tab, that send the right shortcut for the detected OS:

| Key | PC | Mac | Linux |
|---|---|---|---|
| Insp | Ctrl+Shift+I | Cmd+Opt+I | Ctrl+Shift+I |
| Shot | Print Screen | Cmd+Shift+5 | Print Screen |
| Lock | Win+L | Ctrl+Cmd+Q | Lock-screen media key |
| Kill | Alt+F4 | Cmd+Opt+Esc | Alt+F4 |
| Calc | Calculator key | Opens Calculator via Spotlight | Calculator key |
| VolMx | Ctrl+Alt+V | Opt+Volume Up (Sound settings) | nothing |
| WinX | Win+X | nothing | nothing |

**Status display** (the half plugged into USB):
- The layer name on a neon bar ("Fun", "RGB"), or "CAPS" on the base layer while caps lock is on.
- A neon Apple, Windows or Tux logo.
- Which modifiers are held (SHF, CTL, ALT/OPT, WIN/CMD/SUP).

**Second display** (the other half):
- Typing speed and a graph of the last minute. On the RGB layer it switches to the lighting effect, colour, brightness, speed and on/off state.
- Luna the keyboard pet, redrawn in neon, at the bottom of both screens. She sits, walks or runs with your typing speed, barks on Shift or caps lock, sneaks on Ctrl or Cmd, and jumps on Space.

**Screen timeout.** The displays and backlight turn off after 10 minutes without typing on either half. splitkb's default is 2 minutes.

## Building

This builds with [vial-qmk](https://github.com/vial-kb/vial-qmk). Set up QMK as described in this repo's main README, then:

```bash
qmk compile -kb splitkb/halcyon/lily58/rev2 -km blurryllama -e HLC_TFT_DISPLAY=1 -e TARGET=lily58_blurryllama_display
```

GitHub Actions also builds it on every push; the result is in the repo's Releases.

**Flashing.** Flash the same `.uf2` to both halves. For each half, plug it in on its own, then either double-tap its reset button or hold BOOT while plugging it in. It appears as an `RPI-RP2` drive; copy the `.uf2` onto it. The halves send each other extra data, so they must run the same build.

**Vial layout.** Every new build resets your Vial layout to the default in `keymap.json`, because Vial gives each build a random ID. Save your layout in Vial first (File → Save current layout) and load it again after flashing.

## Customising

| To change | Where |
|---|---|
| Layout and default keys | `keymap.json`, or in Vial |
| Shortcut keys and what each OS sends | `os_shortcuts` table in `blurryllama.c`; the key names Vial shows are in `vial.json` under `customKeycodes` |
| Colours | The colour list at the top of `display.c`, with the hex value next to each |
| Layer names | `layer_names` in `display.c`. `RGB_LAYER` sets which layer shows the lighting info. |
| Shortened lighting effect names | `effect_short_names` in `display.c` |
| Screen timeout | `HLC_BACKLIGHT_TIMEOUT` in `config.h`, in milliseconds |
| Luna's animation speed | `LUNA_FRAME_MS` in `display.c` |
| Second display performance | `SECOND_JOB_GAP_MS` in `display.c`: the gap between drawing jobs, which leaves time to scan keys on that half. Raise it if keys get dropped. |

**Images and fonts** are stored as C files in `graphics/`, made with QMK's painter tools in QMK MSYS. Run these from the `graphics/` folder:

- **Logos:** `python3 prepare_logos.py <apple image> <windows image> <tux image>`, then convert the PNGs as described at the top of `prepare_logos.py`. The Apple and Windows images should be white or coloured logos on a plain background, and the Tux image a PNG with transparency.
- **Luna:** `python3 prepare_luna.py`, then convert the PNGs as described at the top of the script. Her original sprites are in `luna_sprites.h`.
- **Fonts:** see the comment above the font definitions in `display.c`.

**Notes for further changes:**
- Luna's images use a 256-colour palette, which needs `QUANTUM_PAINTER_SUPPORTS_256_PALETTE` in `config.h`. Without it, drawing them corrupts memory.
- `config.h` allows 6 fonts to be loaded; raise `QUANTUM_PAINTER_NUM_FONTS` if you add more.
- The second display's drawing blocks key scanning on that half, which is why it does one small job per pass. Keep new drawing small, or split it into steps.

## Credits

- Luna the keyboard pet is by HellSingCoder.
- Russo One by Jovanny Lemonad, under the SIL Open Font License (`graphics/fonts/OFL.txt`).
- Apple, Windows and the Tux mascot are trademarks or logos of their owners, and are used here only to show which OS is connected.
- Based on splitkb's Halcyon userspace and display module.
