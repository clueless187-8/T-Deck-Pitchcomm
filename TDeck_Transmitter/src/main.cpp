/*
 * BHS PitchCom V1 - T-Deck Plus Coach Device
 * LilyGo T-Deck Plus ESP32-S3 with Touch Screen
 * 
 * Touch UI - tap buttons to select and send via LoRa
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include "TouchDrvGT911.hpp"

// ============================================================================
// HARDWARE PINS (T-Deck Plus)
// ============================================================================
#define TDECK_PERI_POWERON  10
#define TDECK_TFT_BACKLIGHT 42
#define TDECK_I2C_SDA       18
#define TDECK_I2C_SCL       8
#define TDECK_TOUCH_INT     16
#define TDECK_KB_SLAVE_ADDR 0x55  // Keyboard I2C address

// Trackball pins
#define TDECK_TRACKBALL_UP    3
#define TDECK_TRACKBALL_DOWN  15
#define TDECK_TRACKBALL_LEFT  1
#define TDECK_TRACKBALL_RIGHT 2
#define TDECK_TRACKBALL_CLICK 0

// LoRa SX1262 pins (T-Deck Plus)
#define LORA_NSS    9
#define LORA_DIO1   45
#define LORA_BUSY   13
#define LORA_RST    17
#define LORA_MOSI   41
#define LORA_MISO   38
#define LORA_SCK    40

// ============================================================================
// DISPLAY
// ============================================================================
TFT_eSPI tft = TFT_eSPI();

// Colors (RGB565)
#define BG_COLOR      0x0000
#define PANEL_COLOR   0x18C3
#define TEXT_COLOR    0xFFFF
#define GREEN_COLOR   0x07E0
#define RED_COLOR     0xF800
#define CYAN_COLOR    0x07FF
#define YELLOW_COLOR  0xFFE0
#define PURPLE_COLOR  0xC01F
#define BLUE_COLOR    0x001F
#define GRAY_COLOR    0x7BEF
#define DARK_GRAY     0x31A6

// ============================================================================
// LORA SX1262
// ============================================================================
SPIClass radioSPI(HSPI);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, radioSPI);

typedef struct {
  uint8_t type;
  uint8_t pitch;
  uint8_t zone;
  uint8_t pickoff;
  uint8_t thirdSign;
  uint16_t number;
} PitchSignal;

PitchSignal currentSignal;
bool loraReady = false;

// ============================================================================
// STATE
// ============================================================================
enum PitchType { PITCH_NONE = 255, FB = 0, CB = 1, CH = 2, SL = 3, PO = 4 };

struct {
  PitchType pitch = PITCH_NONE;
  uint8_t zone = 0;
  uint8_t pickoff = 0;
  uint8_t thirdSign = 0;
  uint16_t counts[4] = {0, 0, 0, 0};
  uint16_t signalCount = 0;
  PitchType lastPitch = PITCH_NONE;
} state;

// ============================================================================
// TOUCH - GT911 I2C Capacitive Touch
// ============================================================================
TouchDrvGT911 touch;
int16_t touchX = -1, touchY = -1;
bool touched = false;
bool lastTouched = false;

bool readTouch(int16_t &x, int16_t &y) {
  int16_t touchXArray[1], touchYArray[1];
  uint8_t pointCount = touch.getPoint(touchXArray, touchYArray, 1);
  
  if (pointCount > 0) {
    x = touchXArray[0];
    y = touchYArray[0];
    Serial.printf("TOUCH: x=%d, y=%d\n", x, y);
    return true;
  }
  
  return false;
}

// ============================================================================
// TRACKBALL & KEYBOARD
// ============================================================================
int16_t cursorX = 160, cursorY = 120;
unsigned long lastMove = 0;

uint8_t readKeyboard() {
  Wire.requestFrom(TDECK_KB_SLAVE_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0;
}

void updateCursor() {
  bool moved = false;
  if (digitalRead(TDECK_TRACKBALL_UP) == LOW) { cursorY -= 5; moved = true; }
  if (digitalRead(TDECK_TRACKBALL_DOWN) == LOW) { cursorY += 5; moved = true; }
  if (digitalRead(TDECK_TRACKBALL_LEFT) == LOW) { cursorX -= 5; moved = true; }
  if (digitalRead(TDECK_TRACKBALL_RIGHT) == LOW) { cursorX += 5; moved = true; }
  
  if (moved) {
    cursorX = constrain(cursorX, 0, 319);
    cursorY = constrain(cursorY, 0, 239);
    lastMove = millis();
  }
}

void drawCursor() {
  tft.drawCircle(cursorX, cursorY, 3, YELLOW_COLOR);
  tft.drawPixel(cursorX, cursorY, YELLOW_COLOR);
}

// ============================================================================
// BUTTON DEFINITIONS
// ============================================================================
struct Button {
  int16_t x, y, w, h;
  const char* label;
  uint16_t color;
  uint8_t id;
};

// Pitch buttons - 4 buttons on left side
Button pitchBtns[] = {
  {5,   52, 75, 32, "FB", RED_COLOR, 0},
  {5,   87, 75, 32, "CB", CYAN_COLOR, 1},
  {5,  122, 75, 32, "CH", YELLOW_COLOR, 2},
  {5,  157, 75, 32, "SL", PURPLE_COLOR, 3},
};

// Zone buttons (3x3 grid)
Button zoneBtns[9];

// Pickoff buttons - right side, redistributed
Button pickoffBtns[] = {
  {205, 52, 36, 38, "PK1", RED_COLOR, 1},
  {245, 52, 36, 38, "PK2", RED_COLOR, 2},
  {285, 52, 36, 38, "PK3", RED_COLOR, 3},
};

// 1/3 Sign buttons - right side, 2x2 grid
Button thirdBtns[] = {
  {205, 95, 57, 48, "3a", BLUE_COLOR, 1},
  {265, 95, 57, 48, "3b", BLUE_COLOR, 2},
  {205, 146, 57, 48, "3c", BLUE_COLOR, 3},
  {265, 146, 57, 48, "3d", BLUE_COLOR, 4},
};

// Action buttons
Button undoBtn = {5, 195, 75, 35, "UNDO", GRAY_COLOR, 0};
Button resetBtn = {85, 195, 75, 35, "RESET", RED_COLOR, 0};
Button sendBtn = {5, 195, 320, 40, "SEND", GREEN_COLOR, 0};

void initZoneButtons() {
  int startX = 85, startY = 52;
  int cellW = 38, cellH = 46;
  int gap = 3;
  
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      int idx = row * 3 + col;
      zoneBtns[idx] = {
        (int16_t)(startX + col * (cellW + gap)),
        (int16_t)(startY + row * (cellH + gap)),
        (int16_t)cellW, (int16_t)cellH,
        "", GREEN_COLOR, (uint8_t)(idx + 1)
      };
    }
  }
}

bool inButton(Button &btn, int16_t x, int16_t y) {
  return x >= btn.x && x < btn.x + btn.w && y >= btn.y && y < btn.y + btn.h;
}

// ============================================================================
// LORA SX1262
// ============================================================================
void setupLoRa() {
  // Initialize LoRa SPI
  radioSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  
  Serial.print("[LoRa] Initializing SX1262... ");
  int state = radio.begin(915.0);  // 915 MHz for North America
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("success!");
    
    // Configure LoRa parameters
    radio.setSpreadingFactor(10);     // SF10 for good range
    radio.setBandwidth(125.0);        // 125 kHz bandwidth
    radio.setCodingRate(8);           // CR 4/8
    radio.setOutputPower(22);         // Max power 22 dBm
    radio.setPreambleLength(8);       // 8 symbol preamble
    radio.setSyncWord(0x12);          // Private network
    
    loraReady = true;
    Serial.println("[LoRa] Ready to transmit");
  } else {
    Serial.print("failed, code ");
    Serial.println(state);
    loraReady = false;
  }
}

void sendSignal() {
  if (!loraReady) return;
  
  currentSignal.type = 0;
  currentSignal.pitch = (state.pitch == PITCH_NONE) ? 255 : state.pitch;
  currentSignal.zone = state.zone;
  currentSignal.pickoff = state.pickoff;
  currentSignal.thirdSign = state.thirdSign;
  currentSignal.number = state.signalCount;
  
  int state = radio.transmit((uint8_t*)&currentSignal, sizeof(currentSignal));
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] Signal sent successfully");
  } else {
    Serial.print("[LoRa] Transmit failed, code ");
    Serial.println(state);
  }
}

void sendReset() {
  if (!loraReady) return;
  currentSignal.type = 1;
  radio.transmit((uint8_t*)&currentSignal, sizeof(currentSignal));
  Serial.println("[LoRa] Reset sent");
}

// ============================================================================
// UI DRAWING
// ============================================================================
const char* pitchNames[] = {"FB", "CB", "CH", "SL", "PO"};
const uint16_t pitchColors[] = {RED_COLOR, CYAN_COLOR, YELLOW_COLOR, PURPLE_COLOR, GRAY_COLOR};

uint16_t dimColor(uint16_t c, float f) {
  return (((uint16_t)(((c >> 11) & 0x1F) * f) << 11) |
          ((uint16_t)(((c >> 5) & 0x3F) * f) << 5) |
          (uint16_t)((c & 0x1F) * f));
}

void drawButton(Button &btn, bool selected, bool enabled = true) {
  uint16_t bgColor = selected ? btn.color : (enabled ? DARK_GRAY : 0x1082);
  uint16_t txtColor = selected ? BG_COLOR : (enabled ? btn.color : GRAY_COLOR);
  
  if (selected) {
    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 5, bgColor);
  } else {
    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 5, 0x1082);
    tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 5, enabled ? btn.color : GRAY_COLOR);
  }
  
  tft.setTextColor(txtColor);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(btn.label, btn.x + btn.w/2, btn.y + btn.h/2);
}

void drawUI() {
  Serial.println("Drawing UI...");
  tft.fillScreen(BG_COLOR);
  
  // Header
  tft.setTextSize(1);
  tft.setTextColor(GREEN_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("BHS PITCHCOM", 5, 5);
  
  // Connection status (LoRa always ready)
  tft.setTextColor(loraReady ? GREEN_COLOR : GRAY_COLOR);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(loraReady ? "LORA" : "OFF", 315, 5);
  
  // Pitch counters
  tft.setTextSize(1);
  int cx = 10;
  for (int i = 0; i < 4; i++) {
    tft.setTextColor(pitchColors[i]);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(pitchNames[i], cx, 22);
    tft.setTextSize(2);
    tft.drawNumber(state.counts[i], cx, 33);
    tft.setTextSize(1);
    cx += 50;
  }
  
  // Total
  tft.setTextColor(BLUE_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("TOT", cx + 10, 22);
  tft.setTextSize(2);
  int total = state.counts[0] + state.counts[1] + state.counts[2] + state.counts[3];
  tft.drawNumber(total, cx + 10, 33);
  
  // Pitch buttons
  tft.setTextSize(2);
  for (int i = 0; i < 4; i++) {
    drawButton(pitchBtns[i], state.pitch == i);
  }
  
  // Pickoff buttons
  tft.setTextSize(1);
  for (int i = 0; i < 3; i++) {
    drawButton(pickoffBtns[i], state.pickoff == i + 1);
  }
  
  // Zone buttons
  tft.setTextSize(2);
  for (int i = 0; i < 9; i++) {
    bool sel = (state.zone == i + 1);
    Button &btn = zoneBtns[i];
    
    if (sel) {
      tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 4, GREEN_COLOR);
    } else {
      tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 4, DARK_GRAY);
      tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 4, GRAY_COLOR);
    }
    
    tft.setTextColor(sel ? BG_COLOR : GRAY_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawNumber(i + 1, btn.x + btn.w/2, btn.y + btn.h/2);
  }
  
  // 1/3 Sign buttons (3a, 3b, 3c, 3d)
  tft.setTextSize(1);
  for (int i = 0; i < 4; i++) {
    drawButton(thirdBtns[i], state.thirdSign == i + 1);
  }
  
  // Bottom buttons - half SEND, half RESET
  tft.setTextSize(2);
  
  // Send button (left half)
  tft.fillRoundRect(5, 206, 152, 30, 6, GREEN_COLOR);
  tft.setTextColor(BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("SEND", 81, 221);
  
  // Reset button (right half)
  tft.fillRoundRect(163, 206, 152, 30, 6, RED_COLOR);
  tft.setTextColor(TEXT_COLOR);
  tft.drawString("RESET", 239, 221);
  
  Serial.println("UI drawn to screen");
}

// ============================================================================
// TOUCH HANDLING
// ============================================================================
void handleTouch(int16_t x, int16_t y) {
  Serial.printf("handleTouch: x=%d, y=%d\n", x, y);
  bool changed = false;
  
  // Pitch buttons (FB, CB, CH, SL)
  for (int i = 0; i < 4; i++) {
    if (inButton(pitchBtns[i], x, y)) {
      state.pitch = (state.pitch == i) ? PITCH_NONE : (PitchType)i;
      changed = true;
      break;
    }
  }
  
  // Zone buttons
  for (int i = 0; i < 9; i++) {
    if (inButton(zoneBtns[i], x, y)) {
      state.zone = (state.zone == i + 1) ? 0 : i + 1;
      changed = true;
      break;
    }
  }
  
  // Pickoff buttons
  for (int i = 0; i < 3; i++) {
    if (inButton(pickoffBtns[i], x, y)) {
      state.pickoff = (state.pickoff == i + 1) ? 0 : i + 1;
      changed = true;
      break;
    }
  }
  
  // 1/3 Sign buttons
  for (int i = 0; i < 4; i++) {
    if (inButton(thirdBtns[i], x, y)) {
      state.thirdSign = (state.thirdSign == i + 1) ? 0 : i + 1;
      changed = true;
      break;
    }
  }
  
  // Send button (left half)
  if (y >= 206 && y <= 236 && x >= 5 && x <= 157) {
    // Save for undo
    state.lastPitch = state.pitch;
    
    // Increment counter
    if (state.pitch != PITCH_NONE && state.pitch != PO) {
      state.counts[state.pitch]++;
    }
    state.signalCount++;
    
    // Send
    sendSignal();
    
    // Clear selection
    state.pitch = PITCH_NONE;
    state.zone = 0;
    state.pickoff = 0;
    state.thirdSign = 0;
    changed = true;
  }
  
  // Reset button (right half)
  if (y >= 206 && y <= 236 && x >= 163 && x <= 315) {
    state.pitch = PITCH_NONE;
    state.zone = 0;
    state.pickoff = 0;
    state.thirdSign = 0;
    for (int i = 0; i < 4; i++) state.counts[i] = 0;
    state.signalCount = 0;
    sendReset();
    changed = true;
  }
  
  if (changed) {
    drawUI();
  }
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for USB serial to initialize
  Serial.println();
  Serial.println("========================================");
  Serial.println("BHS PitchCom T-Deck Plus starting...");
  Serial.println("========================================");
  
  // Power on peripherals
  pinMode(TDECK_PERI_POWERON, OUTPUT);
  digitalWrite(TDECK_PERI_POWERON, HIGH);
  delay(100);
  
  // Backlight
  pinMode(TDECK_TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TDECK_TFT_BACKLIGHT, HIGH);
  
  // I2C
  Wire.begin(TDECK_I2C_SDA, TDECK_I2C_SCL);
  Wire.setClock(400000);
  
  // Display - TFT_eSPI will initialize SPI automatically
  Serial.println("Initializing display...");
  tft.init();
  tft.setRotation(1);
  
  // Initialize GT911 touch controller
  Serial.println("Initializing GT911 touch...");
  pinMode(TDECK_TOUCH_INT, INPUT);
  
  touch.setPins(-1, TDECK_TOUCH_INT);
  if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L)) {
    Serial.println("Failed to find GT911 - trying alternate address...");
    if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_H)) {
      Serial.println("GT911 touch init FAILED!");
    } else {
      Serial.println("GT911 touch init SUCCESS (0x28)");
    }
  } else {
    Serial.println("GT911 touch init SUCCESS (0x14)");
  }
  
  // Configure GT911 for T-Deck display
  touch.setMaxCoordinates(320, 240);
  touch.setSwapXY(true);
  touch.setMirrorXY(false, true);
  
  // Test pattern
  Serial.println("Drawing test pattern...");
  tft.fillScreen(TFT_RED);
  delay(1000);
  tft.fillScreen(TFT_GREEN);
  delay(1000);
  tft.fillScreen(TFT_BLUE);
  delay(1000);
  tft.fillScreen(BG_COLOR);
  
  // Init zone buttons
  initZoneButtons();
  
  // ESP-NOW
  setupLoRa();
  
  // Initial draw
  drawUI();
  
  Serial.println("Ready!");
}

unsigned long lastHeartbeat = 0;

void loop() {
  // Check for serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 't' || cmd == 'T') {
      Serial.println("\n=== TOUCH TEST ===");
      int16_t x, y;
      bool t = readTouch(x, y);
      Serial.printf("Touch result: %s\n", t ? "DETECTED" : "none");
      if (t) {
        Serial.printf("Position: (%d, %d)\n", x, y);
      }
    }
  }
  
  // Heartbeat debug every 5 seconds
  if (millis() - lastHeartbeat > 5000) {
    Serial.println("Heartbeat (send 't' to test touch)");
    lastHeartbeat = millis();
  }
  
  // Read touch
  int16_t x, y;
  touched = readTouch(x, y);
  
  // Detect tap (touch release)
  if (lastTouched && !touched && touchX >= 0) {
    Serial.printf("TAP DETECTED at (%d, %d)\n", touchX, touchY);
    handleTouch(touchX, touchY);
  }
  
  if (touched) {
    touchX = x;
    touchY = y;
  }
  
  lastTouched = touched;
  
  delay(20);
}
