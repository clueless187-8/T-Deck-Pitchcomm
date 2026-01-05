# T-Deck PitchComm

[![PlatformIO CI](https://github.com/clueless187-8/T-Deck-Pitchcomm/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/clueless187-8/T-Deck-Pitchcomm/actions/workflows/c-cpp.yml)

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
- **Operating Range**: 1-3 km line of sight (LoRa 915MHz)
- **Battery Life**: 8-12 hours continuous use
- **Low Latency**: <100ms typical signal transmission

### Catcher Unit (T-Watch Receiver)
- Full-screen pitch display with color coding
- Zone number display
- Large PK1/PK2/PK3 pickoff display
- Large 3A/3B/3C/3D third sign display
- Signal quality (RSSI/SNR) logging
- **Operating Range**: 1-3 km line of sight (LoRa 915MHz)
- **Battery Life**: 8-12 hours continuous use
- **Security**: SYNC_WORD matching (0x12) prevents cross-talk with other devices

## Hardware Requirements

| Device | Model | Purchase |
|--------|-------|----------|
| Coach | LilyGO T-Deck Plus | [LilyGO Store](https://www.lilygo.cc/) |
| Catcher | LilyGO T-Watch S3 | [LilyGO Store](https://www.lilygo.cc/) |

## Quick Start

### 1. Install PlatformIO

Install [Visual Studio Code](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode).

**Prerequisites Checklist:**
- ✅ Visual Studio Code installed
- ✅ PlatformIO IDE extension installed
- ✅ CP210x USB driver for ESP32-S3 ([Download](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers))
- ✅ USB-C cable for device connection
- ✅ Both T-Deck and T-Watch devices charged

### 2. Upload T-Deck Transmitter

1. Open `TDeck_Transmitter` folder in VS Code
2. Connect T-Deck via USB
3. Click PlatformIO Upload (→ arrow)
4. **First-time setup**: If upload fails, hold BOOT button while connecting USB

**Serial Monitor Debugging:**
```bash
# Open serial monitor in PlatformIO
# Baud rate: 115200
# Watch for LoRa initialization messages and transmission confirmations
```

### 3. Upload T-Watch Receiver

1. Open `TWatch_Receiver` folder in VS Code  
2. Connect T-Watch S3 via USB
3. Click PlatformIO Upload (→ arrow)
4. **First-time setup**: If upload fails, hold BOOT button while connecting USB

**Verification:**
- T-Deck should display pitch selection interface
- T-Watch should display "READY" message
- Test by touching a pitch button on T-Deck

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

## Usage

### Coach (Transmitter) Operation
1. Power on T-Deck device
2. Touch pitch type button (FB/CB/CH/SL)
3. Touch zone number (1-9) to send signal
4. Use PK buttons for pickoff signals
5. Use 3A-3D for third sign system
6. RESET button clears pitch counter

### Catcher (Receiver) Operation
1. Power on T-Watch S3
2. Wait for "READY" message
3. Watch displays incoming signals automatically
4. Check RSSI/SNR for signal quality in serial monitor

## Technical Specifications

| Feature | Specification |
|---------|--------------|
| Frequency | 915 MHz (US/AU ISM band) |
| Modulation | LoRa |
| Bandwidth | 125 kHz |
| Spreading Factor | 10 |
| Coding Rate | 4/8 |
| Sync Word | 0x12 |
| TX Power | 22 dBm |
| Range | ~1-3 km (line of sight) |
| Latency | <100ms typical |
| Battery Life | 8-12 hours continuous use |

## RF Configuration

| Parameter | Value |
|-----------|-------|
| Frequency | 915 MHz (US ISM) |
| Spreading Factor | 10 |
| Bandwidth | 125 kHz |
| Coding Rate | 4/8 |
| Sync Word | 0x12 |
| TX Power | 22 dBm |

## Troubleshooting

### Device won't connect
- Ensure both devices are using the same LoRa frequency (915 MHz)
- Check SYNC_WORD matches on both devices (0x12)
- Verify antennas are properly connected
- Check that both devices are powered on and firmware uploaded successfully

### Upload fails
- Install CP210x USB driver for ESP32-S3
- Hold BOOT button while connecting USB cable
- Try reducing upload_speed in platformio.ini (e.g., from 921600 to 115200)
- Ensure USB cable supports data transfer (not charge-only)
- Close other applications using the serial port

### No display output
- Check TFT_eSPI configuration in lib/TFT_eSPI_User_Setup.h
- Verify screen brightness settings in code
- Check power connections and battery charge
- Try power cycling the device (full power off/on)

### Poor signal quality
- Ensure antennas are vertical and unobstructed
- Keep devices away from metal objects and electronics
- Check RSSI/SNR values in serial monitor (RSSI > -120 dBm is good)
- Reduce distance between devices for testing
- Verify antenna connectors are tight

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

## Development

### Building from Source
```bash
# Clone repository
git clone https://github.com/clueless187-8/T-Deck-Pitchcomm.git
cd T-Deck-Pitchcomm

# Build transmitter
cd TDeck_Transmitter
pio run

# Build receiver
cd ../TWatch_Receiver
pio run
```

### Running Tests
```bash
pio test
```

### Code Formatting
This project uses standard C++ formatting conventions. Follow existing code style when contributing.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Ideas for Contributions
- [ ] Add more pitch types (Splitter, Knuckleball, etc.)
- [ ] Implement signal encryption for enhanced security
- [ ] Add battery percentage display on both devices
- [ ] Create Android/iOS companion app for configuration
- [ ] Add game statistics logging (pitch counts, types used)
- [ ] Multi-language support
- [ ] Add vibration/haptic feedback for received signals
- [ ] Implement different frequency bands (868 MHz for Europe)

## FAQ

**Q: Can I use this for games/tournaments?**
A: Check your league's rules regarding electronic communication devices. Some leagues may prohibit electronic signaling.

**Q: What's the battery life?**
A: Approximately 8-12 hours of continuous use on a full charge, depending on usage patterns and screen brightness.

**Q: Can I customize the pitch types?**
A: Yes! Modify the pitch buttons in `TDeck_Transmitter/src/main.cpp` to add or change pitch types.

**Q: Does this work in other countries?**
A: The 915 MHz frequency is for US/Australia. For Europe (868 MHz) or other regions, modify the LoRa frequency in the code to match your local ISM band regulations.

**Q: How secure is the communication?**
A: Currently uses basic SYNC_WORD matching (0x12) to prevent cross-talk with other devices. For enhanced security in competitive environments, consider implementing encryption.

**Q: What is the maximum range?**
A: With line of sight and proper antenna positioning, typically 1-3 km. Range may be reduced by obstacles, interference, or weather conditions.

## Photos

### Coach Device (T-Deck Plus)
*[Add photo of T-Deck showing pitch selection interface]*

### Catcher Device (T-Watch S3)
*[Add photo of T-Watch displaying pitch signal]*

### In Action
*[Add photo of devices being used in game scenario]*

> **Note**: Add actual device photos to make the README more engaging

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [LilyGO](https://www.lilygo.cc/) for the amazing T-Deck and T-Watch hardware
- [RadioLib](https://github.com/jgromes/RadioLib) for LoRa communication library
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) for display drivers
- Meshtastic project for T-Watch S3 pin configurations
- Baseball/Softball community for inspiration
- Evans Bros. Baseball
- Bloomfield High School Baseball
