from __future__ import annotations
import tkinter as tk

# Computer keyboard → MIDI note (C4 = 60)
KB_MAP = {
    'a': 60, 'w': 61, 's': 62, 'e': 63, 'd': 64,
    'f': 65, 't': 66, 'g': 67, 'y': 68, 'h': 69,
    'u': 70, 'j': 71, 'k': 72, 'o': 73, 'l': 74,
    'p': 75, ';': 76,
}

WW = 28   # white key width
WH = 95   # white key height
BW = 18   # black key width
BH = 60   # black key height

# Which positions in 0..6 have a black key to the right
BLACK_AFTER = {0, 1, 3, 4, 5}  # C D F G A

BG = '#0b0b18'

class PianoKeyboard(tk.Canvas):
    def __init__(self, parent, note_on_cb, note_off_cb, start_octave: int = 3,
                 n_octaves: int = 3, **kwargs):
        n_whites = n_octaves * 7 + 1
        w = n_whites * WW + 2
        super().__init__(parent, width=w, height=WH + 4, bg=BG,
                         highlightthickness=0, **kwargs)
        self.note_on  = note_on_cb
        self.note_off = note_off_cb
        self.start    = start_octave * 12
        self.n_oct    = n_octaves
        self._pressed: set[int] = set()
        self._key_items: dict[int, list] = {}  # note -> canvas item ids

        self._draw_keys()
        self.bind('<Button-1>',        self._mouse_down)
        self.bind('<ButtonRelease-1>', self._mouse_up)

    def _note_for_white(self, idx: int) -> int:
        oct_n, pos = divmod(idx, 7)
        offsets = [0, 2, 4, 5, 7, 9, 11]
        return self.start + oct_n * 12 + offsets[pos]

    def _draw_keys(self):
        n_whites = self.n_oct * 7 + 1
        # White keys
        for i in range(n_whites):
            note = self._note_for_white(i)
            x = i * WW + 1
            ids = [self.create_rectangle(x, 1, x + WW - 1, WH,
                                         fill='#f0f0f0', outline='#555577')]
            self._key_items[note] = ids

        # Black keys (drawn on top)
        for i in range(n_whites - 1):
            oct_i, pos = divmod(i, 7)
            if pos not in BLACK_AFTER:
                continue
            offsets = [0, 2, 4, 5, 7, 9, 11]
            white_note = self.start + oct_i * 12 + offsets[pos]
            black_note = white_note + 1
            x = (i + 1) * WW - BW // 2
            ids = [self.create_rectangle(x, 1, x + BW, BH,
                                         fill='#1a1a2e', outline='#7b52d8')]
            self._key_items[black_note] = ids

    def _note_at(self, x: int, y: int) -> int:
        # Check black keys first (drawn on top)
        n_whites = self.n_oct * 7
        for i in range(n_whites):
            oct_i, pos = divmod(i, 7)
            if pos not in BLACK_AFTER:
                continue
            offsets = [0, 2, 4, 5, 7, 9, 11]
            white_note = self.start + oct_i * 12 + offsets[pos]
            black_note = white_note + 1
            bx = (i + 1) * WW - BW // 2
            if bx <= x <= bx + BW and y <= BH:
                return black_note
        # White keys
        wi = x // WW
        if 0 <= wi <= n_whites:
            return self._note_for_white(wi)
        return -1

    def _color(self, note: int, pressed: bool) -> str:
        is_black = (note % 12) in {1, 3, 6, 8, 10}
        if pressed:
            return '#7b52d8'
        return '#1a1a2e' if is_black else '#f0f0f0'

    def _press(self, note: int):
        if note < 0 or note in self._pressed:
            return
        self._pressed.add(note)
        for item in self._key_items.get(note, []):
            self.itemconfig(item, fill=self._color(note, True))
        self.note_on(note, 100)

    def _release(self, note: int):
        if note not in self._pressed:
            return
        self._pressed.discard(note)
        for item in self._key_items.get(note, []):
            self.itemconfig(item, fill=self._color(note, False))
        self.note_off(note)

    def _mouse_down(self, e):
        self._press(self._note_at(e.x, e.y))

    def _mouse_up(self, e):
        for note in list(self._pressed):
            self._release(note)

    # Called by main window for computer keyboard
    def kb_press(self, char: str):
        note = KB_MAP.get(char.lower())
        if note is not None:
            self._press(note)

    def kb_release(self, char: str):
        note = KB_MAP.get(char.lower())
        if note is not None:
            self._release(note)
