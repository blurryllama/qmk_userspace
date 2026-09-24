VIA_ENABLE = yes
VIAL_ENABLE = yes
VIALRGB_ENABLE = yes

ENCODER_MAP_ENABLE = yes

# This adds module functionality to your keyboard (files found in users/halcyon_modules)
USER_NAME := halcyon_modules

SRC += halcyon_overrides.c

# blurryllama: Mac/PC detection and shortcuts
OS_DETECTION_ENABLE = yes
WPM_ENABLE = yes
SRC += blurryllama.c

ifdef HLC_TFT_DISPLAY
	SRC += display.c \
	       graphics/logo_apple.qgf.c \
	       graphics/logo_windows.qgf.c \
	       graphics/logo_tux.qgf.c \
	       graphics/russo_one_35.qff.c \
	       graphics/russo_one_23.qff.c \
	       graphics/russo_one_20.qff.c \
	       graphics/russo_one_14.qff.c \
	       graphics/russo_one_52_digits.qff.c \
	       graphics/luna_sit_0.qgf.c \
	       graphics/luna_sit_1.qgf.c \
	       graphics/luna_walk_0.qgf.c \
	       graphics/luna_walk_1.qgf.c \
	       graphics/luna_run_0.qgf.c \
	       graphics/luna_run_1.qgf.c \
	       graphics/luna_bark_0.qgf.c \
	       graphics/luna_bark_1.qgf.c \
	       graphics/luna_sneak_0.qgf.c \
	       graphics/luna_sneak_1.qgf.c
endif
