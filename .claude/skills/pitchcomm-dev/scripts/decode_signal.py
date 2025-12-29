#!/usr/bin/env python3
"""
PitchSignal Decoder for T-Deck PitchComm

Decodes raw hex bytes from LoRa packets into human-readable signal information.

Usage:
    python decode_signal.py "00 02 05 00 00 0A 00"
    python decode_signal.py 00020500000A00
"""

import sys

PITCH_NAMES = {
    0: 'Fastball (FB)',
    1: 'Curveball (CB)',
    2: 'Changeup (CH)',
    3: 'Slider (SL)',
    4: 'Pickoff (PO)',
    255: 'None',
}

PITCH_COLORS = {
    0: 'Red',
    1: 'Yellow',
    2: 'Green',
    3: 'Cyan',
    4: 'Magenta',
}

SIGNAL_TYPES = {
    0: 'Pitch Signal',
    1: 'Reset Signal',
}

THIRD_SIGN_NAMES = {
    0: 'None',
    1: '3A',
    2: '3B',
    3: '3C',
    4: '3D',
}

def parse_hex_string(hex_str: str) -> bytes:
    """Parse hex string with or without spaces."""
    clean = hex_str.replace(' ', '').replace('0x', '')
    return bytes.fromhex(clean)

def decode_signal(data: bytes) -> dict:
    """Decode PitchSignal struct from bytes."""
    if len(data) < 7:
        raise ValueError(f"Expected 7 bytes, got {len(data)}")

    return {
        'type': data[0],
        'pitch': data[1],
        'zone': data[2],
        'pickoff': data[3],
        'thirdSign': data[4],
        'number': data[5] | (data[6] << 8),  # Little-endian uint16_t
    }

def format_signal(sig: dict) -> str:
    """Format decoded signal as human-readable string."""
    lines = []
    lines.append("=" * 50)
    lines.append("PitchSignal Decoded")
    lines.append("=" * 50)

    # Signal type
    sig_type = SIGNAL_TYPES.get(sig['type'], f"Unknown ({sig['type']})")
    lines.append(f"Type:       {sig_type}")

    # Pitch
    pitch_name = PITCH_NAMES.get(sig['pitch'], f"Unknown ({sig['pitch']})")
    pitch_color = PITCH_COLORS.get(sig['pitch'], 'N/A')
    lines.append(f"Pitch:      {pitch_name}")
    if sig['pitch'] < 255:
        lines.append(f"Color:      {pitch_color}")

    # Zone
    if sig['zone'] > 0:
        lines.append(f"Zone:       {sig['zone']} (Strike Zone Grid)")
    else:
        lines.append("Zone:       None")

    # Pickoff
    if sig['pickoff'] > 0:
        lines.append(f"Pickoff:    PK{sig['pickoff']} (Base {sig['pickoff']})")
    else:
        lines.append("Pickoff:    None")

    # Third Sign
    third = THIRD_SIGN_NAMES.get(sig['thirdSign'], f"Unknown ({sig['thirdSign']})")
    lines.append(f"Third Sign: {third}")

    # Signal number
    lines.append(f"Number:     #{sig['number']}")

    lines.append("=" * 50)

    # Visual zone representation
    if sig['zone'] > 0 and sig['type'] == 0:
        lines.append("")
        lines.append("Strike Zone:")
        lines.append("+---+---+---+")
        for row in range(3):
            row_str = "|"
            for col in range(3):
                zone_num = row * 3 + col + 1
                if zone_num == sig['zone']:
                    row_str += " X |"
                else:
                    row_str += f" {zone_num} |"
            lines.append(row_str)
            lines.append("+---+---+---+")

    return '\n'.join(lines)

def main():
    if len(sys.argv) < 2:
        print("Usage: python decode_signal.py <hex_bytes>")
        print()
        print("Examples:")
        print('  python decode_signal.py "00 02 05 00 00 0A 00"')
        print("  python decode_signal.py 00020500000A00")
        print()
        print("Byte format (7 bytes):")
        print("  [0] type      - 0=pitch, 1=reset")
        print("  [1] pitch     - 0=FB, 1=CB, 2=CH, 3=SL, 4=PO, 255=none")
        print("  [2] zone      - 1-9 strike zone, 0=none")
        print("  [3] pickoff   - 0=none, 1-3=base")
        print("  [4] thirdSign - 0=none, 1-4=A/B/C/D")
        print("  [5-6] number  - uint16_t little-endian signal count")
        sys.exit(1)

    hex_input = ' '.join(sys.argv[1:])

    try:
        data = parse_hex_string(hex_input)
        signal = decode_signal(data)
        print(format_signal(signal))
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Failed to decode: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
