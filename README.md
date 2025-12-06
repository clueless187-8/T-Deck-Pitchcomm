# T-Deck PitchComm

**Professional RF Communication System for Baseball**

Real-time, encrypted pitch signal transmission from coach to catcher using LoRa radio technology.

![License](https://img.shields.io/badge/License-MIT-blue.svg)
![Platform](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)
![Frequency](https://img.shields.io/badge/Frequency-915MHz-orange.svg)

---

## Overview

T-Deck PitchComm is a standalone RF communication system designed for baseball operations. It enables coaches to transmit pitch signals directly to catchers using secure, low-latency LoRa radio communication—eliminating the need for traditional hand signals that can be intercepted by opposing teams.

### Key Features

- **Sub-60ms latency** - Real-time signal delivery
- **400+ meter range** - Covers any baseball field with margin
- **Standalone operation** - No WiFi, cellular, or internet required
- **Tactile input** - Physical keyboard for eyes-free operation
- **Haptic feedback** - Vibration patterns for discrete signal reception
- **CRC validation** - Error-checked packet integrity
- **Sequence tracking** - Duplicate packet rejection

---

## System Architecture

```
┌─────────────────┐         915 MHz LoRa         ┌─────────────────┐
│   COACH UNIT    │ ─────────────────────────────│  CATCHER UNIT   │
│   (T-Deck Plus) │          ~5ms airtime        │   (T-Watch)     │
├─────────────────┤                              ├─────────────────┤
│ • ESP32-S3      │                              │ • ESP32-S3      │
│ • SX1262 Radio  │                              │ • SX1262 Radio  │
│ • 320x240 TFT   │                              │ • 240x240 TFT   │
│ • QWERTY KB     │                              │ • Vibration     │
└─────────────────┘                              └─────────────────┘
     [1-9,0,P]                                      [Haptic Alert]
      Keypress                                      + Display
```

---

## Hardware Requirements

### Coach Unit (Transmitter)
- **LilyGO T-Deck Plus** (ESP32-S3 with integrated keyboard)
- USB-C data cable
- Optional: External antenna for extended range

### Catcher Unit (Receiver)  
- **LilyGO T-Watch S3** or **T-Watch LoRa32**
- USB-C data cable
- Wrist strap for secure wear

---

## RF Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| Frequency | 915.0 MHz | US ISM band (FCC Part 15) |
| Modulation | LoRa | Chirp spread spectrum |
| Bandwidth | 250 kHz | Optimized for speed |
| Spreading Factor | SF7 | Fastest data rate |
| Coding Rate | 4/5 | Balance of speed/reliability |
| TX Power | +22 dBm | 158 mW |
| Sensitivity | -124 dBm | At SF7/250kHz |
| Link Budget | 154 dB | Excellent margin |
| Fade Margin | 65+ dB | Robust in multipath |

---

## Pitch Signal Mapping

| Key | Pitch | Code | Vibration Pattern |
|-----|-------|------|-------------------|
| 1 | Fastball | 0x01 | Single long pulse |
| 2 | Curveball | 0x02 | Double short pulse |
| 3 | Slider | 0x03 | Triple pulse |
| 4 | Changeup | 0x04 | Extended pulse |
| 5 | Cutter | 0x05 | Rapid burst |
| 6 | Sinker | 0x06 | Long-short-long |
| 7 | Splitter | 0x07 | Short-pause-short |
| 8 | Knuckleball | 0x08 | Wave pattern |
| 9 | Screwball | 0x09 | Long-short |
| 0 | Pitchout | 0x0A | Extra long |
| P | Pickoff | 0x0B | Alert pattern |

---

## Quick Start

### 1. Install Arduino IDE
Download from [arduino.cc](https://www.arduino.cc/en/software)

### 2. Add ESP32 Board Support
Add to Preferences → Additional Board Manager URLs:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

### 3. Install Libraries
- **RadioLib** (6.4.0+)
- **TFT_eSPI** (2.5.0+)

### 4. Configure Display
Copy `CoachUnit/User_Setup.h` to your TFT_eSPI library folder.

### 5. Upload Firmware
See [docs/INSTALLATION.md](docs/INSTALLATION.md) for detailed instructions.

---

## Project Structure

```
T-Deck-Pitchcomm/
├── CoachUnit/
│   ├── CoachUnit.ino        # Main transmitter firmware
│   ├── config.h             # Hardware pin definitions
│   └── User_Setup.h         # TFT_eSPI configuration
├── CatcherUnit/
│   └── CatcherUnit.ino      # Receiver firmware
├── docs/
│   ├── INSTALLATION.md      # Setup instructions
│   └── PROTOCOL.md          # RF protocol specification
├── LICENSE                  # MIT License
└── README.md                # This file
```

---

## Safety Features

- **3-second boot delay** - USB recovery window for re-flashing
- **Watchdog timer** - Automatic recovery from system hangs
- **Graceful degradation** - System continues if non-critical hardware fails
- **Early USB initialization** - Serial available before hardware init

---

## Legal Compliance

- **US:** 915 MHz operation compliant with FCC Part 15
- **EU:** Modify to 868 MHz for ETSI compliance
- **Other regions:** Verify local ISM band regulations

---

## Contributing

Contributions welcome. Please submit pull requests for:
- Bug fixes
- Additional pitch types
- Hardware support for other ESP32 LoRa devices
- Documentation improvements

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

## Acknowledgments

- Espressif Systems (ESP32-S3)
- Semtech (SX1262 LoRa)
- LilyGO (T-Deck hardware)
- RadioLib contributors
