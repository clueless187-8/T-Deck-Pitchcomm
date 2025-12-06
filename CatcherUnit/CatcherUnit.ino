/*
 * ============================================================================
 * T-DECK PITCHCOMM - CATCHER UNIT RECEIVER
 * ============================================================================
 * 
 * Production Firmware v1.0
 * 
 * Continuous receive mode with haptic feedback for pitch signal reception.
 * 
 * Hardware: LilyGO T-Watch S3 or T-Watch LoRa32
 * Radio:    Semtech SX1262 LoRa @ 915 MHz
 * Display:  ST7789 240x240 IPS
 * Feedback: Vibration motor + visual display
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

// ============================================================================
// HARDWARE CONFIGURATION - T-WATCH S3 / LORA32
// ============================================================================
// Adjust these pins for your specific watch model

// Power
#define BOARD_POWERON       21

// Display
#define TFT_BL              45

// SPI Bus
#define SPI_SCK             18
#define SPI_MISO            38
#define SPI_MOSI            13

// LoRa Radio (SX1262)
#define LORA_CS             5
#define LORA_RST            8
#define LORA_DIO1           9
#define LORA_BUSY           34

// I2C Bus
#define I2C_SDA             10
#define I2C_SCL             11

// Vibration Motor
#define MOTOR_PIN           4

// ============================================================================
// RF PARAMETERS (MUST MATCH TRANSMITTER)
// ============================================================================
#define LORA_FREQ           915.0
#define LORA_BW             250.0
#define LORA_SF             7
#define LORA_CR             5
#define LORA_POWER          22
#define LORA_PREAMBLE       8

#define PROTOCOL_VERSION    0x01
#define WDT_TIMEOUT_SEC     10

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

TFT_eSPI tft = TFT_eSPI();
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, SPI);

// System state
struct SystemState {
    bool displayOK      = false;
    bool radioOK        = false;
    bool systemReady    = false;
    uint32_t bootTime   = 0;
    uint32_t lastRX     = 0;
    uint32_t rxCount    = 0;
    uint32_t rxErrors   = 0;
    int lastRSSI        = 0;
    float lastSNR       = 0;
    uint8_t lastSeq     = 0xFF;
    String lastPitch    = "";
} sys;

// Protocol packet structure
struct __attribute__((packed)) SignalPacket {
    uint8_t  header;
    uint8_t  version;
    uint8_t  pitchCode;
    uint8_t  sequence;
    uint16_t checksum;
};

// Pitch definitions with vibration patterns
struct PitchType {
    uint8_t code;
    const char* name;
    const char* shortName;
    uint32_t color;
    int vib[6];     // {on_ms, off_ms, on_ms, off_ms, on_ms, off_ms}
};

const PitchType PITCHES[] = {
    {0x01, "FASTBALL",    "FB", TFT_RED,         {200, 0, 0, 0, 0, 0}},
    {0x02, "CURVEBALL",   "CB", TFT_BLUE,        {100, 100, 100, 0, 0, 0}},
    {0x03, "SLIDER",      "SL", TFT_GREEN,       {100, 100, 100, 100, 100, 0}},
    {0x04, "CHANGEUP",    "CH", TFT_YELLOW,      {300, 0, 0, 0, 0, 0}},
    {0x05, "CUTTER",      "CT", TFT_CYAN,        {50, 50, 50, 50, 50, 50}},
    {0x06, "SINKER",      "SI", TFT_MAGENTA,     {150, 100, 150, 0, 0, 0}},
    {0x07, "SPLITTER",    "SP", 0xFD20,          {100, 200, 100, 0, 0, 0}},
    {0x08, "KNUCKLEBALL", "KB", 0x780F,          {50, 100, 50, 100, 50, 100}},
    {0x09, "SCREWBALL",   "SC", TFT_GREENYELLOW, {200, 50, 100, 0, 0, 0}},
    {0x0A, "PITCHOUT",    "PO", TFT_WHITE,       {500, 0, 0, 0, 0, 0}},
    {0x0B, "PICKOFF",     "PK", TFT_PINK,        {100, 50, 100, 50, 200, 0}}
};
const int NUM_PITCHES = sizeof(PITCHES) / sizeof(PITCHES[0]);

// Interrupt flag
volatile bool rxFlag = false;

void IRAM_ATTR rxISR() {
    rxFlag = true;
}

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

bool initDisplay();
bool initRadio();
void drawBootScreen();
void drawMainScreen();
void drawStatusBar();
void drawPitchReceived(const PitchType* pitch, int rssi, float snr);
void processPacket();
void vibrate(const int* pattern);
uint16_t calcCRC16(uint8_t* data, size_t len);
const PitchType* findPitch(uint8_t code);

// ============================================================================
// BOOT SEQUENCE
// ============================================================================

void setup() {
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 1: USB SERIAL FIRST
    // ─────────────────────────────────────────────────────────────────────
    Serial.begin(115200);
    
    Serial.println();
    Serial.println(F("╔════════════════════════════════════════╗"));
    Serial.println(F("║   T-DECK PITCHCOMM - CATCHER UNIT      ║"));
    Serial.println(F("║        Firmware v1.0 Production        ║"));
    Serial.println(F("╚════════════════════════════════════════╝"));
    Serial.println();
    Serial.println(F(">>> 3-SECOND RECOVERY WINDOW <<<"));
    
    for (int i = 3; i > 0; i--) {
        Serial.printf("    %d...\n", i);
        delay(1000);
    }
    Serial.println(F("Proceeding...\n"));
    
    sys.bootTime = millis();
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 2: WATCHDOG
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[WDT ] "));
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    Serial.println(F("Armed"));
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 3: GPIO
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[GPIO] "));
    
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(LORA_CS, HIGH);
    
    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);
    
    Serial.println(F("Configured"));
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 4: SPI
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[SPI ] "));
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    Serial.println(F("Ready"));
    
    esp_task_wdt_reset();
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 5: DISPLAY
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[DISP] "));
    sys.displayOK = initDisplay();
    Serial.println(sys.displayOK ? F("OK") : F("FAILED"));
    
    if (sys.displayOK) drawBootScreen();
    
    esp_task_wdt_reset();
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 6: LORA RADIO
    // ─────────────────────────────────────────────────────────────────────
    Serial.print(F("[LORA] "));
    sys.radioOK = initRadio();
    Serial.println(sys.radioOK ? F("RX Mode Active") : F("FAILED"));
    
    esp_task_wdt_reset();
    
    // ─────────────────────────────────────────────────────────────────────
    // PHASE 7: READY
    // ─────────────────────────────────────────────────────────────────────
    sys.systemReady = sys.radioOK;
    
    Serial.println();
    Serial.println(F("╔════════════════════════════════════════╗"));
    Serial.printf("║  Boot: %lu ms                           \n", millis() - sys.bootTime);
    Serial.printf("║  Radio: %s                              \n", sys.radioOK ? "OK" : "FAIL");
    Serial.printf("║  System: %s                          \n", sys.systemReady ? "READY" : "DEGRADED");
    Serial.println(F("╚════════════════════════════════════════╝"));
    Serial.println();
    
    if (sys.systemReady) {
        Serial.println(F("Listening on 915 MHz...\n"));
        
        // Ready confirmation vibration
        digitalWrite(MOTOR_PIN, HIGH);
        delay(100);
        digitalWrite(MOTOR_PIN, LOW);
    }
    
    delay(1000);
    if (sys.displayOK) drawMainScreen();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    esp_task_wdt_reset();
    
    if (rxFlag) {
        rxFlag = false;
        processPacket();
    }
    
    // Periodic status update
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate > 5000) {
        lastUpdate = millis();
        if (sys.displayOK) drawStatusBar();
    }
    
    delay(10);
}

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================

bool initDisplay() {
    tft.init();
    tft.setRotation(0);  // Portrait
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    return true;
}

bool initRadio() {
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
        Serial.printf("init failed: %d\n", state);
        return false;
    }
    
    radio.setDio2AsRfSwitch(true);
    radio.setCurrentLimit(140.0);
    radio.setCRC(2);
    radio.setDio1Action(rxISR);
    
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("startReceive failed: %d\n", state);
        return false;
    }
    
    return true;
}

// ============================================================================
// DISPLAY
// ============================================================================

void drawBootScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("CATCHER RX", 120, 40, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("v1.0", 120, 80, 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Initializing...", 120, 140, 2);
}

void drawMainScreen() {
    tft.fillScreen(TFT_BLACK);
    
    // Header
    tft.fillRect(0, 0, 240, 35, TFT_NAVY);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.drawString("CATCHER RX", 120, 8, 2);
    
    // Status LED
    tft.fillCircle(220, 17, 8, sys.systemReady ? TFT_GREEN : TFT_RED);
    
    // Waiting indicator
    tft.fillRect(10, 50, 220, 120, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WAITING", 120, 100, 4);
    tft.drawString("for signal", 120, 130, 2);
    
    drawStatusBar();
}

void drawStatusBar() {
    tft.fillRect(0, 200, 240, 40, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "RX:%lu", sys.rxCount);
    tft.drawString(buf, 5, 210, 1);
    
    snprintf(buf, sizeof(buf), "RSSI:%d", sys.lastRSSI);
    tft.drawString(buf, 5, 225, 1);
    
    uint32_t up = (millis() - sys.bootTime) / 1000;
    snprintf(buf, sizeof(buf), "%lus", up);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(buf, 235, 210, 1);
}

void drawPitchReceived(const PitchType* pitch, int rssi, float snr) {
    tft.fillRect(10, 50, 220, 120, pitch->color);
    
    uint32_t textColor = (pitch->color == TFT_YELLOW || pitch->color == TFT_WHITE ||
                          pitch->color == TFT_GREENYELLOW || pitch->color == TFT_CYAN)
                          ? TFT_BLACK : TFT_WHITE;
    
    tft.setTextColor(textColor, pitch->color);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(pitch->name, 120, 90, 4);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "RSSI:%d SNR:%.1f", rssi, snr);
    tft.drawString(buf, 120, 130, 2);
    
    // Execute haptic feedback
    vibrate(pitch->vib);
    
    delay(1000);
    
    // Return to waiting
    tft.fillRect(10, 50, 220, 120, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WAITING", 120, 100, 4);
    tft.drawString("for signal", 120, 130, 2);
}

// ============================================================================
// PACKET PROCESSING
// ============================================================================

void processPacket() {
    SignalPacket pkt;
    int state = radio.readData((uint8_t*)&pkt, sizeof(pkt));
    
    int rssi = radio.getRSSI();
    float snr = radio.getSNR();
    
    sys.lastRSSI = rssi;
    sys.lastSNR = snr;
    
    // Restart RX
    radio.startReceive();
    
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RX  ] Error: %d\n", state);
        sys.rxErrors++;
        return;
    }
    
    // Validate header
    if (pkt.header != 0xBB) {
        Serial.println(F("[RX  ] Invalid header"));
        return;
    }
    
    // Validate version
    if (pkt.version != PROTOCOL_VERSION) {
        Serial.println(F("[RX  ] Version mismatch"));
        return;
    }
    
    // Validate CRC
    uint16_t crc = calcCRC16((uint8_t*)&pkt, sizeof(pkt) - 2);
    if (crc != pkt.checksum) {
        Serial.println(F("[RX  ] CRC fail"));
        sys.rxErrors++;
        return;
    }
    
    // Duplicate check
    if (pkt.sequence == sys.lastSeq) {
        Serial.println(F("[RX  ] Duplicate"));
        return;
    }
    sys.lastSeq = pkt.sequence;
    
    // Valid packet
    sys.rxCount++;
    sys.lastRX = millis();
    
    const PitchType* pitch = findPitch(pkt.pitchCode);
    
    Serial.println(F("══════════ SIGNAL RECEIVED ══════════"));
    Serial.printf("  Pitch: %s (0x%02X)\n", pitch ? pitch->name : "UNK", pkt.pitchCode);
    Serial.printf("  Seq:   %d\n", pkt.sequence);
    Serial.printf("  RSSI:  %d dBm\n", rssi);
    Serial.printf("  SNR:   %.1f dB\n", snr);
    Serial.printf("  Total: %lu\n", sys.rxCount);
    Serial.println(F("══════════════════════════════════════"));
    
    if (pitch && sys.displayOK) {
        drawPitchReceived(pitch, rssi, snr);
    } else {
        // Unknown - generic alert
        digitalWrite(MOTOR_PIN, HIGH);
        delay(300);
        digitalWrite(MOTOR_PIN, LOW);
    }
}

// ============================================================================
// UTILITIES
// ============================================================================

void vibrate(const int* pattern) {
    for (int i = 0; i < 6; i += 2) {
        if (pattern[i] > 0) {
            digitalWrite(MOTOR_PIN, HIGH);
            delay(pattern[i]);
            digitalWrite(MOTOR_PIN, LOW);
            if (pattern[i + 1] > 0) {
                delay(pattern[i + 1]);
            }
        }
    }
}

const PitchType* findPitch(uint8_t code) {
    for (int i = 0; i < NUM_PITCHES; i++) {
        if (PITCHES[i].code == code) return &PITCHES[i];
    }
    return nullptr;
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
