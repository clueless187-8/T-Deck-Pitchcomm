/*
 * T-DECK PLUS GPIO & BACKLIGHT TEST
 * ==================================
 * Minimal test - verifies power and backlight only
 * No SPI, no display commands
 */

#define BOARD_POWERON   10
#define TFT_BL          42

void setup() {
    Serial.begin(115200);
    delay(3000);
    
    Serial.println("\n========================================");
    Serial.println("T-DECK PLUS GPIO TEST");
    Serial.println("========================================\n");
    
    // Enable board power
    Serial.println("[1] Setting BOARD_POWERON (GPIO10) HIGH...");
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(500);
    Serial.println("[1] DONE - Board power enabled\n");
    
    // Configure backlight
    Serial.println("[2] Configuring backlight (GPIO42)...");
    pinMode(TFT_BL, OUTPUT);
    Serial.println("[2] DONE\n");
    
    Serial.println("========================================");
    Serial.println("BACKLIGHT TEST - Watch the screen!");
    Serial.println("========================================\n");
}

void loop() {
    // Backlight ON - Full brightness
    Serial.println(">>> BACKLIGHT ON (255)");
    analogWrite(TFT_BL, 255);
    delay(2000);
    
    // Backlight 50%
    Serial.println(">>> BACKLIGHT 50% (128)");
    analogWrite(TFT_BL, 128);
    delay(2000);
    
    // Backlight LOW
    Serial.println(">>> BACKLIGHT LOW (32)");
    analogWrite(TFT_BL, 32);
    delay(2000);
    
    // Backlight OFF
    Serial.println(">>> BACKLIGHT OFF (0)");
    analogWrite(TFT_BL, 0);
    delay(2000);
    
    Serial.println("\n--- Cycle complete, repeating ---\n");
}
