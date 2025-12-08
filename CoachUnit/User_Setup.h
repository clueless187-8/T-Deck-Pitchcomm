/*
 * TFT_eSPI User_Setup.h for LilyGO T-Deck Plus
 * Used by: Coach Unit Firmware
 *
 * IMPORTANT: Copy this file to your TFT_eSPI library folder:
 *   Windows: C:\Users\<USERNAME>\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
 *   macOS:   ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
 *   Linux:   ~/Arduino/libraries/TFT_eSPI/User_Setup.h
 *
 * You must restart Arduino IDE after copying this file.
 *
 * Pin Configuration verified from LilyGO T-Deck Plus Wiki:
 * https://github.com/Xinyuan-LilyGO/T-Deck
 */

// ============================================================================
// DRIVER SELECTION
// ============================================================================
#define ST7789_DRIVER

// ============================================================================
// DISPLAY DIMENSIONS
// ============================================================================
#define TFT_WIDTH   320
#define TFT_HEIGHT  240

// ============================================================================
// PIN DEFINITIONS - T-Deck Plus (ESP32-S3)
// ============================================================================
#define TFT_MOSI    41      // SPI MOSI
#define TFT_SCLK    40      // SPI Clock
#define TFT_CS      12      // Chip Select
#define TFT_DC      11      // Data/Command
#define TFT_RST     -1      // Reset (not connected, handled by BOARD_POWERON)
#define TFT_BL      42      // Backlight control

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================
#define TFT_RGB_ORDER TFT_BGR    // T-Deck uses BGR color order
#define TFT_INVERSION_ON         // Color inversion required for ST7789

// ============================================================================
// SPI CONFIGURATION
// ============================================================================
#define SPI_FREQUENCY       40000000    // 40 MHz SPI clock
#define SPI_READ_FREQUENCY  20000000    // 20 MHz for reads

// ============================================================================
// FONT CONFIGURATION
// ============================================================================
#define LOAD_GLCD           // Original Adafruit 8 pixel font
#define LOAD_FONT2          // Small 16 pixel high font
#define LOAD_FONT4          // Medium 26 pixel high font
#define LOAD_FONT6          // Large 48 pixel high font (numbers only)
#define LOAD_FONT7          // 7 segment 48 pixel high font (numbers only)
#define LOAD_FONT8          // Large 75 pixel high font (numbers only)
#define LOAD_GFXFF          // FreeFonts
#define SMOOTH_FONT         // Enable smooth font rendering
