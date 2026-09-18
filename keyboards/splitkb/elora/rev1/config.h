// Copyright 2024 splitkb.com (support@splitkb.com)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Make it easier to enter the bootloader
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_TIMEOUT 1000U

// I2C0, onboard SSD1306 socket and I2C to Myriad module
#define I2C_DRIVER   I2CD0
#define I2C1_SDA_PIN GP0
#define I2C1_SCL_PIN GP1
// We need to slow down the I2C clock because the default of 400.000
// fails to communicate with Zetta ZD24C02A EEPROM on a Myriad card.
#define I2C1_CLOCK_SPEED 100000

// SPI1, both for onboard matrix data and SPI to Myriad module
#define SPI_DRIVER   SPID1
#define SPI_SCK_PIN  GP10
#define SPI_MOSI_PIN GP11
#define SPI_MISO_PIN GP12

// UART1, communication between the two halves
#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_DRIVER SIOD1
#define SERIAL_USART_TX_PIN GP20
#define SERIAL_USART_RX_PIN GP21

// Potential onboard speaker, not populated by default
#define AUDIO_PIN GP23

// Transmitting pointing device status to the master side
#define SPLIT_POINTING_ENABLE
#define POINTING_DEVICE_COMBINED

// VBUS detection
#define USB_VBUS_PIN GP25

// Define matrix size
#define MATRIX_COLS 8
#define MATRIX_ROWS 12

// Encoders
// 3 onboard, 1 for Myriad
#define NUM_ENCODERS_LEFT 4
#define NUM_ENCODERS_RIGHT 4
#define ENCODER_RESOLUTION 2

// OLED display
#define OLED_DISPLAY_128X64
// Manage each panel locally so a display connected only to the secondary half works.
// Share input activity so both halves start their timeout animation together.
#undef SPLIT_OLED_ENABLE
#define SPLIT_ACTIVITY_ENABLE
#define OLED_TIMEOUT 0
// Poll often enough to advance as soon as the preceding frame has drained.
#define OLED_UPDATE_INTERVAL 20
// Saves I2C bandwidth at the cost of a 1 KB cache when using an SH1106.
#define OLED_SH1106_CACHE_ENABLE

#ifndef ELORA_OLED_TIMEOUT
#    define ELORA_OLED_TIMEOUT 60000
#endif
// Target durations in milliseconds. Slow transfers stretch the animation to
// preserve small wave steps and allow keyboard scans between OLED blocks.
#ifndef ELORA_OLED_SLEEP_DURATION
#    define ELORA_OLED_SLEEP_DURATION 1800
#endif
#ifndef ELORA_OLED_WAKE_DURATION
#    define ELORA_OLED_WAKE_DURATION 1200
#endif
