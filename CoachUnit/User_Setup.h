/*
 * ============================================================================
 * TFT_eSPI CONFIGURATION - T-DECK PLUS
 * ============================================================================
 * 
 * INSTALLATION:
 * Copy this file to your TFT_eSPI library folder, replacing User_Setup.h:
 * 
 *   Windows: C:\Users\<USER>\Documents\Arduino\libraries\TFT_eSPI\
 *   Linux:   ~/Arduino/libraries/TFT_eSPI/
 *   macOS:   ~/Documents/Arduino/libraries/TFT_eSPI/
 * 
 * Restart Arduino IDE after copying.
 * 
 * MIT License
 * Copyright (c) 2025 clueless187-8
 * https://github.com/clueless187-8/T-Deck-Pitchcomm
 * 
 * ============================================================================
 */

// ============================================================================
// DRIVER SELECTION
// ============================================================================
#define ST7789_DRIVER

#define TFT_WIDTH   240
#define TFT_HEIGHT  320

// ============================================================================
// ESP32-S3 PIN DEFINITIONS (T-DECK PLUS)
// ============================================================================
#define TFT_MISO    38
#define TFT_MOSI    41
#define TFT_SCLK    40
#define TFT_CS      12
#define TFT_DC      11
#define TFT_RST     -1      // Connected to EN (hardware reset)
#define TFT_BL      42      // Backlight control

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
#define SPI_FREQUENCY       40000000    // 40 MHz (conservative)
#define SPI_READ_FREQUENCY  20000000    // 20 MHz read speed

// ============================================================================
// DISPLAY SETTINGS
// ============================================================================
#define TFT_INVERSION_ON
#define TFT_RGB_ORDER TFT_RGB

// ============================================================================
// ESP32 SPECIFIC
// ============================================================================
#define SUPPORT_TRANSACTIONS
