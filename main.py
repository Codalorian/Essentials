#!/usr/bin/env python3
from __future__ import annotations
"""
OpenOmni — open-source synthesizer  (Windows / macOS / Linux)

Quick start
───────────
  python main.py                        # auto-detect everything
  python main.py --list-devices         # show audio + MIDI devices, then exit

Device routing to a DAW
───────────────────────
  Windows  — install VB-Cable + loopMIDI (see --hint):
    python main.py --device "CABLE Input" --midi "OpenOmni MIDI"

  macOS    — install BlackHole + enable IAC Driver:
    python main.py --device "BlackHole 2ch" --midi "IAC Driver Bus 1"

  Linux    — use PipeWire-JACK (see setup_jack.sh):
    pw-jack python3 main.py

Keyboard play (no MIDI controller needed)
─────────────────────────────────────────
  a w s e d f t g y h u j k o l p ;   →   C4 – E5
  ESC = all notes off
"""
import sys
import argparse
import platform

SAMPLE_RATE = 44100
BLOCK_SIZE  = 512


# ─── Audio ────────────────────────────────────────────────────────────────────

def _run_sounddevice(synth, device=None, sample_rate=SAMPLE_RATE):
    import sounddevice as sd
    import numpy as np

    extra = None
    if platform.system() == 'Windows' and device:
        try:
            extra = sd.WasapiSettings(exclusive=True)
        except Exception:
            pass

    def audio_cb(outdata, frames, time, status):
        if status:
            print(f'[audio] {status}', file=sys.stderr)
        outdata[:] = synth.render(frames)

    try:
        stream = sd.OutputStream(
            samplerate=sample_rate, blocksize=BLOCK_SIZE,
            dtype='float32', channels=2, device=device,
            callback=audio_cb, extra_settings=extra)
    except Exception:
        stream = sd.OutputStream(
            samplerate=sample_rate, blocksize=BLOCK_SIZE,
            dtype='float32', channels=2, device=device,
            callback=audio_cb)

    stream.start()
    info = sd.query_devices(stream.device, 'output')
    print(f'[audio] Output: {info["name"]!r} @ {sample_rate} Hz')
    return stream


def _try_jack(synth, auto_connect):
    try:
        import jack as _p
        _p.Client('_openomni_probe').close()
    except Exception as exc:
        print(f'[JACK] Not available ({exc})', file=sys.stderr)
        return None
    from engine.jack_client import JACKClient
    jc = JACKClient(synth, auto_connect=auto_connect)
    jc.start()
    return jc


# ─── Entry point ─────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='OpenOmni synthesizer',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument('--device', metavar='NAME_OR_INDEX',
        help='Audio output device. Windows: "CABLE Input"  macOS: "BlackHole 2ch"')
    parser.add_argument('--midi', metavar='NAME_OR_INDEX',
        help='Preferred MIDI port (name substring). Leave blank = auto-detect.')
    parser.add_argument('--sr', type=int, default=SAMPLE_RATE, metavar='HZ')
    parser.add_argument('--list-devices', action='store_true',
        help='Print available audio + MIDI devices and exit')
    parser.add_argument('--hint', action='store_true',
        help='Print DAW routing instructions for this OS and exit')
    jack_grp = parser.add_mutually_exclusive_group()
    jack_grp.add_argument('--jack', action='store_true', help='Force JACK mode')
    jack_grp.add_argument('--sd',   action='store_true', help='Force sounddevice mode')
    parser.add_argument('--no-autoconnect', action='store_true')
    args = parser.parse_args()

    if args.list_devices:
        from engine.device_manager import print_audio_devices, print_midi_devices
        print_audio_devices()
        print_midi_devices()
        return

    if args.hint:
        from engine.device_manager import routing_hint
        print(routing_hint())
        return

    from engine.device_manager import find_audio_device
    audio_device = find_audio_device(args.device) if args.device else None
    if args.device and audio_device is None:
        sys.exit(f'Audio device {args.device!r} not found. Run --list-devices.')

    from engine.synth import OpenOmni
    from engine.midi_manager import MidiManager

    synth = OpenOmni(args.sr)

    # Start MIDI manager (auto-detects keyboard, reconnects on hot-plug)
    midi_mgr = MidiManager(synth, preferred=args.midi or None)
    midi_mgr.start()

    jack_client = None
    stream      = None
    on_linux    = platform.system() == 'Linux'

    if args.jack:
        jack_client = _try_jack(synth, auto_connect=not args.no_autoconnect)
        if not jack_client:
            sys.exit('JACK not available. Use --sd for sounddevice mode.')
    elif args.sd or not on_linux:
        stream = _run_sounddevice(synth, device=audio_device, sample_rate=args.sr)
    else:
        jack_client = _try_jack(synth, auto_connect=not args.no_autoconnect)
        if not jack_client:
            print('[audio] Falling back to sounddevice.')
            stream = _run_sounddevice(synth, device=audio_device, sample_rate=args.sr)

    print('OpenOmni ready  —  close window or Ctrl-C to quit.')

    try:
        from gui.main_window import MainWindow
        app = MainWindow(synth, midi_mgr=midi_mgr)
        app.mainloop()
    except KeyboardInterrupt:
        pass
    finally:
        synth.all_notes_off()
        midi_mgr.stop()
        if jack_client:
            jack_client.stop()
        if stream:
            stream.stop()
            stream.close()


if __name__ == '__main__':
    main()
