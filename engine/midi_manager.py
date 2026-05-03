"""
MIDI auto-detection, hot-plug, and dispatch.

MidiManager polls every 2 seconds for the best available MIDI port,
auto-connects, reconnects on disconnect, and switches when a higher-priority
device appears (e.g. you plug in your keyboard after launch).

Port priority (highest first):
  1. Known controller brands / keyboard keywords
  2. Any real hardware port
  3. Virtual / routing ports (loopMIDI, IAC, Through)
"""
from __future__ import annotations

import threading
import logging

log = logging.getLogger(__name__)

_KEYBOARD_WORDS = {
    'keyboard', 'piano', 'keys', 'synth', 'controller',
    'launchpad', 'novation', 'akai', 'arturia', 'native instruments',
    'korg', 'roland', 'yamaha', 'casio', 'oxygen', 'mpk', 'keystep',
    'minilab', 'impact', 'keystation',
}
_VIRTUAL_WORDS = {
    'through', 'loopmidi', 'iac driver', 'midi yoke',
    'virtual', 'loopbe',
}


def _score(name: str) -> int:
    n = name.lower()
    if any(k in n for k in _KEYBOARD_WORDS):
        return 3
    if any(v in n for v in _VIRTUAL_WORDS):
        return 1
    return 2


def _best_port(ports: list, preferred: str = None) -> str:
    if not ports:
        return None
    if preferred:
        for p in ports:
            if preferred.lower() in p.lower():
                return p
    return max(ports, key=_score)


def dispatch(synth, msg) -> None:
    """Translate a mido Message into synth calls."""
    t = msg.type
    if t == 'note_on' and msg.velocity > 0:
        synth.note_on(msg.note, msg.velocity)
    elif t == 'note_off' or (t == 'note_on' and msg.velocity == 0):
        synth.note_off(msg.note)
    elif t == 'control_change':
        c, v = msg.control, msg.value
        if   c == 1:   synth.set_param('lfo_depth', v / 127.0)
        elif c == 7:   synth.set_param('master_volume', v / 127.0)
        elif c == 11:  synth.set_param('master_volume', v / 127.0)
        elif c == 74:  synth.set_param('filter_cutoff', 200 + v / 127.0 * 17800)
        elif c == 71:  synth.set_param('filter_resonance', v / 127.0 * 4.0)
        elif c == 123: synth.all_notes_off()
        elif c == 64:
            # sustain pedal — hold notes while pedal is down
            if not hasattr(synth, '_sustain_held'):
                synth._sustain_held = set()
            if v >= 64:
                synth._sustain_on = True
            else:
                synth._sustain_on = False
                for n in list(getattr(synth, '_sustain_held', set())):
                    synth.note_off(n)
                synth._sustain_held.clear()
    elif t == 'pitchwheel':
        synth.set_param('pitch_bend_semi', msg.pitch / 8192.0 * 2.0)


class MidiManager:
    """Hot-plug MIDI manager. Call start() once; it runs in a daemon thread."""

    POLL = 2.0  # seconds between device scans

    def __init__(self, synth, preferred: str = None):
        self.synth    = synth
        self.preferred = preferred
        self._port    = None        # open mido port object
        self._name    = None        # currently open port name
        self._lock    = threading.Lock()
        self._stop    = threading.Event()
        self.on_change = None       # callback(name: str | None)

    # ── Public API ────────────────────────────────────────────────────────────

    def start(self):
        t = threading.Thread(target=self._run, name='MidiManager', daemon=True)
        t.start()

    def stop(self):
        self._stop.set()
        with self._lock:
            self._close()

    @property
    def current_name(self) -> str:
        return self._name

    def set_preferred(self, name: str):
        """Switch to a specific port by name (empty string = auto)."""
        self.preferred = name or None
        with self._lock:
            self._close()  # will reconnect on next tick

    def available_ports(self) -> list:
        try:
            import mido
            return mido.get_input_names()
        except Exception:
            return []

    # ── Internal ──────────────────────────────────────────────────────────────

    def _run(self):
        while not self._stop.is_set():
            try:
                self._tick()
            except Exception as exc:
                log.debug('MidiManager: %s', exc)
            self._stop.wait(self.POLL)

    def _tick(self):
        import mido
        ports = mido.get_input_names()
        best  = _best_port(ports, self.preferred)

        with self._lock:
            if best == self._name:
                return  # already on best port

            self._close()

            if not best:
                self._notify(None)
                return

            try:
                self._port = mido.open_input(
                    best, callback=lambda msg: dispatch(self.synth, msg))
                self._name = best
                print(f'[midi] Connected: {best!r}')
                self._notify(best)
            except Exception as exc:
                log.debug('Cannot open %r: %s', best, exc)
                self._name = None
                self._notify(None)

    def _close(self):
        if self._port:
            try:
                self._port.close()
            except Exception:
                pass
            self._port = None
            self._name = None

    def _notify(self, name: str):
        cb = self.on_change
        if cb:
            try:
                cb(name)
            except Exception:
                pass
