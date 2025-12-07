/*
 * ============================================================================
 * TFT_eSPI CONFIGURATION - T-DECK PLUS
 * ============================================================================
 * 
 * VERIFIED AGAINST: Xinyuan-LilyGO/T-Deck commit 6adb888 (2024-07-26)
 * Source: https://wiki.lilygo.cc/get_started/en/Wearable/T-Deck-Plus/T-Deck-Plus.html
 * 
 * INSTALLATION:
 * Copy this file to your TFT_eSPI library folder, replacing User_Setup.h:
 * 
 *   Windows: C:\Users\<USER>\Documents\Arduino\libraries\TFT_eSPI\
 *   Linux:   ~/Arduino/libraries/TFT_eSPI/
 *   macOS:   ~/Documents/Arduino/libraries/TFT_eSPI/
 * 
 * CRITICAL: Restart Arduino IDE after copying.
 * 
 * MIT License
 * Copyright (c) 2025 clueless187-8
 * https://github.com/clueless187-8/T-Deck-Pitchcomm
 * 
 * ============================================================================
 */

// ============================================================================
// DRIVER SELECTION - VERIFIED ST7789 (2.8" IPS LCD 320x240)
// ============================================================================
#define ST7789_DRIVER

#define TFT_WIDTH   320
#define TFT_HEIGHT  240

// ============================================================================
// CRITICAL: T-DECK CUSTOM INITIALIZATION SEQUENCE
// ============================================================================
// LilyGO updated ST7789 init on 2024-07-26. This define selects the
// alternative gamma/voltage settings required for T-Deck's display panel.
// Without this, the display will not initialize correctly.
#define INIT_SEQUENCE_2

// ============================================================================
// ESP32-S3 PIN DEFINITIONS (T-DECK PLUS)
// Source: LILYGO Wiki utilities.h
// ============================================================================
#define TFT_MISO    38      // BOARD_SPI_MISO
#define TFT_MOSI    41      // BOARD_SPI_MOSI
#define TFT_SCLK    40      // BOARD_SPI_SCK
#define TFT_CS      12      // BOARD_TFT_CS
#define TFT_DC      11      // BOARD_TFT_DC
#define TFT_RST     -1      // Connected to EN (hardware reset via RST button)
#define TFT_BL      42      // BOARD_TFT_BACKLIGHT / BOARD_BL_PIN

// ============================================================================
// FONTS
// ============================================================================
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ============================================================================
// SPI CONFIGURATION
// ============================================================================
#define SPI_FREQUENCY       40000000    // 40 MHz (conservative for reliability)
#define SPI_READ_FREQUENCY  20000000    // 20 MHz read speed

// ============================================================================
// DISPLAY SETTINGS - VERIFIED FROM LILYGO COMMIT 6adb888
// ============================================================================
#define TFT_RGB_ORDER TFT_RGB           // Color order: Red-Green-Blue (NOT BGR)
// #define TFT_RGB_ORDER TFT_BGR        // DO NOT USE - causes color inversion
// #define TFT_INVERSION_ON             // Only needed for early T-Deck samples
// #define TFT_INVERSION_OFF            // Default for current production units

// ============================================================================
// ESP32 SPECIFIC
// ============================================================================
#define SUPPORT_TRANSACTIONS
