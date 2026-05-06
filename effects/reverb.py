from __future__ import annotations
import numpy as np
from scipy.signal import lfilter

# Freeverb delay tunings (samples @ 44100 Hz)
_COMB_L   = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617]
_COMB_R   = [1139, 1211, 1300, 1379, 1445, 1514, 1580, 1640]
_APASS_L  = [556, 441, 341, 225]
_APASS_R  = [579, 464, 364, 248]

_FIXED_GAIN  = 0.015
_SCALE_ROOM  = 0.28
_OFFSET_ROOM = 0.7
_SCALE_DAMP  = 0.4
_APASS_FDBK  = 0.5


class _Comb:
    """
    Damped feedback comb filter — fully numpy-vectorised.

    All Freeverb comb delays (1116-1640) are > our block size (1024), so
    reads and writes in one block never alias. The inner 1-pole lowpass
    (damping) runs via scipy lfilter (C-speed).
    """

    def __init__(self, size: int):
        self.buf      = np.zeros(size, dtype=np.float64)
        self.idx      = 0
        self.feedback = 0.84
        self.damp1    = 0.2
        self.damp2    = 0.8
        self._fs      = 0.0   # filterstore state

    def process(self, inp: np.ndarray) -> np.ndarray:
        n       = len(inp)
        L       = len(self.buf)
        indices = (self.idx + np.arange(n)) % L

        # Read all L-delayed outputs at once (no aliasing since L > n)
        outputs = self.buf[indices].copy()

        # 1-pole LP across outputs: filterstore[i] = out[i]*d2 + fs[i-1]*d1
        filterstores, zi = lfilter(
            [self.damp2], [1.0, -self.damp1], outputs,
            zi=np.array([self._fs]))
        self._fs = float(zi[0])

        self.buf[indices] = inp + filterstores * self.feedback
        self.idx = (self.idx + n) % L
        return outputs


class _Allpass:
    """All-pass filter via scipy lfilter.  H(z) = (g - z^-L)/(1 - g·z^-L)"""

    def __init__(self, size: int, feedback: float = _APASS_FDBK):
        L        = size
        self._b  = np.zeros(L + 1)
        self._b[0] = feedback
        self._b[L] = -1.0
        self._a  = np.zeros(L + 1)
        self._a[0] = 1.0
        self._a[L] = -feedback
        self._zi = np.zeros(L)

    def process(self, inp: np.ndarray) -> np.ndarray:
        out, self._zi = lfilter(self._b, self._a, inp, zi=self._zi)
        return out


class Reverb:
    def __init__(self, sample_rate: int = 44100):
        sc = sample_rate / 44100
        self.combs_l = [_Comb(int(d * sc))    for d in _COMB_L]
        self.combs_r = [_Comb(int(d * sc))    for d in _COMB_R]
        self.apall_l = [_Allpass(int(d * sc)) for d in _APASS_L]
        self.apall_r = [_Allpass(int(d * sc)) for d in _APASS_R]
        self.wet = 0.5; self.room_size = 0.5; self.damp = 0.5; self.width = 1.0
        self._update()

    def set_params(self, room_size: float, damp: float, wet: float, width: float = 1.0):
        self.room_size = room_size; self.damp = damp
        self.wet = wet;             self.width = width
        self._update()

    def _update(self):
        fb = self.room_size * _SCALE_ROOM + _OFFSET_ROOM
        d1 = self.damp * _SCALE_DAMP
        for c in self.combs_l + self.combs_r:
            c.feedback = fb; c.damp1 = d1; c.damp2 = 1.0 - d1
        self.wet1 = self.wet * (self.width / 2 + 0.5)
        self.wet2 = self.wet * ((1 - self.width) / 2)

    def process(self, stereo: np.ndarray) -> np.ndarray:
        mono = (stereo[:, 0].astype(np.float64) +
                stereo[:, 1].astype(np.float64)) * _FIXED_GAIN

        outL = sum(c.process(mono) for c in self.combs_l)
        outR = sum(c.process(mono) for c in self.combs_r)
        for a in self.apall_l: outL = a.process(outL)
        for a in self.apall_r: outR = a.process(outR)

        dry = 1.0 - self.wet
        L = outL * self.wet1 + outR * self.wet2 + stereo[:, 0] * dry
        R = outR * self.wet1 + outL * self.wet2 + stereo[:, 1] * dry
        return np.column_stack([L, R]).astype(np.float32)
