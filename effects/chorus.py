from __future__ import annotations
import numpy as np

class Chorus:
    def __init__(self, sample_rate: int = 44100):
        self.sr = sample_rate
        self.max_n = int(sample_rate * 0.06)
        self.buf_l = np.zeros(self.max_n, dtype=np.float32)
        self.buf_r = np.zeros(self.max_n, dtype=np.float32)
        self.write = 0
        self.lfo_l = 0.0
        self.lfo_r = 0.5   # 180° offset for stereo width
        self.rate  = 0.5   # Hz
        self.depth = 0.003  # seconds
        self.base  = 0.020  # 20ms base delay
        self.wet   = 0.40

    def process(self, stereo: np.ndarray) -> np.ndarray:
        n = len(stereo)
        out = stereo.copy()
        w = self.write
        m = self.max_n
        inc = self.rate / self.sr
        wt = self.wet

        for i in range(n):
            ml = np.sin(2 * np.pi * self.lfo_l)
            mr = np.sin(2 * np.pi * self.lfo_r)
            dl = max(1, int((self.base + ml * self.depth) * self.sr))
            dr = max(1, int((self.base + mr * self.depth) * self.sr))
            dl = min(dl, m - 1)
            dr = min(dr, m - 1)

            self.buf_l[w] = stereo[i, 0]
            self.buf_r[w] = stereo[i, 1]
            cl = self.buf_l[(w - dl) % m]
            cr = self.buf_r[(w - dr) % m]
            out[i, 0] = stereo[i, 0] * (1 - wt) + cl * wt
            out[i, 1] = stereo[i, 1] * (1 - wt) + cr * wt

            w = (w + 1) % m
            self.lfo_l = (self.lfo_l + inc) % 1.0
            self.lfo_r = (self.lfo_r + inc) % 1.0

        self.write = w
        return out
