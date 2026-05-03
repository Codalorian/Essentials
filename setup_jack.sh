#!/usr/bin/env bash
# OpenOmni JACK/PipeWire setup for Linux
# Run once: bash setup_jack.sh
set -e

echo "=== OpenOmni JACK setup ==="

# 1. Install system packages
echo "[1/4] Installing pipewire-jack, a2jmidid, qpwgraph..."
sudo apt-get install -y pipewire-jack a2jmidid qpwgraph

# 2. Install Python deps
echo "[2/4] Installing Python JACK-Client..."
pip3 install --quiet JACK-Client

# 3. Verify PipeWire JACK bridge works
echo "[3/4] Verifying JACK bridge..."
pw-jack python3 -c "import jack; c = jack.Client('test'); c.close(); print('JACK bridge OK')"

echo ""
echo "[4/4] Done! To run OpenOmni with JACK/DAW routing:"
echo ""
echo "  pw-jack python3 main.py"
echo ""
echo "Then in qpwgraph:"
echo "  • Connect  OpenOmni:out_L/R  →  your DAW audio input (or system:playback)"
echo "  • Connect  your DAW MIDI out →  OpenOmni:midi_in"
echo ""
echo "For Ableton via Wine:"
echo "  Start a2jmidid first so Wine/Ableton MIDI appears in JACK:"
echo "    a2jmidid -e &"
echo "  Then wire Ableton's MIDI port to OpenOmni:midi_in in qpwgraph."
