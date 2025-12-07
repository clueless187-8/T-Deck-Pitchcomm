/*
 * TFT_eSPI User_Setup.h for LilyGO T-Watch S3
 * Used by: Catcher Unit Firmware
 * 
 * IMPORTANT: Copy this file to D:\Arduino\libraries\TFT_eSPI\User_Setup.h
 *            ONLY when compiling Catcher Unit firmware.
 *            
 * The T-Watch S3 uses different pins than T-Deck Plus.
 * You must swap User_Setup.h files when switching between devices.
 */

// ============================================================================
// DRIVER SELECTION
// ============================================================================
#define ST7789_DRIVER

// ============================================================================
// DISPLAY DIMENSIONS
// ============================================================================
#define TFT_WIDTH   240
#define TFT_HEIGHT  240     // Square display for round watch

// ============================================================================
// PIN DEFINITIONS - T-Watch S3
// ============================================================================
#define TFT_MOSI    13      // SPI MOSI
#define TFT_SCLK    18      // SPI Clock
#define TFT_CS      12      // Chip Select
#define TFT_DC      38      // Data/Command
#define TFT_RST     -1      // Reset (not connected, use software reset)
#define TFT_BL      45      // Backlight control

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================
#define TFT_RGB_ORDER TFT_BGR    // T-Watch S3 uses BGR order
#define TFT_INVERSION_ON         // Color inversion required

// ============================================================================
// SPI CONFIGURATION
// ============================================================================
#define SPI_FREQUENCY       40000000    // 40 MHz
#define SPI_READ_FREQUENCY  20000000    // 20 MHz for reads

// ============================================================================
// OPTIONAL FEATURES
// ============================================================================
// #define SMOOTH_FONT
// #define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
