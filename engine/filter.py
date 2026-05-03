from __future__ import annotations
import numpy as np

class SVFilter:
    """Topology-Preserving Transform State Variable Filter."""

    def __init__(self, sample_rate: int = 44100):
        self.sr = sample_rate
        self.ic1 = 0.0
        self.ic2 = 0.0
        self.mode = 'lowpass'
        self._set_coeffs(2000.0, 0.5)

    def set_params(self, cutoff: float, resonance: float, mode: str = 'lowpass'):
        self.mode = mode
        cutoff = float(np.clip(cutoff, 20.0, self.sr * 0.48))
        Q = float(np.clip(resonance, 0.1, 8.0))
        self._set_coeffs(cutoff, Q)

    def _set_coeffs(self, cutoff: float, Q: float):
        g = np.tan(np.pi * cutoff / self.sr)
        k = 1.0 / Q
        self.a1 = 1.0 / (1.0 + g * (g + k))
        self.a2 = g * self.a1
        self.a3 = g * self.a2
        self.k = k

    def process(self, x: np.ndarray) -> np.ndarray:
        out = np.empty(len(x), dtype=np.float32)
        ic1, ic2 = self.ic1, self.ic2
        a1, a2, a3, k = self.a1, self.a2, self.a3, self.k
        mode = self.mode

        for i in range(len(x)):
            v3 = x[i] - ic2
            v1 = a1 * ic1 + a2 * v3
            v2 = ic2 + a2 * ic1 + a3 * v3
            ic1 = 2.0 * v1 - ic1
            ic2 = 2.0 * v2 - ic2
            if mode == 'lowpass':
                out[i] = v2
            elif mode == 'highpass':
                out[i] = v3 - k * v1 - v2
            else:  # bandpass
                out[i] = v1

        self.ic1, self.ic2 = ic1, ic2
        return out
