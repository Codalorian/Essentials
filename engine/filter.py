from __future__ import annotations
import numpy as np
from scipy.signal import sosfilt


class SVFilter:
    """
    TPT State Variable Filter — vectorised via scipy.signal.sosfilt.

    The TPT SVF is equivalent to the bilinear-transform of a 2-pole analog
    prototype, so it maps exactly to a single biquad (SOS) section:

        Lowpass  numerator: g²/D · [1, 2, 1]
        Highpass numerator: 1/D  · [1,-2, 1]
        Bandpass numerator: gk/D · [1, 0,-1]
        Denominator:               [1, 2(g²-1)/D, (1-gk+g²)/D]

    where g = tan(π·fc/fs),  k = 1/Q,  D = 1 + g(g+k).
    """

    def __init__(self, sample_rate: int = 44100):
        self.sr   = sample_rate
        self.mode = 'lowpass'
        self._sos = np.array([[0., 0., 0., 1., 0., 0.]], dtype=np.float64)
        self._zi  = np.zeros((1, 2), dtype=np.float64)
        self._set_coeffs(2000.0, 0.5)

    def set_params(self, cutoff: float, resonance: float, mode: str = 'lowpass'):
        self.mode = mode
        cutoff = float(np.clip(cutoff, 20.0, self.sr * 0.48))
        Q      = float(np.clip(resonance, 0.1, 8.0))
        self._set_coeffs(cutoff, Q)

    def _set_coeffs(self, cutoff: float, Q: float):
        g  = np.tan(np.pi * cutoff / self.sr)
        k  = 1.0 / Q
        D  = 1.0 + g * (g + k)
        a1 = 2.0 * (g * g - 1.0) / D
        a2 = (1.0 - k * g + g * g) / D
        if self.mode == 'lowpass':
            b = g * g / D
            self._sos[0, :3] = [b, 2.0 * b, b]
        elif self.mode == 'highpass':
            b = 1.0 / D
            self._sos[0, :3] = [b, -2.0 * b, b]
        else:  # bandpass
            b = g * k / D
            self._sos[0, :3] = [b, 0.0, -b]
        self._sos[0, 3:] = [1.0, a1, a2]

    def process(self, x: np.ndarray) -> np.ndarray:
        out, self._zi = sosfilt(self._sos, x.astype(np.float64), zi=self._zi)
        return out.astype(np.float32)
