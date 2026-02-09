# T-Deck PitchComm Examples

This directory contains example sketches for the T-Deck PitchComm baseball/softball pitch communication system.

## Available Examples

### CatcherHUD
Catcher HUD receiver using:
- Hardware: Seeed XIAO nRF52840 + Wio-SX1262 LoRa Module
- Display: HiLetgo 0.49" SSD1306 64x32 I2C OLED
- Mount: All-Star catcher mask — HUD configuration
- Features: Display-only pitch call system with LoRa communication at 915 MHz

### CatcherArmband
Catcher armband receiver using:
- Hardware: Seeed XIAO nRF52840 + Wio-SX1262 LoRa Module
- Display: Seeed 2.13" Monochrome ePaper 122x250 (SSD1680)
- Mount: Forearm armband — low-profile linear enclosure
- Features: ePaper display with partial refresh for sub-500ms pitch call updates

## RF Communication

All examples use the following RF parameters to communicate with the T-Deck Plus Coach Transmitter:
- Frequency: 915.0 MHz
- Spreading Factor: 7
- Bandwidth: 125 kHz
- Coding Rate: 4/5
- Sync Word: 0x34

## Dependencies

These examples require the following Arduino libraries:
- RadioLib (for LoRa communication)
- U8g2lib (for OLED display - CatcherHUD)
- GxEPD2_BW (for ePaper display - CatcherArmband)

## Hardware Requirements

- ESP32-based board (configured for esp32:esp32:esp32)
- LoRa radio module (SX1262)
- Compatible display (OLED or ePaper depending on example)

For more information, see the main project repository.
