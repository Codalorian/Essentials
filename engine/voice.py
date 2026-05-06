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
        self.amp_env   = ADSR(sample_rate)
        self.filt_env  = ADSR(sample_rate)
        self.pitch_env = ADSR(sample_rate)
        self.filt   = SVFilter(sample_rate)
        self.filt_r = SVFilter(sample_rate)
        self.note = -1
        self.age = 0
        self.params: dict = {}

        # Glide state
        self._freq = 0.0
        self._glide_start_inc_a = 0.0
        self._glide_start_inc_b = 0.0
        self._target_inc_a = 0.0
        self._target_inc_b = 0.0
        self._glide_samples = 0
        self._glide_pos = 0

        # Unison phase memory (up to 8 voices)
        self._unison_phases_a = [0.0] * 8
        self._unison_phases_b = [0.13] * 8

    # ─── helpers ────────────────────────────────────────────────────────────

    @staticmethod
    def _calc_inc(freq: float, fine: float, oct: int, semi: int, sr: int) -> float:
        f = freq * (2.0 ** (oct + semi / 12.0 + fine / 1200.0))
        return max(f, 0.01) / sr

    def _render_osc_unison(self, osc, phases: list, n_frames: int,
                           base_inc: float, n_uni: int,
                           detune_cents: float, level: float):
        """Render osc N times with spread detune; returns (L, R) mono arrays."""
        half = detune_cents / 2.0
        offsets = np.linspace(-half, half, n_uni)
        out_l = np.zeros(n_frames, dtype=np.float32)
        out_r = np.zeros(n_frames, dtype=np.float32)

        for i, cents in enumerate(offsets):
            osc.phase = phases[i]
            osc.phase_increment = base_inc * (2.0 ** (cents / 1200.0))
            chunk = osc.render(n_frames)
            phases[i] = osc.phase

            if n_uni > 1:
                t = i / (n_uni - 1)           # 0 → 1 (left → right)
                pan_l = float(np.cos(t * np.pi * 0.5))
                pan_r = float(np.sin(t * np.pi * 0.5))
            else:
                pan_l = pan_r = 0.7071

            out_l += chunk * pan_l
            out_r += chunk * pan_r

        osc.phase_increment = base_inc         # restore unmodulated increment
        norm = n_uni ** 0.5
        return out_l * (level / norm), out_r * (level / norm)

    # ─── MIDI ────────────────────────────────────────────────────────────────

    def note_on(self, note: int, velocity: int, params: dict):
        self.note = note
        self.params = params
        freq = midi_to_hz(note)

        self.osc_a.waveform = params.get('osc_a_wave', 'sine')
        self.osc_b.waveform = params.get('osc_b_wave', 'triangle')

        new_inc_a = self._calc_inc(freq,
            params.get('osc_a_fine', 0.0), params.get('osc_a_oct', 0),
            params.get('osc_a_semi', 0), self.sr)
        new_inc_b = self._calc_inc(freq,
            params.get('osc_b_fine', 0.0), params.get('osc_b_oct', 0),
            params.get('osc_b_semi', 0), self.sr)

        glide_time = float(params.get('glide_time', 0.0))
        is_gliding = (glide_time > 0.0 and self._target_inc_a > 0
                      and self.amp_env.is_active())

        if is_gliding:
            self._glide_start_inc_a = self.osc_a.phase_increment
            self._glide_start_inc_b = self.osc_b.phase_increment
            self._glide_samples = int(glide_time * self.sr)
            self._glide_pos = 0
            # keep current phases for smooth slide
        else:
            self._glide_start_inc_a = new_inc_a
            self._glide_start_inc_b = new_inc_b
            self._glide_samples = 0
            self._glide_pos = 0
            # reset phases for clean attack
            n_uni = max(1, int(round(params.get('unison_voices', 1))))
            for i in range(8):
                self._unison_phases_a[i] = (i / n_uni) % 1.0
                self._unison_phases_b[i] = ((i / n_uni) + 0.13) % 1.0
            self.osc_a.phase = self._unison_phases_a[0]
            self.osc_b.phase = self._unison_phases_b[0]

        self.osc_a.phase_increment = new_inc_a if not is_gliding else self._glide_start_inc_a
        self.osc_b.phase_increment = new_inc_b if not is_gliding else self._glide_start_inc_b

        self._target_inc_a = new_inc_a
        self._target_inc_b = new_inc_b
        self._freq = freq

        self.amp_env.attack  = params.get('attack',  0.05)
        self.amp_env.decay   = params.get('decay',   0.3)
        self.amp_env.sustain = params.get('sustain', 0.6)
        self.amp_env.release = params.get('release', 2.0)
        self.amp_env.note_on(velocity)

        self.filt_env.attack  = params.get('fenv_attack',  0.01)
        self.filt_env.decay   = params.get('fenv_decay',   0.5)
        self.filt_env.sustain = params.get('fenv_sustain', 0.0)
        self.filt_env.release = params.get('fenv_release', 1.0)
        self.filt_env.note_on(velocity)

        # Pitch envelope: fast attack → decays to 0 → creates pitch swoop on each note
        self.pitch_env.attack  = params.get('penv_attack',  0.001)
        self.pitch_env.decay   = params.get('penv_decay',   0.18)
        self.pitch_env.sustain = 0.0
        self.pitch_env.release = 0.05
        self.pitch_env.note_on(velocity)

    def note_off(self):
        self.amp_env.note_off()
        self.filt_env.note_off()
        self.pitch_env.note_off()

    def is_active(self) -> bool:
        return self.amp_env.is_active()

    # ─── Render ──────────────────────────────────────────────────────────────

    def render(self, n_frames: int, lfo_val: float = 0.0) -> np.ndarray:
        p = self.params
        lfo_target = p.get('lfo_target', 'pitch')
        lfo_depth  = float(p.get('lfo_depth', 0.0))
        n_uni      = max(1, int(round(p.get('unison_voices', 1))))
        uni_detune = float(p.get('unison_detune', 0.0))

        # ── Glide: interpolate phase increments toward target ─────────────────
        if self._glide_pos < self._glide_samples:
            t = min(self._glide_pos / self._glide_samples, 1.0)
            self.osc_a.phase_increment = (self._glide_start_inc_a
                + t * (self._target_inc_a - self._glide_start_inc_a))
            self.osc_b.phase_increment = (self._glide_start_inc_b
                + t * (self._target_inc_b - self._glide_start_inc_b))
            self._glide_pos += n_frames

        # ── Pitch modulation (envelope swoop + LFO vibrato + MIDI pitch bend) ──
        pitch_bend  = float(p.get('pitch_bend_semi', 0.0))
        penv_amount = float(p.get('penv_amount', 0.0))
        penv_val    = float(np.mean(self.pitch_env.render(n_frames)))
        pitch_ratio = 2.0 ** ((pitch_bend + penv_val * penv_amount) / 12.0)
        if lfo_target == 'pitch' and lfo_depth > 0:
            pitch_ratio *= 2.0 ** (lfo_val * lfo_depth / 12.0)

        # Save base increments before pitch modulation
        base_inc_a = self.osc_a.phase_increment
        base_inc_b = self.osc_b.phase_increment

        # ── Oscillators with unison ───────────────────────────────────────────
        al, ar = self._render_osc_unison(
            self.osc_a, self._unison_phases_a, n_frames,
            base_inc_a * pitch_ratio, n_uni, uni_detune,
            float(p.get('osc_a_level', 0.8)))

        if p.get('osc_b_enabled', True):
            bl, br = self._render_osc_unison(
                self.osc_b, self._unison_phases_b, n_frames,
                base_inc_b * pitch_ratio, n_uni, uni_detune,
                float(p.get('osc_b_level', 0.4)))
        else:
            bl = br = np.zeros(n_frames, dtype=np.float32)

        # Restore base increments (pitch_ratio must not accumulate)
        self.osc_a.phase_increment = base_inc_a
        self.osc_b.phase_increment = base_inc_b

        audio_l = (al + bl) * 0.5
        audio_r = (ar + br) * 0.5

        # ── Amplitude envelope ────────────────────────────────────────────────
        amp = self.amp_env.render(n_frames)
        audio_l *= amp
        audio_r *= amp

        # ── Volume LFO tremolo ────────────────────────────────────────────────
        if lfo_target == 'volume' and lfo_depth > 0:
            vol_mod = 1.0 + lfo_val * lfo_depth * 0.5
            audio_l *= vol_mod
            audio_r *= vol_mod

        # ── Filter: cutoff with env + LFO + keytrack ─────────────────────────
        base_cut  = float(p.get('filter_cutoff', 4000.0))
        env_amt   = float(p.get('filter_env_amt', 0.3))
        fenv_mean = float(np.mean(self.filt_env.render(n_frames)))
        mod_cut   = base_cut * (1.0 + env_amt * fenv_mean * 3.0)

        keytrack = float(p.get('filter_keytrack', 0.0))
        if keytrack != 0.0 and self.note >= 0:
            key_ratio = midi_to_hz(self.note) / 440.0
            mod_cut *= key_ratio ** keytrack

        if lfo_target == 'filter' and lfo_depth > 0:
            mod_cut *= 1.0 + lfo_val * lfo_depth
        mod_cut = float(np.clip(mod_cut, 30.0, 18000.0))

        res  = float(p.get('filter_resonance', 0.3))
        mode = p.get('filter_mode', 'lowpass')
        self.filt.set_params(mod_cut, res, mode)
        self.filt_r.set_params(mod_cut, res, mode)

        audio_l = self.filt.process(audio_l)
        audio_r = self.filt_r.process(audio_r)

        return np.column_stack([audio_l, audio_r])
