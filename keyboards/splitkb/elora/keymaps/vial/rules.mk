# Copyright 2024 splitkb.com (support@splitkb.com)
# SPDX-License-Identifier: GPL-2.0-or-later

## Vial-specific settings

VIA_ENABLE = yes
VIAL_ENABLE = yes
VIALRGB_ENABLE = yes
ENCODER_MAP_ENABLE = yes

# unicode Umlaut support
UNICODEMAP_ENABLE = yes

# Select the secondary OLED animation at build time.
SECONDARY_SCREEN_ANIMATION ?= matrix

ifeq ($(SECONDARY_SCREEN_ANIMATION),matrix)
    SRC += secondary_screen_matrix.c
    OPT_DEFS += -DSECONDARY_SCREEN_MATRIX
else ifeq ($(SECONDARY_SCREEN_ANIMATION),wave)
    SRC += secondary_screen_wave.c
else ifneq ($(SECONDARY_SCREEN_ANIMATION),none)
    $(error Unsupported SECONDARY_SCREEN_ANIMATION '$(SECONDARY_SCREEN_ANIMATION)'. Use matrix, wave or none)
endif
