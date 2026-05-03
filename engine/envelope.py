from __future__ import annotations
import numpy as np

IDLE, ATTACK, DECAY, SUSTAIN, RELEASE = 0, 1, 2, 3, 4

class ADSR:
    def __init__(self, sample_rate: int = 44100):
        self.sr = sample_rate
        self.state = IDLE
        self.level = 0.0
        self.velocity = 1.0
        self.attack = 0.01
        self.decay = 0.1
        self.sustain = 0.7
        self.release = 0.3

    def note_on(self, velocity: int = 100):
        self.velocity = velocity / 127.0
        self.state = ATTACK

    def note_off(self):
        if self.state != IDLE:
            self.state = RELEASE

    def is_active(self) -> bool:
        return self.state != IDLE

    def render(self, n_frames: int) -> np.ndarray:
        out = np.empty(n_frames, dtype=np.float32)
        level = self.level
        state = self.state
        sr = self.sr

        att_step = 1.0 / max(self.attack * sr, 1)
        dec_step = (1.0 - self.sustain) / max(self.decay * sr, 1)
        rel_rate = 1.0 - (5.0 / max(self.release * sr, 1))  # exponential

        for i in range(n_frames):
            if state == ATTACK:
                level += att_step
                if level >= 1.0:
                    level = 1.0
                    state = DECAY
            elif state == DECAY:
                level -= dec_step
                if level <= self.sustain:
                    level = self.sustain
                    state = SUSTAIN
            elif state == SUSTAIN:
                level = self.sustain
            elif state == RELEASE:
                level *= rel_rate
                if level < 0.0001:
                    level = 0.0
                    state = IDLE
            else:
                level = 0.0
            out[i] = level

        self.level = level
        self.state = state
        return out * self.velocity
