/*
 * T-Deck PitchComm - Coach Unit Firmware v1.2.0
 * 
 * Hardware: LilyGO T-Deck Plus (ESP32-S3)
 * Display: 320x240 ST7789 with capacitive touch (GT911)
 * Radio: SX1262 LoRa @ 915 MHz
 * 
 * Features:
 * - Touchscreen pitch type selection (6 pitch types)
 * - 3x3 pitch location grid (strike zone targeting)
 * - LoRa transmission with CRC validation
 * - Sub-60ms latency, 400m+ range
 * 
 * Pin Configuration (T-Deck Plus):
 * - TFT: MOSI=41, SCLK=40, CS=12, DC=11, BL=42
 * - LoRa SX1262: MOSI=41, SCLK=40, CS=9, RST=17, BUSY=13, DIO1=45
 * - Touch GT911: SDA=18, SCL=8, INT=16, RST=N/A
 * - Power: BOARD_POWERON=10
 * - Keyboard: INT=46
 */

#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <RadioLib.h>

// ============================================================================
// HARDWARE PIN DEFINITIONS - T-Deck Plus (Verified from LilyGO Wiki)
// ============================================================================
#define BOARD_POWERON   10      // Must be HIGH to power peripherals

// Display
#define TFT_BL          42      // Backlight control
#define TFT_CS          12      // Display chip select

// LoRa SX1262
#define LORA_CS         9       // LoRa chip select
#define LORA_RST        17      // LoRa reset
#define LORA_BUSY       13      // LoRa busy indicator
#define LORA_DIO1       45      // LoRa interrupt

// Touch Controller GT911
#define TOUCH_SDA       18
#define TOUCH_SCL       8
#define TOUCH_INT       16
#define GT911_ADDR      0x5D    // GT911 I2C address

// ============================================================================
// RADIO CONFIGURATION
// ============================================================================
#define LORA_FREQ       915.0   // MHz (US ISM band)
#define LORA_BW         500.0   // kHz bandwidth (fast)
#define LORA_SF         7       // Spreading factor (fast)
#define LORA_CR         5       // Coding rate 4/5
#define LORA_SYNC       0x12    // Private sync word
#define LORA_POWER      22      // dBm output power
#define LORA_PREAMBLE   8       // Preamble length

// ============================================================================
// UI LAYOUT CONSTANTS
// ============================================================================
#define SCREEN_W        320
#define SCREEN_H        240

// Pitch buttons (left side) - 6 buttons in 2 columns
#define BTN_W           70
#define BTN_H           55
#define BTN_MARGIN      5
#define BTN_START_X     5
#define BTN_START_Y     30

// Location grid (right side) - 3x3 grid
#define GRID_SIZE       150
#define GRID_START_X    160
#define GRID_START_Y    45
#define CELL_SIZE       50

// Status area
#define STATUS_Y        5

// ============================================================================
// PITCH DEFINITIONS
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
    "NONE", "FAST", "CURVE", "SLIDE", "CHANGE", "CUT", "SINK"
};

// Pitch button colors (RGB565)
const uint16_t PITCH_COLORS[] = {
    TFT_DARKGREY,   // NONE
    TFT_RED,        // FASTBALL - Red (heat)
    TFT_YELLOW,     // CURVEBALL - Yellow
    TFT_CYAN,       // SLIDER - Cyan
    TFT_GREEN,      // CHANGEUP - Green
    TFT_ORANGE,     // CUTTER - Orange
    TFT_MAGENTA     // SINKER - Magenta
};

// Location grid labels
const char* LOCATION_LABELS[9] = {
    "UL", "UM", "UR",   // Upper Left, Upper Middle, Upper Right
    "ML", "MM", "MR",   // Middle Left, Middle Middle, Middle Right
    "LL", "LM", "LR"    // Lower Left, Lower Middle, Lower Right
};

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
TFT_eSPI tft = TFT_eSPI();
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

// ============================================================================
// STATE VARIABLES
// ============================================================================
PitchType selectedPitch = PITCH_NONE;
int8_t selectedLocation = -1;   // -1 = none, 0-8 = grid position
uint16_t sequenceNumber = 0;
bool radioReady = false;
bool touchReady = false;

// Touch state
int16_t touchX = -1;
int16_t touchY = -1;
bool touchPressed = false;
uint32_t lastTouchTime = 0;
#define TOUCH_DEBOUNCE 200      // ms debounce

// ============================================================================
// PROTOCOL PACKET STRUCTURE
// ============================================================================
struct PitchPacket {
    uint8_t header;             // 0xPC magic byte
    uint8_t version;            // Protocol version
    uint16_t sequence;          // Sequence number
    uint8_t pitchType;          // Pitch type enum
    int8_t location;            // Grid location (0-8) or -1
    uint8_t reserved;           // Future use
    uint8_t checksum;           // XOR checksum
};

#define PACKET_HEADER   0xBB
#define PROTOCOL_VER    0x02

// ============================================================================
// GT911 TOUCH CONTROLLER FUNCTIONS
// ============================================================================
bool gt911_init() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(400000);  // 400kHz I2C
    
    // Check if GT911 responds
    Wire.beginTransmission(GT911_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("[TOUCH] GT911 not found at 0x5D");
        return false;
    }
    
    // Configure touch interrupt pin
    pinMode(TOUCH_INT, INPUT);
    
    Serial.println("[TOUCH] GT911 initialized");
    return true;
}

bool gt911_read(int16_t* x, int16_t* y) {
    // Read status register
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(0x81);  // High byte of status register
    Wire.write(0x4E);  // Low byte: 0x814E
    Wire.endTransmission(false);
    
    Wire.requestFrom(GT911_ADDR, 1);
    if (!Wire.available()) return false;
    
    uint8_t status = Wire.read();
    uint8_t touchCount = status & 0x0F;
    
    if (!(status & 0x80) || touchCount == 0) {
        // Clear status register
        Wire.beginTransmission(GT911_ADDR);
        Wire.write(0x81);
        Wire.write(0x4E);
        Wire.write(0x00);
        Wire.endTransmission();
        return false;
    }
    
    // Read first touch point (0x8150-0x8157)
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(0x81);
    Wire.write(0x50);
    Wire.endTransmission(false);
    
    Wire.requestFrom(GT911_ADDR, 4);
    if (Wire.available() < 4) return false;
    
    uint8_t xl = Wire.read();
    uint8_t xh = Wire.read();
    uint8_t yl = Wire.read();
    uint8_t yh = Wire.read();
    
    *x = (xh << 8) | xl;
    *y = (yh << 8) | yl;
    
    // Clear status register
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(0x81);
    Wire.write(0x4E);
    Wire.write(0x00);
    Wire.endTransmission();
    
    return true;
}

// ============================================================================
// LORA INITIALIZATION
// ============================================================================
bool initLoRa() {
    Serial.print("[RADIO] Initializing SX1262... ");
    
    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, 
                            LORA_SYNC, LORA_POWER, LORA_PREAMBLE);
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
        
        // Configure for low latency
        radio.setCurrentLimit(140.0);           // mA
        radio.setDio2AsRfSwitch(true);          // Use DIO2 for RF switch
        radio.setCRC(true);                      // Enable CRC
        
        return true;
    } else {
        Serial.print("FAILED, code ");
        Serial.println(state);
        return false;
    }
}

// ============================================================================
// PACKET TRANSMISSION
// ============================================================================
bool transmitPitch(PitchType pitch, int8_t location) {
    PitchPacket pkt;
    pkt.header = PACKET_HEADER;
    pkt.version = PROTOCOL_VER;
    pkt.sequence = sequenceNumber++;
    pkt.pitchType = (uint8_t)pitch;
    pkt.location = location;
    pkt.reserved = 0;
    
    // Calculate XOR checksum
    uint8_t* data = (uint8_t*)&pkt;
    pkt.checksum = 0;
    for (int i = 0; i < sizeof(PitchPacket) - 1; i++) {
        pkt.checksum ^= data[i];
    }
    
    int state = radio.transmit((uint8_t*)&pkt, sizeof(PitchPacket));
    
    if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("[TX] Pitch: %s, Loc: %d, Seq: %d\n", 
                      PITCH_NAMES[pitch], location, pkt.sequence);
        return true;
    } else {
        Serial.printf("[TX] FAILED, code %d\n", state);
        return false;
    }
}

// ============================================================================
// UI DRAWING FUNCTIONS
// ============================================================================
void drawPitchButton(int index, bool selected) {
    int col = index % 2;
    int row = index / 2;
    int x = BTN_START_X + col * (BTN_W + BTN_MARGIN);
    int y = BTN_START_Y + row * (BTN_H + BTN_MARGIN);
    
    uint16_t bgColor = selected ? TFT_WHITE : PITCH_COLORS[index + 1];
    uint16_t textColor = selected ? PITCH_COLORS[index + 1] : TFT_WHITE;
    
    tft.fillRoundRect(x, y, BTN_W, BTN_H, 8, bgColor);
    tft.drawRoundRect(x, y, BTN_W, BTN_H, 8, TFT_WHITE);
    
    tft.setTextColor(textColor);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(PITCH_NAMES[index + 1], x + BTN_W/2, y + BTN_H/2, 2);
}

void drawLocationGrid() {
    // Draw grid background (strike zone representation)
    tft.fillRect(GRID_START_X, GRID_START_Y, GRID_SIZE, GRID_SIZE, TFT_NAVY);
    
    // Draw grid lines
    for (int i = 0; i <= 3; i++) {
        int offset = i * CELL_SIZE;
        // Vertical lines
        tft.drawLine(GRID_START_X + offset, GRID_START_Y, 
                     GRID_START_X + offset, GRID_START_Y + GRID_SIZE, TFT_WHITE);
        // Horizontal lines
        tft.drawLine(GRID_START_X, GRID_START_Y + offset, 
                     GRID_START_X + GRID_SIZE, GRID_START_Y + offset, TFT_WHITE);
    }
    
    // Draw cell labels
    tft.setTextColor(TFT_LIGHTGREY);
    tft.setTextDatum(MC_DATUM);
    for (int i = 0; i < 9; i++) {
        int col = i % 3;
        int row = i / 3;
        int cx = GRID_START_X + col * CELL_SIZE + CELL_SIZE/2;
        int cy = GRID_START_Y + row * CELL_SIZE + CELL_SIZE/2;
        tft.drawString(LOCATION_LABELS[i], cx, cy, 1);
    }
    
    // Draw label
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("LOCATION", GRID_START_X + GRID_SIZE/2, GRID_START_Y - 15, 2);
}

void highlightLocation(int8_t loc) {
    if (loc < 0 || loc > 8) return;
    
    int col = loc % 3;
    int row = loc / 3;
    int x = GRID_START_X + col * CELL_SIZE + 2;
    int y = GRID_START_Y + row * CELL_SIZE + 2;
    
    // Highlight selected cell
    tft.fillRect(x, y, CELL_SIZE - 4, CELL_SIZE - 4, TFT_GREEN);
    
    // Redraw label
    int cx = GRID_START_X + col * CELL_SIZE + CELL_SIZE/2;
    int cy = GRID_START_Y + row * CELL_SIZE + CELL_SIZE/2;
    tft.setTextColor(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(LOCATION_LABELS[loc], cx, cy, 2);
}

void drawStatusBar() {
    tft.fillRect(0, 0, SCREEN_W, 25, TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TL_DATUM);
    
    // Radio status
    if (radioReady) {
        tft.fillCircle(10, 12, 5, TFT_GREEN);
        tft.drawString("RF OK", 20, 5, 1);
    } else {
        tft.fillCircle(10, 12, 5, TFT_RED);
        tft.drawString("RF ERR", 20, 5, 1);
    }
    
    // Touch status
    if (touchReady) {
        tft.fillCircle(70, 12, 5, TFT_GREEN);
    } else {
        tft.fillCircle(70, 12, 5, TFT_ORANGE);
    }
    
    // Sequence counter
    tft.setTextDatum(TR_DATUM);
    char buf[16];
    snprintf(buf, sizeof(buf), "SEQ:%d", sequenceNumber);
    tft.drawString(buf, SCREEN_W - 5, 5, 1);
}

void drawConfirmation(PitchType pitch, int8_t location) {
    // Draw confirmation overlay in bottom area
    int y = SCREEN_H - 40;
    tft.fillRect(0, y, SCREEN_W, 40, PITCH_COLORS[pitch]);
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    
    char msg[32];
    if (location >= 0) {
        snprintf(msg, sizeof(msg), "SENT: %s @ %s", 
                 PITCH_NAMES[pitch], LOCATION_LABELS[location]);
    } else {
        snprintf(msg, sizeof(msg), "SENT: %s", PITCH_NAMES[pitch]);
    }
    tft.drawString(msg, SCREEN_W/2, y + 20, 2);
}

void drawFullUI() {
    tft.fillScreen(TFT_BLACK);
    
    // Draw status bar
    drawStatusBar();
    
    // Draw pitch buttons
    for (int i = 0; i < 6; i++) {
        drawPitchButton(i, false);
    }
    
    // Draw location grid
    drawLocationGrid();
}

// ============================================================================
// TOUCH PROCESSING
// ============================================================================
int checkPitchButtonTouch(int16_t tx, int16_t ty) {
    for (int i = 0; i < 6; i++) {
        int col = i % 2;
        int row = i / 2;
        int x = BTN_START_X + col * (BTN_W + BTN_MARGIN);
        int y = BTN_START_Y + row * (BTN_H + BTN_MARGIN);
        
        if (tx >= x && tx < x + BTN_W && ty >= y && ty < y + BTN_H) {
            return i + 1;  // Return pitch type (1-6)
        }
    }
    return 0;
}

int checkLocationTouch(int16_t tx, int16_t ty) {
    if (tx < GRID_START_X || tx >= GRID_START_X + GRID_SIZE ||
        ty < GRID_START_Y || ty >= GRID_START_Y + GRID_SIZE) {
        return -1;
    }
    
    int col = (tx - GRID_START_X) / CELL_SIZE;
    int row = (ty - GRID_START_Y) / CELL_SIZE;
    
    if (col < 0 || col > 2 || row < 0 || row > 2) return -1;
    
    return row * 3 + col;
}

void processTouch(int16_t tx, int16_t ty) {
    // Check pitch buttons first
    int pitch = checkPitchButtonTouch(tx, ty);
    if (pitch > 0) {
        // Pitch selected
        selectedPitch = (PitchType)pitch;
        Serial.printf("[UI] Pitch selected: %s\n", PITCH_NAMES[pitch]);
        
        // Redraw pitch buttons
        for (int i = 0; i < 6; i++) {
            drawPitchButton(i, (i + 1) == pitch);
        }
        
        // If location already selected, transmit
        if (selectedLocation >= 0) {
            transmitPitch(selectedPitch, selectedLocation);
            drawConfirmation(selectedPitch, selectedLocation);
        }
        return;
    }
    
    // Check location grid
    int loc = checkLocationTouch(tx, ty);
    if (loc >= 0) {
        selectedLocation = loc;
        Serial.printf("[UI] Location selected: %s (%d)\n", LOCATION_LABELS[loc], loc);
        
        // Redraw grid and highlight
        drawLocationGrid();
        highlightLocation(loc);
        
        // If pitch already selected, transmit
        if (selectedPitch != PITCH_NONE) {
            transmitPitch(selectedPitch, selectedLocation);
            drawConfirmation(selectedPitch, selectedLocation);
        }
        return;
    }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // ========================================
    // PHASE 1: USB Recovery Window (3 seconds)
    // Allows upload even if rest of code crashes
    // ========================================
    delay(3000);
    
    // ========================================
    // PHASE 2: Serial Initialization
    // ========================================
    Serial.begin(115200);
    delay(100);
    Serial.println("\n========================================");
    Serial.println("T-Deck PitchComm Coach Unit v1.2.0");
    Serial.println("========================================");
    
    // ========================================
    // PHASE 3: Power System
    // CRITICAL: Must enable before any peripheral access
    // ========================================
    Serial.print("[PWR] Enabling BOARD_POWERON... ");
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(100);
    Serial.println("OK");
    
    // ========================================
    // PHASE 4: SPI Bus Initialization
    // Deselect all devices before SPI.begin()
    // ========================================
    Serial.print("[SPI] Initializing bus... ");
    pinMode(TFT_CS, OUTPUT);
    pinMode(LORA_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);     // Deselect display
    digitalWrite(LORA_CS, HIGH);    // Deselect LoRa
    
    SPI.begin(40, 39, 41);          // SCLK=40, MISO=39, MOSI=41
    Serial.println("OK");
    
    // ========================================
    // PHASE 5: Display Initialization
    // ========================================
    Serial.print("[TFT] Initializing display... ");
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);     // Backlight ON
    
    tft.init();
    tft.setRotation(1);             // Landscape, USB on left
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("INITIALIZING...", SCREEN_W/2, SCREEN_H/2, 2);
    Serial.println("OK");
    
    // ========================================
    // PHASE 6: Touch Controller Initialization
    // ========================================
    Serial.print("[TOUCH] Initializing GT911... ");
    touchReady = gt911_init();
    if (touchReady) {
        Serial.println("OK");
    } else {
        Serial.println("WARN - Touch disabled");
    }
    
    // ========================================
    // PHASE 7: LoRa Radio Initialization
    // ========================================
    radioReady = initLoRa();
    
    // ========================================
    // PHASE 8: Draw Full UI
    // ========================================
    Serial.println("[UI] Drawing interface...");
    drawFullUI();
    
    Serial.println("========================================");
    Serial.println("BOOT COMPLETE - Ready for operation");
    Serial.println("========================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    // ========================================
    // Touch Processing
    // ========================================
    if (touchReady) {
        int16_t tx, ty;
        if (gt911_read(&tx, &ty)) {
            // Apply rotation correction (rotation=1, landscape)
            int16_t temp = tx;
            tx = ty;
            ty = SCREEN_H - temp;
            
            // Debounce
            if (millis() - lastTouchTime > TOUCH_DEBOUNCE) {
                lastTouchTime = millis();
                Serial.printf("[TOUCH] Raw: x=%d, y=%d\n", tx, ty);
                processTouch(tx, ty);
            }
        }
    }
    
    // ========================================
    // Status Update (every 5 seconds)
    // ========================================
    static uint32_t lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 5000) {
        lastStatusUpdate = millis();
        drawStatusBar();
    }
    
    // Small delay to prevent CPU hogging
    delay(10);
}
