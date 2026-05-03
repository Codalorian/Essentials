from __future__ import annotations
import numpy as np

class LFO:
    def __init__(self, sample_rate: int = 44100):
        self.sr = sample_rate
        self.phase = 0.0
        self.rate = 1.0
        self.waveform = 'sine'

    def tick(self) -> float:
        p = self.phase
        if self.waveform == 'sine':
            val = float(np.sin(2 * np.pi * p))
        elif self.waveform == 'triangle':
            val = 1.0 - 4.0 * abs(p - 0.5)
        elif self.waveform == 'square':
            val = 1.0 if p < 0.5 else -1.0
        elif self.waveform == 'sawtooth':
            val = 2.0 * p - 1.0
        else:
            val = 0.0
        self.phase = (self.phase + self.rate / self.sr) % 1.0
        return val
