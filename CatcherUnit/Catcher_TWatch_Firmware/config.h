/*******************************************************************************
 * T-DECK PITCHCOMM - CATCHER UNIT CONFIGURATION
 * Target Hardware: LilyGO T-Watch S3 (ESP32-S3 + Integrated SX1262)
 * Version: 1.2.0
 * Protocol: 7-byte packet with location field
 * 
 * HARDWARE REFERENCE:
 * ==================
 * MCU: ESP32-S3FN8 (8MB Flash, 8MB PSRAM)
 * Display: ST7789V 240x240 1.54" IPS (Capacitive Touch)
 * LoRa: SX1262 (Integrated on PCB)
 * PMU: AXP2101 Power Management
 * RTC: PCF8563
 * Touch: CST816S
 * 
 * Source: LilyGO T-Watch S3 Official Schematic
 * https://github.com/Xinyuan-LilyGO/TTGO_TWatch_Library
 ******************************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// FIRMWARE VERSION
// =============================================================================
#define FIRMWARE_VERSION    "1.2.0"
#define FIRMWARE_DATE       "2024-12-07"

// =============================================================================
// PROTOCOL DEFINITIONS (MUST MATCH COACH UNIT)
// =============================================================================
#define SYNC_WORD           0xAA
#define PROTOCOL_VERSION    0x02        // v1.2.0 - 7-byte packet with location
#define PACKET_SIZE         7           // SYNC + VER + CMD + PITCH + LOC + CRC_H + CRC_L

// Command Types
#define CMD_PITCH           0x01
#define CMD_ACK             0x02
#define CMD_HEARTBEAT       0x03

// =============================================================================
// T-WATCH S3 PIN DEFINITIONS (ACTIVE LOW WHERE NOTED)
// =============================================================================

// Power Management
#define BOARD_POWERON       -1          // T-Watch S3 uses AXP2101, no GPIO power pin


// Display (ST7789V 1.54" 240x240 IPS)
#define TFT_CS              12
#define TFT_DC              38
#define TFT_RST             -1          // Connected to ESP32 EN via AXP2101
#define TFT_BL              45          // Backlight (AXP2101 controlled)
#define TFT_MOSI            13
#define TFT_SCLK            18
#define TFT_WIDTH           240
#define TFT_HEIGHT          240

// SPI Bus
#define SPI_MOSI            13
#define SPI_MISO            34
#define SPI_SCLK            18

// LoRa SX1262 Module (INTEGRATED - T-Watch S3 Specific)
#define LORA_CS             5           // NSS/CS
#define LORA_RST            8           // NRESET  
#define LORA_DIO1           9           // DIO1 interrupt
#define LORA_BUSY           7           // BUSY status

// I2C Bus (Shared: Touch, PMU, RTC)
#define I2C_SDA             10
#define I2C_SCL             11

// Touch Controller (CST816S)
#define TOUCH_INT           16
#define TOUCH_RST           -1          // Controlled by AXP2101
#define TOUCH_ADDR          0x15

// AXP2101 Power Management Unit
#define PMU_INT             14
#define PMU_ADDR            0x34

// RTC (PCF8563)
#define RTC_INT             17
#define RTC_ADDR            0x51

// Buttons
#define BTN_1               0           // Side button (BOOT)

// Vibration Motor (Controlled via AXP2101 LDO)
#define MOTOR_CHANNEL       1           // AXP2101 motor control channel


// =============================================================================
// RF PARAMETERS (915 MHz ISM Band - FCC Part 15)
// =============================================================================
#define RF_FREQUENCY        915.0       // MHz (US ISM band)
#define RF_BANDWIDTH        125.0       // kHz
#define RF_SPREADING        7           // SF7 (fastest, ~60ms airtime)
#define RF_CODING_RATE      5           // 4/5
#define RF_SYNC_WORD_LORA   0x12        // Private network sync word
#define RF_TX_POWER         22          // dBm (max for SX1262)
#define RF_PREAMBLE_LEN     8           // Symbols
#define RF_CRC_ENABLE       true        // Hardware CRC

// =============================================================================
// TIMING PARAMETERS
// =============================================================================
#define ACK_DELAY_MS        10          // Delay before sending ACK
#define ACK_TIMEOUT_MS      100         // Coach waits this long for ACK
#define DISPLAY_TIMEOUT_MS  5000        // Return to idle after 5 seconds
#define HAPTIC_PULSE_MS     100         // Vibration duration
#define BACKLIGHT_DIM_MS    10000       // Dim backlight after 10 seconds

// =============================================================================
// DISPLAY COLORS (RGB565)
// =============================================================================
#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F
#define COLOR_ORANGE        0xFD20
#define COLOR_DARK_GREEN    0x03E0
#define COLOR_GRAY          0x8410
#define COLOR_DARK_GRAY     0x4208


// Pitch-specific colors
#define COLOR_FASTBALL      0xF800      // Red
#define COLOR_CURVEBALL     0x07E0      // Green
#define COLOR_SLIDER        0x001F      // Blue
#define COLOR_CHANGEUP      0xFFE0      // Yellow
#define COLOR_CUTTER        0xF81F      // Magenta
#define COLOR_SINKER        0xFD20      // Orange
#define COLOR_SPLITTER      0x07FF      // Cyan
#define COLOR_KNUCKLE       0x8410      // Gray
#define COLOR_SCREWBALL     0xFC00      // Dark Orange
#define COLOR_WALK          0x4208      // Dark Gray
#define COLOR_PITCHOUT      0xFFFF      // White

// =============================================================================
// DEBUG CONFIGURATION
// =============================================================================
#define SERIAL_BAUD         115200
#define DEBUG_ENABLED       true

#if DEBUG_ENABLED
    #define DEBUG_PRINT(x)      Serial.print(x)
    #define DEBUG_PRINTLN(x)    Serial.println(x)
    #define DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

// =============================================================================
// AXP2101 PMU CONFIGURATION
// =============================================================================
#define PMU_USE_AXP2101     true        // T-Watch S3 uses AXP2101

#endif // CONFIG_H
