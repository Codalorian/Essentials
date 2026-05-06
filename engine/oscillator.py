from __future__ import annotations
import numpy as np

TABLE_SIZE = 2048

class WavetableOscillator:
    _tables: dict = {}

    def __init__(self, sample_rate: int = 44100):
        self.sr = sample_rate
        self.phase = 0.0
        self.phase_increment = 0.0
        self.waveform = 'sine'
        self._build_tables()

    @classmethod
    def _build_tables(cls):
        if cls._tables:
            return
        t = np.linspace(0, 2 * np.pi, TABLE_SIZE, endpoint=False)

        cls._tables['sine'] = np.sin(t).astype(np.float32)

        tri = np.zeros(TABLE_SIZE, dtype=np.float64)
        for n in range(1, 25, 2):
            tri += ((-1) ** ((n - 1) // 2)) * np.sin(n * t) / n ** 2
        cls._tables['triangle'] = (tri * (8 / np.pi ** 2)).astype(np.float32)

        saw = np.zeros(TABLE_SIZE, dtype=np.float64)
        for n in range(1, 32):
            saw += ((-1) ** (n + 1)) * np.sin(n * t) / n
        cls._tables['sawtooth'] = np.clip(saw * (2 / np.pi), -1, 1).astype(np.float32)

        sq = np.zeros(TABLE_SIZE, dtype=np.float64)
        for n in range(1, 32, 2):
            sq += np.sin(n * t) / n
        cls._tables['square'] = np.clip(sq * (4 / np.pi), -1, 1).astype(np.float32)

        # Warm pad tone: sine + soft 2nd harmonic blend
        soft = np.sin(t) * 0.75 + np.sin(2 * t) * 0.15 + np.sin(3 * t) * 0.08 + np.sin(4 * t) * 0.02
        mx = np.max(np.abs(soft))
        cls._tables['soft'] = (soft / mx).astype(np.float32)

        # Bell: bright glassy character — selective high harmonics give shimmer
        bell = (np.sin(t) * 0.70 + np.sin(2 * t) * 0.18 + np.sin(3 * t) * 0.06
                + np.sin(5 * t) * 0.12 + np.sin(8 * t) * 0.06 + np.sin(10 * t) * 0.03)
        mx = np.max(np.abs(bell))
        cls._tables['bell'] = (bell / mx).astype(np.float32)

        # Gamelan: metallic timbre — strong even harmonics + upper partials
        gamelan = (np.sin(t) * 0.55 + np.sin(2 * t) * 0.32 + np.sin(3 * t) * 0.22
                   + np.sin(5 * t) * 0.14 + np.sin(7 * t) * 0.08 + np.sin(11 * t) * 0.04)
        mx = np.max(np.abs(gamelan))
        cls._tables['gamelan'] = (gamelan / mx).astype(np.float32)

        for k in list(cls._tables):
            cls._tables[k] = np.clip(cls._tables[k], -1.0, 1.0)

    def set_frequency(self, freq_hz: float, detune_cents: float = 0.0,
                      octave: int = 0, semitone: int = 0):
        freq = freq_hz * (2.0 ** (octave + semitone / 12.0 + detune_cents / 1200.0))
        self.phase_increment = max(freq, 0.01) / self.sr

    def render(self, n_frames: int) -> np.ndarray:
        table = self._tables.get(self.waveform, self._tables['sine'])
        phases = (self.phase + np.arange(n_frames, dtype=np.float64) * self.phase_increment) % 1.0
        self.phase = float((self.phase + n_frames * self.phase_increment) % 1.0)

        idx = phases * TABLE_SIZE
        idx_i = idx.astype(np.int32) % TABLE_SIZE
        idx_f = (idx - idx_i).astype(np.float32)
        idx_n = (idx_i + 1) % TABLE_SIZE
        return table[idx_i] * (1.0 - idx_f) + table[idx_n] * idx_f
