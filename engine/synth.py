from __future__ import annotations
import threading
import numpy as np
from .voice import Voice
from .lfo import LFO
from effects.reverb import Reverb
from effects.delay import StereoDelay
from effects.chorus import Chorus

MAX_VOICES = 16

class OpenOmni:
    def __init__(self, sample_rate: int = 44100):
        self.sr = sample_rate
        self.voices = [Voice(sample_rate) for _ in range(MAX_VOICES)]
        self.lfo = LFO(sample_rate)
        self.reverb = Reverb(sample_rate)
        self.delay = StereoDelay(sample_rate)
        self.chorus = Chorus(sample_rate)
        self._lock = threading.Lock()
        self._age = 0
        self._active: dict[int, int] = {}  # note -> voice index
        self.params: dict = {}
        self.load_preset(self._moonlight_preset())

    # ─── Presets ────────────────────────────────────────────────────────────

    def _moonlight_preset(self) -> dict:
        return dict(
            name='Moonlight Bells',
            osc_a_wave='sine',     osc_a_level=0.85, osc_a_oct=0,  osc_a_semi=0,  osc_a_fine=0.0,
            osc_b_wave='soft',     osc_b_level=0.45, osc_b_oct=1,  osc_b_semi=7,  osc_b_fine=-4.0,
            osc_b_enabled=True,
            attack=0.06,  decay=0.4,  sustain=0.55, release=2.8,
            fenv_attack=0.01, fenv_decay=0.6, fenv_sustain=0.0, fenv_release=1.5,
            filter_mode='lowpass', filter_cutoff=3800.0, filter_resonance=0.28, filter_env_amt=0.35,
            lfo_wave='sine', lfo_rate=0.32, lfo_depth=0.18, lfo_target='pitch',
            reverb_size=0.88, reverb_damp=0.45, reverb_wet=0.62,
            delay_time=0.375, delay_feedback=0.38, delay_wet=0.24,
            chorus_rate=0.45, chorus_depth=0.0028, chorus_wet=0.38,
            master_volume=0.80,
        )

    def _dream_pad_preset(self) -> dict:
        return dict(
            name='Dream Pad',
            osc_a_wave='soft',     osc_a_level=0.9,  osc_a_oct=0,  osc_a_semi=0,  osc_a_fine=0.0,
            osc_b_wave='triangle', osc_b_level=0.5,  osc_b_oct=0,  osc_b_semi=0,  osc_b_fine=7.0,
            osc_b_enabled=True,
            attack=0.6,  decay=0.5,  sustain=0.75, release=3.5,
            fenv_attack=0.8, fenv_decay=1.0, fenv_sustain=0.3, fenv_release=2.0,
            filter_mode='lowpass', filter_cutoff=1800.0, filter_resonance=0.2, filter_env_amt=0.5,
            lfo_wave='sine', lfo_rate=0.2, lfo_depth=0.08, lfo_target='pitch',
            reverb_size=0.92, reverb_damp=0.55, reverb_wet=0.75,
            delay_time=0.50,  delay_feedback=0.35, delay_wet=0.22,
            chorus_rate=0.3,  chorus_depth=0.004, chorus_wet=0.5,
            master_volume=0.75,
        )

    def _sad_bells_preset(self) -> dict:
        return dict(
            name='Sad Bells',
            osc_a_wave='sine',     osc_a_level=0.9,  osc_a_oct=0,  osc_a_semi=0,  osc_a_fine=0.0,
            osc_b_wave='sine',     osc_b_level=0.35, osc_b_oct=2,  osc_b_semi=0,  osc_b_fine=2.0,
            osc_b_enabled=True,
            attack=0.01, decay=0.8,  sustain=0.1, release=3.0,
            fenv_attack=0.01, fenv_decay=0.4, fenv_sustain=0.0, fenv_release=1.0,
            filter_mode='lowpass', filter_cutoff=5000.0, filter_resonance=0.4, filter_env_amt=0.2,
            lfo_wave='sine', lfo_rate=0.5, lfo_depth=0.1, lfo_target='pitch',
            reverb_size=0.9,  reverb_damp=0.35, reverb_wet=0.7,
            delay_time=0.333, delay_feedback=0.42, delay_wet=0.3,
            chorus_rate=0.6,  chorus_depth=0.002, chorus_wet=0.25,
            master_volume=0.78,
        )

    def get_preset_list(self) -> list[str]:
        return ['Moonlight Bells', 'Dream Pad', 'Sad Bells']

    def load_preset_by_name(self, name: str):
        presets = {
            'Moonlight Bells': self._moonlight_preset,
            'Dream Pad':       self._dream_pad_preset,
            'Sad Bells':       self._sad_bells_preset,
        }
        fn = presets.get(name, self._moonlight_preset)
        self.load_preset(fn())

    def load_preset(self, preset: dict):
        with self._lock:
            self.params = dict(preset)
            self._apply_effects()

    # ─── Parameters ─────────────────────────────────────────────────────────

    def set_param(self, key: str, value):
        with self._lock:
            self.params[key] = value
            self._apply_effects()

    def _apply_effects(self):
        p = self.params
        self.lfo.rate = p.get('lfo_rate', 0.3)
        self.lfo.waveform = p.get('lfo_wave', 'sine')
        self.reverb.set_params(p.get('reverb_size', 0.85), p.get('reverb_damp', 0.5),
                               p.get('reverb_wet', 0.6))
        self.delay.time = p.get('delay_time', 0.375)
        self.delay.feedback = p.get('delay_feedback', 0.4)
        self.delay.wet = p.get('delay_wet', 0.25)
        self.chorus.rate = p.get('chorus_rate', 0.5)
        self.chorus.depth = p.get('chorus_depth', 0.003)
        self.chorus.wet = p.get('chorus_wet', 0.4)

    # ─── MIDI ────────────────────────────────────────────────────────────────

    def note_on(self, note: int, velocity: int = 100):
        with self._lock:
            idx = self._steal_voice()
            self.voices[idx].note_on(note, velocity, dict(self.params))
            self.voices[idx].age = self._age
            self._age += 1
            self._active[note] = idx

    def note_off(self, note: int):
        with self._lock:
            idx = self._active.pop(note, None)
            if idx is not None:
                self.voices[idx].note_off()

    def all_notes_off(self):
        with self._lock:
            for v in self.voices:
                v.note_off()
            self._active.clear()

    def _steal_voice(self) -> int:
        for i, v in enumerate(self.voices):
            if not v.is_active():
                return i
        return min(range(MAX_VOICES), key=lambda i: self.voices[i].age)

    # ─── Audio render ────────────────────────────────────────────────────────

    def render(self, n_frames: int) -> np.ndarray:
        lfo_val = self.lfo.tick()
        output = np.zeros((n_frames, 2), dtype=np.float32)

        for v in self.voices:
            if v.is_active():
                output += v.render(n_frames, lfo_val)

        # Gentle soft-clip to prevent clipping with many voices
        output = np.tanh(output * 0.65)

        output = self.chorus.process(output)
        output = self.delay.process(output)
        output = self.reverb.process(output)
        output *= self.params.get('master_volume', 0.8)
        return np.clip(output, -1.0, 1.0).astype(np.float32)
