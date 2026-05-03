#!/usr/bin/env bash
# OpenOmni macOS Setup
# Run: bash setup_mac.sh

set -e

echo "=== OpenOmni macOS Setup ==="

# Check Python
if ! command -v python3 &>/dev/null; then
    echo "ERROR: Python 3 not found."
    echo "Install via: brew install python3  OR  https://www.python.org/downloads/"
    exit 1
fi
python3 --version

# pip packages
echo "[1/3] Installing Python packages..."
pip3 install numpy scipy sounddevice mido python-rtmidi JACK-Client

echo ""
echo "[2/3] Detected devices:"
python3 -c "
from engine.device_manager import print_audio_devices, print_midi_devices
print_audio_devices()
print_midi_devices()
"

echo ""
echo "[3/3] Manual steps for DAW routing:"
cat <<'EOF'

  BlackHole (virtual audio — FREE, no kernel extension needed):
    Download: https://existential.audio/blackhole/
    Install, then set multi-output device in Audio MIDI Setup if needed.
    Usage: python3 main.py --device "BlackHole 2ch"
    In Ableton: set audio input to "BlackHole 2ch"

  IAC Driver (virtual MIDI — built into macOS):
    Open "Audio MIDI Setup" → Window → Show MIDI Studio
    Double-click "IAC Driver" → check "Device is online"
    Add a port named "OpenOmni MIDI"
    Usage: python3 main.py --midi "IAC Driver OpenOmni MIDI"
    In Ableton MIDI Preferences → Outputs → enable "IAC Driver OpenOmni MIDI"

=== Run Commands ===

  Standalone:
    python3 main.py

  With BlackHole + IAC to Ableton:
    python3 main.py --device "BlackHole 2ch" --midi "IAC Driver OpenOmni MIDI"

  Show all devices:
    python3 main.py --list-devices

  Show routing instructions:
    python3 main.py --hint
EOF
