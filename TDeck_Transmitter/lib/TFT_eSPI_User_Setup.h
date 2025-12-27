// User Setup for LilyGo T-Deck (ST7789 240x320)
#define USER_SETUP_ID 210

#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_RGB_ORDER TFT_RGB  // Very important!
#define INIT_SEQUENCE_2        // Use init sequence 2 for T-Deck

#define TFT_MISO 38  // Actually used by T-Deck
#define TFT_MOSI 41
#define TFT_SCLK 40
#define TFT_CS   12
#define TFT_DC   11
#define TFT_RST  -1
#define TFT_BL   42
#define TFT_BACKLIGHT_ON 1

#define USE_HSPI_PORT  // Must use HSPI
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  20000000
