from __future__ import annotations
import numpy as np

class StereoDelay:
    def __init__(self, sample_rate: int = 44100):
        self.sr = sample_rate
        self.max_samples = int(sample_rate * 3)
        self.buf_l = np.zeros(self.max_samples, dtype=np.float32)
        self.buf_r = np.zeros(self.max_samples, dtype=np.float32)
        self.write = 0
        self.time = 0.375       # seconds (dotted 8th at ~100bpm)
        self.feedback = 0.40
        self.wet = 0.25

    def process(self, stereo: np.ndarray) -> np.ndarray:
        n = len(stereo)
        delay_n = int(self.time * self.sr)
        out = stereo.copy()
        w = self.write
        m = self.max_samples
        fb = self.feedback
        wt = self.wet
        for i in range(n):
            r = (w - delay_n) % m
            dl = self.buf_l[r]
            dr = self.buf_r[r]
            self.buf_l[w] = stereo[i, 0] + dl * fb
            self.buf_r[w] = stereo[i, 1] + dr * fb
            out[i, 0] = stereo[i, 0] * (1 - wt) + dl * wt
            out[i, 1] = stereo[i, 1] * (1 - wt) + dr * wt
            w = (w + 1) % m
        self.write = w
        return out
