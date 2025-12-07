/*
 * ============================================================================
 * T-DECK PITCHCOMM - COACH TRANSMITTER UNIT (Pitch + Location)
 * ============================================================================
 * HARDWARE: LilyGO T-Deck Plus (ESP32-S3 + SX1262 + CST816S touch)
 * VERSION:  1.2.0 - Added pitch location support (1-4 quadrants)
 * ============================================================================
 * 
 * INPUT MODES:
 *   Keyboard: Press pitch key (1-9,0,P) then location (1-4) or Enter for none
 *   Touch:    Tap pitch button, then tap location quadrant
 *
 * LOCATION GRID:
 *   1 = High Inside    2 = High Outside
 *   3 = Low Inside     4 = Low Outside
 *   0 = No location (center/general)
 *
 * PROTOCOL: [SYNC][VER][CMD][PITCH][LOC][CRC_H][CRC_L] = 7 bytes
 * ============================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <TFT_eSPI.h>
#include "esp_task_wdt.h"
#include "config.h"

// Debug macros
#if defined(DEBUG_BAUD) && (DEBUG_BAUD > 0)
  #define DBG(...)   Serial.print(__VA_ARGS__)
  #define DBGLN(...) Serial.println(__VA_ARGS__)
  #define DBGF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DBG(...)
  #define DBGLN(...)
  #define DBGF(...)
#endif

// Touch controller (CST816S)
#define TOUCH_I2C_ADDR   0x15
#define TOUCH_REG_POINTS  0x02
#define TOUCH_REG_XH     0x03

// Protocol
#define SYNC_WORD       0xAA
#define CMD_PITCH       0x01
#define CMD_ACK         0x81
#define CMD_CANCEL      0x02

// Packet sizes (updated for location byte)
#define PKT_TX_LEN      7
#define PKT_RX_MAX      8

// ACK/retry
#define ACK_TIMEOUT_MS  500
#define ACK_RETRIES_MAX 3
#define ACK_BACKOFF_MS  80

// Touch/input timing
#define TOUCH_DEBOUNCE_MS   200
#define LOCATION_TIMEOUT_MS 5000  // Time to select location after pitch

// ============================================================================
// HARDWARE OBJECTS
// ============================================================================
TFT_eSPI tft = TFT_eSPI();
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
    {0x0A, "Intentional",  "IW", TFT_WHITE},
    {0x0B, "Pitchout",     "PO", TFT_LIGHTGREY}
};
#define NUM_PITCHES (sizeof(PITCHES) / sizeof(PitchType))
const char PITCH_KEYS[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'p'};

// Location definitions
typedef struct {
    uint8_t code;
    const char* name;
    const char* abbrev;
} LocationType;

const LocationType LOCATIONS[] = {
    {0x00, "Center",       "--"},
    {0x01, "High Inside",  "HI"},
    {0x02, "High Outside", "HO"},
    {0x03, "Low Inside",   "LI"},
    {0x04, "Low Outside",  "LO"}
};
#define NUM_LOCATIONS 5

// ============================================================================
// STATE
// ============================================================================
uint8_t txPacket[PKT_TX_LEN] = {0};
uint8_t rxPacket[PKT_RX_MAX] = {0};

uint32_t lastTxTime = 0;
uint8_t txCount = 0;

uint8_t lastKeyCode = 0;
uint32_t lastKeyTime = 0;

bool touchPresent = false;
uint32_t lastTouchTime = 0;

// Two-stage input state
enum InputState {
    STATE_IDLE,
    STATE_AWAITING_LOCATION
};

InputState inputState = STATE_IDLE;
int8_t pendingPitchIdx = -1;
uint32_t pitchSelectedTime = 0;

// Button layouts
typedef struct {
    int16_t x, y, w, h;
    int8_t idx;  // pitch or location index, -1 if unused
} TouchButton;

TouchButton pitchButtons[12];
TouchButton locationButtons[5];  // 4 quadrants + center/skip

bool locationOverlayActive = false;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void initHardware();
void initDisplay();
void initRadio();
void initKeyboard();
void initTouch();
void initButtons();
void drawMainScreen();
void drawPitchButtons();
void drawLocationOverlay(int8_t pitchIdx);
void hideLocationOverlay();
void drawPitchSent(uint8_t pitchIdx, uint8_t locIdx);
void drawStatus(const char* msg, uint16_t color);
void readKeyboard();
void readTouch();
void selectPitch(uint8_t pitchIdx);
void selectLocation(uint8_t locIdx);
void sendPitchWithLocation(uint8_t pitchIdx, uint8_t locIdx);
bool awaitAck(uint32_t timeoutMs);
uint16_t calcCRC16(uint8_t* data, uint8_t len);
bool touchRead(uint16_t& x, uint16_t& y);
void checkInputTimeout();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(DEBUG_BAUD);
    delay(100);
    
    DBGLN("\n\n===========================================");
    DBGLN("  T-DECK PITCHCOMM - COACH UNIT v1.2.0");
    DBGLN("  Pitch + Location Support");
    DBGF("  Firmware: %s\n", FW_VERSION);
    DBGLN("===========================================\n");

    initHardware();
    initDisplay();
    initRadio();
    initKeyboard();
    initTouch();
    initButtons();

    // Watchdog
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WDT_TIMEOUT_SEC * 1000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);

    drawMainScreen();
    drawPitchButtons();
    
    DBGLN("\n[READY] Coach unit operational");
    DBGLN("Workflow: Select pitch -> Select location (1-4) or Enter/tap center\n");
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
    esp_task_wdt_reset();
    
    checkInputTimeout();
    readKeyboard();
    readTouch();

    // Passive ACK check
    if (radio.available()) {
        int state = radio.readData(rxPacket, sizeof(rxPacket));
        if (state == RADIOLIB_ERR_NONE && rxPacket[0] == SYNC_WORD && rxPacket[2] == CMD_ACK) {
            DBGLN("[RX] ACK (passive)");
            drawStatus("ACK OK", TFT_GREEN);
        }
    }
    delay(4);
}

// ============================================================================
// CHECK INPUT TIMEOUT
// ============================================================================
void checkInputTimeout() {
    if (inputState == STATE_AWAITING_LOCATION) {
        if ((millis() - pitchSelectedTime) > LOCATION_TIMEOUT_MS) {
            DBGLN("[INPUT] Location timeout - sending with no location");
            selectLocation(0);  // Default to center/no location
        }
    }
}

// ============================================================================
// INIT HARDWARE
// ============================================================================
void initHardware() {
    DBGLN("[INIT] Hardware...");

    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(100);
    DBGLN("  - BOARD_POWERON HIGH");

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    DBGLN("  - SPI initialized");

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    DBGLN("  - I2C @ 400kHz");
}

// ============================================================================
// INIT DISPLAY
// ============================================================================
void initDisplay() {
    DBGLN("[INIT] Display...");

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    delay(10);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    DBGF("  - ST7789 320x240, rotation=%d\n", tft.getRotation());
}

// ============================================================================
// INIT RADIO
// ============================================================================
void initRadio() {
    DBGLN("[INIT] Radio...");

    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                            RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
                            LORA_POWER, LORA_PREAMBLE);

    if (state != RADIOLIB_ERR_NONE) {
        DBGF("  - FAILED: code %d\n", state);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(20, 100);
        tft.print("RADIO INIT FAILED");
        while (1) {
            esp_task_wdt_reset();
            delay(1000);
        }
    }

    radio.setDio1Action(NULL);
    radio.setRxBoostedGainMode(true);
    radio.startReceive();

    DBGF("  - SX1262: %.1fMHz SF%d +%ddBm\n", LORA_FREQ, LORA_SF, LORA_POWER);
}

// ============================================================================
// INIT KEYBOARD
// ============================================================================
void initKeyboard() {
    DBGLN("[INIT] Keyboard...");
    pinMode(KB_INT, INPUT_PULLUP);

    Wire.beginTransmission(KB_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        DBGF("  - Found @ 0x%02X\n", KB_I2C_ADDR);
    } else {
        DBGLN("  - NOT DETECTED");
    }
}

// ============================================================================
// INIT TOUCH
// ============================================================================
void initTouch() {
    DBGLN("[INIT] Touch...");
    
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        touchPresent = true;
        DBGF("  - CST816S @ 0x%02X\n", TOUCH_I2C_ADDR);
    } else {
        touchPresent = false;
        DBGLN("  - NOT DETECTED");
    }
}

// ============================================================================
// INIT BUTTONS
// ============================================================================
void initButtons() {
    DBGLN("[INIT] Buttons...");
    
    // Pitch buttons: 3 columns x 4 rows
    const int16_t pBtnW = 95, pBtnH = 28;
    const int16_t pGapX = 8, pGapY = 4;
    const int16_t pStartX = 8, pStartY = 115;
    
    int idx = 0;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 3; col++) {
            pitchButtons[idx].x = pStartX + col * (pBtnW + pGapX);
            pitchButtons[idx].y = pStartY + row * (pBtnH + pGapY);
            pitchButtons[idx].w = pBtnW;
            pitchButtons[idx].h = pBtnH;
            pitchButtons[idx].idx = (idx < NUM_PITCHES) ? idx : -1;
            idx++;
        }
    }
    
    // Location buttons: 2x2 grid + center skip button
    // Overlay position (centered on screen)
    const int16_t locW = 70, locH = 50;
    const int16_t locGap = 6;
    const int16_t locStartX = 90, locStartY = 80;
    
    // Quadrant 1: High Inside (top-left)
    locationButtons[1].x = locStartX;
    locationButtons[1].y = locStartY;
    locationButtons[1].w = locW;
    locationButtons[1].h = locH;
    locationButtons[1].idx = 1;
    
    // Quadrant 2: High Outside (top-right)
    locationButtons[2].x = locStartX + locW + locGap;
    locationButtons[2].y = locStartY;
    locationButtons[2].w = locW;
    locationButtons[2].h = locH;
    locationButtons[2].idx = 2;
    
    // Quadrant 3: Low Inside (bottom-left)
    locationButtons[3].x = locStartX;
    locationButtons[3].y = locStartY + locH + locGap;
    locationButtons[3].w = locW;
    locationButtons[3].h = locH;
    locationButtons[3].idx = 3;
    
    // Quadrant 4: Low Outside (bottom-right)
    locationButtons[4].x = locStartX + locW + locGap;
    locationButtons[4].y = locStartY + locH + locGap;
    locationButtons[4].w = locW;
    locationButtons[4].h = locH;
    locationButtons[4].idx = 4;
    
    // Center/Skip button (below grid)
    locationButtons[0].x = locStartX + (locW + locGap) / 2 - 30;
    locationButtons[0].y = locStartY + 2 * (locH + locGap) + 5;
    locationButtons[0].w = locW + 20;
    locationButtons[0].h = 30;
    locationButtons[0].idx = 0;
    
    DBGF("  - %d pitch buttons, 5 location buttons\n", NUM_PITCHES);
}

// ============================================================================
// READ KEYBOARD
// ============================================================================
void readKeyboard() {
    if (digitalRead(KB_INT) != LOW) return;

    Wire.requestFrom((uint8_t)KB_I2C_ADDR, (uint8_t)1);
    if (!Wire.available()) return;

    uint8_t keyCode = Wire.read();
    if (keyCode == 0) return;
    
    uint32_t now = millis();
    if (keyCode == lastKeyCode && (now - lastKeyTime) < KB_DEBOUNCE_MS) {
        return;
    }

    lastKeyTime = now;
    lastKeyCode = keyCode;

    char key = tolower((char)keyCode);
    DBGF("[KB] Key: 0x%02X '%c'\n", keyCode, key);

    if (inputState == STATE_AWAITING_LOCATION) {
        // Expecting location 1-4 or Enter for no location
        if (key >= '1' && key <= '4') {
            selectLocation(key - '0');
            return;
        }
        if (keyCode == 0x0D || keyCode == 0x0A || key == ' ') {  // Enter or Space
            selectLocation(0);  // No location
            return;
        }
        // ESC or other key cancels
        if (keyCode == 0x1B) {  // ESC
            DBGLN("[KB] Cancelled");
            inputState = STATE_IDLE;
            pendingPitchIdx = -1;
            hideLocationOverlay();
            drawStatus("CANCELLED", TFT_DARKGREY);
            return;
        }
    } else {
        // Expecting pitch selection
        for (int i = 0; i < NUM_PITCHES; i++) {
            if (key == PITCH_KEYS[i]) {
                selectPitch(i);
                return;
            }
        }
    }
    DBGLN("[KB] Unmapped key");
}

// ============================================================================
// READ TOUCH
// ============================================================================
void readTouch() {
    if (!touchPresent) return;

    uint32_t now = millis();
    if ((now - lastTouchTime) < TOUCH_DEBOUNCE_MS) return;

    uint16_t x, y;
    if (!touchRead(x, y)) return;

    lastTouchTime = now;
    DBGF("[TOUCH] x=%d y=%d\n", x, y);

    if (locationOverlayActive) {
        // Check location buttons
        for (int i = 0; i < 5; i++) {
            TouchButton& b = locationButtons[i];
            if (x >= b.x && x < (b.x + b.w) && y >= b.y && y < (b.y + b.h)) {
                DBGF("[TOUCH] Location %d\n", b.idx);
                selectLocation(b.idx);
                return;
            }
        }
        // Touch outside overlay - cancel
        inputState = STATE_IDLE;
        pendingPitchIdx = -1;
        hideLocationOverlay();
        return;
    }
    
    // Check pitch buttons
    for (int i = 0; i < 12; i++) {
        if (pitchButtons[i].idx < 0) continue;
        TouchButton& b = pitchButtons[i];
        if (x >= b.x && x < (b.x + b.w) && y >= b.y && y < (b.y + b.h)) {
            DBGF("[TOUCH] Pitch button %d\n", b.idx);
            selectPitch(b.idx);
            return;
        }
    }
}

// ============================================================================
// TOUCH READ (with rotation)
// ============================================================================
bool touchRead(uint16_t& x, uint16_t& y) {
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(TOUCH_REG_POINTS);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)1);
    if (!Wire.available()) return false;
    
    uint8_t points = Wire.read() & 0x0F;
    if (points == 0) return false;
    
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(TOUCH_REG_XH);
    if (Wire.endTransmission(false) != 0) return false;

    Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)4);
    if (Wire.available() < 4) return false;

    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();

    uint16_t rawX = ((xh & 0x0F) << 8) | xl;
    uint16_t rawY = ((yh & 0x0F) << 8) | yl;

    if (rawX > 240 || rawY > 320) return false;

    // Rotation=1 mapping
    x = rawY;
    y = 240 - rawX;
    
    if (x >= 320) x = 319;
    if (y >= 240) y = 239;

    return true;
}

// ============================================================================
// SELECT PITCH (Stage 1)
// ============================================================================
void selectPitch(uint8_t pitchIdx) {
    if (pitchIdx >= NUM_PITCHES) return;
    
    DBGF("[INPUT] Pitch selected: %s\n", PITCHES[pitchIdx].name);
    
    pendingPitchIdx = pitchIdx;
    pitchSelectedTime = millis();
    inputState = STATE_AWAITING_LOCATION;
    
    drawLocationOverlay(pitchIdx);
}

// ============================================================================
// SELECT LOCATION (Stage 2)
// ============================================================================
void selectLocation(uint8_t locIdx) {
    if (pendingPitchIdx < 0 || pendingPitchIdx >= NUM_PITCHES) {
        inputState = STATE_IDLE;
        return;
    }
    
    if (locIdx > 4) locIdx = 0;
    
    DBGF("[INPUT] Location selected: %s\n", LOCATIONS[locIdx].name);
    
    hideLocationOverlay();
    sendPitchWithLocation(pendingPitchIdx, locIdx);
    
    inputState = STATE_IDLE;
    pendingPitchIdx = -1;
}

// ============================================================================
// SEND PITCH WITH LOCATION
// ============================================================================
void sendPitchWithLocation(uint8_t pitchIdx, uint8_t locIdx) {
    if (pitchIdx >= NUM_PITCHES) return;

    uint32_t now = millis();
    if ((now - lastTxTime) < TX_COOLDOWN_MS) {
        DBGLN("[TX] Cooldown");
        return;
    }

    const PitchType* pitch = &PITCHES[pitchIdx];
    const LocationType* loc = &LOCATIONS[locIdx];
    
    DBGF("[TX] %s @ %s (0x%02X, 0x%02X)\n", 
         pitch->name, loc->name, pitch->code, loc->code);

    // Build packet: [SYNC][VER][CMD][PITCH][LOC][CRC_H][CRC_L]
    txPacket[0] = SYNC_WORD;
    txPacket[1] = PROTOCOL_VERSION;
    txPacket[2] = CMD_PITCH;
    txPacket[3] = pitch->code;
    txPacket[4] = loc->code;

    uint16_t crc = calcCRC16(txPacket, 5);
    txPacket[5] = crc >> 8;
    txPacket[6] = crc & 0xFF;

    drawPitchSent(pitchIdx, locIdx);

    bool acked = false;
    for (uint8_t attempt = 0; attempt < ACK_RETRIES_MAX; attempt++) {
        int state = radio.transmit(txPacket, PKT_TX_LEN);
        lastTxTime = millis();
        txCount++;

        if (state == RADIOLIB_ERR_NONE) {
            DBGF("  TX #%u OK\n", attempt + 1);
            radio.startReceive();
            if (awaitAck(ACK_TIMEOUT_MS)) {
                DBGLN("  ACK received");
                acked = true;
                break;
            }
            DBGLN("  ACK timeout");
        } else {
            DBGF("  TX FAIL: %d\n", state);
        }
        delay(ACK_BACKOFF_MS);
    }

    drawStatus(acked ? "ACK OK" : "NO ACK", acked ? TFT_GREEN : TFT_YELLOW);
}

// ============================================================================
// AWAIT ACK
// ============================================================================
bool awaitAck(uint32_t timeoutMs) {
    uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        esp_task_wdt_reset();
        if (radio.available()) {
            int len = radio.readData(rxPacket, sizeof(rxPacket));
            if (len >= 4 && rxPacket[0] == SYNC_WORD && rxPacket[2] == CMD_ACK) {
                return true;
            }
        }
        delay(2);
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
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================
void drawMainScreen() {
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(60, 8);
    tft.print("PITCHCOMM");

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(85, 42);
    tft.print("COACH UNIT");

    tft.drawFastHLine(8, 68, 304, TFT_DARKGREY);

    tft.setTextSize(2);
    tft.setCursor(15, 78);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.print("READY");

    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(15, 100);
    tft.printf("%.0fMHz +%ddBm SF%d | v%s", LORA_FREQ, LORA_POWER, LORA_SF, FW_VERSION);
}

void drawPitchButtons() {
    tft.setTextSize(2);
    for (int i = 0; i < 12; i++) {
        TouchButton& b = pitchButtons[i];
        if (b.idx < 0) {
            tft.fillRoundRect(b.x, b.y, b.w, b.h, 4, TFT_BLACK);
            continue;
        }
        
        uint16_t col = PITCHES[b.idx].color;
        const char* label = PITCHES[b.idx].abbrev;
        
        tft.fillRoundRect(b.x, b.y, b.w, b.h, 4, TFT_BLACK);
        tft.drawRoundRect(b.x, b.y, b.w, b.h, 4, col);
        
        tft.setTextColor(col, TFT_BLACK);
        int16_t tx = b.x + (b.w / 2) - 12;
        int16_t ty = b.y + (b.h / 2) - 7;
        tft.setCursor(tx, ty);
        tft.print(label);
    }
}

void drawLocationOverlay(int8_t pitchIdx) {
    locationOverlayActive = true;
    
    // Semi-transparent overlay effect (dark background)
    tft.fillRect(70, 60, 180, 170, TFT_BLACK);
    tft.drawRect(70, 60, 180, 170, TFT_WHITE);
    
    // Title showing selected pitch
    tft.setTextColor(PITCHES[pitchIdx].color, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(80, 65);
    tft.print(PITCHES[pitchIdx].abbrev);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print(" Location?");
    
    // Draw location buttons
    tft.setTextSize(2);
    for (int i = 1; i <= 4; i++) {
        TouchButton& b = locationButtons[i];
        tft.fillRoundRect(b.x, b.y, b.w, b.h, 6, TFT_NAVY);
        tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, TFT_CYAN);
        
        tft.setTextColor(TFT_WHITE, TFT_NAVY);
        int16_t tx = b.x + (b.w / 2) - 12;
        int16_t ty = b.y + (b.h / 2) - 7;
        tft.setCursor(tx, ty);
        tft.print(LOCATIONS[i].abbrev);
    }
    
    // Skip/Center button
    TouchButton& skip = locationButtons[0];
    tft.fillRoundRect(skip.x, skip.y, skip.w, skip.h, 4, TFT_DARKGREY);
    tft.drawRoundRect(skip.x, skip.y, skip.w, skip.h, 4, TFT_LIGHTGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setCursor(skip.x + 12, skip.y + 8);
    tft.print("SKIP");
}

void hideLocationOverlay() {
    locationOverlayActive = false;
    
    // Redraw affected area
    tft.fillRect(70, 60, 180, 170, TFT_BLACK);
    
    // Redraw pitch buttons that were covered
    drawPitchButtons();
    
    // Redraw header elements
    tft.drawFastHLine(8, 68, 304, TFT_DARKGREY);
}

void drawPitchSent(uint8_t pitchIdx, uint8_t locIdx) {
    if (pitchIdx >= NUM_PITCHES) return;
    const PitchType* pitch = &PITCHES[pitchIdx];
    const LocationType* loc = &LOCATIONS[locIdx];

    tft.fillRect(15, 75, 200, 22, TFT_BLACK);
    
    tft.setTextSize(2);
    tft.setTextColor(pitch->color, TFT_BLACK);
    tft.setCursor(15, 78);
    tft.print(pitch->abbrev);
    
    if (locIdx > 0) {
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.print(" @ ");
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.print(loc->abbrev);
    }
}

void drawStatus(const char* msg, uint16_t color) {
    tft.fillRect(220, 75, 95, 22, TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(220, 78);
    tft.print(msg);
}
