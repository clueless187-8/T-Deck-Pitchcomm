# ARMBAND ePaper RECEIVER — Build Guide

## System Overview
Forearm-mounted catcher receiver for the T-Deck PitchComm system. Features a 2.13" ePaper display for sunlight-readable pitch calls in a thin armband form factor.

## Hardware

| Component | Part | Specs |
|-----------|------|-------|
| MCU | Seeed XIAO nRF52840 | ARM Cortex-M4F, 21×17.8mm |
| LoRa | Wio-SX1262 | 915 MHz, +22 dBm, PE4259 RF switch |
| Display | Seeed 2.13" ePaper | 122×250, SSD1680, SPI, monochrome B/W |
| Battery | 603048 LiPo | 3.7V 800mAh, 30×48×6mm |
| Antenna | Molex 105262-0002 | 915 MHz flex dipole, 79×10mm |
| Enclosure | SLA resin 2-piece | 90×35×10mm, snap-fit, display window |

## Pin Mapping (ALL 11 GPIO CONSUMED)

### SPI Bus — SHARED between LoRa and ePaper

| Pin | Function | Device |
|-----|----------|--------|
| D8 | SCK | **Shared** (SX1262 + ePaper) |
| D9 | MISO | SX1262 only (ePaper is write-only) |
| D10 | MOSI | **Shared** (SX1262 + ePaper) |
| D4 | NSS (CS) | SX1262 chip select |
| D0 | CS | ePaper chip select |

### LoRa Control

| Pin | Function |
|-----|----------|
| D1 | DIO1 (RX interrupt) |
| D2 | SX1262 RESET |
| D3 | SX1262 BUSY |
| D5 | RF_SW (PE4259) — **MUST BE HIGH** |

### ePaper Control

| Pin | Function |
|-----|----------|
| D6 | DC (data/command) |
| D7 | BUSY |
| RST | Tied to 3V3 via 10kΩ (**no GPIO — hardware pull-up**) |

## SPI Bus Arbitration — CRITICAL

Both the SX1262 and ePaper share SCK (D8) and MOSI (D10). Only one device may be active at a time. Firmware manages this via chip select arbitration:

```cpp
void selectLoRa() {
    digitalWrite(EPAPER_CS, HIGH);   // Deselect ePaper
}
void selectEPaper() {
    digitalWrite(LORA_NSS, HIGH);    // Deselect LoRa
}
```

Before any LoRa operation, call `selectLoRa()`. Before any display update, call `selectEPaper()`. RadioLib and GxEPD2 manage their own CS internally after this.

## RF Protocol (Matched to T-Deck Coach TX)

- **Frequency:** 915.0 MHz
- **Spreading Factor:** 7
- **Bandwidth:** 125.0 kHz
- **Coding Rate:** 4/5
- **Sync Word:** 0x34
- **TX Power:** +22 dBm
- **Preamble:** 8 symbols
- **CRC:** Enabled

### Packet Structure (6 bytes)
```
[0xCC] [0x01] [0x01] [CMD] [SEQ] [XOR checksum]
```

### Command Codes

| Code | Pitch | Code | Pitch |
|------|-------|------|-------|
| 0x01 | FB Inside | 0x07 | Splitter |
| 0x02 | FB Outside | 0x08 | Screwball |
| 0x03 | Curveball | 0x09 | Pickoff 1st |
| 0x04 | Changeup | 0x0A | Pickoff 2nd |
| 0x05 | Slider | 0x10 | Pitchout (URGENT) |
| 0x06 | Cutter | 0xFF | Timeout |

## RF Switch — MANDATORY Configuration
```cpp
pinMode(RF_SW, OUTPUT);
digitalWrite(RF_SW, HIGH);
radio.setDio2AsRfSwitch(true);
```
> **Failure to set RF_SW HIGH drops RSSI from -10 dBm to -50 dBm at close range.**

## ePaper Display Behavior

- **Partial refresh:** ~300-500ms, used for all pitch call updates
- **Full refresh:** ~2-3 seconds, forced every 20 partial updates to prevent ghosting
- **Urgent calls** (pickoff, pitchout, timeout): inverted display (white on black)
- **Display hold:** 8 seconds after last call, then reverts to READY screen
- **Sunlight readable:** ePaper uses reflected light — higher contrast in direct sun

## Physical Build

### Enclosure (90 × 35 × 10 mm)
- **Material:** SLA ABS-like tough resin
- **Wall thickness:** 1.5mm
- **Two-piece:** bottom tray + display lid, snap-fit closure
- **Display window:** 61×32mm cutout, 0.5mm polycarbonate bonded with Loctite 4014
- **Ports:** USB-C (9×3.5mm, short end), strap loops (6mm channels, both ends)
- **Gasket:** 0.8mm silicone O-ring in 1.0W × 0.6D groove
- **Corner radius:** 4mm external, 0.3mm fillet for skin comfort

### Internal Layout
- ePaper panel face-up under display window
- XIAO+Wio-SX1262 B2B stack in right half of cavity
- 603048 LiPo battery alongside electronics in left half
- Flex antenna adhesive-mounted flat in bottom shell
- EVA foam pad between antenna and electronics

### Armband Mounting
- 40mm elastic strap with velcro closure
- Threads through strap loops on both short ends
- Display faces up (glance down to read)
- Antenna end oriented toward dugout for best signal path
- Wear on non-throwing arm (glove arm for catchers)

## Arduino IDE Setup

1. Board URL: `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
2. Install **Seeed nRF52 Boards** v1.x (Adafruit-based, NOT mbed variant)
3. Board: **Seeed XIAO nRF52840**
4. Libraries:
   - RadioLib >= 7.1.2
   - GxEPD2 >= 1.5.0
   - Adafruit GFX >= 1.11.0

## Assembly Sequence

1. Apply conformal coating (MG Chemicals 422B) to XIAO+Wio stack — mask USB-C port
2. Solder 10kΩ resistor between ePaper RST and 3V3
3. Solder battery wires: RED→BAT+, BLACK→BAT-
4. Connect ePaper FPC: CS→D0, DC→D6, BUSY→D7, SCK→D8, MOSI→D10, VCC→3V3, GND→GND
5. Connect U.FL coax from Molex antenna to Wio-SX1262
6. Adhesive-mount flex antenna flat in bottom tray
7. Place EVA foam over antenna
8. Seat battery + electronics into cavity, route USB-C to port
9. Place ePaper panel face-up in lid recess
10. Seat gasket, snap lid closed
11. Thread strap through end loops

## Link Budget (90 ft pitcher-to-catcher)

- Free-space path loss: 60.4 dB
- SX1262 max link budget: 159 dB
- **Available margin: 98.6 dB** — absorbs body loss, arm shielding, multipath

## Runtime

- XIAO nRF52840 + SX1262 RX idle: ~15mA
- ePaper display: 0μA static (only draws during refresh)
- 800mAh / 15mA = **53+ hours** continuous receive
- Practical runtime: **40+ hours** with periodic display refreshes

## Weight Budget

| Component | Weight |
|-----------|--------|
| XIAO nRF52840 | 3.0g |
| Wio-SX1262 | 3.0g |
| 2.13" ePaper | 5.0g |
| 603048 LiPo | 15.0g |
| Flex antenna + coax | 2.0g |
| SLA enclosure (2-piece) | 8.0g |
| Armband strap | 5.0g |
| Foam + gasket + misc | 2.0g |
| **TOTAL** | **~43g** |

> Comparable to a standard fitness tracker.
