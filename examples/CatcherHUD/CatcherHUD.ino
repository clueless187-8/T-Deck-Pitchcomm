/*
 * ============================================================================
 * CATCHER HUD RECEIVER v2.0.0 — DISPLAY ONLY
 * ============================================================================
 * Hardware:  Seeed XIAO nRF52840 + Wio-SX1262 LoRa Module
 * Display:   HiLetgo 0.49" SSD1306 64x32 I2C OLED (addr 0x3C)
 * Mount:     All-Star catcher mask — HUD configuration
 * 
 * RF LINK:   Matched to T-Deck Plus Coach Transmitter
 *            915.0 MHz | SF7 | BW125 | CR4/5 | Sync 0x34
 *            6-byte packet: [0xCC][0x01][0x01][CMD][SEQ][XOR]
 * 
 * NO VIBRATION — display-only pitch call system
 * 
 * PIN ALLOCATION:
 *   D0  = FREE (spare)
 *   D1  = SX1262 DIO1 (RX interrupt)
 *   D2  = SX1262 RESET
 *   D3  = SX1262 BUSY
 *   D4  = SX1262 NSS (chip select)
 *   D5  = RF Switch (PE4259)
 *   D6  = OLED SDA (software I2C)
 *   D7  = OLED SCL (software I2C)
 *   D8  = SPI SCK  (to SX1262)
 *   D9  = SPI MISO (to SX1262)
 *   D10 = SPI MOSI (to SX1262)
 * ============================================================================
 */

#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>

// ============================================================================
// HARDWARE PIN DEFINITIONS — XIAO nRF52840 + Wio-SX1262
// ============================================================================
#define LORA_NSS        D4
#define LORA_DIO1       D1
#define LORA_RESET      D2
#define LORA_BUSY       D3
#define RF_SW_PIN       D5

#define OLED_SDA        D6
#define OLED_SCL        D7

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
// DISPLAY — HiLetgo 0.49" SSD1306 64x32 via Software I2C
// ============================================================================
U8G2_SSD1306_64X32_1F_F_SW_I2C display(
    U8G2_R0,
    /* clock=*/ OLED_SCL,
    /* data=*/  OLED_SDA,
    /* reset=*/ U8X8_PIN_NONE
);

// ============================================================================
// RADIO — SX1262
// ============================================================================
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RESET, LORA_BUSY);

// ============================================================================
// PITCH DISPLAY LOOKUP
// ============================================================================
typedef struct {
    uint8_t     cmd;
    const char* line1;          // Top line — large
    const char* line2;          // Bottom line — small detail
    bool        invert;         // Inverted for urgent calls
} CallInfo;

const CallInfo callTable[] = {
    { CMD_FB_IN,    "FB",     "INSIDE",   false },
    { CMD_FB_OUT,   "FB",     "OUTSIDE",  false },
    { CMD_CURVE,    "CURVE",  "",         false },
    { CMD_CHANGE,   "CHNG",   "",         false },
    { CMD_SLIDER,   "SLIDE",  "",         false },
    { CMD_CUTTER,   "CUT",    "",         false },
    { CMD_SPLIT,    "SPLIT",  "",         false },
    { CMD_SCREW,    "SCRW",   "",         false },
    { CMD_PICK1,    "PICK",   "1ST",      true  },
    { CMD_PICK2,    "PICK",   "2ND",      true  },
    { CMD_PITCHOUT, "PITCH",  "OUT!",     true  },
    { CMD_TIMEOUT,  "TIME",   "OUT",      true  },
};
const uint8_t CALL_COUNT = sizeof(callTable) / sizeof(callTable[0]);

// ============================================================================
// STATE
// ============================================================================
volatile bool   rxFlag      = false;
uint8_t         lastSeq     = 0;
uint8_t         lastCmd     = 0;
int16_t         lastRSSI    = 0;
float           lastSNR     = 0.0;
unsigned long   lastRxTime  = 0;
unsigned long   clearTime   = 0;
bool            showing     = false;
uint32_t        rxCount     = 0;
uint32_t        errCount    = 0;

// ============================================================================
// ISR
// ============================================================================
void onReceive() {
    rxFlag = true;
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void showCall(const char* line1, const char* line2, bool invert) {
    display.clearBuffer();

    if (invert) {
        display.drawBox(0, 0, 64, 32);
        display.setDrawColor(0);
    } else {
        display.setDrawColor(1);
    }

    if (line2[0] != '\0') {
        // Two-line layout
        display.setFont(u8g2_font_helvB14_tr);
        int16_t w1 = display.getStrWidth(line1);
        display.drawStr((64 - w1) / 2, 16, line1);

        display.setFont(u8g2_font_helvB10_tr);
        int16_t w2 = display.getStrWidth(line2);
        display.drawStr((64 - w2) / 2, 30, line2);
    } else {
        // Single-line — vertically centered
        display.setFont(u8g2_font_helvB14_tr);
        int16_t w1 = display.getStrWidth(line1);
        display.drawStr((64 - w1) / 2, 22, line1);
    }

    display.setDrawColor(1);
    display.sendBuffer();

    showing   = true;
    clearTime = millis() + 5000;
}

void showStandby() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(0, 7, "RDY");

    if (rxCount > 0) {
        int bars = 0;
        if (lastRSSI > -60)       bars = 4;
        else if (lastRSSI > -80)  bars = 3;
        else if (lastRSSI > -100) bars = 2;
        else                      bars = 1;

        for (int i = 0; i < bars; i++) {
            int h = 2 + (i * 2);
            display.drawBox(50 + (i * 4), 8 - h, 3, h);
        }
    }

    display.sendBuffer();
    showing = false;
}

void showSplash() {
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_helvB10_tr);
    const char* s = "HUD";
    display.drawStr((64 - display.getStrWidth(s)) / 2, 14, s);
    display.setFont(u8g2_font_5x7_tr);
    const char* v = "v2.0";
    display.drawStr((64 - display.getStrWidth(v)) / 2, 28, v);
    display.sendBuffer();
    delay(1200);
}

void showSyncing() {
    display.clearBuffer();
    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(2, 14, "RF SYNC");
    display.drawStr(2, 26, "915 MHz");
    display.sendBuffer();
}

void showError(const char* msg) {
    display.clearBuffer();
    display.drawBox(0, 0, 64, 32);
    display.setDrawColor(0);
    display.setFont(u8g2_font_5x7_tr);
    display.drawStr(2, 10, "ERROR");
    display.drawStr(2, 24, msg);
    display.setDrawColor(1);
    display.sendBuffer();
}

// ============================================================================
// PACKET VALIDATION
// ============================================================================
bool validatePacket(uint8_t* pkt, uint8_t len) {
    if (len != PKT_LENGTH)      return false;
    if (pkt[0] != PKT_MAGIC)    return false;
    if (pkt[1] != PKT_VERSION)  return false;
    if (pkt[2] != ADDR_CATCHER) return false;

    uint8_t chk = pkt[0] ^ pkt[1] ^ pkt[2] ^ pkt[3] ^ pkt[4];
    if (pkt[5] != chk)          return false;

    return true;
}

const CallInfo* lookupCall(uint8_t cmd) {
    for (uint8_t i = 0; i < CALL_COUNT; i++) {
        if (callTable[i].cmd == cmd) return &callTable[i];
    }
    return NULL;
}

// ============================================================================
// PROCESS RECEIVED PACKET
// ============================================================================
void processPacket() {
    uint8_t pkt[16];
    int state = radio.readData(pkt, PKT_LENGTH);

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RX] READ ERR: %d\n", state);
        errCount++;
        radio.startReceive();
        return;
    }

    lastRSSI = radio.getRSSI();
    lastSNR  = radio.getSNR();

    if (!validatePacket(pkt, PKT_LENGTH)) {
        Serial.printf("[RX] BAD PKT: %02X %02X %02X %02X %02X %02X\n",
            pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5]);
        errCount++;
        radio.startReceive();
        return;
    }

    uint8_t cmd = pkt[3];
    uint8_t seq = pkt[4];

    // Duplicate suppression — coach sends 3 copies per call
    if (seq == lastSeq && cmd == lastCmd && (millis() - lastRxTime < 500)) {
        radio.startReceive();
        return;
    }

    lastSeq    = seq;
    lastCmd    = cmd;
    lastRxTime = millis();
    rxCount++;

    const CallInfo* call = lookupCall(cmd);

    if (call != NULL) {
        Serial.printf("[RX] %s %s (0x%02X) SEQ:%d RSSI:%d SNR:%.1f\n",
            call->line1, call->line2, cmd, seq, lastRSSI, lastSNR);
        showCall(call->line1, call->line2, call->invert);
    } else {
        Serial.printf("[RX] UNK 0x%02X SEQ:%d RSSI:%d\n", cmd, seq, lastRSSI);
        char hexBuf[6];
        snprintf(hexBuf, sizeof(hexBuf), "0x%02X", cmd);
        showCall(hexBuf, "???", true);
    }

    radio.startReceive();
}

// ============================================================================
// RADIO INIT
// ============================================================================
bool initRadio() {
    Serial.println("[RADIO] Init SX1262...");

    pinMode(RF_SW_PIN, OUTPUT);
    digitalWrite(RF_SW_PIN, HIGH);

    int state = radio.begin(RF_FREQ, RF_BW, RF_SF, RF_CR,
                            RF_SYNC, RF_POWER, RF_PREAMBLE, RF_TCXO_V);

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] INIT FAIL: %d\n", state);
        return false;
    }

    radio.setDio2AsRfSwitch(true);
    radio.setCurrentLimit(140.0);
    radio.setCRC(2);
    radio.setDio1Action(onReceive);

    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[RADIO] RX START FAIL: %d\n", state);
        return false;
    }

    Serial.printf("[RADIO] OK %.1fMHz SF%d BW%.0f CR4/%d SYNC:0x%02X\n",
        RF_FREQ, RF_SF, RF_BW, RF_CR, RF_SYNC);
    return true;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  CATCHER HUD v2.0.0 — DISPLAY ONLY");
    Serial.println("  XIAO nRF52840 + Wio-SX1262");
    Serial.println("  0.49\" SSD1306 64x32 OLED");
    Serial.println("  All-Star Mask Mount");
    Serial.println("========================================");

    display.begin();
    display.setContrast(220);
    Serial.println("[DISPLAY] SSD1306 64x32 OK");

    showSplash();
    showSyncing();

    if (!initRadio()) {
        Serial.println("[FATAL] Radio init failed");
        showError("NO RADIO");
        while (true) { delay(1000); }
    }

    showStandby();
    Serial.println("[SYSTEM] HUD operational — awaiting TX\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    if (rxFlag) {
        rxFlag = false;
        processPacket();
    }

    if (showing && millis() > clearTime) {
        showStandby();
    }

    // Link health monitor
    static unsigned long lastHealth = 0;
    if (millis() - lastHealth > 30000) {
        lastHealth = millis();

        if (rxCount > 0 && (millis() - lastRxTime > 60000)) {
            Serial.println("[WARN] No RX 60s");
            display.clearBuffer();
            display.setFont(u8g2_font_5x7_tr);
            display.drawStr(4, 14, "NO LINK");
            display.drawStr(4, 26, "CHECK TX");
            display.sendBuffer();
            showing = false;
        }

        Serial.printf("[STAT] RX:%lu ERR:%lu RSSI:%d SNR:%.1f\n",
            rxCount, errCount, lastRSSI, lastSNR);
    }

    delay(1);
}
