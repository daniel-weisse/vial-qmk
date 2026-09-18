// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rev1.h"
#include "spi_master.h"
#include "myriad.h"

//// HW init

// Make sure all external hardware is
// in a known-good state on powerup
void keyboard_pre_init_kb(void) {
    /// SPI Chip Select pins for various hardware
    // Matrix CS
    gpio_set_pin_output(GP13);
    gpio_write_pin_high(GP13);
    // Myriad Module CS
    gpio_set_pin_output(GP9);
    gpio_write_pin_high(GP9);

    gpio_set_pin_output(ELORA_CC1_PIN);
    gpio_write_pin_low(ELORA_CC1_PIN);

    gpio_set_pin_output(ELORA_CC2_PIN);
    gpio_write_pin_low(ELORA_CC2_PIN);

    // We have to get the SPI interface working quite early,
    // So make sure it is available well before we need it
    spi_init();
    
    keyboard_pre_init_user();
}

//// Encoder functionality

// The encoders are hooked in to the same shift registers as the switch matrix, so we can just piggyback on that.

// Clone of a variant in quantum/matrix_common.c, but matrix-agnostic
bool mat_is_on(matrix_row_t mat[], uint8_t row, uint8_t col) {
    return (mat[row] & ((matrix_row_t)1 << col));
}

uint8_t encoder_read_pads_from(uint8_t index, bool pad_b, matrix_row_t mat[]) {
    // First two matrix rows:
    //
    // Pin  A   B   C   D   E   F   G   H
    // Left: 
    //   { __, __, __, __, __, __, A1, B1 },
    //   { A3, B3, A2, B2, __, __, __, __ }
    // Right:
    //   { A1, B1, __, __, __, __, __, __ },
    //   { __, __, __, __, A2, B2, A3, B3 }
    //
    // See also rev1.h

    bool pad_value = false;

    if (is_keyboard_left()) {
        if (index == 0) {
            pad_value = pad_b ? mat_is_on(mat, 0, 7) : mat_is_on(mat, 0, 6); // B1, A1 
        } else if (index == 1) {
            pad_value = pad_b ? mat_is_on(mat, 1, 3) : mat_is_on(mat, 1, 2); // B2, A2
        } else if (index == 2) {
            pad_value = pad_b ? mat_is_on(mat, 1, 1) : mat_is_on(mat, 1, 0); // B3, A3
        }
    } else {
        if (index == 0) {
            pad_value = pad_b ? mat_is_on(mat, 0, 1) : mat_is_on(mat, 0, 0); // B1, A1
        } else if (index == 1) {
            pad_value = pad_b ? mat_is_on(mat, 1, 5) : mat_is_on(mat, 1, 4); // B2, A2
        } else if (index == 2) {
            pad_value = pad_b ? mat_is_on(mat, 1, 7) : mat_is_on(mat, 1, 6); // B3, A3
        }
    }

    return pad_value ? 1 : 0;
}

void encoder_quadrature_init_pin(uint8_t index, bool pad_b) {
    // At this point the first matrix scan hasn't happened yet,
    // so we can't use raw_matrix to initialize our encoder state
    // as it contains all zeroes - so we have to do our own first scan
    // The pins for myriad are initialized in myriad.c

    matrix_row_t mat[MATRIX_ROWS];

    encoder_read_pads_from(index, pad_b, mat);
}

extern matrix_row_t raw_matrix[MATRIX_ROWS]; // From quantum/matrix_common.c

uint8_t encoder_quadrature_read_pin(uint8_t index, bool pad_b) {
    // The matrix code already keeps the raw matrix up-to-date,
    // so we only have to read the values from it
    if(index <= 2) {
        return encoder_read_pads_from(index, pad_b, raw_matrix);
    } else {
        #ifdef MYRIAD_ENABLE
            return myriad_hook_encoder(index, pad_b);
        #endif
        return 0;
    }
    return 0;
}

//// Default functionality

#ifdef OLED_ENABLE
oled_rotation_t oled_init_kb(oled_rotation_t rotation) {
    if (is_keyboard_left()) {
        return OLED_ROTATION_270;
    } else {
        return OLED_ROTATION_90;
    }
}

static void elora_oled_render_content(void) {
    if (!oled_task_user()) {
        return;
    }

    if (is_keyboard_master()) {
        oled_write_P(PSTR("Elora rev1\n\n"), false);

        // Keyboard Layer Status
        // Ideally we'd print the layer name, but no way to know that for sure
        // Fallback option: just print the layer number
        uint8_t layer = get_highest_layer(layer_state | default_layer_state);
        oled_write_P(PSTR("Layer: "), false);
        oled_write(get_u8_str(layer, ' '), false);

        // Keyboard LED Status
        led_t led_state = host_keyboard_led_state();
        oled_write_P(led_state.num_lock ? PSTR(" NUM") : PSTR("    "), false);
        oled_write_P(led_state.caps_lock ? PSTR("CAP") : PSTR("   "), false);
        oled_write_P(led_state.scroll_lock ? PSTR("SCR") : PSTR("   "), false);

        // QMK Logo
        // clang-format off
        static const char PROGMEM qmk_logo[] = {
            0x81,0x82,0x83,0x84,0x0a,
            0xa1,0xa2,0xa3,0xa4,0x85,0x86,0x87,0x88,0x89,0x0a,
            0xc1,0xc2,0xc3,0xc4,0xa5,0xa6,0xa7,0xa8,0xa9,0x0a,
            0x8a,0x8b,0x8c,0x8d,0xc5,0xc6,0xc7,0xc8,0xc9,0x0a,
            0xaa,0xab,0xac,0xad,0xae,0xaf,0xb0,0xb1,0xb2,0xb3,0x00
        };
        // clang-format on
        oled_set_cursor(0, oled_max_lines()-5);
        oled_write_P(qmk_logo, false);
    } else {
        // Elora sigil
        // clang-format off
        static const char PROGMEM elora_logo[] = {
            0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,128,128,192,224,224,240,248,120, 56, 60,188,158,158,222,206,207,207,207,239,239,239,239,239,239,207,207,207,206,222,158,158,188, 60, 56,120,248,240,224,224,192,128,128,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,192,224,248,252,126, 31,143,199,227,243,249,124, 60, 30, 31, 15,  7,  7,  3,  3,  3,131,193,225,241,249,253,255,255,255,255,127, 63, 31, 15,  7,  7,  7,143,223,254,252,252,249,243,227,199,143, 31,126,252,248,224,192,  0,  0,  0,  0,  0,
            0,192,240,254,255, 63,  7,227,248,252,127, 31, 15,  3,  1,  0,  0,  0,128,192,224,240,248,252,254,255,255,255,127, 63, 31, 15,  7,  3,  1,128,192,224,240,248,252,254,255,255,255,255,127, 63, 31, 15,  7, 15, 31,255,252,248,227,  7, 63,255,254,240,192,  0,252,255,255,255,  1,224,255,255,255,  7,  0,  0,  0,  0,  0,  0,  0,  0, 31, 31, 31, 31, 31, 15,  7,  3,  1,  0,  0,  0,240,248,252,254,255,255,255,255,127, 63, 31, 15,  7,  3,  1,128,192,224,240,248,252,254,255,255,255,255,255,255,224,  1,255,255,255,252,
            63,255,255,255,128,  7,255,255,255,224,  0,  0,  0,  0,  0,  0,  0,  0,  0,128,192,224,240,248,248,248,248,248,248,  0,  3,  3,  3,  3,  3,  3,  1,128,192,224,240,248,252,254,255,255,255,127, 63, 31, 15,  7,  3,  1,224,255,255,255,  7,128,255,255,255, 63,  0,  3, 15,127,255,252,224,199, 31, 63,254,248,240,192,128,  0,  0,  0,  0, 31, 31, 31, 31, 31, 31, 15,  7,  3,  1,  0,  0,  0,  0,  0,  0, 62, 63, 63, 63, 63, 63, 31, 15,  7,  3,  1,  0,  0,  0,128,192,240,248,254, 63, 31,199,224,252,255,127, 15,  3,  0, 
            0,  0,  0,  0,  0,  3,  7, 31, 63,126,248,241,227,199,207,159, 62, 60,120,248,240,224,224,192,192,192,192,128,128,128,128,128,128,128,128,128,128,192,192,192,192,224,224,240,248,120, 60, 62,159,207,199,227,241,248,126, 63, 31,  7,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  3,  7,  7, 15, 31, 30, 28, 60, 61,121,121,123,115,243,243,243,247,247,247,247,247,247,243,243,243,115,123,121,121, 61, 60, 28, 30, 31, 15,  7,  7,  3,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        };
        // clang-format on
        oled_set_cursor(0, (oled_max_lines()/2)-4); // logo is 8 lines high, so center vertically
        oled_write_raw_P(elora_logo, sizeof(elora_logo));
    }
}

bool oled_task_kb(void) {
    // Let the driver finish the current frame without blocking keyboard scanning.
    if (oled_is_dirty()) {
        return false;
    }

    // Keep the unmasked content so incremental user renderers can also wake intact.
    static uint8_t content[OLED_MATRIX_SIZE];
    static uint8_t previous_frame[OLED_MATRIX_SIZE];
    static bool awake               = true;
    static uint16_t reveal          = 256;
    static uint32_t frame_started;
    // Anchor the ordered dither to screen coordinates so the pattern does not flicker.
    static const uint8_t dither[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5},
    };

    const bool active              = last_input_activity_elapsed() < ELORA_OLED_TIMEOUT;
    const uint16_t previous_reveal = reveal;
    if (active != awake) {
        awake         = active;
        frame_started = timer_read32();
    }

    if (reveal != (awake ? 256 : 0)) {
        const uint32_t duration = awake ? ELORA_OLED_WAKE_DURATION : ELORA_OLED_SLEEP_DURATION;
        const uint32_t elapsed  = timer_elapsed32(frame_started);
        // Limit motion to about two pixels per completed frame, even if I2C stalls.
        const uint16_t step = elapsed >= duration ? 8 : MIN(8, elapsed * 256 / duration);
        if (step) {
            reveal = awake ? MIN(256, reveal + step) : reveal - MIN(reveal, step);
            frame_started = timer_read32();
        }
    }

    // Do not redraw while asleep: dirty OLED buffers automatically power the panel on.
    if (!awake && !reveal && !previous_reveal) {
        oled_off();
        return false;
    }

    uint8_t *buffer = oled_read_raw(0).current_element;
    memcpy(previous_frame, buffer, sizeof(previous_frame));
    memcpy(buffer, content, sizeof(content));
    elora_oled_render_content();
    memcpy(content, buffer, sizeof(content));
    memcpy(buffer, previous_frame, sizeof(previous_frame));

    // Both halves are rotated 90 degrees, so their logical width is the panel height.
    // Doubled coordinates put the circle exactly between the four central pixels.
    const uint16_t width          = OLED_DISPLAY_HEIGHT;
    const uint16_t height         = OLED_DISPLAY_WIDTH;
    const uint32_t corner_squared = (width - 1) * (width - 1) + (height - 1) * (height - 1);
    const uint32_t radius_squared = corner_squared * reveal * reveal / (256UL * 256);

    // Give the two-pixel bright wave a three-pixel dithered fade on each edge.
    // Radii are doubled, matching the pixel coordinates below.
    uint16_t radius = 0;
    if (reveal < 256) {
        while ((uint32_t)radius * radius < radius_squared) {
            ++radius;
        }
    }
    const uint16_t inner_radius       = radius > 4 ? radius - 4 : 0;
    const uint16_t fade_inner_radius  = inner_radius > 6 ? inner_radius - 6 : 0;
    const uint16_t fade_outer_radius  = radius + 6;
    const uint32_t inner_squared      = (uint32_t)inner_radius * inner_radius;
    const uint32_t fade_inner_squared = (uint32_t)fade_inner_radius * fade_inner_radius;
    const uint32_t fade_outer_squared = (uint32_t)fade_outer_radius * fade_outer_radius;

    for (uint16_t index = 0; index < OLED_MATRIX_SIZE; ++index) {
        uint8_t pixels = content[index];
        if (reveal < 256) {
            const uint16_t x      = index % width;
            const int16_t dx      = 2 * x - (width - 1);
            const uint16_t page_y = (index / width) * 8;
            for (uint8_t bit = 0; bit < 8; ++bit) {
                const uint16_t y                = page_y + bit;
                const int16_t dy                = 2 * y - (height - 1);
                const uint32_t distance_squared = dx * dx + dy * dy;
                if (distance_squared > radius_squared) {
                    pixels &= ~(1 << bit);
                }

                uint8_t brightness = 0;
                if (reveal && distance_squared >= inner_squared && distance_squared <= radius_squared) {
                    brightness = 16;
                } else if (reveal && distance_squared > fade_inner_squared && distance_squared < inner_squared) {
                    brightness = 16 * (distance_squared - fade_inner_squared) / (inner_squared - fade_inner_squared);
                } else if (reveal && distance_squared > radius_squared && distance_squared < fade_outer_squared) {
                    brightness = 16 * (fade_outer_squared - distance_squared) / (fade_outer_squared - radius_squared);
                }
                if (brightness > dither[y % 4][x % 4]) {
                    pixels |= 1 << bit;
                }
            }
        }
        oled_write_raw_byte(pixels, index);
    }

    // The next task powers down only after the final black frame has drained.
    oled_on();
    return false;
}
#endif

#ifdef ENCODER_ENABLE
bool encoder_update_kb(uint8_t index, bool clockwise) {
    if (!encoder_update_user(index, clockwise)) {
        return false;
    }

    if (index == 0 || index == 1 || index == 2) {
        // Left side
        // Arrow keys
        if (clockwise) {
            tap_code(KC_RIGHT);
        } else {
            tap_code(KC_LEFT);
        }
    } else if (index == 4 || index == 5 || index == 6) {
        // Right side
        // Page up/Page down
        if (clockwise) {
            tap_code(KC_PGDN);
        } else {
            tap_code(KC_PGUP);
        }
    } else if (index == 3 || index == 7) {
        // Myriad
        // Volume control
        if (clockwise) {
            tap_code(KC_VOLU);
        } else {
            tap_code(KC_VOLD);
        }
    }
    return true;
}
#endif
