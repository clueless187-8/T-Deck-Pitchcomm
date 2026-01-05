/**
 * Heltec Wireless Stick Lite V3 - PitchComm Receiver
 *
 * Smallest receiver for pitch signals from T-Deck
 * Uses external 0.49" OLED (64x32 SSD1306) via I2C
 *
 * Hardware: ESP32-S3 + SX1262 (built-in) + External 64x32 OLED
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <RadioLib.h>

// =============================================================================
// Pin Definitions - Heltec Wireless Stick Lite V3
// =============================================================================

// External OLED Display (I2C) - Connect to these pins
// You can change these to any available GPIO
#define OLED_SDA        17
#define OLED_SCL        18

// LoRa SX1262 pins (built into Heltec Stick Lite V3)
#define LORA_MISO       11
#define LORA_MOSI       10
#define LORA_SCK        9
#define LORA_CS         8
#define LORA_RST        12
#define LORA_DIO1       14
#define LORA_BUSY       13

// LED (built-in)
#define LED_PIN         35

// =============================================================================
// Objects
// =============================================================================

// 0.49" OLED Display - 64x32 SSD1306 I2C (Address 0x3C)
U8G2_SSD1306_64X32_1F_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

// LoRa Radio
SPIClass radioSPI(FSPI);
SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, radioSPI);

// =============================================================================
// Signal Structure (must match T-Deck transmitter)
// =============================================================================
typedef struct {
  uint8_t type;       // 0=pitch, 1=reset
  uint8_t pitch;      // 0=FB, 1=CB, 2=CH, 3=SL, 4=PO, 255=none
  uint8_t zone;       // 1-9
  uint8_t pickoff;    // 0=none, 1-3=base
  uint8_t thirdSign;  // 0=none, 1-4=A/B/C/D
  uint16_t number;    // signal count
} PitchSignal;

const char* pitchNames[] = {"FB", "CB", "CH", "SL", "PO"};

bool loraReady = false;
PitchSignal lastSignal;
unsigned long lastReceived = 0;
volatile bool receivedFlag = false;

// =============================================================================
// Display Functions (optimized for tiny 64x32 OLED)
// =============================================================================

void drawStartup() {
  display.clearBuffer();
  display.setFont(u8g2_font_helvB08_tr);
  display.drawStr(4, 12, "PitchComm");
  display.setFont(u8g2_font_5x7_tr);
  display.drawStr(8, 28, loraReady ? "LoRa OK" : "LoRa FAIL");
  display.sendBuffer();
}

void drawWaiting() {
  display.clearBuffer();
  display.setFont(u8g2_font_helvB08_tr);
  display.drawStr(4, 20, "Waiting");
  display.sendBuffer();
}

void drawSignal(PitchSignal &sig) {
  display.clearBuffer();

  if (sig.type == 1) {
    // Reset signal
    display.setFont(u8g2_font_helvB12_tr);
    display.drawStr(2, 22, "RESET");
    display.sendBuffer();
    return;
  }

  bool hasPitch = (sig.pitch < 5);

  // Pickoff-only signal
  if (sig.pickoff > 0 && !hasPitch) {
    display.setFont(u8g2_font_helvB18_tr);
    char pkStr[5];
    snprintf(pkStr, sizeof(pkStr), "PK%d", sig.pickoff);
    display.drawStr(4, 26, pkStr);
    display.sendBuffer();
    return;
  }

  // Third sign only
  if (sig.thirdSign > 0 && !hasPitch) {
    display.setFont(u8g2_font_helvB18_tr);
    const char* thirdNames[] = {"", "3A", "3B", "3C", "3D"};
    if (sig.thirdSign <= 4) {
      display.drawStr(14, 26, thirdNames[sig.thirdSign]);
    }
    display.sendBuffer();
    return;
  }

  // Main pitch display
  if (hasPitch) {
    // Large pitch name on left side
    display.setFont(u8g2_font_helvB18_tr);
    display.drawStr(0, 26, pitchNames[sig.pitch]);

    // Zone number on right side (if present)
    if (sig.zone > 0 && sig.zone <= 9) {
      display.setFont(u8g2_font_helvB14_tr);
      char zoneStr[2];
      snprintf(zoneStr, sizeof(zoneStr), "%d", sig.zone);
      display.drawStr(50, 24, zoneStr);
    }

    // Small pickoff indicator at top-right
    if (sig.pickoff > 0) {
      display.setFont(u8g2_font_4x6_tr);
      char pkStr[3];
      snprintf(pkStr, sizeof(pkStr), "P%d", sig.pickoff);
      display.drawStr(50, 6, pkStr);
    }

    // Small third sign indicator
    if (sig.thirdSign > 0 && sig.thirdSign <= 4) {
      display.setFont(u8g2_font_4x6_tr);
      char tsStr[3];
      snprintf(tsStr, sizeof(tsStr), "3%c", 'A' + sig.thirdSign - 1);
      display.drawStr(50, 32, tsStr);
    }
  }

  display.sendBuffer();
}

// =============================================================================
// LoRa Interrupt Handler
// =============================================================================
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  receivedFlag = true;
}

// =============================================================================
// LoRa Setup
// =============================================================================
void setupLoRa() {
  Serial.println("[LoRa] Init SPI...");
  radioSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  Serial.println("[LoRa] Init SX1262...");
  int state = radio.begin(915.0);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] OK");

    // Match T-Deck transmitter settings
    radio.setSpreadingFactor(10);
    radio.setBandwidth(125.0);
    radio.setCodingRate(8);
    radio.setSyncWord(0x12);
    radio.setOutputPower(22);
    radio.setPreambleLength(8);

    radio.setDio1Action(setFlag);

    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("[LoRa] RX mode");
      loraReady = true;
    } else {
      Serial.printf("[LoRa] RX fail: %d\n", state);
    }
  } else {
    Serial.printf("[LoRa] Init fail: %d\n", state);
    loraReady = false;
  }
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Heltec Stick Lite V3 Receiver ===");

  // LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize I2C for external OLED
  Wire.begin(OLED_SDA, OLED_SCL);

  // Initialize OLED display
  Serial.println("[OLED] Init...");
  if (display.begin()) {
    Serial.println("[OLED] OK");
    display.setContrast(255);
  } else {
    Serial.println("[OLED] Failed - check wiring!");
  }

  // Initialize LoRa
  setupLoRa();

  // Startup screen
  drawStartup();
  delay(2000);

  if (loraReady) {
    drawWaiting();
    digitalWrite(LED_PIN, HIGH);
  }

  Serial.println("=== Ready ===\n");
}

// =============================================================================
// Loop
// =============================================================================
void loop() {
  if (!loraReady) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(500);
    return;
  }

  if (receivedFlag) {
    receivedFlag = false;
    digitalWrite(LED_PIN, LOW);

    int state = radio.readData((uint8_t*)&lastSignal, sizeof(lastSignal));

    if (state == RADIOLIB_ERR_NONE) {
      Serial.printf("RX: p=%d z=%d pk=%d 3rd=%d RSSI=%.0f\n",
        lastSignal.pitch, lastSignal.zone,
        lastSignal.pickoff, lastSignal.thirdSign,
        radio.getRSSI());

      drawSignal(lastSignal);
      lastReceived = millis();
    }

    radio.startReceive();
    digitalWrite(LED_PIN, HIGH);
  }

  // Return to waiting after 30s
  if (lastReceived > 0 && millis() - lastReceived > 30000) {
    drawWaiting();
    lastReceived = 0;
  }

  delay(10);
}
