/*
 * ============================================================================
 * T-DECK PITCHCOMM - COACH UNIT TRANSMITTER
 * ============================================================================
 * 
 * Production Firmware v1.0
 * 
 * Real-time pitch signal transmission for baseball operations.
 * 
 * Hardware: LilyGO T-Deck Plus (ESP32-S3)
 * Radio:    Semtech SX1262 LoRa @ 915 MHz
 * Display:  ST7789 320x240 IPS
 * Input:    Integrated QWERTY keyboard
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
#include <esp_task_wdt.h>
#include "config.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

TFT_eSPI tft = TFT_eSPI();
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);

// System state tracking
struct SystemState {
    bool displayOK      = false;
    bool radioOK        = false;
    bool keyboardOK     = false;
    bool systemReady    = false;
    uint32_t bootTime   = 0;
    uint32_t lastTX     = 0;
    uint32_t txCount    = 0;
    uint32_t txErrors   = 0;
    int lastRSSI        = 0;
    String lastPitch    = "";
} sys;

// Pitch type definitions
struct PitchType {
    char key;
    const char* name;
    const char* shortName;
    uint8_t code;
    uint32_t color;
};

const PitchType PITCHES[] = {
    {'1', "FASTBALL",     "FB",  0x01, TFT_RED},
    {'2', "CURVEBALL",    "CB",  0x02, TFT_BLUE},
    {'3', "SLIDER",       "SL",  0x03, TFT_GREEN},
    {'4', "CHANGEUP",     "CH",  0x04, TFT_YELLOW},
    {'5', "CUTTER",       "CT",  0x05, TFT_CYAN},
    {'6', "SINKER",       "SI",  0x06, TFT_MAGENTA},
    {'7', "SPLITTER",     "SP",  0x07, TFT_ORANGE},
    {'8', "KNUCKLEBALL",  "KB",  0x08, TFT_PURPLE},
    {'9', "SCREWBALL",    "SC",  0x09, TFT_GREENYELLOW},
    {'0', "PITCHOUT",     "PO",  0x0A, TFT_WHITE},
    {'p', "PICKOFF",      "PK",  0x0B, TFT_PINK},
    {'P', "PICKOFF",      "PK",  0x0B, TFT_PINK}
};
const int NUM_PITCHES = sizeof(PITCHES) / sizeof(PITCHES[0]);

// RF Protocol packet structure
struct __attribute__((packed)) SignalPacket {
    uint8_t  header;      // 0xBB magic byte
    uint8_t  version;     // Protocol version
    uint8_t  pitchCode;   // Pitch identifier
    uint8_t  sequence;    // Deduplication counter
    uint16_t checksum;    // CRC16-Modbus
};

uint8_t txSequence = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void safeBoot();
bool initDisplay();
bool initRadio();
bool initKeyboard();
void drawBootScreen();
void drawMainScreen();
void drawStatusBar();
void drawPitchConfirm(const PitchType* pitch, bool success);
void handleKey(char key);
bool transmitSignal(const PitchType* pitch);
uint16_t calcCRC16(uint8_t* data, size_t len);
void pollKeyboard();

// ============================================================================
// BOOT SEQUENCE
// ============================================================================

void setup() {
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 1: USB SERIAL - CRITICAL FIRST STEP
    // ─────────────────────────────────────────────────────────────────────
    Serial.begin(115200);
    
    Serial.println();
    Serial.println(F("╔════════════════════════════════════════╗"));
    Serial.println(F("║     T-DECK PITCHCOMM - COACH UNIT      ║"));
    Serial.println(F("║         Firmware v1.0 Production       ║"));
    Serial.println(F("╚════════════════════════════════════════╝"));
    Serial.println();
    Serial.println(F(">>> 3-SECOND RECOVERY WINDOW <<<"));
    Serial.println(F("Hold BOOT button now to enter bootloader"));
    Serial.println();
    
    // Recovery delay - DO NOT REMOVE
    for (int i = 3; i > 0; i--) {
        Serial.printf("    %d...\n", i);
        delay(1000);
    }
    Serial.println(F("Proceeding with initialization...\n"));
    
    sys.bootTime = millis();
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 2: WATCHDOG TIMER
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[WDT ] "));
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    Serial.println(F("Watchdog armed (10s timeout)"));
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 3: GPIO CONFIGURATION
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[GPIO] "));
    
    // Power rail enable
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    
    // Display backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    // LoRa chip select (deassert)
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    
    // Keyboard interrupt
    pinMode(KB_INT, INPUT_PULLUP);
    
    Serial.println(F("Configured"));
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 4: BUS INITIALIZATION
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[I2C ] "));
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    Serial.println(F("400kHz bus ready"));
    
    Serial.print(F("[SPI ] "));
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    Serial.println(F("Bus ready"));
    
    esp_task_wdt_reset();
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 5: DISPLAY SUBSYSTEM
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[DISP] "));
    sys.displayOK = initDisplay();
    if (sys.displayOK) {
        Serial.println(F("ST7789 320x240 OK"));
        drawBootScreen();
    } else {
        Serial.println(F("FAILED - continuing headless"));
    }
    
    esp_task_wdt_reset();
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 6: LORA RADIO SUBSYSTEM
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[LORA] "));
    sys.radioOK = initRadio();
    if (sys.radioOK) {
        Serial.printf("SX1262 @ %.1f MHz / +%d dBm\n", LORA_FREQ, LORA_POWER);
        if (sys.displayOK) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString("LoRa: OK (915.0 MHz, +22 dBm)", 20, 120, 2);
        }
    } else {
        Serial.println(F("INIT FAILED - CHECK WIRING"));
        if (sys.displayOK) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("LoRa: FAILED", 20, 120, 2);
        }
    }
    
    esp_task_wdt_reset();
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 7: KEYBOARD SUBSYSTEM
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[KB  ] "));
    sys.keyboardOK = initKeyboard();
    if (sys.keyboardOK) {
        Serial.printf("Found at 0x%02X\n", KB_I2C_ADDR);
        if (sys.displayOK) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString("Keyboard: OK", 20, 140, 2);
        }
    } else {
        Serial.println(F("Not detected - using Serial input"));
    }
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 8: SYSTEM READY
    // ─────────────────────────────────────────────────────────────────────
    sys.systemReady = sys.radioOK;  // Radio is the only critical subsystem
    
    Serial.println();
    Serial.println(F("╔════════════════════════════════════════╗"));
    Serial.printf("║  Boot complete: %lu ms                  \n", millis() - sys.bootTime);
    Serial.printf("║  Display:  %s                          \n", sys.displayOK ? "OK" : "FAIL");
    Serial.printf("║  Radio:    %s                          \n", sys.radioOK ? "OK" : "FAIL");
    Serial.printf("║  Keyboard: %s                          \n", sys.keyboardOK ? "OK" : "WARN");
    Serial.printf("║  System:   %s                       \n", sys.systemReady ? "READY" : "DEGRADED");
    Serial.println(F("╚════════════════════════════════════════╝"));
    Serial.println();
    
    if (sys.systemReady) {
        Serial.println(F("Pitch keys: 1-9, 0, P"));
        Serial.println(F("Transmitting on 915 MHz LoRa\n"));
    }
    
    delay(1500);
    
    if (sys.displayOK) {
        drawMainScreen();
    }
}

// ============================================================================
// MAIN EXECUTION LOOP
// ============================================================================

void loop() {
    esp_task_wdt_reset();
    
    // Poll keyboard for input
    pollKeyboard();
    
    // Accept serial input as backup
    if (Serial.available()) {
        char c = Serial.read();
        if (c != '\n' && c != '\r') {
            handleKey(c);
        }
    }
    
    // Periodic status refresh
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 5000) {
        lastStatus = millis();
        if (sys.displayOK) {
            drawStatusBar();
        }
    }
    
    delay(10);
}

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================

bool initDisplay() {
    tft.init();
    tft.setRotation(1);  // Landscape orientation
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    return true;
}

bool initRadio() {
    Serial.println();
    Serial.println(F("       Configuring SX1262..."));
    
    int state = radio.begin(
        LORA_FREQ,
        LORA_BW,
        LORA_SF,
        LORA_CR,
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
        LORA_POWER,
        LORA_PREAMBLE
    );
    
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("       begin() failed: %d\n", state);
        return false;
    }
    
    // RF switch control via DIO2
    radio.setDio2AsRfSwitch(true);
    
    // PA current limit
    radio.setCurrentLimit(140.0);
    
    // Hardware CRC
    radio.setCRC(2);
    
    Serial.printf("       Freq: %.1f MHz\n", LORA_FREQ);
    Serial.printf("       BW:   %.0f kHz\n", LORA_BW);
    Serial.printf("       SF:   %d\n", LORA_SF);
    Serial.printf("       PWR:  +%d dBm\n", LORA_POWER);
    
    return true;
}

bool initKeyboard() {
    Wire.beginTransmission(KB_I2C_ADDR);
    return (Wire.endTransmission() == 0);
}

// ============================================================================
// DISPLAY RENDERING
// ============================================================================

void drawBootScreen() {
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("T-DECK PITCHCOMM", 160, 20, 4);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Coach Unit v1.0", 160, 55, 2);
    
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Initializing...", 20, 100, 2);
}

void drawMainScreen() {
    tft.fillScreen(TFT_BLACK);
    
    // Header bar
    tft.fillRect(0, 0, 320, 30, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("COACH TRANSMITTER", 160, 6, 2);
    
    // Status LED
    tft.fillCircle(300, 15, 8, sys.systemReady ? TFT_GREEN : TFT_RED);
    
    // Pitch key legend
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("PITCH KEYS:", 10, 40, 2);
    
    int x = 10, y = 60;
    for (int i = 0; i < NUM_PITCHES - 2; i++) {
        tft.setTextColor(PITCHES[i].color, TFT_BLACK);
        char buf[16];
        snprintf(buf, sizeof(buf), "%c=%s", PITCHES[i].key, PITCHES[i].shortName);
        tft.drawString(buf, x, y, 2);
        x += 60;
        if (x > 280) {
            x = 10;
            y += 20;
        }
    }
    
    // Ready indicator
    tft.fillRect(0, 180, 320, 60, TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("READY", 160, 200, 4);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Press key to transmit", 160, 225, 2);
    
    drawStatusBar();
}

void drawStatusBar() {
    tft.fillRect(0, 230, 320, 10, TFT_BLACK);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(BL_DATUM);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "TX:%lu ERR:%lu", sys.txCount, sys.txErrors);
    tft.drawString(buf, 5, 240, 1);
    
    uint32_t uptime = (millis() - sys.bootTime) / 1000;
    snprintf(buf, sizeof(buf), "UP:%lus", uptime);
    tft.setTextDatum(BR_DATUM);
    tft.drawString(buf, 315, 240, 1);
}

void drawPitchConfirm(const PitchType* pitch, bool success) {
    uint32_t bgColor = success ? pitch->color : TFT_RED;
    
    tft.fillRect(0, 180, 320, 60, bgColor);
    
    // Contrast text color selection
    uint32_t textColor = (bgColor == TFT_YELLOW || bgColor == TFT_WHITE ||
                          bgColor == TFT_GREENYELLOW || bgColor == TFT_CYAN)
                          ? TFT_BLACK : TFT_WHITE;
    
    tft.setTextColor(textColor, bgColor);
    tft.setTextDatum(MC_DATUM);
    
    if (success) {
        tft.drawString(pitch->name, 160, 195, 4);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("SENT", 160, 220, 2);
    } else {
        tft.drawString("TX FAILED", 160, 200, 4);
    }
    
    delay(500);
    
    // Restore ready state
    tft.fillRect(0, 180, 320, 60, TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("READY", 160, 200, 4);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Press key to transmit", 160, 225, 2);
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void pollKeyboard() {
    if (!sys.keyboardOK) return;
    
    Wire.requestFrom(KB_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        char key = Wire.read();
        if (key != 0) {
            handleKey(key);
        }
    }
}

void handleKey(char key) {
    Serial.printf("[KEY ] '%c' (0x%02X)\n", key, key);
    
    for (int i = 0; i < NUM_PITCHES; i++) {
        if (PITCHES[i].key == key) {
            Serial.printf("[MTCH] %s\n", PITCHES[i].name);
            
            bool success = transmitSignal(&PITCHES[i]);
            
            if (sys.displayOK) {
                drawPitchConfirm(&PITCHES[i], success);
            }
            return;
        }
    }
    
    Serial.printf("[WARN] Unknown key: '%c'\n", key);
}

// ============================================================================
// RF TRANSMISSION
// ============================================================================

bool transmitSignal(const PitchType* pitch) {
    if (!sys.radioOK) {
        Serial.println(F("[ERR ] Radio unavailable"));
        sys.txErrors++;
        return false;
    }
    
    // Construct packet
    SignalPacket pkt;
    pkt.header    = 0xBB;
    pkt.version   = PROTOCOL_VERSION;
    pkt.pitchCode = pitch->code;
    pkt.sequence  = txSequence++;
    pkt.checksum  = calcCRC16((uint8_t*)&pkt, sizeof(pkt) - 2);
    
    // Transmit
    uint32_t t0 = micros();
    int state = radio.transmit((uint8_t*)&pkt, sizeof(pkt));
    uint32_t txTime = micros() - t0;
    
    sys.lastTX = millis();
    
    if (state == RADIOLIB_ERR_NONE) {
        sys.txCount++;
        sys.lastPitch = pitch->name;
        
        Serial.println(F("[TX  ] SUCCESS"));
        Serial.printf("       Pitch: %s (0x%02X)\n", pitch->name, pitch->code);
        Serial.printf("       Seq:   %d\n", pkt.sequence);
        Serial.printf("       Time:  %lu µs\n", txTime);
        Serial.printf("       Total: %lu\n", sys.txCount);
        
        return true;
    } else {
        sys.txErrors++;
        Serial.printf("[TX  ] FAILED: %d\n", state);
        return false;
    }
}

uint16_t calcCRC16(uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
        }
    }
    return crc;
}

// ============================================================================
// END OF FIRMWARE
// ============================================================================
