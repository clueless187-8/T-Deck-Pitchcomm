/**
 * Heltec WiFi LoRa 32 V3 - PitchComm Receiver
 *
 * Receives pitch signals from T-Deck transmitter via LoRa 915MHz
 * Displays on 0.96" OLED (128x64 SSD1306)
 *
 * Hardware: ESP32-S3 + SX1262 + SSD1306 OLED
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <RadioLib.h>

// =============================================================================
// Heltec WiFi LoRa 32 V3 Pin Definitions
// =============================================================================

// OLED Display (I2C) - SSD1306 128x64
#define OLED_SDA        17
#define OLED_SCL        18
#define OLED_RST        21

// LoRa SX1262 pins
#define LORA_MISO       11
#define LORA_MOSI       10
#define LORA_SCK        9
#define LORA_CS         8
#define LORA_RST        12
#define LORA_DIO1       14
#define LORA_BUSY       13

// Vext control (powers OLED and external peripherals)
#define VEXT_CTRL       36

// LED
#define LED_PIN         35

// =============================================================================
// Objects
// =============================================================================

// OLED Display - Hardware I2C, 128x64
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

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

// Pitch names
const char* pitchNames[] = {"FB", "CB", "CH", "SL", "PO"};

bool loraReady = false;
PitchSignal lastSignal;
unsigned long lastReceived = 0;
volatile bool receivedFlag = false;

// =============================================================================
// Display Functions (optimized for 128x64 OLED)
// =============================================================================

void drawStartup() {
  display.clearBuffer();
  display.setFont(u8g2_font_helvB14_tr);
  display.drawStr(15, 25, "PitchComm");
  display.setFont(u8g2_font_helvR10_tr);
  display.drawStr(25, 45, "Receiver");

  display.setFont(u8g2_font_6x10_tr);
  if (loraReady) {
    display.drawStr(30, 60, "LoRa: Ready");
  } else {
    display.drawStr(28, 60, "LoRa: FAILED");
  }
  display.sendBuffer();
}

void drawWaiting() {
  display.clearBuffer();
  display.setFont(u8g2_font_helvR12_tr);
  display.drawStr(20, 38, "Waiting...");
  display.sendBuffer();
}

void drawSignal(PitchSignal &sig) {
  display.clearBuffer();

  // Signal number in top-left corner
  display.setFont(u8g2_font_5x7_tr);
  char numStr[8];
  snprintf(numStr, sizeof(numStr), "#%d", sig.number);
  display.drawStr(0, 7, numStr);

  if (sig.type == 1) {
    // Reset signal - large centered text
    display.setFont(u8g2_font_helvB24_tr);
    display.drawStr(12, 45, "RESET");
    display.sendBuffer();
    return;
  }

  bool hasPitch = (sig.pitch < 5);

  // Pickoff-only signal
  if (sig.pickoff > 0 && !hasPitch) {
    display.setFont(u8g2_font_helvB24_tr);
    char pkStr[5];
    snprintf(pkStr, sizeof(pkStr), "PK%d", sig.pickoff);
    display.drawStr(25, 45, pkStr);
    display.sendBuffer();
    return;
  }

  // Third sign only
  if (sig.thirdSign > 0 && !hasPitch) {
    display.setFont(u8g2_font_helvB24_tr);
    const char* thirdNames[] = {"", "3A", "3B", "3C", "3D"};
    if (sig.thirdSign <= 4) {
      display.drawStr(40, 45, thirdNames[sig.thirdSign]);
    } else {
      display.drawStr(40, 45, "3?");
    }
    display.sendBuffer();
    return;
  }

  // Main pitch display
  if (hasPitch) {
    // Large pitch name centered
    display.setFont(u8g2_font_helvB24_tr);
    int pitchWidth = display.getStrWidth(pitchNames[sig.pitch]);
    int xPos = (128 - pitchWidth) / 2;

    if (sig.zone > 0) {
      // Pitch + Zone layout
      display.drawStr(xPos - 15, 35, pitchNames[sig.pitch]);

      // Zone number to the right
      display.setFont(u8g2_font_helvB18_tr);
      char zoneStr[3];
      snprintf(zoneStr, sizeof(zoneStr), "%d", sig.zone);
      display.drawStr(xPos + pitchWidth + 5, 35, zoneStr);
    } else {
      // Pitch only - centered
      display.drawStr(xPos, 40, pitchNames[sig.pitch]);
    }
  }

  // Bottom row: pickoff and/or third sign indicators
  display.setFont(u8g2_font_6x10_tr);
  int bottomY = 60;
  int xOffset = 0;

  if (sig.pickoff > 0 && hasPitch) {
    char pkStr[5];
    snprintf(pkStr, sizeof(pkStr), "PK%d", sig.pickoff);
    display.drawStr(xOffset, bottomY, pkStr);
    xOffset += 30;
  }

  if (sig.thirdSign > 0 && hasPitch) {
    const char* thirdNames[] = {"", "3A", "3B", "3C", "3D"};
    if (sig.thirdSign <= 4) {
      display.drawStr(xOffset, bottomY, thirdNames[sig.thirdSign]);
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
  Serial.println("[LoRa] Initializing SPI...");
  radioSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

  Serial.println("[LoRa] Initializing SX1262...");
  int state = radio.begin(915.0);  // 915 MHz for Americas

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] SX1262 init OK");

    // Match T-Deck transmitter settings exactly
    radio.setSpreadingFactor(10);
    radio.setBandwidth(125.0);
    radio.setCodingRate(8);
    radio.setSyncWord(0x12);
    radio.setOutputPower(22);
    radio.setPreambleLength(8);

    // Set up interrupt on DIO1
    radio.setDio1Action(setFlag);

    // Start receiving
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("[LoRa] Receive mode started");
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
  delay(1000);
  Serial.println("\n\n=== Heltec LoRa V3 PitchComm Receiver ===");

  // Enable Vext to power OLED
  pinMode(VEXT_CTRL, OUTPUT);
  digitalWrite(VEXT_CTRL, LOW);  // LOW = ON for Heltec V3
  delay(100);

  // LED indicator
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize OLED display
  Serial.println("[Display] Initializing OLED...");
  display.begin();
  display.setContrast(255);
  display.clearBuffer();
  display.sendBuffer();
  Serial.println("[Display] OLED ready");

  // Initialize LoRa
  setupLoRa();

  // Show startup screen
  drawStartup();
  delay(2000);

  if (loraReady) {
    drawWaiting();
    digitalWrite(LED_PIN, HIGH);  // LED on when ready
  }

  Serial.println("=== Ready ===\n");
}

// =============================================================================
// Loop
// =============================================================================
void loop() {
  if (!loraReady) {
    // Blink LED to indicate error
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(500);
    return;
  }

  // Check if packet received via interrupt
  if (receivedFlag) {
    receivedFlag = false;

    // Flash LED on receive
    digitalWrite(LED_PIN, LOW);

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

    digitalWrite(LED_PIN, HIGH);
  }

  // Show waiting screen if no signal for 30 seconds
  if (lastReceived > 0 && millis() - lastReceived > 30000) {
    drawWaiting();
    lastReceived = 0;
  }

  delay(10);
}
