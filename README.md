# T-Deck PitchComm

**Baseball/Softball Pitch Communication System using LoRa**

Real-time wireless pitch signal transmission from coach (T-Deck) to catcher (T-Watch S3) using LoRa radio at 915 MHz. No more BLE/wifi with subscription fees and limited range. Easily modified GUI for your needs.

## System Overview

```
┌─────────────────────┐      LoRa 915MHz      ┌─────────────────────┐
│   COACH DEVICE      │ ────────────────────> │   CATCHER DEVICE    │
│   (T-Deck Plus)     │                       │   (T-Watch S3)      │
├─────────────────────┤                       ├─────────────────────┤
│ • ESP32-S3          │                       │ • ESP32-S3          │
│ • 320x240 TFT       │                       │ • 240x240 TFT       │
│ • Touch Screen      │                       │ • SX1262 LoRa       │
│ • SX1262 LoRa       │                       │ • AXP2101 PMIC      │
└─────────────────────┘                       └─────────────────────┘
```

## Features

### Coach Unit (T-Deck Transmitter)
- Touch screen UI for pitch selection
- 4 pitch types: FB (Fastball), CB (Curveball), CH (Changeup), SL (Slider)
- 3x3 zone grid (1-9) for pitch location
- Pickoff signals: PK1, PK2, PK3
- Third sign signals: 3A, 3B, 3C, 3D
- Pitch counters and RESET function

### Catcher Unit (T-Watch Receiver)
- Full-screen pitch display with color coding
- Zone number display
- Large PK1/PK2/PK3 pickoff display
- Large 3A/3B/3C/3D third sign display
- Signal quality (RSSI/SNR) logging

## Hardware Requirements

| Device | Model | Purchase |
|--------|-------|----------|
| Coach | LilyGO T-Deck Plus | [LilyGO Store](https://www.lilygo.cc/) |
| Catcher | LilyGO T-Watch S3 | [LilyGO Store](https://www.lilygo.cc/) |

## Quick Start

### 1. Install PlatformIO

Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode).

### 2. Upload T-Deck Transmitter

1. Open `TDeck_Transmitter` folder in VS Code
2. Connect T-Deck via USB
3. Click PlatformIO Upload (→ arrow)

### 3. Upload T-Watch Receiver

1. Open `TWatch_Receiver` folder in VS Code  
2. Connect T-Watch S3 via USB
3. Click PlatformIO Upload (→ arrow)

## Project Structure

```
T-Deck-Pitchcomm/
├── TDeck_Transmitter/          # Coach device firmware
│   ├── platformio.ini
│   ├── src/main.cpp
│   └── lib/TFT_eSPI_User_Setup.h
├── TWatch_Receiver/            # Catcher device firmware
│   ├── platformio.ini
│   ├── src/main.cpp
│   └── lib/TFT_eSPI_User_Setup.h
└── README.md
```

## RF Configuration

| Parameter | Value |
|-----------|-------|
| Frequency | 915 MHz (US ISM) |
| Spreading Factor | 10 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/8 |
| Sync Word | 0x12 |
| TX Power | 22 dBm |

## Signal Protocol

```c
typedef struct {
  uint8_t type;       // 0=pitch, 1=reset
  uint8_t pitch;      // 0=FB, 1=CB, 2=CH, 3=SL, 255=none
  uint8_t zone;       // 1-9 strike zone, 0=none
  uint8_t pickoff;    // 0=none, 1-3=base
  uint8_t thirdSign;  // 0=none, 1-4=A/B/C/D
  uint16_t number;    // signal count
} PitchSignal;
```

## Display Colors

| Signal | Color |
|--------|-------|
| Fastball (FB) | Red |
| Curveball (CB) | Yellow |
| Changeup (CH) | Green |
| Slider (SL) | Cyan |
| Pickoff (PK) | Blue |
| Third Sign (3x) | Red |

## License

MIT License - See [LICENSE](LICENSE) for details.

## Acknowledgments

- LilyGO for the T-Deck and T-Watch hardware
- Meshtastic project for T-Watch S3 pin configurations
- RadioLib for SX1262 LoRa driver
- Evans Bros. Baseball
- Bloomfield High School Baseball
