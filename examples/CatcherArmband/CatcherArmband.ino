/*
 * ============================================================================
 * CATCHER ARMBAND RECEIVER v1.0.0 — ePaper Display
 * ============================================================================
 * Hardware:  Seeed XIAO nRF52840 + Wio-SX1262 LoRa Module
 * Display:   Seeed 2.13" Monochrome ePaper 122x250 (SSD1680)
 * Mount:     Forearm armband — low-profile linear enclosure
 * 
 * RF LINK:   Matched to T-Deck Plus Coach Transmitter
 *            915.0 MHz | SF7 | BW125 | CR4/5 | Sync 0x34
 *            6-byte packet: [0xCC][0x01][0x01][CMD][SEQ][XOR]
 * 
 * DISPLAY:   Partial refresh for sub-500ms pitch call updates
 *            Full refresh every 20 cycles to prevent ghosting
 * 
 * SPI BUS:   SHARED between SX1262 (D4 CS) and ePaper (D0 CS)
 *            Only one device active at a time — CS arbitration
 * 
 * PIN ALLOCATION (ALL 11 GPIO USED):
 *   D0  = ePaper CS (chip select)
 *   D1  = SX1262 DIO1 (RX interrupt)
 *   D2  = SX1262 RESET
 *   D3  = SX1262 BUSY
 *   D4  = SX1262 NSS (chip select)
 *   D5  = RF Switch (PE4259) — MUST be HIGH
 *   D6  = ePaper DC (data/command)
 *   D7  = ePaper BUSY
 *   D8  = SPI SCK  (shared bus)
 *   D9  = SPI MISO (SX1262 only)
 *   D10 = SPI MOSI (shared bus)
 * 
 *   ePaper RST = tied to 3V3 via 10kΩ (no GPIO needed)
 * ============================================================================
 */

#include <SPI.h>
#include <RadioLib.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// ============================================================================
// HARDWARE PIN DEFINITIONS — XIAO nRF52840
// ============================================================================

// --- LoRa Radio (SX1262 via Wio module) ---
#define LORA_NSS        D4
#define LORA_DIO1       D1
#define LORA_RESET      D2
#define LORA_BUSY       D3
#define RF_SW_PIN       D5

// --- ePaper Display (2.13" SSD1680, shared SPI) ---
#define EPAPER_CS       D0
#define EPAPER_DC       D6
#define EPAPER_BUSY     D7
#define EPAPER_RST      -1      // Tied to 3V3 via 10kΩ — no GPIO

// --- Shared SPI Bus ---
// SCK  = D8  (shared)
// MISO = D9  (LoRa only — ePaper is write-only)
// MOSI = D10 (shared)

// ============================================================================
// RF PARAMETERS — IDENTICAL TO T-DECK PLUS COACH TRANSMITTER
// ============================================================================
#define RF_FREQ         915.0
#define RF_BW           125.0
#define RF_SF           7
#define RF_CR           5
#define RF_SYNC         0x34
#define RF_POWER        22
#define RF_PREAMBLE     8
#define RF_TCXO_V       1.8

// ============================================================================
// PACKET PROTOCOL — MATCHED TO COACH TX
// ============================================================================
#define PKT_MAGIC       0xCC
#define PKT_VERSION     0x01
#define ADDR_CATCHER    0x01
#define PKT_LENGTH      6

// ============================================================================
// COMMAND TABLE — MATCHED TO T-DECK PLUS COACH TRANSMITTER
// ============================================================================
#define CMD_FB_IN       0x01
#define CMD_FB_OUT      0x02
#define CMD_CURVE       0x03
#define CMD_CHANGE      0x04
#define CMD_SLIDER      0x05
#define CMD_CUTTER      0x06
#define CMD_SPLIT       0x07
#define CMD_SCREW       0x08
#define CMD_PICK1       0x09
#define CMD_PICK2       0x0A
#define CMD_PITCHOUT    0x10
#define CMD_TIMEOUT     0xFF

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================
#define PARTIAL_REFRESH_LIMIT   20    // Full refresh every N partial updates
#define DISPLAY_HOLD_MS         8000  // Hold pitch call for 8 seconds
#define SCREEN_WIDTH            250
#define SCREEN_HEIGHT           122

// ============================================================================
// DEVICE INSTANCES
// ============================================================================

// LoRa radio — SX1262 on shared SPI
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RESET, LORA_BUSY);

// ePaper display — 2.13" BN (black/white, SSD1680 driver)
// Constructor: GxEPD2_213_BN(CS, DC, RST, BUSY)
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
    GxEPD2_213_BN(EPAPER_CS, EPAPER_DC, EPAPER_RST, EPAPER_BUSY)
);

// ============================================================================
// STATE TRACKING
// ============================================================================
volatile bool rxFlag = false;
uint8_t lastSeq = 0xFF;
unsigned long lastCallTime = 0;
bool displayingCall = false;
int partialCount = 0;
int16_t lastRSSI = 0;
bool systemReady = false;

// ============================================================================
// RX INTERRUPT HANDLER
// ============================================================================
void rxISR(void) {
    rxFlag = true;
}

// ============================================================================
// SPI BUS ARBITRATION
// ============================================================================
// Critical: only one SPI device can be active at a time.
// Before talking to ePaper, deselect LoRa. Before talking to LoRa, deselect ePaper.

void selectLoRa() {
    digitalWrite(EPAPER_CS, HIGH);  // Deselect ePaper
    // LoRa CS is managed by RadioLib internally
}

void selectEPaper() {
    digitalWrite(LORA_NSS, HIGH);   // Deselect LoRa
    // ePaper CS is managed by GxEPD2 internally
}

// ============================================================================
// PITCH DECODE — RETURNS DISPLAY STRINGS
// ============================================================================
struct PitchInfo {
    const char* line1;      // Primary call (large text)
    const char* line2;      // Detail line (smaller)
    bool urgent;            // Inverted display for urgent calls
};

PitchInfo decodePitch(uint8_t cmd) {
    switch (cmd) {
        case CMD_FB_IN:     return {"FASTBALL",  "INSIDE",      false};
        case CMD_FB_OUT:    return {"FASTBALL",  "OUTSIDE",     false};
        case CMD_CURVE:     return {"CURVE",     "BALL",        false};
        case CMD_CHANGE:    return {"CHANGE",    "UP",          false};
        case CMD_SLIDER:    return {"SLIDER",    "",            false};
        case CMD_CUTTER:    return {"CUTTER",    "",            false};
        case CMD_SPLIT:     return {"SPLITTER",  "",            false};
        case CMD_SCREW:     return {"SCREW",     "BALL",        false};
        case CMD_PICK1:     return {"PICKOFF",   "1ST BASE",    true};
        case CMD_PICK2:     return {"PICKOFF",   "2ND BASE",    true};
        case CMD_PITCHOUT:  return {"PITCH",     "OUT!",        true};
        case CMD_TIMEOUT:   return {"TIME",      "OUT",         true};
        default:            return {"???",       "UNKNOWN",     false};
    }
}

// ============================================================================
// ePAPER DISPLAY FUNCTIONS
// ============================================================================

void displayBootScreen() {
    selectEPaper();
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        // Title
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Center "PITCHCOMM" 
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds("PITCHCOMM", 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 45);
        display.print("PITCHCOMM");
        
        // Subtitle
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds("ARMBAND RX v1.0", 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 72);
        display.print("ARMBAND RX v1.0");
        
        // Frequency info
        display.getTextBounds("915 MHz LoRa", 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 95);
        display.print("915 MHz LoRa");
        
        // Border
        display.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, GxEPD_BLACK);
        display.drawRect(4, 4, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8, GxEPD_BLACK);
        
    } while (display.nextPage());
    
    partialCount = 0;  // Reset after full refresh
}

void displayStandby() {
    selectEPaper();
    
    // Use partial refresh for standby screen
    if (partialCount >= PARTIAL_REFRESH_LIMIT) {
        display.setFullWindow();
        partialCount = 0;
    } else {
        display.setPartialWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        partialCount++;
    }
    
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds("READY", 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 55);
        display.print("READY");
        
        // Signal indicator
        display.setFont(&FreeSans9pt7b);
        if (lastRSSI != 0) {
            char rssiStr[20];
            snprintf(rssiStr, sizeof(rssiStr), "RSSI: %d dBm", lastRSSI);
            display.getTextBounds(rssiStr, 0, 0, &x1, &y1, &w, &h);
            display.setCursor((SCREEN_WIDTH - w) / 2, 85);
            display.print(rssiStr);
        } else {
            display.getTextBounds("Awaiting signal...", 0, 0, &x1, &y1, &w, &h);
            display.setCursor((SCREEN_WIDTH - w) / 2, 85);
            display.print("Awaiting signal...");
        }
        
        // Thin border
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GxEPD_BLACK);
        
    } while (display.nextPage());
}

void displayPitchCall(PitchInfo pitch) {
    selectEPaper();
    
    // Force full refresh periodically to clear ghosting
    if (partialCount >= PARTIAL_REFRESH_LIMIT) {
        display.setFullWindow();
        partialCount = 0;
    } else {
        display.setPartialWindow(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        partialCount++;
    }
    
    display.firstPage();
    do {
        if (pitch.urgent) {
            // INVERTED — white text on black background for urgency
            display.fillScreen(GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE);
        } else {
            display.fillScreen(GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
        }
        
        int16_t x1, y1;
        uint16_t w, h;
        
        // Primary pitch call — LARGE
        display.setFont(&FreeSansBold24pt7b);
        display.getTextBounds(pitch.line1, 0, 0, &x1, &y1, &w, &h);
        int16_t primaryX = (SCREEN_WIDTH - w) / 2;
        int16_t primaryY;
        
        if (strlen(pitch.line2) > 0) {
            primaryY = 52;  // Higher if there's a second line
        } else {
            primaryY = 70;  // Centered vertically if single line
        }
        display.setCursor(primaryX, primaryY);
        display.print(pitch.line1);
        
        // Secondary detail line
        if (strlen(pitch.line2) > 0) {
            display.setFont(&FreeSansBold12pt7b);
            display.getTextBounds(pitch.line2, 0, 0, &x1, &y1, &w, &h);
            display.setCursor((SCREEN_WIDTH - w) / 2, 90);
            display.print(pitch.line2);
        }
        
        // RSSI bar in bottom-right corner
        display.setFont(NULL);  // Default 6x8 font
        char rssiStr[12];
        snprintf(rssiStr, sizeof(rssiStr), "%ddBm", lastRSSI);
        if (pitch.urgent) {
            display.setTextColor(GxEPD_WHITE);
        }
        display.setCursor(SCREEN_WIDTH - 48, SCREEN_HEIGHT - 12);
        display.print(rssiStr);
        
        // Border — double for urgent
        if (pitch.urgent) {
            display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GxEPD_WHITE);
            display.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, GxEPD_WHITE);
            display.drawRect(4, 4, SCREEN_WIDTH - 8, SCREEN_HEIGHT - 8, GxEPD_WHITE);
        } else {
            display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GxEPD_BLACK);
            display.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, GxEPD_BLACK);
        }
        
    } while (display.nextPage());
}

void displayError(const char* msg) {
    selectEPaper();
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds("RF ERROR", 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 40);
        display.print("RF ERROR");
        
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, 70);
        display.print(msg);
        
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GxEPD_BLACK);
    } while (display.nextPage());
    
    partialCount = 0;
}

// ============================================================================
// LORA INITIALIZATION
// ============================================================================
bool initLoRa() {
    selectLoRa();
    
    // PE4259 RF switch — MANDATORY
    pinMode(RF_SW_PIN, OUTPUT);
    digitalWrite(RF_SW_PIN, HIGH);
    
    Serial.print("[LORA] Initializing SX1262...");
    int state = radio.begin(RF_FREQ, RF_BW, RF_SF, RF_CR, RF_SYNC, RF_POWER, RF_PREAMBLE, RF_TCXO_V);
    
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print(" FAILED: ");
        Serial.println(state);
        return false;
    }
    Serial.println(" OK");
    
    // DIO2 as RF switch control — CRITICAL for Wio-SX1262
    radio.setDio2AsRfSwitch(true);
    
    // Enable CRC (2-byte for compatibility with all receivers)
    radio.setCRC(2);
    
    // Set interrupt on DIO1
    radio.setPacketReceivedAction(rxISR);
    
    // Start continuous receive
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("[LORA] RX start failed: ");
        Serial.println(state);
        return false;
    }
    
    Serial.println("[LORA] RX active — listening on 915.0 MHz");
    return true;
}

// ============================================================================
// PACKET VALIDATION
// ============================================================================
bool validatePacket(uint8_t* data, size_t len) {
    if (len != PKT_LENGTH) return false;
    if (data[0] != PKT_MAGIC) return false;
    if (data[1] != PKT_VERSION) return false;
    if (data[2] != ADDR_CATCHER) return false;
    
    // XOR checksum over bytes 0..4
    uint8_t xorCheck = 0;
    for (int i = 0; i < PKT_LENGTH - 1; i++) {
        xorCheck ^= data[i];
    }
    return (xorCheck == data[PKT_LENGTH - 1]);
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("============================================");
    Serial.println("  CATCHER ARMBAND RECEIVER v1.0.0");
    Serial.println("  2.13\" ePaper | XIAO nRF52840 | SX1262");
    Serial.println("============================================");
    
    // Initialize CS pins HIGH (deselected) before SPI starts
    pinMode(EPAPER_CS, OUTPUT);
    digitalWrite(EPAPER_CS, HIGH);
    pinMode(LORA_NSS, OUTPUT);
    digitalWrite(LORA_NSS, HIGH);
    
    // Initialize SPI bus
    SPI.begin();
    
    // Initialize ePaper display
    Serial.print("[DISP] Initializing 2.13\" ePaper...");
    selectEPaper();
    display.init(0);    // 0 = no debug output on serial
    display.setRotation(1);   // Landscape — 250 wide × 122 tall
    Serial.println(" OK");
    
    // Boot screen (full refresh)
    displayBootScreen();
    delay(2000);
    
    // Initialize LoRa
    if (!initLoRa()) {
        displayError("SX1262 init failed!");
        while (1) { delay(1000); }  // Halt
    }
    
    // Show standby screen
    displayStandby();
    systemReady = true;
    
    Serial.println("[SYS] System ready — awaiting pitch calls");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    // Check for received packet
    if (rxFlag) {
        rxFlag = false;
        
        // Temporarily select LoRa for packet read
        selectLoRa();
        
        uint8_t data[16];
        size_t len = sizeof(data);
        int state = radio.readData(data, len);
        
        if (state == RADIOLIB_ERR_NONE) {
            lastRSSI = radio.getRSSI();
            
            Serial.print("[RX] Packet: ");
            for (size_t i = 0; i < len && i < 8; i++) {
                Serial.print(data[i], HEX);
                Serial.print(" ");
            }
            Serial.print(" RSSI=");
            Serial.print(lastRSSI);
            Serial.println(" dBm");
            
            if (validatePacket(data, len)) {
                uint8_t cmd = data[3];
                uint8_t seq = data[4];
                
                // Duplicate suppression — coach sends triple-redundant packets
                if (seq != lastSeq) {
                    lastSeq = seq;
                    
                    PitchInfo pitch = decodePitch(cmd);
                    
                    Serial.print("[CALL] ");
                    Serial.print(pitch.line1);
                    Serial.print(" ");
                    Serial.println(pitch.line2);
                    
                    // Update ePaper display with pitch call
                    displayPitchCall(pitch);
                    
                    lastCallTime = millis();
                    displayingCall = true;
                }
            } else {
                Serial.println("[RX] Invalid packet — checksum/format mismatch");
            }
        } else {
            Serial.print("[RX] Read error: ");
            Serial.println(state);
        }
        
        // Restart receive
        selectLoRa();
        radio.startReceive();
    }
    
    // Revert to standby after hold time expires
    if (displayingCall && (millis() - lastCallTime > DISPLAY_HOLD_MS)) {
        displayStandby();
        displayingCall = false;
        
        // Restart receive after display update
        selectLoRa();
        radio.startReceive();
    }
    
    // Low-power idle
    delay(10);
}
