---
name: pitchcomm-dev
description: Assist with T-Deck PitchComm development including LoRa configuration validation, firmware building, protocol debugging, and signal testing. Use when working with LoRa settings, PlatformIO builds, pitch signals, or hardware troubleshooting.
---

# PitchComm Development Helper

This skill assists with developing the T-Deck PitchComm wireless pitch communication system.

## Project Overview

- **Coach Unit**: T-Deck Plus (transmitter) - touch UI for pitch selection
- **Catcher Unit**: T-Watch S3 (receiver) - displays pitch signals
- **Communication**: LoRa SX1262 at 915 MHz

## LoRa Configuration Validation

The most common issue is mismatched LoRa parameters between devices. Both must use identical settings:

| Parameter | Required Value | TX Location | RX Location |
|-----------|---------------|-------------|-------------|
| Frequency | 915.0 MHz | main.cpp:218 | main.cpp:190 |
| Spreading Factor | 10 | main.cpp:224 | main.cpp:196 |
| Bandwidth | 125.0 kHz | main.cpp:225 | main.cpp:197 |
| Coding Rate | 8 | main.cpp:226 | main.cpp:198 |
| Sync Word | 0x12 | main.cpp:229 | main.cpp:199 |
| Output Power | 22 dBm | main.cpp:227 | main.cpp:200 |

### Validate LoRa Configuration

Run this script to check for mismatches:

```bash
python3 .claude/skills/pitchcomm-dev/scripts/validate_lora.py
```

## Building Firmware

### Build Both Devices

```bash
# Build transmitter (T-Deck Plus)
cd TDeck_Transmitter && pio run

# Build receiver (T-Watch S3)
cd TWatch_Receiver && pio run
```

### Upload Firmware

```bash
# Upload to T-Deck (check port first)
cd TDeck_Transmitter && pio run -t upload

# Upload to T-Watch (check port first)
cd TWatch_Receiver && pio run -t upload
```

### Monitor Serial Output

```bash
# Monitor T-Deck
cd TDeck_Transmitter && pio device monitor -b 115200

# Monitor T-Watch
cd TWatch_Receiver && pio device monitor -b 115200
```

## Signal Protocol

The PitchSignal struct (7 bytes) is transmitted over LoRa:

```c
typedef struct {
  uint8_t type;       // 0=pitch, 1=reset
  uint8_t pitch;      // 0=FB, 1=CB, 2=CH, 3=SL, 4=PO, 255=none
  uint8_t zone;       // 1-9 strike zone, 0=none
  uint8_t pickoff;    // 0=none, 1-3=base
  uint8_t thirdSign;  // 0=none, 1-4=A/B/C/D
  uint16_t number;    // signal count (little-endian)
} PitchSignal;
```

### Pitch Type Codes

| Code | Pitch | Color |
|------|-------|-------|
| 0 | Fastball (FB) | Red |
| 1 | Curveball (CB) | Yellow |
| 2 | Changeup (CH) | Green |
| 3 | Slider (SL) | Cyan |
| 4 | Pickoff (PO) | Magenta |
| 255 | None | - |

### Decode Signal Bytes

Run to decode a received packet:

```bash
python3 .claude/skills/pitchcomm-dev/scripts/decode_signal.py "00 02 05 00 00 0A 00"
```

## Hardware Pin Reference

### T-Deck Plus (Transmitter)

| Function | Pin |
|----------|-----|
| LoRa NSS | 9 |
| LoRa DIO1 | 45 |
| LoRa BUSY | 13 |
| LoRa RST | 17 |
| LoRa MOSI | 41 |
| LoRa MISO | 38 |
| LoRa SCK | 40 |
| I2C SDA | 18 |
| I2C SCL | 8 |
| Touch INT | 16 |
| TFT Backlight | 42 |
| Power On | 10 |

### T-Watch S3 (Receiver)

| Function | Pin |
|----------|-----|
| LoRa CS | 5 |
| LoRa DIO1 | 9 |
| LoRa BUSY | 7 |
| LoRa RST | 8 |
| LoRa MOSI | 1 |
| LoRa MISO | 4 |
| LoRa SCK | 3 |
| I2C SDA | 10 |
| I2C SCL | 11 |
| TFT Backlight | 45 |

## Troubleshooting

### No Communication Between Devices

1. **Run LoRa validation script** - most common issue
2. Check sync word matches (0x12 on both)
3. Verify both devices are on 915 MHz
4. Check antenna connections
5. Monitor serial output for "LoRa: Ready"

### Touch Not Working (T-Deck)

1. Check I2C address 0x14 (GT911 touch controller)
2. Verify Touch INT pin 16 is connected
3. Check I2C pull-ups on SDA/SCL

### Display Issues

1. Verify TFT_eSPI User_Setup.h matches hardware
2. Check backlight pin is HIGH
3. Confirm SPI pins in platformio.ini

### Signal Quality Issues

- RSSI below -110 dBm = poor signal
- SNR below 0 = packet errors likely
- Try increasing spreading factor (trades speed for range)
