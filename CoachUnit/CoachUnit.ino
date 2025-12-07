/*
 * ============================================================================
 * T-DECK PITCHCOMM - COACH TRANSMITTER UNIT
 * ============================================================================
 * 
 * HARDWARE: LilyGO T-Deck Plus (ESP32-S3 + SX1262)
 * VERIFIED: Pin definitions from LILYGO Wiki (wiki.lilygo.cc)
 * 
 * PIN VERIFICATION SOURCE:
 * https://wiki.lilygo.cc/get_started/en/Wearable/T-Deck-Plus/T-Deck-Plus.html
 * 
 * MIT License
 * Copyright (c) 2025 clueless187-8
 * https://github.com/clueless187-8/T-Deck-Pitchcomm
 * 
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <TFT_eSPI.h>
#include "esp_task_wdt.h"
#include "config.h"

// ============================================================================
// HARDWARE OBJECTS
// ============================================================================
TFT_eSPI tft = TFT_eSPI();

// SX1262 Radio - Uses shared SPI bus
// Constructor: SX1262(cs, irq, rst, busy)
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// ============================================================================
// PITCH DEFINITIONS
// ============================================================================
typedef struct {
    uint8_t code;
    const char* name;
    const char* abbrev;
    uint16_t color;
} PitchType;

// Standard pitch call mapping (keyboard keys 1-9, 0, P)
const PitchType PITCHES[] = {
    {0x01, "Fastball",     "FB", TFT_RED},
    {0x02, "Curveball",    "CB", TFT_BLUE},
    {0x03, "Slider",       "SL", TFT_GREEN},
    {0x04, "Changeup",     "CH", TFT_YELLOW},
    {0x05, "Cutter",       "CT", TFT_ORANGE},
    {0x06, "Sinker",       "SI", TFT_PURPLE},
    {0x07, "Splitter",     "SP", TFT_CYAN},
    {0x08, "Knuckle",      "KN", TFT_MAGENTA},
    {0x09, "Screwball",    "SC", TFT_PINK},
    {0x0A, "Intentional",  "IW", TFT_WHITE},   // Key 0
    {0x0B, "Pitchout",     "PO", TFT_LIGHTGREY} // Key P
};
#define NUM_PITCHES (sizeof(PITCHES) / sizeof(PitchType))

// Key to pitch index mapping (1-9, 0, P)
const char PITCH_KEYS[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'p'};

// ============================================================================
// PROTOCOL DEFINITIONS
// ============================================================================
// Packet structure: [SYNC][VER][CMD][DATA][CRC16]
#define SYNC_WORD       0xAA
#define CMD_PITCH       0x01
#define CMD_ACK         0x81
#define CMD_CANCEL      0x02

// Packet buffer
uint8_t txPacket[8];
uint8_t rxPacket[8];

// ============================================================================
// STATE VARIABLES
// ============================================================================
volatile bool transmitting = false;
volatile bool ackReceived = false;
uint32_t lastTxTime = 0;
uint8_t currentPitch = 0;
uint8_t txCount = 0;

// Keyboard state
uint8_t lastKeyCode = 0;
uint32_t lastKeyTime = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void initHardware();
void initDisplay();
void initRadio();
void initKeyboard();
void drawMainScreen();
void drawPitchSent(uint8_t pitchIdx);
void drawStatus(const char* msg, uint16_t color);
void readKeyboard();
void sendPitch(uint8_t pitchIdx);
bool waitForAck(uint32_t timeout);
uint16_t calcCRC16(uint8_t* data, uint8_t len);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(DEBUG_BAUD);
    DBGLN("\n\n===========================================");
    DBGLN("  T-DECK PITCHCOMM - COACH UNIT");
    DBGF("  Firmware Version: %s\n", FW_VERSION);
    DBGLN("===========================================\n");
    
    initHardware();
    initDisplay();
    initRadio();
    initKeyboard();
    
    // Initialize watchdog (ESP-IDF v5.x API)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = (1 << 0) | (1 << 1),  // Both cores
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);  // Add current task
    
    drawMainScreen();
    DBGLN("\n[READY] Coach unit operational");
    DBGLN("Press 1-9, 0, or P to transmit pitch calls\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    esp_task_wdt_reset();
    
    readKeyboard();
    
    // Check for incoming ACKs
    if (radio.available()) {
        int state = radio.readData(rxPacket, sizeof(rxPacket));
        if (state == RADIOLIB_ERR_NONE) {
            // Verify ACK packet
            if (rxPacket[0] == SYNC_WORD && rxPacket[2] == CMD_ACK) {
                ackReceived = true;
                DBGLN("[RX] ACK received from catcher");
                drawStatus("ACK RECEIVED", TFT_GREEN);
            }
        }
    }
    
    delay(10);
}

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================
void initHardware() {
    DBGLN("[INIT] Hardware initialization...");
    
    // CRITICAL: Enable peripheral power rail
    // This powers the display, LoRa, keyboard, and other peripherals
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(100);  // Allow power to stabilize
    DBGLN("  - Peripheral power enabled (GPIO10)");
    
    // Initialize SPI bus (shared by display, LoRa, SD card)
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    DBGLN("  - SPI bus initialized");
    
    // Initialize I2C bus (keyboard, touch, sensors)
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);  // 400kHz I2C
    DBGLN("  - I2C bus initialized (400kHz)");
}

// ============================================================================
// DISPLAY INITIALIZATION
// ============================================================================
void initDisplay() {
    DBGLN("[INIT] Display initialization...");
    
    // Initialize backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    // Initialize TFT
    tft.init();
    tft.setRotation(1);  // Landscape, USB on left
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    
    DBGF("  - ST7789 320x240 initialized (rotation=%d)\n", tft.getRotation());
}

// ============================================================================
// RADIO INITIALIZATION
// ============================================================================
void initRadio() {
    DBGLN("[INIT] Radio initialization...");
    
    // Initialize SX1262
    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, 
                            RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 
                            LORA_POWER, LORA_PREAMBLE);
    
    if (state != RADIOLIB_ERR_NONE) {
        DBGF("  - ERROR: Radio init failed (code %d)\n", state);
        drawStatus("RADIO INIT FAILED", TFT_RED);
        while (1) {
            esp_task_wdt_reset();
            delay(1000);
        }
    }
    
    // Configure for low latency
    radio.setDio1Action(NULL);  // Polling mode
    radio.setRxBoostedGainMode(true);
    
    DBGF("  - SX1262 configured: %.1f MHz, SF%d, BW%.0fkHz\n", 
         LORA_FREQ, LORA_SF, LORA_BW);
    DBGF("  - TX Power: +%d dBm\n", LORA_POWER);
    
    // Start receive mode
    radio.startReceive();
    DBGLN("  - Receive mode active");
}

// ============================================================================
// KEYBOARD INITIALIZATION
// ============================================================================
void initKeyboard() {
    DBGLN("[INIT] Keyboard initialization...");
    
    // Configure keyboard interrupt pin
    pinMode(KB_INT, INPUT_PULLUP);
    
    // Verify keyboard presence on I2C
    Wire.beginTransmission(KB_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        DBGF("  - T-Keyboard found at 0x%02X\n", KB_I2C_ADDR);
    } else {
        DBGLN("  - WARNING: Keyboard not detected!");
    }
}

// ============================================================================
// READ KEYBOARD
// ============================================================================
void readKeyboard() {
    // Check for key interrupt
    if (digitalRead(KB_INT) == LOW) {
        // Read key from ESP32-C3 keyboard controller
        Wire.requestFrom((uint8_t)KB_I2C_ADDR, (uint8_t)1);
        if (Wire.available()) {
            uint8_t keyCode = Wire.read();
            
            // Debounce
            if (millis() - lastKeyTime < KB_DEBOUNCE_MS) return;
            if (keyCode == lastKeyCode) return;
            
            lastKeyCode = keyCode;
            lastKeyTime = millis();
            
            // Convert to ASCII
            char key = (char)keyCode;
            DBGF("[KB] Key pressed: 0x%02X ('%c')\n", keyCode, key);
            
            // Check if it's a pitch key
            for (int i = 0; i < NUM_PITCHES; i++) {
                if (tolower(key) == PITCH_KEYS[i]) {
                    sendPitch(i);
                    break;
                }
            }
        }
    }
}

// ============================================================================
// SEND PITCH COMMAND
// ============================================================================
void sendPitch(uint8_t pitchIdx) {
    if (pitchIdx >= NUM_PITCHES) return;
    
    // Enforce TX cooldown
    if (millis() - lastTxTime < TX_COOLDOWN_MS) {
        DBGLN("[TX] Cooldown active, ignoring");
        return;
    }
    
    const PitchType* pitch = &PITCHES[pitchIdx];
    DBGF("[TX] Sending: %s (%s) - Code 0x%02X\n", 
         pitch->name, pitch->abbrev, pitch->code);
    
    // Build packet
    txPacket[0] = SYNC_WORD;
    txPacket[1] = PROTOCOL_VERSION;
    txPacket[2] = CMD_PITCH;
    txPacket[3] = pitch->code;
    
    // Calculate CRC
    uint16_t crc = calcCRC16(txPacket, 4);
    txPacket[4] = crc >> 8;
    txPacket[5] = crc & 0xFF;
    
    // Transmit
    drawPitchSent(pitchIdx);
    
    int state = radio.transmit(txPacket, 6);
    lastTxTime = millis();
    txCount++;
    
    if (state == RADIOLIB_ERR_NONE) {
        DBGLN("  - Transmission successful");
        
        // Return to receive mode and wait for ACK
        radio.startReceive();
        
        if (waitForAck(500)) {
            DBGLN("  - ACK confirmed!");
        } else {
            DBGLN("  - No ACK received (timeout)");
            drawStatus("NO ACK", TFT_YELLOW);
        }
    } else {
        DBGF("  - TX failed (code %d)\n", state);
        drawStatus("TX FAILED", TFT_RED);
        radio.startReceive();
    }
}

// ============================================================================
// WAIT FOR ACK
// ============================================================================
bool waitForAck(uint32_t timeout) {
    ackReceived = false;
    uint32_t start = millis();
    
    while (millis() - start < timeout) {
        esp_task_wdt_reset();
        
        if (radio.available()) {
            int state = radio.readData(rxPacket, sizeof(rxPacket));
            if (state == RADIOLIB_ERR_NONE) {
                if (rxPacket[0] == SYNC_WORD && rxPacket[2] == CMD_ACK) {
                    return true;
                }
            }
        }
        delay(1);
    }
    return false;
}

// ============================================================================
// CRC-16 CCITT
// ============================================================================
uint16_t calcCRC16(uint8_t* data, uint8_t len) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================
void drawMainScreen() {
    tft.fillScreen(TFT_BLACK);
    
    // Header
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(60, 10);
    tft.print("PITCHCOMM");
    
    // Subheader
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(90, 45);
    tft.print("COACH UNIT");
    
    // Divider
    tft.drawFastHLine(10, 70, 300, TFT_DARKGREY);
    
    // Status
    tft.setTextSize(2);
    tft.setCursor(20, 90);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("READY");
    
    // RF Info
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(20, 120);
    tft.printf("RF: %.0f MHz | PWR: +%d dBm", LORA_FREQ, LORA_POWER);
    tft.setCursor(20, 135);
    tft.printf("SF%d | BW%.0fkHz | CR4/%d", LORA_SF, LORA_BW, LORA_CR);
    
    // Instructions
    tft.setCursor(20, 200);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.print("Keys: 1-9=Pitches 0=IW P=Pitchout");
}

void drawPitchSent(uint8_t pitchIdx) {
    if (pitchIdx >= NUM_PITCHES) return;
    const PitchType* pitch = &PITCHES[pitchIdx];
    
    // Clear pitch area
    tft.fillRect(10, 150, 300, 45, TFT_BLACK);
    
    // Draw pitch name with color
    tft.setTextSize(3);
    tft.setTextColor(pitch->color, TFT_BLACK);
    tft.setCursor(80, 155);
    tft.print(pitch->name);
    
    // Draw "SENT" indicator
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(20, 160);
    tft.print("TX:");
}

void drawStatus(const char* msg, uint16_t color) {
    tft.fillRect(150, 85, 160, 25, TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(150, 90);
    tft.print(msg);
}
