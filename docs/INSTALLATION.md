# Installation Guide

Complete setup instructions for T-Deck PitchComm system.

---

## Prerequisites

### Software Requirements

| Software | Version | Download |
|----------|---------|----------|
| Arduino IDE | 2.0+ | [arduino.cc](https://www.arduino.cc/en/software) |
| ESP32 Board Package | 2.0.11+ | Via Board Manager |
| USB Drivers | Latest | See below |

### Hardware Requirements

**Coach Unit:**
- LilyGO T-Deck Plus (ESP32-S3)
- USB-C data cable (not charge-only)

**Catcher Unit:**
- LilyGO T-Watch S3 or T-Watch LoRa32
- USB-C data cable

---

## Step 1: Arduino IDE Setup

### 1.1 Install Arduino IDE

Download and install Arduino IDE 2.x from:
```
https://www.arduino.cc/en/software
```

### 1.2 Add ESP32 Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In "Additional Board Manager URLs", add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**
5. Go to **Tools → Board → Boards Manager**
6. Search for `esp32`
7. Install **esp32 by Espressif Systems** (version 2.0.11 or later)

---

## Step 2: Install Required Libraries

### Via Library Manager

1. Go to **Sketch → Include Library → Manage Libraries**
2. Search and install:

| Library | Author | Version |
|---------|--------|---------|
| RadioLib | Jan Gromes | 6.4.0+ |
| TFT_eSPI | Bodmer | 2.5.0+ |

---

## Step 3: Configure TFT_eSPI Display Driver

**CRITICAL STEP** - The display will not work without this configuration.

### 3.1 Locate Your TFT_eSPI Library Folder

**Windows:**
```
C:\Users\<YOUR_USERNAME>\Documents\Arduino\libraries\TFT_eSPI\
```

**macOS:**
```
~/Documents/Arduino/libraries/TFT_eSPI/
```

**Linux:**
```
~/Arduino/libraries/TFT_eSPI/
```

### 3.2 Replace User_Setup.h

1. Navigate to the TFT_eSPI library folder
2. **Backup** the existing `User_Setup.h` file
3. Copy `CoachUnit/User_Setup.h` from this repository to replace it
4. **Restart Arduino IDE**

---

## Step 4: Install USB Drivers

### Windows

**ESP32-S3 Native USB:**
Most Windows 10/11 systems auto-install. If not:
```
https://dl.espressif.com/dl/idf-driver/idf-driver-esp32-usb-jtag-2021-07-15.zip
```

**CH340 (if present):**
```
https://www.wch-ic.com/downloads/CH341SER_EXE.html
```

**CP210x (if present):**
```
https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
```

### macOS / Linux

Usually no additional drivers required for ESP32-S3 native USB.

---

## Step 5: Configure Arduino IDE Board Settings

### For Coach Unit (T-Deck Plus)

**Tools Menu:**

```
Board:              ESP32S3 Dev Module
Port:               (Select your COM port)
USB CDC On Boot:    Enabled          ← CRITICAL
USB Mode:           Hardware CDC and JTAG
Upload Mode:        UART0 / Hardware CDC
CPU Frequency:      240MHz (WiFi)
Flash Mode:         QIO 80MHz
Flash Size:         16MB (128Mb)
Partition Scheme:   Default 4MB with spiffs
PSRAM:              OPI PSRAM
Upload Speed:       921600
```

### For Catcher Unit (T-Watch)

Similar settings - adjust Flash Size if your watch has different memory.

---

## Step 6: Upload Firmware

### 6.1 Connect Device

1. Connect T-Deck Plus via USB-C
2. Wait for Windows to recognize the device
3. Note the COM port number in Device Manager

### 6.2 Enter Bootloader Mode

**If automatic upload fails, use manual bootloader entry:**

```
1. HOLD the BOOT button
2. PRESS and RELEASE the RESET button
3. Wait 2 seconds
4. RELEASE the BOOT button
5. Click Upload within 5 seconds
```

### 6.3 Upload

1. Open `CoachUnit/CoachUnit.ino` in Arduino IDE
2. Select correct **Port** under Tools menu
3. Click **Upload** (→ arrow button)
4. Wait for "Done uploading" message

### 6.4 Verify Boot

Open **Serial Monitor** (Tools → Serial Monitor) at 115200 baud.

You should see:
```
╔════════════════════════════════════════╗
║     T-DECK PITCHCOMM - COACH UNIT      ║
║         Firmware v1.0 Production       ║
╚════════════════════════════════════════╝

>>> 3-SECOND RECOVERY WINDOW <<<
```

---

## Step 7: Upload Catcher Unit

Repeat Steps 5-6 using `CatcherUnit/CatcherUnit.ino` for the T-Watch device.

---

## Troubleshooting

### "Cannot configure port" Error

1. Close all Serial Monitors and terminal programs
2. Kill stray processes:
   ```powershell
   Get-Process | Where-Object {$_.Name -match "arduino|serial"} | Stop-Process -Force
   ```
3. Enter bootloader mode manually
4. Retry upload

### Display Shows Nothing

1. Verify `User_Setup.h` was copied correctly
2. Ensure Arduino IDE was restarted after copying
3. Check TFT_BL pin is being set HIGH

### Radio Initialization Failed

1. Verify SPI pin definitions match your hardware
2. Check `config.h` pin assignments
3. Ensure RadioLib library is installed

### Device Not Recognized (No COM Port)

1. Try different USB cable (must be data-capable)
2. Try different USB port (rear motherboard preferred)
3. Install USB drivers manually
4. Check Device Manager for errors

### Bricked Device (No USB Response)

1. Enter bootloader manually (BOOT + power cycle)
2. If still unresponsive:
   ```
   esptool.exe --port COM6 erase_flash
   ```
3. Re-upload firmware

---

## Post-Installation Verification

### Coach Unit Test

1. Power on T-Deck Plus
2. Wait for "READY" on display
3. Press key `1` on keyboard
4. Display should flash "FASTBALL" then return to READY
5. Serial Monitor shows TX confirmation

### Catcher Unit Test

1. Power on T-Watch
2. Wait for "WAITING" on display
3. Transmit from Coach Unit
4. Display shows pitch name with RSSI/SNR
5. Vibration motor activates with pattern

### Range Test

1. Start at 10 meters apart
2. Send signals, verify reception
3. Increase distance incrementally
4. Nominal range: 400+ meters line-of-sight

---

## Firmware Updates

To update firmware:

1. Enter bootloader mode
2. Upload new sketch
3. Firmware version displayed on boot

---

## Support

Open an issue at:
```
https://github.com/clueless187-8/T-Deck-Pitchcomm/issues
```
