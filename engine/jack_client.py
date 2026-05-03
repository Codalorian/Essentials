from __future__ import annotations
"""
JACK client for OpenOmni.

Creates a JACK client named 'OpenOmni' with:
  - Stereo audio output ports  (OpenOmni:out_L, OpenOmni:out_R)
  - JACK MIDI input port        (OpenOmni:midi_in)

Wire these in qpwgraph or Carla:
  - MIDI: Ableton/DAW MIDI out  →  OpenOmni:midi_in
  - Audio: OpenOmni:out_L/R     →  DAW audio input  OR  system:playback_1/2
"""

import sys
import threading
import numpy as np

# MIDI status byte masks
_NOTE_OFF  = 0x80
_NOTE_ON   = 0x90
_CC        = 0xB0
_PBEND     = 0xE0
_ALL_NOTES = 0x7B   # CC 123

class JACKClient:
    def __init__(self, synth, name: str = 'OpenOmni', auto_connect: bool = True):
        import jack  # deferred so sounddevice path can work without jack installed
        self._jack = jack

        self.synth = synth
        self.auto_connect = auto_connect
        self._shutdown_event = threading.Event()
        self._sustain = False  # sustain pedal state
        self._sustained_notes: set[int] = set()

        self.client = jack.Client(name)
        self.out_l   = self.client.outports.register('out_L')
        self.out_r   = self.client.outports.register('out_R')
        self.midi_in = self.client.midi_inports.register('midi_in')

        self.client.set_process_callback(self._process)
        self.client.set_shutdown_callback(self._on_shutdown)

    # ─── JACK callbacks ──────────────────────────────────────────────────────

    def _process(self, frames: int):
        # ── MIDI ──────────────────────────────────────────────────────────────
        for _offset, raw in self.midi_in.incoming_midi_events():
            if not raw:
                continue
            status = raw[0] & 0xF0
            ch     = raw[0] & 0x0F

            if status == _NOTE_ON and len(raw) >= 3:
                note, vel = raw[1], raw[2]
                if vel == 0:
                    self._handle_note_off(note)
                else:
                    self.synth.note_on(note, vel)
                    if self._sustain:
                        self._sustained_notes.add(note)

            elif status == _NOTE_OFF and len(raw) >= 3:
                self._handle_note_off(raw[1])

            elif status == _CC and len(raw) >= 3:
                ctrl, val = raw[1], raw[2]
                if ctrl == 64:          # sustain pedal
                    if val >= 64:
                        self._sustain = True
                    else:
                        self._sustain = False
                        for n in list(self._sustained_notes):
                            self.synth.note_off(n)
                        self._sustained_notes.clear()
                elif ctrl == 1:         # mod wheel → lfo depth
                    self.synth.set_param('lfo_depth', val / 127.0)
                elif ctrl == 7:         # channel volume
                    self.synth.set_param('master_volume', val / 127.0)
                elif ctrl == 74:        # filter cutoff (common mapping)
                    self.synth.set_param('filter_cutoff', 200 + val / 127.0 * 17800)
                elif ctrl == 71:        # filter resonance
                    self.synth.set_param('filter_resonance', val / 127.0 * 4.0)
                elif ctrl == _ALL_NOTES:
                    self.synth.all_notes_off()

            elif status == _PBEND and len(raw) >= 3:
                bend = ((raw[2] << 7) | raw[1]) - 8192
                semitones = bend / 8192.0 * 2.0   # ±2 semitones range
                self.synth.set_param('pitch_bend_semi', semitones)

        # ── Audio ─────────────────────────────────────────────────────────────
        audio = self.synth.render(frames)
        self.out_l.get_array()[:] = audio[:, 0]
        self.out_r.get_array()[:] = audio[:, 1]

    def _handle_note_off(self, note: int):
        if self._sustain:
            self._sustained_notes.add(note)
        else:
            self.synth.note_off(note)

    def _on_shutdown(self, status, reason: str = ''):
        print(f'[JACK] Server shutdown: {reason}', file=sys.stderr)
        self._shutdown_event.set()

    # ─── Lifecycle ───────────────────────────────────────────────────────────

    def start(self):
        self.client.activate()
        sr = self.client.samplerate
        if sr != self.synth.sr:
            print(f'[JACK] Warning: JACK sample rate {sr} != synth rate {self.synth.sr}. '
                  'Audio will be pitched/sped — restart with matching rate.')

        if self.auto_connect:
            self._connect_to_system()

        print(f'[JACK] Client "OpenOmni" active at {sr} Hz, '
              f'block size {self.client.blocksize}')
        self._print_ports()

    def stop(self):
        try:
            self.client.deactivate()
            self.client.close()
        except Exception:
            pass

    def wait(self):
        """Block until JACK server shuts down."""
        self._shutdown_event.wait()

    # ─── Helpers ─────────────────────────────────────────────────────────────

    def _connect_to_system(self):
        jack = self._jack
        try:
            sinks = self.client.get_ports(
                is_physical=True, is_input=True, is_audio=True)
            if len(sinks) >= 2:
                self.out_l.connect(sinks[0])
                self.out_r.connect(sinks[1])
                print(f'[JACK] Auto-connected to {sinks[0].name} / {sinks[1].name}')
            elif len(sinks) == 1:
                self.out_l.connect(sinks[0])
                self.out_r.connect(sinks[0])
                print(f'[JACK] Auto-connected (mono) to {sinks[0].name}')
        except jack.JackError as exc:
            print(f'[JACK] Auto-connect failed: {exc}')

    def _print_ports(self):
        print('[JACK] Ports registered:')
        print(f'  Audio out : OpenOmni:out_L  /  OpenOmni:out_R')
        print(f'  MIDI  in  : OpenOmni:midi_in')
        print('[JACK] Connect MIDI in qpwgraph or with:')
        print('  pw-link <your_daw_midi_port> OpenOmni:midi_in')
