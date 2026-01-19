/**
 * T-Watch S3 PitchCom Receiver
 * Receives pitch signals from T-Deck via LoRa
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <XPowersLib.h>
#include <TFT_eSPI.h>
#include <RadioLib.h>

// =============================================================================
// T-Watch S3 Pin Definitions
// =============================================================================
#define I2C_SDA 10
#define I2C_SCL 11
#define TFT_BL 45

// LoRa SX1262 pins
#define LORA_MISO  4
#define LORA_MOSI  1
#define LORA_SCK   3
#define LORA_CS    5
#define LORA_RST   8
#define LORA_DIO1  9
#define LORA_BUSY  7

// DRV2605L Haptic Driver I2C Address
#define DRV2605_ADDR 0x5A

// =============================================================================
// Objects
// =============================================================================
XPowersAXP2101 pmu;
TFT_eSPI tft = TFT_eSPI();
SPIClass radioSPI(FSPI);
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, radioSPI);

// =============================================================================
// DRV2605L Haptic Driver Functions
// =============================================================================
void drv2605_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(DRV2605_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t drv2605_read(uint8_t reg) {
  Wire.beginTransmission(DRV2605_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(DRV2605_ADDR, (uint8_t)1);
  return Wire.read();
}

bool drv2605_init() {
  // Check if DRV2605 is present
  Wire.beginTransmission(DRV2605_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("DRV2605 not found!");
    return false;
  }
  
  // Reset
  drv2605_write(0x01, 0x00); // Out of standby
  delay(10);
  
  // Set to ERM motor mode
  drv2605_write(0x1A, 0x36); // ERM mode, closed loop
  
  // Set library
  drv2605_write(0x03, 0x01); // Library 1 (ERM)
  
  // Set to internal trigger mode
  drv2605_write(0x01, 0x00); // Mode: Internal trigger
  
  Serial.println("DRV2605 initialized");
  return true;
}

void vibrate(int duration) {
  // Use RTP (Real-Time Playback) mode for custom duration
  drv2605_write(0x01, 0x05); // RTP mode
  drv2605_write(0x02, 0x7F); // Full amplitude
  delay(duration);
  drv2605_write(0x02, 0x00); // Stop
  drv2605_write(0x01, 0x00); // Back to internal trigger mode
}

void vibrateEffect(uint8_t effect) {
  // Play a waveform from library
  drv2605_write(0x04, effect); // Set waveform
  drv2605_write(0x05, 0x00);   // End sequence
  drv2605_write(0x01, 0x00);   // Internal trigger mode
  drv2605_write(0x0C, 0x01);   // GO!
}

void vibratePattern(int count, int onTime, int offTime) {
  for (int i = 0; i < count; i++) {
    vibrate(onTime);
    if (i < count - 1) delay(offTime);
  }
}

void vibratePitch(uint8_t pitch) {
  switch (pitch) {
    case 0: // FB - Fastball: 1 long buzz
      vibrate(300);
      break;
    case 1: // CB - Curveball: 2 short buzzes
      vibratePattern(2, 150, 100);
      break;
    case 2: // CH - Changeup: 3 short buzzes
      vibratePattern(3, 100, 100);
      break;
    case 3: // SL - Slider: 1 short + 1 long
      vibrate(100);
      delay(100);
      vibrate(250);
      break;
    case 4: // PO - Pickoff: rapid pulses
      vibratePattern(4, 75, 75);
      break;
    default:
      vibrate(200);
      break;
  }
}

// =============================================================================
// Signal Structure
// =============================================================================
typedef struct {
  uint8_t type;
  uint8_t pitch;
  uint8_t zone;
  uint8_t pickoff;
  uint8_t thirdSign;
  uint16_t number;
} PitchSignal;

const char* pitchNames[] = {"FB", "CB", "CH", "SL", "PO"};
const uint16_t pitchColors[] = {TFT_RED, TFT_YELLOW, TFT_GREEN, TFT_CYAN, TFT_MAGENTA};

bool loraReady = false;
bool hapticReady = false;
PitchSignal lastSignal;
unsigned long lastReceived = 0;

// =============================================================================
// Display Functions
// =============================================================================
void drawStartup() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("PitchCom", 120, 80);
  tft.drawString("Receiver", 120, 110);
  tft.setTextSize(1);
  tft.setTextColor(loraReady ? TFT_GREEN : TFT_RED);
  tft.drawString(loraReady ? "LoRa: Ready" : "LoRa: FAILED", 120, 150);
  tft.setTextColor(hapticReady ? TFT_GREEN : TFT_RED);
  tft.drawString(hapticReady ? "Haptic: Ready" : "Haptic: FAILED", 120, 170);
}

void drawWaiting() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(2);
  tft.drawString("Waiting...", 120, 120);
}

void drawSignal(PitchSignal &sig) {
  if (sig.type == 1) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    tft.drawString("RESET", 120, 120);
    if (hapticReady) vibrate(500);
    return;
  }

  bool hasPitch = (sig.pitch < 5);

  if (sig.pickoff > 0 && !hasPitch) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(6);
    tft.drawString("PK" + String(sig.pickoff), 120, 120);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("#" + String(sig.number), 5, 5);
    if (hapticReady) vibratePattern(4, 75, 75);
    return;
  }

  if (sig.thirdSign > 0 && !hasPitch) {
    const char* thirdNames[] = {"", "3A", "3B", "3C", "3D"};
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLUE);
    tft.setTextSize(6);
    if (sig.thirdSign <= 4) {
      tft.drawString(thirdNames[sig.thirdSign], 120, 120);
    } else {
      tft.drawString("3?", 120, 120);
    }
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("#" + String(sig.number), 5, 5);
    if (hapticReady) vibratePattern(2, 200, 150);
    return;
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  if (hasPitch) {
    tft.setTextColor(pitchColors[sig.pitch]);
    tft.setTextSize(6);
    tft.drawString(pitchNames[sig.pitch], 120, 80);
    if (hapticReady) vibratePitch(sig.pitch);
  }
  
  if (sig.zone > 0 && sig.zone <= 9) {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(4);
    tft.drawString(String(sig.zone), 120, 150);
  }
  
  if (sig.pickoff > 0) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED);
    tft.drawString("PK" + String(sig.pickoff), 120, 200);
  }
  
  if (sig.thirdSign > 0) {
    const char* thirdNames[] = {"", "3A", "3B", "3C", "3D"};
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLUE);
    if (sig.thirdSign <= 4) {
      tft.drawString(thirdNames[sig.thirdSign], 200, 20);
    }
  }
  
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("#" + String(sig.number), 5, 5);
}

// =============================================================================
// LoRa Setup
// =============================================================================
void setFlag(void);

void setupLoRa() {
  Serial.println("[LoRa] Initializing...");
  radioSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  
  int state = radio.begin(915.0);
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] SX1262 init OK");
    radio.setSpreadingFactor(10);
    radio.setBandwidth(125.0);
    radio.setCodingRate(8);
    radio.setSyncWord(0x12);
    radio.setOutputPower(22);
    radio.setDio1Action(setFlag);
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("[LoRa] Receive mode started");
      loraReady = true;
    }
  } else {
    Serial.printf("[LoRa] Init failed: %d\n", state);
  }
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== T-Watch S3 PitchCom Receiver ===");

  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);
  
  if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
    Serial.println("PMIC: OK");
    pmu.setALDO1Voltage(3300); pmu.enableALDO1();
    pmu.setALDO2Voltage(3300); pmu.enableALDO2();
    pmu.setALDO3Voltage(3300); pmu.enableALDO3();
    pmu.setALDO4Voltage(3300); pmu.enableALDO4();
    pmu.setBLDO1Voltage(3300); pmu.enableBLDO1();
    pmu.setBLDO2Voltage(3300); pmu.enableBLDO2(); // Haptic power
    delay(100);
  } else {
    Serial.println("PMIC: FAILED");
  }

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Initialize haptic driver
  hapticReady = drv2605_init();
  
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  
  setupLoRa();
  drawStartup();
  
  // Test vibration
  if (hapticReady) {
    Serial.println("Testing vibration...");
    vibrate(200);
  }
  
  delay(2000);
  
  if (loraReady) {
    drawWaiting();
  }
  
  Serial.println("=== Ready ===\n");
}

// =============================================================================
// Loop
// =============================================================================
volatile bool receivedFlag = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;
}

void loop() {
  if (!loraReady) {
    delay(1000);
    return;
  }
  
  if (receivedFlag) {
    receivedFlag = false;
    int state = radio.readData((uint8_t*)&lastSignal, sizeof(lastSignal));
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.printf("RX: type=%d pitch=%d zone=%d pick=%d 3rd=%d #%d\n",
        lastSignal.type, lastSignal.pitch, lastSignal.zone,
        lastSignal.pickoff, lastSignal.thirdSign, lastSignal.number);
      
      drawSignal(lastSignal);
      lastReceived = millis();
    }
    
    radio.startReceive();
  }
  
  if (lastReceived > 0 && millis() - lastReceived > 30000) {
    drawWaiting();
    lastReceived = 0;
  }
  
  delay(10);
}
