from __future__ import annotations
import tkinter as tk
import math

BG      = '#0b0b18'
PANEL   = '#13132a'
KNOB_BG = '#1e1e3a'
RING    = '#383860'
ACCENT  = '#7b52d8'
GLOW    = '#a97cff'
TEXT    = '#c8c8e8'
DIM     = '#666688'

class Knob(tk.Canvas):
    def __init__(self, parent, label: str, min_val: float, max_val: float,
                 default: float, fmt: str = '{:.2f}', callback=None,
                 size: int = 54, **kwargs):
        h = size + 24
        super().__init__(parent, width=size, height=h,
                         bg=BG, highlightthickness=0, **kwargs)
        self.label    = label
        self.min_val  = min_val
        self.max_val  = max_val
        self.value    = default
        self.default  = default
        self.fmt      = fmt
        self.callback = callback
        self.size     = size
        self._dy      = None
        self._v0      = default

        self.bind('<Button-1>',      self._click)
        self.bind('<B1-Motion>',     self._drag)
        self.bind('<Double-Button-1>', lambda _: self._set(default))
        self.bind('<Button-4>',      lambda _: self._nudge(+1))
        self.bind('<Button-5>',      lambda _: self._nudge(-1))
        self.bind('<MouseWheel>',    self._wheel)
        self._draw()

    # ── interaction ──────────────────────────────────────────────────────────

    def _click(self, e):
        self._dy = e.y
        self._v0 = self.value

    def _drag(self, e):
        if self._dy is None:
            return
        delta = (self._dy - e.y) / 150.0 * (self.max_val - self.min_val)
        self._set(self._v0 + delta)

    def _wheel(self, e):
        step = (self.max_val - self.min_val) / 100
        self._set(self.value + step * (1 if e.delta > 0 else -1))

    def _nudge(self, d):
        step = (self.max_val - self.min_val) / 100
        self._set(self.value + step * d)

    def _set(self, val):
        self.value = max(self.min_val, min(self.max_val, float(val)))
        self._draw()
        if self.callback:
            self.callback(self.value)

    def get(self) -> float:
        return self.value

    def set(self, val: float):
        self._set(val)

    # ── drawing ──────────────────────────────────────────────────────────────

    def _draw(self):
        self.delete('all')
        s = self.size
        cx = s // 2
        cy = s // 2
        r  = s // 2 - 3
        norm = (self.value - self.min_val) / (self.max_val - self.min_val)

        # Outer ring
        self.create_oval(cx - r - 2, cy - r - 2, cx + r + 2, cy + r + 2,
                         fill=RING, outline='')
        # Body
        self.create_oval(cx - r, cy - r, cx + r, cy + r,
                         fill=KNOB_BG, outline='')

        # Track arc (225° start, 270° sweep CW)
        r_arc = r - 3
        self.create_arc(cx - r_arc, cy - r_arc, cx + r_arc, cy + r_arc,
                        start=225 - norm * 270, extent=-(1 - norm) * 270,
                        style='arc', outline='#2a2a4a', width=3)
        # Value arc
        if norm > 0.002:
            self.create_arc(cx - r_arc, cy - r_arc, cx + r_arc, cy + r_arc,
                            start=225, extent=-norm * 270,
                            style='arc', outline=ACCENT, width=3)

        # Indicator dot at pointer position
        angle = math.radians(225 - norm * 270)
        px = cx + (r - 5) * math.cos(angle)
        py = cy - (r - 5) * math.sin(angle)
        self.create_oval(px - 3, py - 3, px + 3, py + 3, fill=GLOW, outline='')

        # Centre dot
        self.create_oval(cx - 2, cy - 2, cx + 2, cy + 2, fill=ACCENT, outline='')

        # Label
        self.create_text(cx, s + 4,  text=self.label,
                         fill=TEXT, font=('Helvetica', 7, 'bold'), anchor='n')
        self.create_text(cx, s + 14, text=self.fmt.format(self.value),
                         fill=DIM, font=('Helvetica', 6), anchor='n')


class OptionRow(tk.Frame):
    """Labelled OptionMenu that matches the dark theme."""
    def __init__(self, parent, label: str, options: list[str],
                 default: str, callback=None, **kwargs):
        super().__init__(parent, bg=BG, **kwargs)
        tk.Label(self, text=label, bg=BG, fg=DIM,
                 font=('Helvetica', 7, 'bold')).pack(side='left', padx=(0, 4))
        self.var = tk.StringVar(value=default)
        m = tk.OptionMenu(self, self.var, *options,
                          command=lambda v: callback(v) if callback else None)
        m.config(bg=PANEL, fg=TEXT, activebackground=ACCENT,
                 activeforeground='white', font=('Helvetica', 8),
                 bd=0, highlightthickness=0, relief='flat',
                 indicatoron=False, width=8)
        m['menu'].config(bg=PANEL, fg=TEXT, activebackground=ACCENT,
                         activeforeground='white', font=('Helvetica', 8))
        m.pack(side='left')

    def get(self) -> str:
        return self.var.get()

    def set(self, val: str):
        self.var.set(val)
