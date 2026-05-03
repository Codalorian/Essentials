from __future__ import annotations
import numpy as np

# Freeverb tunings at 44100 Hz
_COMB_L   = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617]
_COMB_R   = [1139, 1211, 1300, 1379, 1445, 1514, 1580, 1640]
_APASS_L  = [556, 441, 341, 225]
_APASS_R  = [579, 464, 364, 248]

_FIXED_GAIN   = 0.015
_SCALE_ROOM   = 0.28
_OFFSET_ROOM  = 0.7
_SCALE_DAMP   = 0.4
_APASS_FDBK   = 0.5

class _Comb:
    def __init__(self, size: int):
        self.buf = np.zeros(size, dtype=np.float64)
        self.idx = 0
        self.feedback = 0.84
        self.damp1 = 0.2
        self.damp2 = 0.8
        self.fs = 0.0

    def process_buf(self, inp: np.ndarray) -> np.ndarray:
        out = np.empty(len(inp), dtype=np.float64)
        buf = self.buf; n = len(buf)
        idx = self.idx; fs = self.fs
        fb = self.feedback; d1 = self.damp1; d2 = self.damp2
        for i in range(len(inp)):
            o = buf[idx]
            fs = o * d2 + fs * d1
            buf[idx] = inp[i] + fs * fb
            idx = (idx + 1) % n
            out[i] = o
        self.idx = idx; self.fs = fs
        return out

class _Allpass:
    def __init__(self, size: int):
        self.buf = np.zeros(size, dtype=np.float64)
        self.idx = 0

    def process_buf(self, inp: np.ndarray) -> np.ndarray:
        out = np.empty(len(inp), dtype=np.float64)
        buf = self.buf; n = len(buf); idx = self.idx
        for i in range(len(inp)):
            bo = buf[idx]
            out[i] = -inp[i] + bo
            buf[idx] = inp[i] + bo * _APASS_FDBK
            idx = (idx + 1) % n
        self.idx = idx
        return out

class Reverb:
    def __init__(self, sample_rate: int = 44100):
        sc = sample_rate / 44100
        self.combs_l  = [_Comb(int(d * sc))    for d in _COMB_L]
        self.combs_r  = [_Comb(int(d * sc))    for d in _COMB_R]
        self.apall_l  = [_Allpass(int(d * sc)) for d in _APASS_L]
        self.apall_r  = [_Allpass(int(d * sc)) for d in _APASS_R]
        self.wet = 0.5
        self.room_size = 0.5
        self.damp = 0.5
        self.width = 1.0
        self._update()

    def set_params(self, room_size: float, damp: float, wet: float, width: float = 1.0):
        self.room_size = room_size
        self.damp = damp
        self.wet = wet
        self.width = width
        self._update()

    def _update(self):
        fb = self.room_size * _SCALE_ROOM + _OFFSET_ROOM
        d1 = self.damp * _SCALE_DAMP
        d2 = 1.0 - d1
        for c in self.combs_l + self.combs_r:
            c.feedback = fb
            c.damp1 = d1
            c.damp2 = d2
        self.wet1 = self.wet * (self.width / 2 + 0.5)
        self.wet2 = self.wet * ((1 - self.width) / 2)

    def process(self, stereo: np.ndarray) -> np.ndarray:
        mono = (stereo[:, 0] + stereo[:, 1]) * _FIXED_GAIN

        outL = sum(c.process_buf(mono) for c in self.combs_l)
        outR = sum(c.process_buf(mono) for c in self.combs_r)
        for a in self.apall_l:
            outL = a.process_buf(outL)
        for a in self.apall_r:
            outR = a.process_buf(outR)

        dry = 1.0 - self.wet
        L = outL * self.wet1 + outR * self.wet2 + stereo[:, 0] * dry
        R = outR * self.wet1 + outL * self.wet2 + stereo[:, 1] * dry
        return np.column_stack([L, R]).astype(np.float32)
