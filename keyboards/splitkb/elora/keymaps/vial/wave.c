// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "lib/lib8tion/lib8tion.h"

#ifdef OLED_ENABLE
// Elora rotates both panels into portrait orientation.
#define WAVE_WIDTH OLED_DISPLAY_HEIGHT
#define WAVE_HEIGHT OLED_DISPLAY_WIDTH
#define WAVE_PEAK_SPACING 32
#define WAVE_TRAVEL ((WAVE_WIDTH / 2 - 6) * 256L)
#define WAVE_STEP_MS 4
#define WAVE_BUILDUP_MS 4000

static uint8_t wave_frame[OLED_MATRIX_SIZE];

// Position is in 1/256 pixels, velocity in 1/256 pixels per second.
static int32_t wave_displacement;
static int32_t wave_velocity;
static int32_t wave_activity;

static void wave_update_spring(uint16_t elapsed, bool input_changed, bool typing) {
    if (input_changed) {
        // Push in the current direction so typing adds momentum without teleporting.
        const int8_t direction = wave_velocity < 0 || (!wave_velocity && wave_displacement > 0) ? -1 : 1;
        const int32_t limit = WAVE_TRAVEL * (8 + 14 * wave_activity / 65536L);
        // Start with a small nudge and earn stronger pushes through sustained typing.
        const int32_t impulse = WAVE_TRAVEL * (128 + 1408 * wave_activity / 65536L) / 256;
        wave_velocity = MAX(-limit, MIN(limit, wave_velocity + direction * impulse));
    }

    // Small physics steps keep the spring stable even when OLED transfers are slow.
    while (elapsed) {
        const uint16_t step = MIN(elapsed, WAVE_STEP_MS);
        elapsed -= step;
        if (typing) {
            // Build speed over four seconds, independent of OLED frame rate.
            wave_activity = MIN(65536L, wave_activity + 65536L * step / WAVE_BUILDUP_MS);
        } else {
            wave_activity -= wave_activity * step / 400;
            if (wave_activity < 256) {
                wave_activity = 0;
            }
        }
        // About 3.5 swings/second while typing, easing down to 1.3 at rest.
        const int32_t frequency = 8 + 14 * wave_activity / 65536L;
        const int32_t acceleration = -frequency * frequency * wave_displacement - 4 * wave_velocity;
        wave_velocity += acceleration * step / 1000;
        wave_displacement += wave_velocity * step / 1000;

        // Leave room for the soft trace at either edge of the panel.
        if (wave_displacement > WAVE_TRAVEL || wave_displacement < -WAVE_TRAVEL) {
            wave_displacement = wave_displacement > 0 ? WAVE_TRAVEL : -WAVE_TRAVEL;
            wave_velocity = 0;
        }
    }

    // Stop subpixel motion completely so an idle OLED no longer needs redraws.
    if (!wave_activity && labs(wave_displacement) < 128 && labs(wave_velocity) < 1024) {
        wave_displacement = 0;
        wave_velocity = 0;
    }
}

// All lobes belong to one spring and swing through the baseline together.
static int16_t wave_position(uint8_t y) {
    const uint8_t peak = y / WAVE_PEAK_SPACING;
    const uint8_t angle = (y % WAVE_PEAK_SPACING) * 128 / WAVE_PEAK_SPACING;
    const int16_t shape = (int16_t)sin8(angle) - 128;
    const int16_t offset = shape * wave_displacement / (128 * 256L);
    return WAVE_WIDTH / 2 + (peak % 2 ? -offset : offset);
}

static void wave_dot(int16_t center_x, int16_t center_y) {
    // Fixed screen-space Bayer dithering gives the thick trace soft edges.
    static const uint8_t dither[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5},
    };
    for (int16_t dy = -3; dy <= 3; ++dy) {
        const int16_t y = center_y + dy;
        if (y < 0 || y >= WAVE_HEIGHT) {
            continue;
        }
        for (int16_t dx = -3; dx <= 3; ++dx) {
            const int16_t x = center_x + dx;
            if (x < 0 || x >= WAVE_WIDTH) {
                continue;
            }
            const uint8_t distance = dx * dx + dy * dy;
            const uint8_t brightness = distance <= 2 ? 16 : distance <= 4 ? 12 : distance <= 8 ? 8 : distance <= 10 ? 4 : 0;
            if (brightness > dither[y % 4][x % 4]) {
                wave_frame[(y / 8) * WAVE_WIDTH + x] |= 1 << (y % 8);
            }
        }
    }
}

bool oled_task_user(void) {
    if (is_keyboard_master()) {
        return true;
    }

    static uint32_t last_frame;
    static uint32_t last_activity;
    static bool rendered;
    const int32_t previous_displacement = wave_displacement;
    const uint32_t now = timer_read32();
    // rev1.c calls this only after the previous frame's dirty blocks drain.
    // Cap elapsed time to avoid a jump after sleep or an I2C stall.
    const uint32_t frame_elapsed = timer_elapsed32(last_frame);
    const uint16_t elapsed = MIN(frame_elapsed, 100);
    last_frame = now;

    if (frame_elapsed > 1000) {
        wave_displacement = 0;
        wave_velocity = 0;
        wave_activity = 0;
    }

    // SPLIT_ACTIVITY_ENABLE shares key activity from either half immediately.
    // Ignore old timestamps after waking, including the initial zero timestamp.
    const uint32_t activity = last_matrix_activity_time();
    const bool typing = activity && last_matrix_activity_elapsed() < 200;
    const bool input_changed = activity != last_activity && typing;
    last_activity = activity;
    wave_update_spring(elapsed, input_changed, typing);
    if (rendered && previous_displacement == wave_displacement) {
        return false;
    }
    rendered = true;

    memset(wave_frame, 0, sizeof(wave_frame));
    int16_t previous_x = wave_position(0);
    for (int16_t y = 0; y < WAVE_HEIGHT; ++y) {
        const int16_t x = wave_position(y);
        // Keep the thick trace connected while the ripple shakes it.
        const int16_t direction = x >= previous_x ? 1 : -1;
        while (previous_x != x) {
            wave_dot(previous_x, y);
            previous_x += direction;
        }
        wave_dot(x, y);
    }
    for (uint16_t index = 0; index < sizeof(wave_frame); ++index) {
        oled_write_raw_byte(wave_frame[index], index);
    }
    return false;
}
#endif
