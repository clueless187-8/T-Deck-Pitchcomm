/*
 * ============================================================================
 * T-DECK PITCHCOMM - HARDWARE CONFIGURATION
 * ============================================================================
 * 
 * VERIFIED PIN DEFINITIONS from official LilyGO sources:
 * - https://wiki.lilygo.cc/get_started/en/Wearable/T-Deck-Plus/T-Deck-Plus.html
 * - https://github.com/Xinyuan-LilyGO/T-Deck (examples/UnitTest/utilities.h)
 * 
 * Hardware: LilyGO T-Deck Plus
 * MCU: ESP32-S3FN16R8 (16MB Flash, 8MB PSRAM)
 * Display: 2.8" ST7789 IPS LCD (320x240)
 * Radio: Semtech SX1262
 * 
 * MIT License
 * Copyright (c) 2025 clueless187-8
 * https://github.com/clueless187-8/T-Deck-Pitchcomm
 * 
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// FIRMWARE IDENTIFICATION
// ============================================================================
#define FW_VERSION          "1.0.0"
#define PROTOCOL_VERSION    0x01

// ============================================================================
// WATCHDOG CONFIGURATION
// ============================================================================
#define WDT_TIMEOUT_SEC     10

// ============================================================================
// T-DECK PLUS PIN MAPPING (VERIFIED FROM LILYGO WIKI)
// ============================================================================

/*
 * Source: https://wiki.lilygo.cc/get_started/en/Wearable/T-Deck-Plus/T-Deck-Plus.html
 * 
 * //! The board peripheral power control pin needs to be set to HIGH when using the peripheral
 * #define BOARD_POWERON       10
 * #define BOARD_I2S_WS        5
 * #define BOARD_I2S_BCK       7
 * #define BOARD_I2S_DOUT      6
 * #define BOARD_I2C_SDA       18
 * #define BOARD_I2C_SCL       8
 * #define BOARD_BAT_ADC       4
 * #define BOARD_TOUCH_INT     16
 * #define BOARD_KEYBOARD_INT  46
 * #define BOARD_SDCARD_CS     39
 * #define BOARD_TFT_CS        12
 * #define RADIO_CS_PIN        9
 * #define BOARD_TFT_DC        11
 * #define BOARD_TFT_BACKLIGHT 42
 * #define BOARD_SPI_MOSI      41
 * #define BOARD_SPI_MISO      38
 * #define BOARD_SPI_SCK       40
 * #define BOARD_TBOX_G02      2
 * #define BOARD_TBOX_G01      3
 * #define BOARD_TBOX_G04      1
 * #define BOARD_TBOX_G03      15
 * #define BOARD_ES7210_MCLK   48
 * #define BOARD_ES7210_LRCK   21
 * #define BOARD_ES7210_SCK    47
 * #define BOARD_ES7210_DIN    14
 * #define RADIO_BUSY_PIN      13
 * #define RADIO_RST_PIN       17
 * #define RADIO_DIO1_PIN      45
 * #define BOARD_BOOT_PIN      0
 * #define BOARD_BL_PIN        42
 * #define BOARD_GPS_TX_PIN    43
 * #define BOARD_GPS_RX_PIN    44
 */

// --------------------------------------------------------------------------
// POWER MANAGEMENT
// --------------------------------------------------------------------------
// CRITICAL: Must be set HIGH before using display, LoRa, or other peripherals
#define BOARD_POWERON       10

// --------------------------------------------------------------------------
// DISPLAY (ST7789 via SPI)
// --------------------------------------------------------------------------
#define TFT_CS              12      // BOARD_TFT_CS
#define TFT_DC              11      // BOARD_TFT_DC
#define TFT_RST             -1      // No dedicated RST (tied to EN via RST button)
#define TFT_BL              42      // BOARD_TFT_BACKLIGHT / BOARD_BL_PIN
#define TFT_WIDTH           320
#define TFT_HEIGHT          240

// --------------------------------------------------------------------------
// SPI BUS (Shared by Display, LoRa, SD Card)
// --------------------------------------------------------------------------
#define SPI_SCK             40      // BOARD_SPI_SCK
#define SPI_MISO            38      // BOARD_SPI_MISO
#define SPI_MOSI            41      // BOARD_SPI_MOSI

// --------------------------------------------------------------------------
// LORA RADIO (SX1262)
// --------------------------------------------------------------------------
#define LORA_CS             9       // RADIO_CS_PIN
#define LORA_RST            17      // RADIO_RST_PIN
#define LORA_DIO1           45      // RADIO_DIO1_PIN (IRQ)
#define LORA_BUSY           13      // RADIO_BUSY_PIN

// --------------------------------------------------------------------------
// I2C BUS (Keyboard, Touch, Sensors)
// --------------------------------------------------------------------------
#define I2C_SDA             18      // BOARD_I2C_SDA
#define I2C_SCL             8       // BOARD_I2C_SCL

// --------------------------------------------------------------------------
// KEYBOARD (ESP32-C3 I2C Slave)
// --------------------------------------------------------------------------
#define KB_I2C_ADDR         0x55    // Default T-Keyboard I2C address
#define KB_INT              46      // BOARD_KEYBOARD_INT

// --------------------------------------------------------------------------
// TRACKBALL (T-Box GPIO pins)
// --------------------------------------------------------------------------
#define TB_UP               3       // BOARD_TBOX_G01
#define TB_DOWN             15      // BOARD_TBOX_G03
#define TB_LEFT             1       // BOARD_TBOX_G04
#define TB_RIGHT            2       // BOARD_TBOX_G02
#define TB_CLICK            0       // BOARD_BOOT_PIN

// --------------------------------------------------------------------------
// AUDIO (ES7210 Codec + MAX98357A)
// --------------------------------------------------------------------------
#define I2S_WS              5       // BOARD_I2S_WS
#define I2S_BCK             7       // BOARD_I2S_BCK
#define I2S_DOUT            6       // BOARD_I2S_DOUT
#define I2S_MCLK            48      // BOARD_ES7210_MCLK
#define I2S_LRCK            21      // BOARD_ES7210_LRCK
#define I2S_SCK             47      // BOARD_ES7210_SCK
#define I2S_DIN             14      // BOARD_ES7210_DIN

// --------------------------------------------------------------------------
// SD CARD
// --------------------------------------------------------------------------
#define SD_CS               39      // BOARD_SDCARD_CS

// --------------------------------------------------------------------------
// TOUCH PANEL
// --------------------------------------------------------------------------
#define TOUCH_INT           16      // BOARD_TOUCH_INT

// --------------------------------------------------------------------------
// BATTERY ADC
// --------------------------------------------------------------------------
#define BATT_ADC            4       // BOARD_BAT_ADC

// --------------------------------------------------------------------------
// GPS (T-Deck Plus only)
// --------------------------------------------------------------------------
#define GPS_TX              43      // BOARD_GPS_TX_PIN
#define GPS_RX              44      // BOARD_GPS_RX_PIN

// ============================================================================
// LORA RF PARAMETERS
// ============================================================================

// Operating Frequency (ISM Band)
#define LORA_FREQ           915.0   // MHz - US ISM (902-928 MHz)
// #define LORA_FREQ        868.0   // MHz - EU ISM (863-870 MHz)
// #define LORA_FREQ        433.0   // MHz - Asia ISM

// Transmit Power
#define LORA_POWER          22      // dBm (SX1262 max: +22 dBm)

// Modulation Parameters (optimized for minimum latency)
#define LORA_BW             250.0   // kHz bandwidth
#define LORA_SF             7       // Spreading factor
#define LORA_CR             5       // Coding rate 4/5
#define LORA_PREAMBLE       8       // Preamble symbols

/*
 * Link Budget Analysis:
 * 
 * TX Power:           +22 dBm
 * RX Sensitivity:     -124 dBm (SF7, 250kHz)
 * Link Budget:        146 dB
 * 
 * Path Loss @ 400m:   ~81 dB (free space, 915 MHz)
 * Fade Margin:        65 dB (excellent multipath tolerance)
 * 
 * Time on Air (6-byte packet): ~5.4 ms
 * Channel Capacity:   ~10.9 kbps
 */

// ============================================================================
// DISPLAY COLOR DEFINITIONS
// ============================================================================
#define TFT_PURPLE          0x780F
#define TFT_ORANGE          0xFD20
#define TFT_PINK            0xF81F

// ============================================================================
// OPERATIONAL PARAMETERS
// ============================================================================
#define TX_COOLDOWN_MS      100     // Minimum inter-transmission interval
#define KB_DEBOUNCE_MS      50      // Key debounce time

// ============================================================================
// DEBUG CONFIGURATION
// ============================================================================
#define DEBUG_SERIAL        1
#define DEBUG_BAUD          115200

#if DEBUG_SERIAL
    #define DBG(x)          Serial.print(x)
    #define DBGLN(x)        Serial.println(x)
    #define DBGF(...)       Serial.printf(__VA_ARGS__)
#else
    #define DBG(x)
    #define DBGLN(x)
    #define DBGF(...)
#endif

#endif // CONFIG_H
