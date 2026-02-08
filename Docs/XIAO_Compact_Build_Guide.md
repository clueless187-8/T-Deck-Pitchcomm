# XIAO nRF52840 Catcher HUD — Compact Build Guide

## System Overview
Backup catcher receiver for the T-Deck PitchComm system. Mounts inside an All-Star FM25 catcher mask as a heads-up display.

## Hardware
| Component | Part | Specs |
|-----------|------|-------|
| MCU | Seeed XIAO nRF52840 | ARM Cortex-M4F, 21×17.8mm |
| LoRa | Wio-SX1262 | 915 MHz, +22 dBm, PE4259 RF switch |
| Display | HiLetgo 0.49" SSD1306 | 64×32 I2C OLED (0x3C) |
| Battery | 501230 LiPo | 3.7V 150mAh, 8.8hr runtime |
| Antenna | Molex 105262-0002 | 915 MHz flex dipole, 79×10×0.1mm |

## Pin Mapping (Wio-SX1262 on XIAO)
| Pin | Function | Domain |
|-----|----------|--------|
| D1 | DIO1 (IRQ) | LoRa Control |
| D2 | RESET | LoRa Control |
| D3 | BUSY | LoRa Control |
| D4 | NSS (CS) | SPI |
| D5 | RF_SW (PE4259) | **CRITICAL** |
| D6 | SDA (OLED) | I2C Tether |
| D7 | SCL (OLED) | I2C Tether |
| D8 | SCK | SPI |
| D9 | MISO | SPI |
| D10 | MOSI | SPI |
| BAT+ | LiPo RED | Power |
| BAT- | LiPo BLACK | Power |

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

## Physical Build

### Enclosure
- **Material:** SLA ABS-like resin (or FDM PETG)
- **Dimensions:** 25 × 22 × 18mm (two-piece snap-fit)
- **Wall thickness:** 1.2mm
- **Gasket:** 0.8mm silicone O-ring in 1.0W × 0.6D groove
- **Ports:** USB-C (9×3.5mm), I2C wire (∅3.5mm), U.FL coax (∅2.5mm)

### I2C Display Tether
- **Length:** 75–100mm (3–4 inches)
- **Wire:** 30AWG silicone-jacketed stranded (4 conductors)
- **Signals:** SDA (green), SCL (blue), VCC (red), GND (black)
- **Pull-ups:** 4.7kΩ on SDA/SCL to 3V3 (check OLED module first)
- **Strain relief:** Heat shrink + RTV silicone at both ends

### Mask Mounting
- **Main module:** Sewn ripstop nylon pocket on forehead pad (cage-facing side)
- **Display:** SLA clip bracket on top horizontal cage bar (upper peripheral vision)
- **Antenna:** Adhesive-mount inside shell top, facing outward toward dugout
- **Wire routing:** Behind padding, Kapton tape at 15–20mm intervals, PET braided sleeve

### Moisture Protection
1. MG Chemicals 422B silicone conformal coating (2 coats, mask USB-C port)
2. SLA resin snap-fit enclosure with silicone gasket
3. 0.5mm polycarbonate window over display face

## Arduino IDE Setup
1. Board URL: `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
2. Install **Seeed nRF52 Boards** v1.x (Adafruit-based, NOT mbed variant)
3. Board: **Seeed XIAO nRF52840**
4. Libraries: RadioLib ≥ 7.1.2, U8g2 ≥ 2.35.0

## Assembly Sequence
1. Apply conformal coating to XIAO+Wio stack (mask USB-C port with Kapton)
2. Solder battery wires to BAT+/BAT- pads
3. Solder I2C tether: D6→SDA, D7→SCL, 3V3→VCC, GND→GND
4. Heat shrink all joints + RTV strain relief
5. Connect U.FL coax to Wio-SX1262
6. Place EVA foam in bottom enclosure
7. Seat electronics + battery into cavity
8. Route I2C and coax through ports, seal with RTV
9. Seat gasket, snap lid closed

## Link Budget (90 ft pitcher-to-catcher)
- Free-space path loss: 60.4 dB
- SX1262 max link budget: 159 dB
- **Available margin: 98.6 dB** — absorbs body loss, cage shielding, multipath, and poor antenna match with >60 dB to spare

## Weight Budget
| Component | Weight |
|-----------|--------|
| XIAO nRF52840 | ~3.0g |
| Wio-SX1262 | ~3.0g |
| 501230 LiPo | ~3.5g |
| SLA enclosure (both halves) | ~4.0g |
| OLED + tether + bracket | ~2.0g |
| Antenna + coax | ~2.0g |
| Gasket + foam + misc | ~0.5g |
| **TOTAL** | **~18g** |

> On a 578g All-Star FM25 mask — imperceptible.
