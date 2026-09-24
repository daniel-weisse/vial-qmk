// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Myriad boilerplate
#define MYRIAD_ENABLE

/// Vial-specific settings

// Increase the EEPROM size for layout options
#define VIA_EEPROM_LAYOUT_OPTIONS_SIZE 2

#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define RGB_MATRIX_KEYPRESSES

// Default is 2, needed because keymap has 7 layers
#define DYNAMIC_KEYMAP_LAYER_COUNT 8

#define VIAL_KEYBOARD_UID {0xB3, 0x8D, 0x94, 0xDA, 0xB3, 0xD7, 0xDC, 0x3D}

#define VIAL_UNLOCK_COMBO_ROWS {3, 9}
#define VIAL_UNLOCK_COMBO_COLS {2, 5}

// unicode Umlaut support
#define UNICODE_SELECTED_MODES UNICODE_MODE_LINUX, UNICODE_MODE_WINCOMPOSE

// OLED SH1106 support because I bought the wrong displays
#define OLED_IC OLED_IC_SH1106
#define OLED_COLUMN_OFFSET 2

#if defined(SECONDARY_SCREEN_MATRIX) && defined(OLED_ENABLE)
// Send a cumulative key-down count to the secondary OLED.
#    define SPLIT_TRANSACTION_IDS_USER MATRIX_SCREEN_SYNC
#endif
