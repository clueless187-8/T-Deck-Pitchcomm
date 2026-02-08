# Catcher HUD Receiver — Compact Build Guide

## XIAO nRF52840 + Wio-SX1262 + 0.49" SSD1306 OLED

**Backup catcher receiver for T-Deck Plus PitchComm system**
Mounts inside All-Star FM25 catcher mask as heads-up display.

---

## Hardware Bill of Materials

| # | Component | Spec | Source | ~Cost |
|---|-----------|------|--------|-------|
| 1 | Seeed XIAO nRF52840 | ARM Cortex-M4F, BLE 5.0 | Seeed Studio | $10 |
| 2 | Wio-SX1262 LoRa Module | 915 MHz, +22 dBm, PE4259 RF switch | Seeed Studio | $10 |
| 3 | HiLetgo 0.49" SSD1306 OLED | 64×32 I2C (0x3C) | Amazon | $7 |
| 4 | 501230 LiPo Battery | 3.7V 150mAh, 5×12×30mm | Amazon | $6 |
| 5 | Molex 105262-0002 Antenna | 915 MHz flex dipole, 79×10mm | Newark/Mouser | $3 |
| 6 | 30AWG Silicone Wire (4-color) | Stranded, 0.8mm OD | Amazon | $8 |
| 7 | 1.5mm Adhesive Heat Shrink | 3:1 marine-grade polyolefin | Amazon | $5 |
| 8 | MG Chemicals 422B | Silicone conformal coating | Amazon | $12 |
| 9 | 2mm EVA Foam Pad | Vibration dampener | Amazon | $3 |

**Total: ~$64**

---

## Pin Mapping

### Wio-SX1262 → XIAO (SPI + Control — via B2B connector)

| Function | XIAO Pin | Direction |
|----------|----------|-----------|
| SPI SCK  | D8       | Output    |
| SPI MISO | D9       | Input     |
| SPI MOSI | D10      | Output    |
| NSS (CS) | D4       | Output    |
| DIO1 (IRQ) | D1     | Input     |
| RESET    | D2       | Output    |
| BUSY     | D3       | Input     |
| RF_SW    | D5       | Output    |

### 0.49" OLED → XIAO (I2C Tether — 75-100mm wire)

| Function | XIAO Pin | Wire Color |
|----------|----------|------------|
| SDA      | D6       | GREEN      |
| SCL      | D7       | BLUE       |
| VCC      | 3V3      | RED        |
| GND      | GND      | BLACK      |

### Battery → XIAO

| Function | XIAO Pad | Wire Color |
|----------|----------|------------|
| BAT+     | BAT+     | RED        |
| BAT-     | BAT-     | BLACK      |

### Free Pins

| Pin | Status |
|-----|--------|
| D0  | SPARE  |

---

## RF Protocol (Matched to T-Deck Plus Coach TX)

```
Frequency:       915.0 MHz
Spreading Factor: 7
Bandwidth:       125.0 kHz
Coding Rate:     4/5
Sync Word:       0x34
TX Power:        +22 dBm
Preamble:        8 symbols
CRC:             Enabled
```

### Packet Structure (6 bytes)

```
[0xCC] [0x01] [0x01] [CMD] [SEQ] [XOR]
  |      |      |      |     |     |
  Magic  Ver   Addr   Cmd   Seq  Checksum
```

### Command Codes

| Code | Key | Pitch/Play |
|------|-----|------------|
| 0x01 | 1   | FB Inside  |
| 0x02 | 2   | FB Outside |
| 0x03 | 3   | Curveball  |
| 0x04 | 4   | Changeup   |
| 0x05 | 5   | Slider     |
| 0x06 | 6   | Cutter     |
| 0x07 | 7   | Splitter   |
| 0x08 | 8   | Screwball  |
| 0x09 | 9   | Pickoff 1st |
| 0x0A | 0   | Pickoff 2nd |
| 0x10 | P   | Pitchout (URGENT) |
| 0xFF | —   | Timeout    |

---

## Critical RF Switch Configuration

The Wio-SX1262 uses a **PE4259 RF switch** that requires dual control. Failure to configure results in -50 dBm RSSI (vs -10 dBm expected at close range).

```cpp
// MANDATORY in setup()
pinMode(D5, OUTPUT);        // RF_SW pin
digitalWrite(D5, HIGH);     // Enable RX path
radio.setDio2AsRfSwitch(true);  // SX1262 DIO2 controls TX/RX switching
```

---

## Arduino IDE Setup

1. **Board Package URL:** `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
2. Install **"Seeed nRF52 Boards"** v1.x (Adafruit-based fork)
3. **DO NOT** use mbed-enabled variant — RadioLib compilation fails
4. Board: `Tools → Board → Seeed nRF52 Boards → "Seeed XIAO nRF52840"`
5. Libraries: **RadioLib** ≥ 7.1.2, **U8g2** ≥ 2.35.0

---

## Compact Build Assembly

### Physical Layout

- **Main module** (XIAO + Wio-SX1262 stack + battery): Rear/top of mask shell, behind forehead padding
- **Display**: Remote-mount on inner top bar of mask cage, upper peripheral vision
- **I2C tether**: 75-100mm (3-4") from main module to display
- **Antenna**: Inside mask shell top, facing outward toward dugout

### Stack Profile

```
┌─────────────────────┐
│   Components ~3mm   │ ← MCU, passives on top of XIAO
├─────────────────────┤
│   XIAO PCB   1.2mm  │
├─────────────────────┤
│   B2B gap    1.5mm  │
├─────────────────────┤
│   Wio-SX1262 1.2mm  │
├─────────────────────┤
│   SX1262 chip ~2mm  │
└─────────────────────┘
      Total: ~12mm

Battery alongside: 5 × 12 × 30mm
```

### Enclosure Dimensions

```
Outer: 25 × 22 × 18 mm
Walls: 1.2mm SLA resin
Gasket: 0.8mm silicone O-ring groove
Ports: USB-C (9×3.5mm), I2C (∅3.5mm), U.FL (∅2.5mm)
Snap-fit: 4× tabs, 1.0×2.0mm, 0.3mm deflection
```

### Battery Selection

**501230 LiPo** — 3.7V 150mAh
- Dimensions: 5.0 × 12 × 30mm
- Weight: ~3.5g
- Runtime: **8.8 hours** at 17mA draw (12mA XIAO + 5mA SX1262 RX)
- Connection: Solder directly to BAT+/BAT- pads

### Antenna

**Molex 105262-0002** flex PCB antenna
- Balanced dipole design (ground-plane independent)
- 79 × 10 × 0.1mm with 150mm U.FL cable
- Adhesive mount inside top of mask shell
- Link margin at 90ft: **>98 dB** (LoRa SF7 @ 915 MHz)

### I2C Tether Wiring

30AWG silicone-jacketed stranded wire, 4 conductors:
- Twist SDA+GND together, SCL+VCC together (noise immunity)
- Sleeve in 1/8" PET braided sleeving
- Strain relief: heat shrink + RTV silicone at both ends
- External 4.7kΩ pull-ups on SDA/SCL recommended

### Moisture Protection

1. **Inner:** MG Chemicals 422B silicone conformal coating (2 coats, mask USB-C + battery contacts)
2. **Outer:** SLA resin snap-fit enclosure with silicone gasket

### Display Mounting

- 3D-print clip bracket (1mm-wall SLA resin)
- C-clips snap onto ∅4-5mm cage top bar
- 0.5mm polycarbonate window over display face
- Position: upper peripheral vision (browline)

### Wire Routing (Safety Critical)

- **All wires fully secured** — zero exposed loops
- Route behind padding along structural channels
- Kapton tape at 15-20mm intervals
- Never route across joints, hinge points, or shifting padding areas
- Three-layer strain relief at every termination point

---

## System Specifications

| Parameter | Value |
|-----------|-------|
| Total weight | ~18g (on 578g mask) |
| Main module dims | 25 × 22 × 18mm |
| Display dims | 15 × 16 × 4mm (with bracket) |
| Runtime | 8.8 hours (150mAh @ 17mA) |
| Frequency | 915.0 MHz LoRa |
| Link margin @ 90ft | >98 dB |
| Display hold time | 5 seconds |
| Latency | <60ms |

---

## Link Budget Analysis

```
TX Power:           +22 dBm
TX Antenna Gain:     +2 dBi (T-Deck whip)
RX Antenna Gain:     -2 dBi (flex dipole, body proximity)
Path Loss (90ft):   -60.4 dB
Body Absorption:    -10 dB
Cage Shielding:     -10 dB

Received Power:     -58.4 dBm
RX Sensitivity:     -137 dBm (SF12) / -124 dBm (SF7)
Fade Margin:         65.6 dB (SF7)
```

---

## Test Procedure

1. Flash `Catcher_HUD_Receiver_v2.ino` to XIAO nRF52840
2. Bench test: verify OLED displays "STANDBY" on power-up
3. Transmit from T-Deck Plus coach unit — verify pitch call appears on display
4. Field test at 10m, 50m, 100m, 200m — confirm RSSI > -100 dBm at 200m
5. Impact test: tap mask with 30-50g acceleration — verify no disconnect
6. Moisture test: light mist spray — verify no display artifacts
7. Full game test: 4+ hours continuous operation

---

## File Index

```
XIAO_Catcher_HUD/
├── Catcher_HUD_Receiver_v2.ino    — Production firmware
├── Compact_Build_Guide.md          — This document
├── Schematics/
│   ├── wiring_schematic.svg        — Full electrical connections
│   └── mechanical_assembly.svg     — 4-view dimensional layout
└── Mechanical/
    ├── shapr3d_enclosure.svg       — Enclosure modeling reference
    └── exploded_assembly.svg       — Component assembly rendition
```
