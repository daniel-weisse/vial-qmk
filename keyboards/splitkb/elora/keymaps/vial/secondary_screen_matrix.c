// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H

#ifdef OLED_ENABLE
#include "transactions.h"

// Both Elora panels are rotated into portrait orientation.
#define MATRIX_WIDTH OLED_DISPLAY_HEIGHT
#define MATRIX_HEIGHT OLED_DISPLAY_WIDTH
#define MATRIX_STREAM_COUNT 24
#define MATRIX_LANES 8
#define MATRIX_GLYPH_WIDTH 5
#define MATRIX_GLYPH_HEIGHT 7
#define MATRIX_TRAIL_LENGTH 12
#define MATRIX_TRAIL_WIDTH 2
#define MATRIX_SPRITE_HEIGHT (MATRIX_GLYPH_HEIGHT + MATRIX_TRAIL_LENGTH)

typedef struct {
    // Remaining travel in 1/256 pixels, including the trail leaving the top.
    uint16_t remaining;
    uint8_t lane;
    uint8_t glyph;
    uint8_t speed;
} matrix_stream_t;

static matrix_stream_t matrix_streams[MATRIX_STREAM_COUNT];
static uint8_t matrix_frame[OLED_MATRIX_SIZE];
static uint16_t matrix_random_state = 0xACE1;
static uint8_t matrix_next_stream;
static volatile uint32_t matrix_press_count;
static uint32_t matrix_rendered_presses;

// Original five-bit glyphs inspired by the mirrored katakana in Matrix code rain.
// The renderer mirrors these angular strokes horizontally, with no digit set.
static const uint8_t PROGMEM matrix_glyphs[][MATRIX_GLYPH_HEIGHT] = {
    {31, 1, 5, 6, 4, 4, 8},       // a
    {1, 2, 4, 12, 20, 4, 4},      // i
    {4, 4, 31, 17, 1, 2, 12},     // u
    {0, 31, 4, 4, 4, 4, 31},      // e
    {2, 31, 6, 10, 18, 2, 6},     // o
    {8, 31, 9, 9, 9, 17, 3},      // ka
    {4, 31, 4, 31, 4, 4, 2},      // ki
    {8, 15, 17, 1, 2, 4, 24},     // ku
    {8, 8, 15, 18, 2, 4, 8},      // ke
    {0, 31, 1, 1, 1, 1, 31},      // ko
    {10, 31, 10, 10, 2, 4, 8},    // sa
    {16, 9, 17, 1, 2, 4, 24},     // shi
    {0, 31, 1, 2, 4, 10, 17},     // su
    {8, 9, 31, 9, 10, 8, 7},      // se
    {17, 9, 9, 1, 2, 4, 24},      // so
    {8, 15, 17, 13, 2, 4, 24},    // ta
    {3, 28, 4, 31, 4, 8, 16},     // chi
    {0, 21, 21, 1, 2, 4, 24},     // tsu
    {31, 0, 31, 4, 4, 8, 16},     // te
    {8, 8, 8, 12, 10, 8, 8},      // to
    {4, 4, 31, 4, 4, 8, 16},      // na
    {0, 14, 0, 0, 0, 0, 31},      // ni
    {31, 1, 9, 6, 6, 9, 16},      // nu
    {4, 31, 2, 4, 14, 21, 4},     // ne
    {1, 1, 2, 2, 4, 8, 16},       // no
    {0, 10, 10, 10, 10, 17, 17},  // ha
    {16, 16, 19, 28, 16, 16, 15}, // hi
    {31, 1, 1, 2, 2, 4, 24},      // fu
    {0, 4, 10, 17, 1, 0, 0},      // he
    {4, 31, 4, 4, 21, 21, 4},     // ho
    {31, 1, 2, 4, 10, 4, 2},      // ma
    {24, 6, 0, 24, 6, 24, 6},     // mi
};

void matrix_scan_user(void) {
    static matrix_row_t previous[MATRIX_ROWS];
    if (!is_keyboard_master()) {
        return;
    }
    // The master has the debounced matrix for both halves before this hook.
    // Count every rising edge, including chords, without depending on OLED speed.
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        const matrix_row_t current = matrix_get_row(row);
        matrix_row_t pressed = current & ~previous[row];
        previous[row] = current;
        while (pressed) {
            ++matrix_press_count;
            pressed &= pressed - 1;
        }
    }
}

static void matrix_receive_presses(uint8_t length, const void *data, uint8_t output_length, void *output) {
    (void)output_length;
    (void)output;
    if (length == sizeof(uint32_t)) {
        uint32_t count;
        memcpy(&count, data, sizeof(count));
        matrix_press_count = count;
    }
}

void keyboard_post_init_user(void) {
    transaction_register_rpc(MATRIX_SCREEN_SYNC, matrix_receive_presses);
}

void housekeeping_task_user(void) {
    static uint32_t last_sync;
    static uint32_t last_sent = UINT32_MAX;
    if (!is_keyboard_master() || timer_elapsed32(last_sync) < 20) {
        return;
    }
    const uint32_t count = matrix_press_count;
    if (count != last_sent || timer_elapsed32(last_sync) >= 1000) {
        // Cumulative counts make retries safe and preserve rapid key presses.
        last_sync = timer_read32();
        if (transaction_rpc_send(MATRIX_SCREEN_SYNC, sizeof(count), &count)) {
            last_sent = count;
        }
    }
}

static uint16_t matrix_random(void) {
    matrix_random_state ^= matrix_random_state << 7;
    matrix_random_state ^= matrix_random_state >> 9;
    matrix_random_state ^= matrix_random_state << 8;
    return matrix_random_state;
}

static uint8_t matrix_random_lane(void) {
    // Gentle, symmetric bias: center columns get 10 shares, outer columns 7.
    static const uint8_t weights[MATRIX_LANES] = {7, 8, 9, 10, 10, 9, 8, 7};
    uint8_t choice = matrix_random() % 68;
    for (uint8_t lane = 0; lane < MATRIX_LANES; ++lane) {
        if (choice < weights[lane]) {
            return lane;
        }
        choice -= weights[lane];
    }
    return MATRIX_LANES - 1;
}

static void matrix_pixel(int16_t x, int16_t y, uint8_t brightness) {
    // Anchor the fade to screen pixels so overlapping trail samples do not flicker.
    static const uint8_t dither[4][4] = {
        {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5},
    };
    if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT && brightness > dither[y % 4][x % 4]) {
        matrix_frame[(y / 8) * MATRIX_WIDTH + x] |= 1 << (y % 8);
    }
}

static void matrix_draw_glyph(uint8_t lane, int16_t y, uint8_t glyph) {
    // Fixed columns and glyph sizes keep every stream moving straight upward.
    const int16_t x = (2 * lane + 1) * MATRIX_WIDTH / (2 * MATRIX_LANES) - MATRIX_GLYPH_WIDTH / 2;

    // A narrow streak follows the glyph without spreading across its full width.
    const int16_t trail_x = x + (MATRIX_GLYPH_WIDTH - MATRIX_TRAIL_WIDTH) / 2;
    for (uint8_t offset = 0; offset < MATRIX_TRAIL_LENGTH; ++offset) {
        const uint8_t brightness = 1 + 9 * (MATRIX_TRAIL_LENGTH - 1 - offset) / (MATRIX_TRAIL_LENGTH - 1);
        for (uint8_t column = 0; column < MATRIX_TRAIL_WIDTH; ++column) {
            matrix_pixel(trail_x + column, y + MATRIX_GLYPH_HEIGHT + offset, brightness);
        }
    }

    for (uint8_t row = 0; row < MATRIX_GLYPH_HEIGHT; ++row) {
        const uint8_t bits = pgm_read_byte(&matrix_glyphs[glyph][row]);
        for (uint8_t column = 0; column < MATRIX_GLYPH_WIDTH; ++column) {
            if (bits & (1 << column)) {
                matrix_pixel(x + column, y + row, 16);
            }
        }
    }
}

static void matrix_launch(void) {
    // Keep queued presses until a slot is free rather than replacing a live glyph.
    for (uint8_t index = 0; index < MATRIX_STREAM_COUNT; ++index) {
        matrix_stream_t *stream = &matrix_streams[matrix_next_stream];
        matrix_next_stream = (matrix_next_stream + 1) % MATRIX_STREAM_COUNT;
        if (stream->remaining) {
            continue;
        }
        stream->remaining = (MATRIX_HEIGHT + MATRIX_SPRITE_HEIGHT) * 256 - 1;
        stream->lane = matrix_random_lane();
        stream->glyph = matrix_random() % (sizeof(matrix_glyphs) / sizeof(matrix_glyphs[0]));
        stream->speed = 18 + matrix_random() % 13;
        ++matrix_rendered_presses;
        return;
    }
}

bool oled_task_user(void) {
    if (is_keyboard_master()) {
        return true;
    }

    static uint32_t last_frame;
    static bool rendered;
    const uint32_t frame_elapsed = timer_elapsed32(last_frame);
    // Bound movement after an I2C stall, just like the shared OLED transitions.
    const uint16_t elapsed = MIN(frame_elapsed, 64);
    last_frame = timer_read32();
    if (frame_elapsed > 1000) {
        memset(matrix_streams, 0, sizeof(matrix_streams));
        rendered = false;
    }

    const uint32_t pending = matrix_press_count - matrix_rendered_presses;
    // A restarted master can reset its counter while this half remains powered.
    if (pending > UINT16_MAX) {
        matrix_rendered_presses = matrix_press_count;
    }
    const bool input_changed = matrix_press_count != matrix_rendered_presses;
    bool changed = !rendered || input_changed;
    for (uint8_t index = 0; index < MATRIX_STREAM_COUNT; ++index) {
        matrix_stream_t *stream = &matrix_streams[index];
        if (stream->remaining) {
            const uint16_t step = elapsed * stream->speed;
            stream->remaining -= MIN(stream->remaining, step);
            changed = true;
        }
    }
    if (input_changed) {
        matrix_launch();
    }
    if (!changed) {
        return false;
    }
    rendered = true;

    memset(matrix_frame, 0, sizeof(matrix_frame));
    for (uint8_t index = 0; index < MATRIX_STREAM_COUNT; ++index) {
        const matrix_stream_t *stream = &matrix_streams[index];
        if (!stream->remaining) {
            continue;
        }
        const int16_t y = (stream->remaining >> 8) - MATRIX_SPRITE_HEIGHT;
        matrix_draw_glyph(stream->lane, y, stream->glyph);
    }
    for (uint16_t index = 0; index < sizeof(matrix_frame); ++index) {
        oled_write_raw_byte(matrix_frame[index], index);
    }
    return false;
}
#endif
