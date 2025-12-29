#!/usr/bin/env python3
"""
LoRa Configuration Validator for T-Deck PitchComm

Compares LoRa settings between transmitter and receiver to detect mismatches.
"""

import re
import sys
from pathlib import Path

# Expected configuration values
EXPECTED_CONFIG = {
    'frequency': 915.0,
    'spreading_factor': 10,
    'bandwidth': 125.0,
    'coding_rate': 8,
    'sync_word': 0x12,
    'output_power': 22,
}

def extract_lora_config(filepath: Path) -> dict:
    """Extract LoRa configuration from a main.cpp file."""
    config = {}

    try:
        content = filepath.read_text()
    except FileNotFoundError:
        return None

    # Patterns to match RadioLib configuration calls
    patterns = {
        'frequency': r'radio\.begin\s*\(\s*([\d.]+)\s*\)',
        'spreading_factor': r'radio\.setSpreadingFactor\s*\(\s*(\d+)\s*\)',
        'bandwidth': r'radio\.setBandwidth\s*\(\s*([\d.]+)\s*\)',
        'coding_rate': r'radio\.setCodingRate\s*\(\s*(\d+)\s*\)',
        'sync_word': r'radio\.setSyncWord\s*\(\s*(0x[0-9a-fA-F]+|\d+)\s*\)',
        'output_power': r'radio\.setOutputPower\s*\(\s*(\d+)\s*\)',
    }

    for param, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            value = match.group(1)
            if param == 'sync_word':
                config[param] = int(value, 16) if value.startswith('0x') else int(value)
            elif param in ('frequency', 'bandwidth'):
                config[param] = float(value)
            else:
                config[param] = int(value)

    return config

def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent.parent.parent

    tx_path = project_root / 'TDeck_Transmitter' / 'src' / 'main.cpp'
    rx_path = project_root / 'TWatch_Receiver' / 'src' / 'main.cpp'

    print("=" * 60)
    print("LoRa Configuration Validator for T-Deck PitchComm")
    print("=" * 60)
    print()

    # Extract configurations
    tx_config = extract_lora_config(tx_path)
    rx_config = extract_lora_config(rx_path)

    if tx_config is None:
        print(f"ERROR: Could not read {tx_path}")
        sys.exit(1)

    if rx_config is None:
        print(f"ERROR: Could not read {rx_path}")
        sys.exit(1)

    # Compare configurations
    print(f"{'Parameter':<20} {'Transmitter':<15} {'Receiver':<15} {'Expected':<15} {'Status'}")
    print("-" * 80)

    all_match = True

    for param, expected in EXPECTED_CONFIG.items():
        tx_val = tx_config.get(param, 'NOT FOUND')
        rx_val = rx_config.get(param, 'NOT FOUND')

        # Format values for display
        if param == 'sync_word':
            tx_display = f"0x{tx_val:02X}" if isinstance(tx_val, int) else tx_val
            rx_display = f"0x{rx_val:02X}" if isinstance(rx_val, int) else rx_val
            exp_display = f"0x{expected:02X}"
        elif param in ('frequency', 'bandwidth'):
            tx_display = f"{tx_val} MHz" if param == 'frequency' else f"{tx_val} kHz"
            rx_display = f"{rx_val} MHz" if param == 'frequency' else f"{rx_val} kHz"
            exp_display = f"{expected} MHz" if param == 'frequency' else f"{expected} kHz"
        elif param == 'output_power':
            tx_display = f"{tx_val} dBm"
            rx_display = f"{rx_val} dBm"
            exp_display = f"{expected} dBm"
        else:
            tx_display = str(tx_val)
            rx_display = str(rx_val)
            exp_display = str(expected)

        # Check for mismatches
        if tx_val == rx_val == expected:
            status = "OK"
        elif tx_val != rx_val:
            status = "MISMATCH!"
            all_match = False
        elif tx_val != expected or rx_val != expected:
            status = "WARNING"
            all_match = False
        else:
            status = "OK"

        print(f"{param:<20} {tx_display:<15} {rx_display:<15} {exp_display:<15} {status}")

    print()

    if all_match:
        print("SUCCESS: All LoRa configurations match!")
        print("Both devices should be able to communicate.")
    else:
        print("FAILURE: Configuration mismatches detected!")
        print("Fix the mismatched parameters before testing communication.")
        sys.exit(1)

if __name__ == '__main__':
    main()
