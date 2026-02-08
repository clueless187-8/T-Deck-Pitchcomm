# XIAO Armband ePaper Receiver — Build Guide

## System Overview
Forearm-mounted catcher receiver using 2.13" ePaper display for sunlight-readable pitch calls. Matches T-Deck Plus coach transmitter protocol byte-for-byte.

## Hardware BOM
| Component | Part | Size | Est. Cost |
|-----------|------|------|-----------|
| MCU | Seeed XIAO nRF52840 | 21x17.8mm | $9.90 |
| LoRa | Wio-SX1262 (B2B stack) | 21x17.8mm | $9.90 |
| Display | Seeed 2.13" ePaper 122x250 | 59x30mm | $6.50 |
| Battery | 603048 LiPo 800mAh | 6x30x48mm | $5.00 |
| Antenna | Molex 105262-0002 flex | 79x10mm | $3.00 |
| Enclosure | SLA resin (custom) | 100x35x12mm | ~$5.00 |
| Misc | FPC cable, resistor, wire | — | ~$2.00 |
| **Total** | | | **~$41.30** |

## Pin Allocation (ALL GPIO CONSUMED)
| Pin | Function | Domain |
|-----|----------|--------|
| D0 | ePaper CS | SPI select (display) |
| D1 | DIO1 (IRQ) | LoRa control |
| D2 | SX1262 RESET | LoRa control |
| D3 | SX1262 BUSY | LoRa control |
| D4 | SX1262 NSS | SPI select (radio) |
| D5 | RF_SW (PE4259) | **MUST be HIGH** |
| D6 | ePaper DC | Display data/command |
| D7 | ePaper BUSY | Display busy signal |
| D8 | SPI SCK | **Shared bus** |
| D9 | SPI MISO | LoRa read (radio only) |
| D10 | SPI MOSI | **Shared bus** |
| BAT+ | LiPo positive | Power |
| BAT- | LiPo negative | Power |

> **ePaper RST** tied to 3V3 via 10k resistor — saves one GPIO.

## Shared SPI Bus Architecture
SX1262 and SSD1680 share SCK (D8) and MOSI (D10). Firmware manages CS arbitration — only one device active at a time. MISO (D9) is LoRa-only (ePaper is write-only SPI).

## RF Protocol (Matched to T-Deck Coach TX)
- Frequency: 915.0 MHz | SF7 | BW125 | CR4/5 | Sync 0x34
- Packet: [0xCC][0x01][0x01][CMD][SEQ][XOR] (6 bytes)
- TX Power: +22 dBm | Preamble: 8 symbols | CRC enabled

## ePaper Display Behavior
- Partial refresh: ~300-500ms for pitch call updates
- Full refresh: every 20 partial updates (ghosting prevention)
- Urgent calls (pickoff/pitchout/timeout): inverted white-on-black
- Hold time: 8 seconds then reverts to READY
- Sunlight readability: excellent — contrast increases in direct sun

## Enclosure
- Form factor: 100 x 35 x 12mm (thin linear)
- Material: SLA ABS-like resin
- Ports: USB-C (charging/programming), U.FL antenna egress
- Mount: Nylon armband sleeve or Velcro strap

## Battery
- Cell: 603048 LiPo 3.7V 800mAh
- Runtime: 40+ hours (ePaper zero standby power, LoRa RX ~6mA)
- Charging: USB-C via XIAO onboard charger

## Arduino IDE Setup
1. Board URL: https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
2. Install Seeed nRF52 Boards (Adafruit-based, NOT mbed)
3. Board: Seeed XIAO nRF52840
4. Libraries: RadioLib >= 7.1.2, GxEPD2 >= 1.5.0, Adafruit GFX >= 1.11.0

## CRITICAL: RF Switch Config
```cpp
pinMode(RF_SW_PIN, OUTPUT);
digitalWrite(RF_SW_PIN, HIGH);
radio.setDio2AsRfSwitch(true);
```
Failure drops RSSI by 40+ dB.

## Assembly Sequence
1. Solder 10k resistor: ePaper RST pad to 3V3 rail
2. Wire ePaper FPC: CS->D0, DC->D6, BUSY->D7, SCK->D8, MOSI->D10, VCC->3V3, GND
3. Stack Wio-SX1262 on XIAO (B2B connector)
4. Solder battery to BAT+/BAT-
5. Connect U.FL coax to flex antenna
6. Conformal coat (mask USB-C with Kapton)
7. Seat in enclosure, route antenna along armband length
