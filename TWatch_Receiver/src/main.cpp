/**
 * T-Watch S3 PitchCom Receiver
 * Receives pitch signals from T-Deck via LoRa
 * Based on Meshtastic initialization patterns
 */
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <XPowersLib.h>
#include <TFT_eSPI.h>
#include <RadioLib.h>

// =============================================================================
// T-Watch S3 Pin Definitions (from Meshtastic variant.h)
// =============================================================================

// I2C for PMIC
#define I2C_SDA 10
#define I2C_SCL 11

// Display backlight
#define TFT_BL 45

// Vibration motor (T-Watch S3)
#define MOTOR_PIN 21

// LoRa SX1262 pins (from Meshtastic t-watch-s3/variant.h)
#define LORA_MISO  4
#define LORA_MOSI  1
#define LORA_SCK   3
#define LORA_CS    5
#define LORA_RST   8
#define LORA_DIO1  9   // IRQ
#define LORA_BUSY  7

// =============================================================================
// Objects
// =============================================================================
XPowersAXP2101 pmu;
TFT_eSPI tft = TFT_eSPI();
SPIClass radioSPI(FSPI);
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, radioSPI);

// =============================================================================
// Signal Structure (must match T-Deck transmitter)
// =============================================================================
typedef struct {
  uint8_t type;       // 0=pitch, 1=reset
  uint8_t pitch;      // 0=FB, 1=CB, 2=CH, 3=SL, 4=PO
  uint8_t zone;       // 1-9
  uint8_t pickoff;    // 0=none, 1-3=base
  uint8_t thirdSign;  // 0=none, 1=yes
  uint16_t number;    // signal count
} PitchSignal;

// Pitch names and colors (for colored text on black background)
const char* pitchNames[] = {"FB", "CB", "CH", "SL", "PO"};
const uint16_t pitchColors[] = {TFT_RED, TFT_YELLOW, TFT_GREEN, TFT_CYAN, TFT_MAGENTA};

bool loraReady = false;

// =============================================================================
// Vibration Functions
// =============================================================================
void vibrateShort() {
  digitalWrite(MOTOR_PIN, HIGH);
  delay(50);
  digitalWrite(MOTOR_PIN, LOW);
}

void vibrateLong() {
  digitalWrite(MOTOR_PIN, HIGH);
  delay(200);
  digitalWrite(MOTOR_PIN, LOW);
}

void vibratePattern(int count, int onTime, int offTime) {
  for (int i = 0; i < count; i++) {
    digitalWrite(MOTOR_PIN, HIGH);
    delay(onTime);
    digitalWrite(MOTOR_PIN, LOW);
    if (i < count - 1) delay(offTime);
  }
}

// Vibrate based on pitch type
void vibrateForPitch(uint8_t pitch) {
  switch (pitch) {
    case 0:  // FB - Fastball: 1 short pulse
      vibrateShort();
      break;
    case 1:  // CB - Curveball: 2 short pulses
      vibratePattern(2, 50, 100);
      break;
    case 2:  // CH - Changeup: 3 short pulses
      vibratePattern(3, 50, 100);
      break;
    case 3:  // SL - Slider: 1 long pulse
      vibrateLong();
      break;
    case 4:  // PO - Pitchout: 2 long pulses
      vibratePattern(2, 150, 100);
      break;
    default:
      vibrateShort();
      break;
  }
}

void vibrateForPickoff() {
  // Rapid triple pulse for pickoff
  vibratePattern(3, 80, 80);
}

void vibrateForThirdSign() {
  // Double long pulse for third sign
  vibratePattern(2, 200, 150);
}
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
  tft.drawString(loraReady ? "LoRa: Ready" : "LoRa: FAILED", 120, 160);
}

void drawWaiting() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(2);
  tft.drawString("Waiting...", 120, 120);
}

void drawSignal(PitchSignal &sig) {
  // Always use black background
  tft.fillScreen(TFT_BLACK);

  if (sig.type == 1) {
    // Reset signal - white text
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    tft.drawString("RESET", 120, 120);
    return;
  }

  // T-Deck sends pitch=255 for non-pitch signals
  bool hasPitch = (sig.pitch < 5);

  // Check if this is a pickoff-only signal (no pitch)
  if (sig.pickoff > 0 && !hasPitch) {
    // Pickoff display - blue text on black
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_BLUE);
    tft.setTextSize(6);
    tft.drawString("PK" + String(sig.pickoff), 120, 120);

    // Signal number (small, top left)
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("#" + String(sig.number), 5, 5);

    // Vibrate for pickoff
    vibrateForPickoff();
    return;
  }

  // Check if this is a third sign signal
  if (sig.thirdSign > 0 && !hasPitch) {
    // Third sign display - orange text on black
    const char* thirdNames[] = {"", "3A", "3B", "3C", "3D"};
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_ORANGE);
    tft.setTextSize(6);
    if (sig.thirdSign <= 4) {
      tft.drawString(thirdNames[sig.thirdSign], 120, 120);
    } else {
      tft.drawString("3?", 120, 120);
    }

    // Signal number (small, top left)
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("#" + String(sig.number), 5, 5);

    // Vibrate for third sign
    vibrateForThirdSign();
    return;
  }

  // Pitch signal - colored text on black background
  uint16_t textColor = hasPitch ? pitchColors[sig.pitch] : TFT_WHITE;

  // Pitch name - big and centered with pitch color
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(textColor);
  tft.setTextSize(6);
  if (hasPitch) {
    tft.drawString(pitchNames[sig.pitch], 120, 80);
  }

  // Zone number - same color as pitch
  if (sig.zone > 0 && sig.zone <= 9) {
    tft.setTextSize(4);
    tft.setTextColor(textColor);
    tft.drawString(String(sig.zone), 120, 150);
  }

  // Pickoff indicator (if combined with pitch) - blue text
  if (sig.pickoff > 0) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLUE);
    tft.drawString("PK" + String(sig.pickoff), 120, 200);
  }

  // Third sign indicator (if combined with pitch) - orange text
  if (sig.thirdSign > 0) {
    const char* thirdNames[] = {"", "3A", "3B", "3C", "3D"};
    tft.setTextSize(2);
    tft.setTextColor(TFT_ORANGE);
    if (sig.thirdSign <= 4) {
      tft.drawString(thirdNames[sig.thirdSign], 200, 20);
    }
  }

  // Signal number (small, top left)
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("#" + String(sig.number), 5, 5);

  // Vibrate for pitch type
  if (hasPitch) {
    vibrateForPitch(sig.pitch);
  }
}

// =============================================================================
// LoRa Setup
// =============================================================================
// Forward declaration of interrupt handler
void setFlag(void);

void setupLoRa() {
  Serial.println("[LoRa] Initializing SPI...");
  radioSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  
  Serial.println("[LoRa] Initializing SX1262...");
  int state = radio.begin(915.0);  // 915 MHz
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] SX1262 init OK");
    
    // Match T-Deck transmitter settings
    radio.setSpreadingFactor(10);
    radio.setBandwidth(125.0);
    radio.setCodingRate(8);
    radio.setSyncWord(0x12);
    radio.setOutputPower(22);
    
    // Set up interrupt on DIO1
    radio.setDio1Action(setFlag);
    
    // Start receiving
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("[LoRa] Receive mode started (interrupt)");
      loraReady = true;
    } else {
      Serial.printf("[LoRa] startReceive failed: %d\n", state);
    }
  } else {
    Serial.printf("[LoRa] Init failed: %d\n", state);
    loraReady = false;
  }
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== T-Watch S3 PitchCom Receiver ===");

  // Init I2C for PMIC
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);
  
  // Initialize PMIC
  if (pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
    Serial.println("PMIC: OK");
    
    // Enable all power rails
    pmu.setALDO1Voltage(3300); pmu.enableALDO1();  // RTC
    pmu.setALDO2Voltage(3300); pmu.enableALDO2();  // TFT Backlight
    pmu.setALDO3Voltage(3300); pmu.enableALDO3();  // Touch
    pmu.setALDO4Voltage(3300); pmu.enableALDO4();  // Radio
    pmu.setBLDO1Voltage(3300); pmu.enableBLDO1();  // SD
    pmu.setBLDO2Voltage(3300); pmu.enableBLDO2();  // Haptic
    
    delay(100);
  } else {
    Serial.println("PMIC: FAILED");
  }

  // Backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Vibration motor
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  // Display
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  
  // LoRa
  setupLoRa();
  
  // Show startup screen
  drawStartup();
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
  
  // Check if packet received via interrupt
  if (receivedFlag) {
    receivedFlag = false;
    
    // Read received data
    int state = radio.readData((uint8_t*)&lastSignal, sizeof(lastSignal));
    
    if (state == RADIOLIB_ERR_NONE) {
      // Got a valid packet!
      Serial.printf("RX: type=%d pitch=%d zone=%d pick=%d 3rd=%d #%d  RSSI=%.1f SNR=%.1f\n",
        lastSignal.type, lastSignal.pitch, lastSignal.zone,
        lastSignal.pickoff, lastSignal.thirdSign, lastSignal.number,
        radio.getRSSI(), radio.getSNR());
      
      drawSignal(lastSignal);
      lastReceived = millis();
    } else {
      Serial.printf("RX error: %d\n", state);
    }
    
    // Restart receive mode
    radio.startReceive();
  }
  
  // Show waiting screen if no signal for 30 seconds
  if (lastReceived > 0 && millis() - lastReceived > 30000) {
    drawWaiting();
    lastReceived = 0;
  }
  
  delay(10);
}
