/*******************************************************************************
 * T-DECK PITCHCOMM - CATCHER UNIT FIRMWARE
 * Target Hardware: LilyGO T-Watch S3 (ESP32-S3 + Integrated SX1262)
 * Version: 1.2.0
 * 
 * RF Receiver for baseball pitch signaling system.
 * Receives 7-byte encrypted packets from Coach Unit via LoRa SX1262.
 * Displays pitch type and location with haptic confirmation.
 * Transmits ACK packet back to coach unit.
 * 
 * Protocol v1.2.0: [SYNC][VER][CMD][PITCH][LOC][CRC_H][CRC_L]
 * 
 * DEPENDENCIES:
 * - TFT_eSPI (configured for T-Watch S3)
 * - RadioLib (SX1262 support)
 * - XPowersLib (AXP2101 PMU control)
 * 
 * Author: PitchComm Development Team
 * License: MIT
 ******************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <RadioLib.h>
#include "config.h"

// AXP2101 Power Management - Required for T-Watch S3
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

// =============================================================================
// HARDWARE OBJECTS
// =============================================================================
TFT_eSPI tft = TFT_eSPI();
XPowersPMU pmu;

// SX1262 LoRa Module (Integrated)
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// =============================================================================
// PITCH DEFINITIONS
// =============================================================================
struct PitchType {
    uint8_t code;
    const char* name;
    const char* abbrev;
    uint16_t color;
};

const PitchType pitchTypes[] = {
    {0x01, "FASTBALL",   "FB", COLOR_FASTBALL},
    {0x02, "CURVEBALL",  "CB", COLOR_CURVEBALL},
    {0x03, "SLIDER",     "SL", COLOR_SLIDER},
    {0x04, "CHANGEUP",   "CH", COLOR_CHANGEUP},
    {0x05, "CUTTER",     "CT", COLOR_CUTTER},
    {0x06, "SINKER",     "SI", COLOR_SINKER},
    {0x07, "SPLITTER",   "SP", COLOR_SPLITTER},
    {0x08, "KNUCKLE",    "KN", COLOR_KNUCKLE},
    {0x09, "SCREWBALL",  "SC", COLOR_SCREWBALL},
    {0x0A, "INT. WALK",  "IW", COLOR_WALK},
    {0x0B, "PITCHOUT",   "PO", COLOR_PITCHOUT}
};
#define PITCH_COUNT (sizeof(pitchTypes) / sizeof(PitchType))


// =============================================================================
// LOCATION DEFINITIONS
// =============================================================================
struct LocationType {
    uint8_t code;
    const char* name;
    const char* abbrev;
    int8_t gridX;       // -1=left, 0=center, 1=right
    int8_t gridY;       // -1=low, 0=center, 1=high
};

const LocationType locationTypes[] = {
    {0x00, "CENTER",       "--",  0,  0},
    {0x01, "HIGH INSIDE",  "HI", -1,  1},
    {0x02, "HIGH OUTSIDE", "HO",  1,  1},
    {0x03, "LOW INSIDE",   "LI", -1, -1},
    {0x04, "LOW OUTSIDE",  "LO",  1, -1}
};
#define LOCATION_COUNT (sizeof(locationTypes) / sizeof(LocationType))

// =============================================================================
// STATE VARIABLES
// =============================================================================
volatile bool rxFlag = false;
uint8_t rxBuffer[PACKET_SIZE];
uint8_t lastPitchCode = 0;
uint8_t lastLocationCode = 0;
unsigned long lastRxTime = 0;
unsigned long displayActiveTime = 0;
bool displayActive = false;
uint32_t packetCount = 0;
uint32_t errorCount = 0;
int16_t lastRSSI = 0;
float lastSNR = 0;
bool pmuInitialized = false;

// =============================================================================
// ISR - Packet Received Flag
// =============================================================================
#if defined(ESP32)
IRAM_ATTR
#endif
void rxISR(void) {
    rxFlag = true;
}

// =============================================================================
// CRC-16 CCITT CALCULATION
// =============================================================================
uint16_t calcCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
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


// =============================================================================
// PMU INITIALIZATION (AXP2101)
// =============================================================================
bool initPMU() {
    DEBUG_PRINTLN("[PMU] Initializing AXP2101...");
    
    if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        DEBUG_PRINTLN("[PMU] AXP2101 init FAILED!");
        return false;
    }
    
    // Configure power channels
    pmu.setDC1Voltage(3300);    // ESP32-S3 core voltage
    pmu.enableDC1();
    
    // Enable backlight power
    pmu.setBLDO1Voltage(3300);  // Backlight
    pmu.enableBLDO1();
    
    // Enable vibration motor
    pmu.setALDO1Voltage(3300);  // Motor LDO
    pmu.enableALDO1();
    
    // Disable unused channels to save power
    pmu.disableDC2();
    pmu.disableDC3();
    pmu.disableDC4();
    pmu.disableDC5();
    
    pmuInitialized = true;
    DEBUG_PRINTLN("[PMU] AXP2101 initialized successfully");
    return true;
}

// =============================================================================
// HAPTIC FEEDBACK (Via AXP2101)
// =============================================================================
void hapticPulse(uint16_t duration_ms) {
    if (pmuInitialized) {
        pmu.enableALDO1();      // Motor ON
        delay(duration_ms);
        pmu.disableALDO1();     // Motor OFF
    }
}

void hapticPattern(uint8_t pulses, uint16_t on_ms, uint16_t off_ms) {
    for (uint8_t i = 0; i < pulses; i++) {
        hapticPulse(on_ms);
        if (i < pulses - 1) {
            delay(off_ms);
        }
    }
}

// =============================================================================
// BACKLIGHT CONTROL
// =============================================================================
void setBacklight(uint8_t level) {
    // level: 0-100%
    if (pmuInitialized) {
        if (level == 0) {
            pmu.disableBLDO1();
        } else {
            pmu.enableBLDO1();
            // Note: AXP2101 doesn't have PWM dimming on BLDO
            // Full on/off only for this channel
        }
    }
}


// =============================================================================
// DISPLAY FUNCTIONS
// =============================================================================
void initDisplay() {
    tft.init();
    tft.setRotation(0);                 // Portrait mode for T-Watch
    tft.fillScreen(COLOR_BLACK);
    tft.setTextColor(COLOR_WHITE, COLOR_BLACK);
    tft.setTextSize(1);
    
    DEBUG_PRINTLN("[DISP] Display initialized 240x240");
}

void drawIdleScreen() {
    tft.fillScreen(COLOR_BLACK);
    
    // Header
    tft.setTextSize(2);
    tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
    tft.setCursor(40, 20);
    tft.print("PITCHCOMM");
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_GRAY, COLOR_BLACK);
    tft.setCursor(55, 45);
    tft.print("CATCHER UNIT");
    
    // Status box
    tft.drawRect(20, 70, 200, 80, COLOR_GRAY);
    
    tft.setTextColor(COLOR_GREEN, COLOR_BLACK);
    tft.setCursor(70, 85);
    tft.print("LISTENING...");
    
    tft.setTextColor(COLOR_GRAY, COLOR_BLACK);
    tft.setCursor(35, 110);
    tft.printf("%.1f MHz  SF%d", RF_FREQUENCY, RF_SPREADING);
    
    tft.setCursor(60, 130);
    tft.printf("Pkts: %lu", packetCount);
    
    // Version info
    tft.setTextColor(COLOR_DARK_GRAY, COLOR_BLACK);
    tft.setCursor(80, 220);
    tft.printf("v%s", FIRMWARE_VERSION);
    
    displayActive = false;
}


// =============================================================================
// DRAW PITCH DISPLAY - FULL SCREEN ALERT
// =============================================================================
void drawPitchDisplay(const PitchType* pitch, const LocationType* location) {
    tft.fillScreen(pitch->color);
    
    // Determine text color based on background brightness
    uint16_t textColor = COLOR_BLACK;
    if (pitch->code == 0x03 || pitch->code == 0x0A) {
        textColor = COLOR_WHITE;
    }
    
    // Draw pitch abbreviation (LARGE)
    tft.setTextSize(6);
    tft.setTextColor(textColor);
    
    int16_t abbrevWidth = strlen(pitch->abbrev) * 36;
    int16_t abbrevX = (TFT_WIDTH - abbrevWidth) / 2;
    tft.setCursor(abbrevX, 40);
    tft.print(pitch->abbrev);
    
    // Draw pitch full name
    tft.setTextSize(2);
    int16_t nameWidth = strlen(pitch->name) * 12;
    int16_t nameX = (TFT_WIDTH - nameWidth) / 2;
    tft.setCursor(nameX, 100);
    tft.print(pitch->name);
    
    // Draw location indicator
    if (location->code != 0x00) {
        drawLocationZone(location, textColor);
    }
    
    // Signal quality at bottom
    tft.setTextSize(1);
    tft.setTextColor(textColor);
    tft.setCursor(10, 220);
    tft.printf("RSSI:%ddBm SNR:%.1fdB", lastRSSI, lastSNR);
    
    displayActive = true;
    displayActiveTime = millis();
}

// =============================================================================
// DRAW LOCATION ZONE - STRIKE ZONE VISUALIZATION
// =============================================================================
void drawLocationZone(const LocationType* location, uint16_t textColor) {
    const int16_t zoneX = 60;
    const int16_t zoneY = 130;
    const int16_t zoneW = 120;
    const int16_t zoneH = 80;
    const int16_t halfW = zoneW / 2;
    const int16_t halfH = zoneH / 2;
    
    tft.drawRect(zoneX, zoneY, zoneW, zoneH, textColor);
    tft.drawFastHLine(zoneX, zoneY + halfH, zoneW, textColor);
    tft.drawFastVLine(zoneX + halfW, zoneY, zoneH, textColor);


    int16_t quadX, quadY;
    
    switch (location->code) {
        case 0x01: quadX = zoneX; quadY = zoneY; break;
        case 0x02: quadX = zoneX + halfW; quadY = zoneY; break;
        case 0x03: quadX = zoneX; quadY = zoneY + halfH; break;
        case 0x04: quadX = zoneX + halfW; quadY = zoneY + halfH; break;
        default: return;
    }
    
    // Fill target quadrant with stipple pattern
    for (int16_t y = quadY + 2; y < quadY + halfH - 2; y += 2) {
        for (int16_t x = quadX + 2; x < quadX + halfW - 2; x += 2) {
            tft.drawPixel(x, y, textColor);
        }
    }
    
    // Draw location label
    tft.setTextSize(2);
    int16_t labelWidth = strlen(location->name) * 12;
    int16_t labelX = (TFT_WIDTH - labelWidth) / 2;
    tft.setCursor(labelX, zoneY + zoneH + 8);
    tft.print(location->name);
}

// =============================================================================
// LORA INITIALIZATION
// =============================================================================
bool initLoRa() {
    DEBUG_PRINTLN("[LORA] Initializing SX1262...");
    
    // Configure SPI for LoRa
    SPI.begin(SPI_SCLK, SPI_MISO, SPI_MOSI, LORA_CS);
    
    // Reset sequence
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(10);
    
    int state = radio.begin(
        RF_FREQUENCY,
        RF_BANDWIDTH,
        RF_SPREADING,
        RF_CODING_RATE,
        RF_SYNC_WORD_LORA,
        RF_TX_POWER,
        RF_PREAMBLE_LEN
    );
    
    if (state != RADIOLIB_ERR_NONE) {
        DEBUG_PRINTF("[LORA] Init FAILED! Code: %d\n", state);
        return false;
    }
    
    radio.setDio1Action(rxISR);
    radio.setCRC(RF_CRC_ENABLE);
    
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        DEBUG_PRINTF("[LORA] startReceive FAILED! Code: %d\n", state);
        return false;
    }
    
    DEBUG_PRINTLN("[LORA] SX1262 initialized - LISTENING");
    DEBUG_PRINTF("[LORA] Freq: %.1f MHz, SF%d, BW%.0fkHz\n", 
                 RF_FREQUENCY, RF_SPREADING, RF_BANDWIDTH);
    
    return true;
}


// =============================================================================
// SEND ACK PACKET
// =============================================================================
void sendAck(uint8_t pitchCode, uint8_t locationCode) {
    uint8_t ackPacket[PACKET_SIZE];
    
    ackPacket[0] = SYNC_WORD;
    ackPacket[1] = PROTOCOL_VERSION;
    ackPacket[2] = CMD_ACK;
    ackPacket[3] = pitchCode;
    ackPacket[4] = locationCode;
    
    uint16_t crc = calcCRC16(ackPacket, 5);
    ackPacket[5] = (crc >> 8) & 0xFF;
    ackPacket[6] = crc & 0xFF;
    
    delay(ACK_DELAY_MS);
    
    int state = radio.transmit(ackPacket, PACKET_SIZE);
    
    if (state == RADIOLIB_ERR_NONE) {
        DEBUG_PRINTF("[ACK] Sent ACK for pitch 0x%02X loc 0x%02X\n", 
                     pitchCode, locationCode);
    } else {
        DEBUG_PRINTF("[ACK] Transmit failed! Code: %d\n", state);
    }
    
    radio.startReceive();
}

// =============================================================================
// LOOKUP FUNCTIONS
// =============================================================================
const PitchType* findPitchByCode(uint8_t code) {
    for (size_t i = 0; i < PITCH_COUNT; i++) {
        if (pitchTypes[i].code == code) {
            return &pitchTypes[i];
        }
    }
    return nullptr;
}

const LocationType* findLocationByCode(uint8_t code) {
    for (size_t i = 0; i < LOCATION_COUNT; i++) {
        if (locationTypes[i].code == code) {
            return &locationTypes[i];
        }
    }
    return &locationTypes[0];
}


// =============================================================================
// PROCESS RECEIVED PACKET
// =============================================================================
void processPacket() {
    int len = radio.getPacketLength();
    
    if (len != PACKET_SIZE) {
        DEBUG_PRINTF("[RX] Invalid length: %d\n", len);
        errorCount++;
        radio.startReceive();
        return;
    }
    
    int state = radio.readData(rxBuffer, PACKET_SIZE);
    
    if (state != RADIOLIB_ERR_NONE) {
        DEBUG_PRINTF("[RX] Read error: %d\n", state);
        errorCount++;
        radio.startReceive();
        return;
    }
    
    lastRSSI = radio.getRSSI();
    lastSNR = radio.getSNR();
    
    DEBUG_PRINTF("[RX] Pkt: %02X %02X %02X %02X %02X %02X %02X  RSSI:%d SNR:%.1f\n",
                 rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3],
                 rxBuffer[4], rxBuffer[5], rxBuffer[6], lastRSSI, lastSNR);
    
    // Validate sync word
    if (rxBuffer[0] != SYNC_WORD) {
        DEBUG_PRINTLN("[RX] Bad sync");
        errorCount++;
        radio.startReceive();
        return;
    }
    
    // Validate protocol version
    if (rxBuffer[1] != PROTOCOL_VERSION) {
        DEBUG_PRINTF("[RX] Version mismatch: 0x%02X\n", rxBuffer[1]);
        errorCount++;
        radio.startReceive();
        return;
    }
    
    // Validate CRC
    uint16_t rxCRC = ((uint16_t)rxBuffer[5] << 8) | rxBuffer[6];
    uint16_t calcCRCVal = calcCRC16(rxBuffer, 5);
    
    if (rxCRC != calcCRCVal) {
        DEBUG_PRINTF("[RX] CRC fail: 0x%04X vs 0x%04X\n", rxCRC, calcCRCVal);
        errorCount++;
        radio.startReceive();
        return;
    }


    // Process command
    uint8_t cmd = rxBuffer[2];
    uint8_t pitchCode = rxBuffer[3];
    uint8_t locationCode = rxBuffer[4];
    
    if (cmd == CMD_PITCH) {
        const PitchType* pitch = findPitchByCode(pitchCode);
        const LocationType* location = findLocationByCode(locationCode);
        
        if (pitch != nullptr) {
            lastPitchCode = pitchCode;
            lastLocationCode = locationCode;
            lastRxTime = millis();
            packetCount++;
            
            DEBUG_PRINTF("[RX] PITCH: %s @ %s\n", pitch->name, location->name);
            
            // Haptic feedback
            if (locationCode != 0x00) {
                hapticPattern(2, HAPTIC_PULSE_MS, 50);
            } else {
                hapticPulse(HAPTIC_PULSE_MS);
            }
            
            drawPitchDisplay(pitch, location);
            sendAck(pitchCode, locationCode);
            
        } else {
            DEBUG_PRINTF("[RX] Unknown pitch: 0x%02X\n", pitchCode);
            errorCount++;
        }
        
    } else if (cmd == CMD_HEARTBEAT) {
        DEBUG_PRINTLN("[RX] Heartbeat");
    } else {
        DEBUG_PRINTF("[RX] Unknown cmd: 0x%02X\n", cmd);
    }
    
    radio.startReceive();
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================
void checkDisplayTimeout() {
    if (displayActive && (millis() - displayActiveTime >= DISPLAY_TIMEOUT_MS)) {
        drawIdleScreen();
    }
}

void checkButton() {
    static bool lastState = HIGH;
    static unsigned long lastDebounce = 0;
    
    bool currentState = digitalRead(BTN_1);
    
    if (currentState != lastState && (millis() - lastDebounce > 50)) {
        lastDebounce = millis();
        lastState = currentState;
        
        if (currentState == LOW && displayActive) {
            drawIdleScreen();
        }
    }
}


// =============================================================================
// SETUP
// =============================================================================
void setup() {
    // USB Recovery Window
    delay(3000);
    
    // Initialize serial
    Serial.begin(SERIAL_BAUD);
    delay(100);
    
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTLN("T-DECK PITCHCOMM - CATCHER UNIT");
    DEBUG_PRINTF("Firmware: %s\n", FIRMWARE_VERSION);
    DEBUG_PRINTF("Protocol: 0x%02X\n", PROTOCOL_VERSION);
    DEBUG_PRINTLN("Hardware: T-Watch S3 + SX1262");
    DEBUG_PRINTLN("========================================\n");
    
    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    DEBUG_PRINTLN("[I2C] Bus initialized");
    
    // Initialize PMU (AXP2101)
    if (!initPMU()) {
        DEBUG_PRINTLN("[WARN] PMU init failed - using fallback");
    }
    
    // Initialize button
    pinMode(BTN_1, INPUT_PULLUP);
    
    // Initialize display
    initDisplay();
    
    // Show startup screen
    tft.fillScreen(COLOR_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_CYAN, COLOR_BLACK);
    tft.setCursor(30, 100);
    tft.print("INITIALIZING...");
    
    // Initialize LoRa
    if (!initLoRa()) {
        tft.fillScreen(COLOR_RED);
        tft.setTextColor(COLOR_WHITE, COLOR_RED);
        tft.setCursor(30, 100);
        tft.print("LORA FAILED!");
        tft.setCursor(30, 130);
        tft.print("Check hardware");
        
        DEBUG_PRINTLN("[FATAL] LoRa init failed");
        while(1) {
            hapticPulse(500);
            delay(1000);
        }
    }
    
    // Draw idle screen
    drawIdleScreen();
    
    // Startup haptic
    hapticPattern(2, 50, 100);
    
    DEBUG_PRINTLN("[SYS] Catcher unit ready\n");
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
    if (rxFlag) {
        rxFlag = false;
        processPacket();
    }
    
    checkDisplayTimeout();
    checkButton();
    
    yield();
}
