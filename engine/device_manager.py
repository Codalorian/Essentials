from __future__ import annotations
"""Cross-platform audio / MIDI device enumeration and selection."""
import sys
import platform

def get_platform() -> str:
    """Return 'windows', 'mac', or 'linux'."""
    p = platform.system().lower()
    if p == 'windows':
        return 'windows'
    if p == 'darwin':
        return 'mac'
    return 'linux'

# ─── Audio devices ────────────────────────────────────────────────────────────

def list_audio_devices() -> list[dict]:
    import sounddevice as sd
    devs = []
    for i, d in enumerate(sd.query_devices()):
        if d['max_output_channels'] >= 2:
            devs.append({'index': i, 'name': d['name'],
                         'api': d['hostapi'],
                         'channels': d['max_output_channels'],
                         'default_sr': int(d['default_samplerate'])})
    return devs

def print_audio_devices():
    import sounddevice as sd
    default_idx = sd.default.device[1]  # output device
    print('\nAvailable audio OUTPUT devices:')
    for d in list_audio_devices():
        marker = '* ' if d['index'] == default_idx else '  '
        api = sd.query_hostapis(d['api'])['name']
        print(f"  {marker}[{d['index']:2d}] {d['name']!r:50s}  {api}")
    print()

def find_audio_device(name_or_index) -> int | None:
    """Return device index matching a name substring or integer index."""
    if name_or_index is None:
        return None
    try:
        return int(name_or_index)
    except (TypeError, ValueError):
        pass
    devs = list_audio_devices()
    needle = str(name_or_index).lower()
    for d in devs:
        if needle in d['name'].lower():
            return d['index']
    return None

# ─── MIDI devices ─────────────────────────────────────────────────────────────

def list_midi_inputs() -> list[str]:
    try:
        import mido
        return mido.get_input_names()
    except Exception:
        return []

def print_midi_devices():
    ports = list_midi_inputs()
    print('\nAvailable MIDI INPUT ports:')
    if not ports:
        print('  (none found)')
    for i, p in enumerate(ports):
        print(f'  [{i}] {p!r}')
    print()

def find_midi_port(name_or_index) -> str | None:
    """Return MIDI port name matching substring or index."""
    ports = list_midi_inputs()
    if not ports:
        return None
    if name_or_index is None:
        return ports[0]
    try:
        return ports[int(name_or_index)]
    except (TypeError, ValueError, IndexError):
        pass
    needle = str(name_or_index).lower()
    for p in ports:
        if needle in p.lower():
            return p
    return None

# ─── Platform hints ───────────────────────────────────────────────────────────

def routing_hint() -> str:
    plat = get_platform()
    if plat == 'windows':
        return (
            'Windows routing to Ableton:\n'
            '  1. Install VB-Cable  https://vb-audio.com/Cable/\n'
            '     Run OpenOmni with:  python main.py --device "CABLE Input"\n'
            '     In Ableton: set audio input to "CABLE Output"\n'
            '  2. Install loopMIDI  https://www.tobias-erichsen.de/software/loopmidi.html\n'
            '     Create a port (e.g. "OpenOmni MIDI"), then:\n'
            '     Run OpenOmni with:  python main.py --midi "OpenOmni MIDI"\n'
            '     In Ableton: set MIDI output to "OpenOmni MIDI"\n'
            '  3. Low-latency tip: use ASIO driver (ASIO4ALL or your audio interface)\n'
            '     python main.py --device "ASIO4ALL"\n'
        )
    if plat == 'mac':
        return (
            'macOS routing to Ableton:\n'
            '  1. Install BlackHole  https://existential.audio/blackhole/\n'
            '     python main.py --device "BlackHole 2ch"\n'
            '     In Ableton: set audio input to "BlackHole 2ch"\n'
            '  2. MIDI: enable IAC Driver in Audio MIDI Setup, then:\n'
            '     python main.py --midi "IAC Driver Bus 1"\n'
        )
    return (
        'Linux routing to Ableton (Wine):\n'
        '  Install: sudo apt install pipewire-jack a2jmidid qpwgraph\n'
        '  Run: pw-jack python3 main.py\n'
        '  Wire in qpwgraph: OpenOmni:out_L/R → DAW input\n'
        '                    DAW MIDI out     → OpenOmni:midi_in\n'
    )
