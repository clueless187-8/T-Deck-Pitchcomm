/*
 * ============================================================================
 * T-DECK PITCHCOMM - HARDWARE CONFIGURATION
 * ============================================================================
 * 
 * Pin definitions and RF parameters for LilyGO T-Deck Plus (ESP32-S3)
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
// T-DECK PLUS PIN MAPPING
// ============================================================================

// Power Management
#define BOARD_POWERON       10      // Main power enable

// Display (ST7789 via SPI)
#define TFT_CS              12
#define TFT_DC              11
#define TFT_RST             -1      // Tied to EN
#define TFT_BL              42      // Backlight PWM
#define TFT_WIDTH           320
#define TFT_HEIGHT          240

// SPI Bus (shared)
#define SPI_SCK             40
#define SPI_MISO            38
#define SPI_MOSI            41

// LoRa Radio (SX1262)
#define LORA_CS             9
#define LORA_RST            17
#define LORA_DIO1           45
#define LORA_BUSY           13

// I2C Bus
#define I2C_SDA             18
#define I2C_SCL             8

// Keyboard
#define KB_I2C_ADDR         0x55
#define KB_INT              46

// Trackball (optional)
#define TB_UP               3
#define TB_DOWN             15
#define TB_LEFT             1
#define TB_RIGHT            2
#define TB_CLICK            0

// Audio
#define SPEAKER_PIN         14

// SD Card
#define SD_CS               39

// Battery ADC
#define BATT_ADC            4

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
