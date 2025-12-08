/*
 * T-Deck PitchComm - Catcher Unit Firmware v1.2.0
 * 
 * Hardware: LilyGO T-Watch S3 (ESP32-S3)
 * Display: 240x240 ST7789 round
 * Radio: SX1262 LoRa @ 915 MHz (integrated)
 * Haptics: DRV2605L vibration motor driver
 * 
 * Features:
 * - LoRa reception with CRC validation
 * - Haptic feedback patterns per pitch type
 * - Visual display of pitch type and location
 * - Sequence-based duplicate rejection
 * 
 * Pin Configuration (T-Watch S3):
 * - TFT: MOSI=13, SCLK=18, CS=12, DC=38, RST=N/A, BL=45
 * - LoRa SX1262: Integrated, CS=5, RST=8, BUSY=4, DIO1=9
 * - Haptic DRV2605L: I2C SDA=10, SCL=11
 * - Power: PWR_EN=15
 */

#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <RadioLib.h>

// ============================================================================
// HARDWARE PIN DEFINITIONS - T-Watch S3
// ============================================================================
#define PWR_EN          15      // Power enable

// Display
#define TFT_BL          45      // Backlight control
#define TFT_CS_PIN      12      // Display chip select

// LoRa SX1262 (integrated)
#define LORA_CS         5       // LoRa chip select
#define LORA_RST        8       // LoRa reset
#define LORA_BUSY       4       // LoRa busy indicator
#define LORA_DIO1       9       // LoRa interrupt

// Haptic Motor Driver DRV2605L
#define HAPTIC_SDA      10
#define HAPTIC_SCL      11
#define DRV2605_ADDR    0x5A    // DRV2605L I2C address

// ============================================================================
// RADIO CONFIGURATION (must match Coach Unit)
// ============================================================================
#define LORA_FREQ       915.0   // MHz (US ISM band)
#define LORA_BW         500.0   // kHz bandwidth
#define LORA_SF         7       // Spreading factor
#define LORA_CR         5       // Coding rate 4/5
#define LORA_SYNC       0x12    // Private sync word

// ============================================================================
// UI LAYOUT CONSTANTS
// ============================================================================
#define SCREEN_W        240
#define SCREEN_H        240
#define CENTER_X        120
#define CENTER_Y        120

// Mini location grid
#define MINI_GRID_SIZE  90
#define MINI_CELL       30
#define MINI_GRID_X     (CENTER_X - MINI_GRID_SIZE/2)
#define MINI_GRID_Y     150

// ============================================================================
// PITCH DEFINITIONS (must match Coach Unit)
// ============================================================================
enum PitchType {
    PITCH_NONE = 0,
    PITCH_FASTBALL = 1,
    PITCH_CURVEBALL = 2,
    PITCH_SLIDER = 3,
    PITCH_CHANGEUP = 4,
    PITCH_CUTTER = 5,
    PITCH_SINKER = 6
};

const char* PITCH_NAMES[] = {
    "NONE", "FASTBALL", "CURVE", "SLIDER", "CHANGE", "CUTTER", "SINKER"
};

const char* PITCH_SHORT[] = {
    "---", "FB", "CB", "SL", "CH", "CT", "SI"
};

// Pitch display colors (RGB565)
const uint16_t PITCH_COLORS[] = {
    TFT_DARKGREY,   // NONE
    TFT_RED,        // FASTBALL
    TFT_YELLOW,     // CURVEBALL
    TFT_CYAN,       // SLIDER
    TFT_GREEN,      // CHANGEUP
    TFT_ORANGE,     // CUTTER
    TFT_MAGENTA     // SINKER
};

// Haptic patterns per pitch (DRV2605 effect IDs)
const uint8_t HAPTIC_PATTERNS[] = {
    0,      // NONE - no vibration
    1,      // FASTBALL - Strong click
    14,     // CURVEBALL - Double click
    27,     // SLIDER - Triple click
    10,     // CHANGEUP - Soft bump
    5,      // CUTTER - Sharp tick
    17      // SINKER - Buzz
};

// Location labels
const char* LOCATION_LABELS[9] = {
    "UL", "UM", "UR",
    "ML", "MM", "MR",
    "LL", "LM", "LR"
};

// ============================================================================
// PROTOCOL PACKET STRUCTURE (must match Coach Unit)
// ============================================================================
struct PitchPacket {
    uint8_t header;
    uint8_t version;
    uint16_t sequence;
    uint8_t pitchType;
    int8_t location;
    uint8_t reserved;
    uint8_t checksum;
};

#define PACKET_HEADER   0xBB
#define PROTOCOL_VER    0x02

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
TFT_eSPI tft = TFT_eSPI();
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// ============================================================================
// STATE VARIABLES
// ============================================================================
bool radioReady = false;
bool hapticReady = false;
uint16_t lastSequence = 0xFFFF;
PitchType currentPitch = PITCH_NONE;
int8_t currentLocation = -1;
uint32_t lastRxTime = 0;
int16_t lastRSSI = 0;
float lastSNR = 0;

// ============================================================================
// DRV2605L HAPTIC DRIVER FUNCTIONS
// ============================================================================
bool drv2605_init() {
    Wire.begin(HAPTIC_SDA, HAPTIC_SCL);
    Wire.setClock(400000);
    
    // Check if DRV2605 responds
    Wire.beginTransmission(DRV2605_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("[HAPTIC] DRV2605L not found");
        return false;
    }
    
    // Reset device
    drv2605_write(0x01, 0x80);  // MODE register, reset bit
    delay(10);
    
    // Set to internal trigger mode
    drv2605_write(0x01, 0x00);  // MODE = Internal trigger
    
    // Set to LRA motor mode (most T-Watch S3 use LRA)
    drv2605_write(0x1A, 0xB6);  // FEEDBACK_CONTROL, LRA mode
    
    // Set library selection (LRA library)
    drv2605_write(0x03, 0x06);  // LIBRARY_SELECTION = LRA
    
    // Set rated voltage
    drv2605_write(0x16, 0x50);  // RATED_VOLTAGE
    
    // Set overdrive voltage
    drv2605_write(0x17, 0x89);  // OD_CLAMP
    
    Serial.println("[HAPTIC] DRV2605L initialized (LRA mode)");
    return true;
}

void drv2605_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(DRV2605_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

void drv2605_playEffect(uint8_t effect) {
    if (!hapticReady || effect == 0) return;
    
    // Set waveform sequence
    drv2605_write(0x04, effect);  // WAVEFORM_SEQUENCE[0]
    drv2605_write(0x05, 0);       // WAVEFORM_SEQUENCE[1] = end
    
    // Trigger playback
    drv2605_write(0x0C, 0x01);    // GO register
}

void playHapticForPitch(PitchType pitch) {
    if (pitch > 0 && pitch <= 6) {
        drv2605_playEffect(HAPTIC_PATTERNS[pitch]);
    }
}

// ============================================================================
// LORA INITIALIZATION
// ============================================================================
bool initLoRa() {
    Serial.print("[RADIO] Initializing SX1262... ");
    
    // T-Watch S3 SPI pins: SCLK=18, MISO=38 (or check), MOSI=13
    SPI.begin(18, 38, 13);
    
    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC, 10, 8);
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
        
        radio.setCurrentLimit(60.0);
        radio.setDio2AsRfSwitch(true);
        radio.setCRC(true);
        
        // Start receiving
        radio.startReceive();
        
        return true;
    } else {
        Serial.print("FAILED, code ");
        Serial.println(state);
        return false;
    }
}

// ============================================================================
// UI DRAWING FUNCTIONS
// ============================================================================
void drawIdleScreen() {
    tft.fillScreen(TFT_BLACK);
    
    // Draw circular border
    tft.drawCircle(CENTER_X, CENTER_Y, 118, TFT_DARKGREY);
    tft.drawCircle(CENTER_X, CENTER_Y, 117, TFT_DARKGREY);
    
    // Title
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("PITCHCOMM", CENTER_X, 20, 2);
    
    // Status
    tft.setTextDatum(MC_DATUM);
    if (radioReady) {
        tft.setTextColor(TFT_GREEN);
        tft.drawString("READY", CENTER_X, CENTER_Y, 4);
    } else {
        tft.setTextColor(TFT_RED);
        tft.drawString("NO SIGNAL", CENTER_X, CENTER_Y, 2);
    }
    
    // Mini location grid outline
    tft.drawRect(MINI_GRID_X, MINI_GRID_Y, MINI_GRID_SIZE, MINI_GRID_SIZE, TFT_DARKGREY);
}

void drawMiniGrid(int8_t highlightCell) {
    // Clear grid area
    tft.fillRect(MINI_GRID_X, MINI_GRID_Y, MINI_GRID_SIZE, MINI_GRID_SIZE, TFT_BLACK);
    
    // Draw grid lines
    for (int i = 0; i <= 3; i++) {
        int offset = i * MINI_CELL;
        tft.drawLine(MINI_GRID_X + offset, MINI_GRID_Y, 
                     MINI_GRID_X + offset, MINI_GRID_Y + MINI_GRID_SIZE, TFT_DARKGREY);
        tft.drawLine(MINI_GRID_X, MINI_GRID_Y + offset, 
                     MINI_GRID_X + MINI_GRID_SIZE, MINI_GRID_Y + offset, TFT_DARKGREY);
    }
    
    // Highlight selected cell
    if (highlightCell >= 0 && highlightCell < 9) {
        int col = highlightCell % 3;
        int row = highlightCell / 3;
        int x = MINI_GRID_X + col * MINI_CELL + 2;
        int y = MINI_GRID_Y + row * MINI_CELL + 2;
        tft.fillRect(x, y, MINI_CELL - 4, MINI_CELL - 4, TFT_GREEN);
    }
}

void displayPitch(PitchType pitch, int8_t location) {
    tft.fillScreen(TFT_BLACK);
    
    // Draw colored border ring based on pitch
    uint16_t color = PITCH_COLORS[pitch];
    for (int r = 115; r <= 119; r++) {
        tft.drawCircle(CENTER_X, CENTER_Y, r, color);
    }
    
    // Draw pitch type (large text)
    tft.setTextColor(color);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(PITCH_SHORT[pitch], CENTER_X, 70, 7);
    
    // Draw pitch name below
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(PITCH_NAMES[pitch], CENTER_X, 120, 2);
    
    // Draw mini location grid
    drawMiniGrid(location);
    
    // Draw signal info at bottom
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextDatum(BC_DATUM);
    char buf[32];
    snprintf(buf, sizeof(buf), "RSSI:%ddBm SNR:%.1f", lastRSSI, lastSNR);
    tft.drawString(buf, CENTER_X, SCREEN_H - 5, 1);
}

// ============================================================================
// PACKET PROCESSING
// ============================================================================
bool validatePacket(PitchPacket* pkt) {
    // Check header
    if (pkt->header != PACKET_HEADER) {
        Serial.println("[RX] Invalid header");
        return false;
    }
    
    // Check version
    if (pkt->version != PROTOCOL_VER) {
        Serial.printf("[RX] Version mismatch: got %d, expected %d\n", 
                      pkt->version, PROTOCOL_VER);
        return false;
    }
    
    // Verify checksum
    uint8_t* data = (uint8_t*)pkt;
    uint8_t calc = 0;
    for (int i = 0; i < sizeof(PitchPacket) - 1; i++) {
        calc ^= data[i];
    }
    if (calc != pkt->checksum) {
        Serial.printf("[RX] Checksum mismatch: calc=%02X, recv=%02X\n", 
                      calc, pkt->checksum);
        return false;
    }
    
    // Check for duplicate (same sequence number)
    if (pkt->sequence == lastSequence) {
        Serial.println("[RX] Duplicate packet rejected");
        return false;
    }
    
    // Validate pitch type
    if (pkt->pitchType > 6) {
        Serial.printf("[RX] Invalid pitch type: %d\n", pkt->pitchType);
        return false;
    }
    
    // Validate location
    if (pkt->location < -1 || pkt->location > 8) {
        Serial.printf("[RX] Invalid location: %d\n", pkt->location);
        return false;
    }
    
    return true;
}

void processReceivedPacket(uint8_t* data, size_t len) {
    if (len != sizeof(PitchPacket)) {
        Serial.printf("[RX] Wrong packet size: %d bytes\n", len);
        return;
    }
    
    PitchPacket* pkt = (PitchPacket*)data;
    
    if (!validatePacket(pkt)) {
        return;
    }
    
    // Get signal quality
    lastRSSI = radio.getRSSI();
    lastSNR = radio.getSNR();
    
    // Update state
    lastSequence = pkt->sequence;
    currentPitch = (PitchType)pkt->pitchType;
    currentLocation = pkt->location;
    lastRxTime = millis();
    
    Serial.printf("[RX] Pitch: %s, Location: %d, Seq: %d, RSSI: %d dBm\n",
                  PITCH_NAMES[currentPitch], currentLocation, 
                  pkt->sequence, lastRSSI);
    
    // Update display
    displayPitch(currentPitch, currentLocation);
    
    // Play haptic feedback
    playHapticForPitch(currentPitch);
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // ========================================
    // PHASE 1: USB Recovery Window
    // ========================================
    delay(3000);
    
    // ========================================
    // PHASE 2: Serial Initialization
    // ========================================
    Serial.begin(115200);
    delay(100);
    Serial.println("\n========================================");
    Serial.println("T-Deck PitchComm Catcher Unit v1.2.0");
    Serial.println("Hardware: T-Watch S3");
    Serial.println("========================================");
    
    // ========================================
    // PHASE 3: Power System
    // ========================================
    Serial.print("[PWR] Enabling power... ");
    pinMode(PWR_EN, OUTPUT);
    digitalWrite(PWR_EN, HIGH);
    delay(100);
    Serial.println("OK");
    
    // ========================================
    // PHASE 4: SPI Bus Initialization
    // ========================================
    Serial.print("[SPI] Initializing bus... ");
    pinMode(TFT_CS_PIN, OUTPUT);
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(TFT_CS_PIN, HIGH);
    digitalWrite(LORA_CS, HIGH);
    Serial.println("OK");
    
    // ========================================
    // PHASE 5: Display Initialization
    // ========================================
    Serial.print("[TFT] Initializing display... ");
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    tft.init();
    tft.setRotation(0);         // Portrait for round watch display
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("INIT...", CENTER_X, CENTER_Y, 2);
    Serial.println("OK");
    
    // ========================================
    // PHASE 6: Haptic Driver Initialization
    // ========================================
    Serial.print("[HAPTIC] Initializing DRV2605L... ");
    hapticReady = drv2605_init();
    if (hapticReady) {
        Serial.println("OK");
        drv2605_playEffect(1);  // Startup buzz
    } else {
        Serial.println("WARN - Haptics disabled");
    }
    
    // ========================================
    // PHASE 7: LoRa Radio Initialization
    // ========================================
    radioReady = initLoRa();
    
    // ========================================
    // PHASE 8: Draw Idle Screen
    // ========================================
    drawIdleScreen();
    
    Serial.println("========================================");
    Serial.println("BOOT COMPLETE - Listening for signals");
    Serial.println("========================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    // ========================================
    // Check for incoming packets
    // ========================================
    if (radioReady) {
        int state = radio.available();
        
        if (state > 0) {
            // Packet received
            uint8_t buffer[64];
            int len = radio.getPacketLength();
            
            if (len > 0 && len <= sizeof(buffer)) {
                state = radio.readData(buffer, len);
                
                if (state == RADIOLIB_ERR_NONE) {
                    processReceivedPacket(buffer, len);
                } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
                    Serial.println("[RX] CRC error - packet corrupted");
                } else {
                    Serial.printf("[RX] Read error: %d\n", state);
                }
            }
            
            // Restart receive mode
            radio.startReceive();
        }
    }
    
    // ========================================
    // Auto-clear display after 10 seconds
    // ========================================
    if (currentPitch != PITCH_NONE && (millis() - lastRxTime > 10000)) {
        currentPitch = PITCH_NONE;
        currentLocation = -1;
        drawIdleScreen();
    }
    
    // Small delay
    delay(10);
}
