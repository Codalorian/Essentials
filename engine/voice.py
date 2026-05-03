from __future__ import annotations
import numpy as np
from .oscillator import WavetableOscillator
from .envelope import ADSR
from .filter import SVFilter

A4_HZ = 440.0
A4_MIDI = 69

def midi_to_hz(note: int) -> float:
    return A4_HZ * (2.0 ** ((note - A4_MIDI) / 12.0))

class Voice:
    def __init__(self, sample_rate: int = 44100):
        self.sr = sample_rate
        self.osc_a = WavetableOscillator(sample_rate)
        self.osc_b = WavetableOscillator(sample_rate)
        self.amp_env = ADSR(sample_rate)
        self.filt_env = ADSR(sample_rate)
        self.filt = SVFilter(sample_rate)
        self.note = -1
        self.age = 0
        self.params: dict = {}

    def note_on(self, note: int, velocity: int, params: dict):
        self.note = note
        self.params = params
        freq = midi_to_hz(note)

        self.osc_a.waveform = params.get('osc_a_wave', 'sine')
        self.osc_a.phase = 0.0
        self.osc_a.set_frequency(freq,
                                  params.get('osc_a_fine', 0.0),
                                  params.get('osc_a_oct', 0),
                                  params.get('osc_a_semi', 0))

        self.osc_b.waveform = params.get('osc_b_wave', 'triangle')
        self.osc_b.phase = 0.1  # slight phase offset avoids exact cancellation
        self.osc_b.set_frequency(freq,
                                  params.get('osc_b_fine', 0.0),
                                  params.get('osc_b_oct', 0),
                                  params.get('osc_b_semi', 0))

        self.amp_env.attack = params.get('attack', 0.05)
        self.amp_env.decay = params.get('decay', 0.3)
        self.amp_env.sustain = params.get('sustain', 0.6)
        self.amp_env.release = params.get('release', 2.0)
        self.amp_env.note_on(velocity)

        self.filt_env.attack = params.get('fenv_attack', 0.01)
        self.filt_env.decay = params.get('fenv_decay', 0.5)
        self.filt_env.sustain = params.get('fenv_sustain', 0.0)
        self.filt_env.release = params.get('fenv_release', 1.0)
        self.filt_env.note_on(velocity)

    def note_off(self):
        self.amp_env.note_off()
        self.filt_env.note_off()

    def is_active(self) -> bool:
        return self.amp_env.is_active()

    def render(self, n_frames: int, lfo_val: float = 0.0) -> np.ndarray:
        p = self.params
        lfo_target = p.get('lfo_target', 'pitch')
        lfo_depth = p.get('lfo_depth', 0.0)

        # Pitch modulation: LFO vibrato + MIDI pitch bend
        pitch_bend = p.get('pitch_bend_semi', 0.0)
        pitch_ratio = 2.0 ** (pitch_bend / 12.0)
        if lfo_target == 'pitch' and lfo_depth > 0:
            pitch_ratio *= 2.0 ** (lfo_val * lfo_depth / 12.0)

        orig_a = self.osc_a.phase_increment
        orig_b = self.osc_b.phase_increment
        if pitch_ratio != 1.0:
            self.osc_a.phase_increment *= pitch_ratio
            self.osc_b.phase_increment *= pitch_ratio

        audio_a = self.osc_a.render(n_frames) * p.get('osc_a_level', 0.8)
        if p.get('osc_b_enabled', True):
            audio_b = self.osc_b.render(n_frames) * p.get('osc_b_level', 0.4)
        else:
            audio_b = np.zeros(n_frames, dtype=np.float32)

        if pitch_ratio != 1.0:
            self.osc_a.phase_increment = orig_a
            self.osc_b.phase_increment = orig_b

        audio = (audio_a + audio_b) * 0.5

        amp = self.amp_env.render(n_frames)
        audio *= amp

        # Filter cutoff with envelope + LFO modulation (single update per buffer)
        base_cut = p.get('filter_cutoff', 4000.0)
        env_amt = p.get('filter_env_amt', 0.3)
        fenv_mean = float(np.mean(self.filt_env.render(n_frames)))
        mod_cut = base_cut * (1.0 + env_amt * fenv_mean * 3.0)
        if lfo_target == 'filter':
            mod_cut *= 1.0 + lfo_val * lfo_depth
        mod_cut = float(np.clip(mod_cut, 30.0, 18000.0))

        self.filt.set_params(mod_cut, p.get('filter_resonance', 0.3),
                              p.get('filter_mode', 'lowpass'))
        audio = self.filt.process(audio)

        stereo = np.column_stack([audio, audio])
        return stereo
