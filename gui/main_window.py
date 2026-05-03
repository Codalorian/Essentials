from __future__ import annotations
import tkinter as tk
from tkinter import ttk
import platform
from .knob import Knob, OptionRow, BG, PANEL, ACCENT, GLOW, TEXT, DIM, RING
from .piano import PianoKeyboard, KB_MAP

SECTION_FONT  = ('Helvetica', 8, 'bold')
HEADER_FONT   = ('Helvetica', 18, 'bold')

def section(parent, title: str, col: int, row: int, colspan: int = 1,
            padx: int = 4, pady: int = 4) -> tk.LabelFrame:
    f = tk.LabelFrame(parent, text=title, fg=ACCENT, bg=PANEL,
                      font=SECTION_FONT, bd=1, relief='flat',
                      highlightbackground=RING, highlightthickness=1)
    f.grid(column=col, row=row, columnspan=colspan,
           sticky='nsew', padx=padx, pady=pady)
    return f

def row_of_knobs(parent, specs: list) -> list[Knob]:
    """specs: list of (label, min, max, default, fmt, param_key)"""
    knobs = []
    for i, spec in enumerate(specs):
        k = Knob(parent, spec[0], spec[1], spec[2], spec[3], fmt=spec[4],
                 size=52)
        k.grid(row=0, column=i, padx=6, pady=6)
        knobs.append(k)
    return knobs


class MainWindow(tk.Tk):
    def __init__(self, synth, midi_mgr=None):
        super().__init__()
        self.synth    = synth
        self.midi_mgr = midi_mgr
        self.title('OpenOmni')
        self.configure(bg=BG)
        self.resizable(False, False)
        self._build_ui()
        self._bind_keyboard()
        self.load_preset_to_ui(synth.params.get('name', 'Moonlight Bells'))
        if midi_mgr:
            midi_mgr.on_change = self._on_midi_device_change

    # ─── Layout ──────────────────────────────────────────────────────────────

    def _build_ui(self):
        root = self

        # ── Header ──────────────────────────────────────────────────────────
        hdr = tk.Frame(root, bg=BG)
        hdr.pack(fill='x', padx=10, pady=(10, 0))

        tk.Label(hdr, text='OPEN', fg=GLOW, bg=BG,
                 font=('Helvetica', 22, 'bold')).pack(side='left')
        tk.Label(hdr, text='OMNI', fg=ACCENT, bg=BG,
                 font=('Helvetica', 22, 'bold')).pack(side='left', padx=(0, 16))

        tk.Label(hdr, text='PRESET:', fg=DIM, bg=BG,
                 font=('Helvetica', 8, 'bold')).pack(side='left')

        self._preset_var = tk.StringVar(value='Moonlight Bells')
        pm = tk.OptionMenu(hdr, self._preset_var, *self.synth.get_preset_list(),
                           command=self._on_preset)
        pm.config(bg=PANEL, fg=TEXT, activebackground=ACCENT,
                  activeforeground='white', font=('Helvetica', 9),
                  bd=0, highlightthickness=0, relief='flat',
                  indicatoron=False, width=14)
        pm['menu'].config(bg=PANEL, fg=TEXT, activebackground=ACCENT,
                          activeforeground='white', font=('Helvetica', 9))
        pm.pack(side='left', padx=6)

        tk.Label(hdr, text='A-; = C4-E5  •  dbl-click knob = reset',
                 fg='#444466', bg=BG, font=('Helvetica', 7)).pack(side='right')

        # ── Device status bar ─────────────────────────────────────────────────
        dbar = tk.Frame(root, bg='#0e0e20')
        dbar.pack(fill='x', padx=0, pady=0)

        self._audio_label = tk.Label(dbar, text='', fg='#556688', bg='#0e0e20',
                                     font=('Helvetica', 7), anchor='w')
        self._audio_label.pack(side='left', padx=8)

        # MIDI port picker
        tk.Label(dbar, text='MIDI:', fg='#444466', bg='#0e0e20',
                 font=('Helvetica', 7)).pack(side='left')
        self._midi_port_var = tk.StringVar(value='scanning...')
        self._midi_menu_btn = tk.Menubutton(
            dbar, textvariable=self._midi_port_var,
            bg='#0e0e20', fg='#556688', activebackground='#1a1a30',
            activeforeground=ACCENT, relief='flat',
            font=('Helvetica', 7), width=28, anchor='w')
        self._midi_port_menu = tk.Menu(
            self._midi_menu_btn, tearoff=False,
            bg=PANEL, fg=TEXT, activebackground=ACCENT,
            activeforeground='white', font=('Helvetica', 8))
        self._midi_menu_btn['menu'] = self._midi_port_menu
        self._midi_menu_btn.pack(side='left', padx=2)

        hint_btn = tk.Button(dbar, text='DAW routing ▸',
                             bg='#0e0e20', fg='#444466', relief='flat',
                             font=('Helvetica', 7), bd=0,
                             activebackground='#1a1a30', activeforeground=ACCENT,
                             command=self._show_routing_hint)
        hint_btn.pack(side='right', padx=8)

        self.after(300,  self._refresh_device_labels)
        self.after(2000, self._poll_midi_ports)

        # ── Main grid ───────────────────────────────────────────────────────
        grid = tk.Frame(root, bg=BG)
        grid.pack(fill='both', padx=6, pady=4)
        for c in range(4):
            grid.columnconfigure(c, weight=1)

        # Row 0: OSC A | OSC B | ENVELOPE | FILTER
        self._build_osc_a(grid)
        self._build_osc_b(grid)
        self._build_env(grid)
        self._build_filter(grid)

        # Row 1: LFO | REVERB | DELAY | CHORUS
        self._build_lfo(grid)
        self._build_reverb(grid)
        self._build_delay(grid)
        self._build_chorus(grid)

        # ── Piano ────────────────────────────────────────────────────────────
        pf = tk.Frame(root, bg=BG)
        pf.pack(pady=(2, 8))
        self.piano = PianoKeyboard(pf, self.synth.note_on, self.synth.note_off,
                                   start_octave=3, n_octaves=3)
        self.piano.pack()

    # ─── Sections ────────────────────────────────────────────────────────────

    def _build_osc_a(self, grid):
        f = section(grid, 'OSC A', col=0, row=0)
        self._osc_a_wave = OptionRow(f, 'WAVE', ['sine', 'soft', 'triangle', 'sawtooth', 'square'],
                                     'sine', callback=lambda v: self.synth.set_param('osc_a_wave', v))
        self._osc_a_wave.pack(pady=(6, 2), padx=4)

        kr = tk.Frame(f, bg=PANEL)
        kr.pack()
        specs = [
            ('OCT',   -3, 3, 0,    '{:.0f}',  'osc_a_oct'),
            ('SEMI',  -12, 12, 0,  '{:.0f}',  'osc_a_semi'),
            ('FINE',  -50, 50, 0,  '{:.1f}',  'osc_a_fine'),
            ('LEVEL', 0,  1, 0.85, '{:.2f}',  'osc_a_level'),
        ]
        self._osc_a_knobs = self._make_knobs(kr, specs)

    def _build_osc_b(self, grid):
        f = section(grid, 'OSC B', col=1, row=0)

        top = tk.Frame(f, bg=PANEL)
        top.pack(pady=(6, 2), padx=4, fill='x')
        self._osc_b_wave = OptionRow(top, 'WAVE', ['soft', 'sine', 'triangle', 'sawtooth', 'square'],
                                     'soft', callback=lambda v: self.synth.set_param('osc_b_wave', v))
        self._osc_b_wave.pack(side='left')
        self._osc_b_en = tk.BooleanVar(value=True)
        tk.Checkbutton(top, text='ON', variable=self._osc_b_en, bg=PANEL, fg=TEXT,
                       selectcolor=ACCENT, activebackground=PANEL,
                       font=('Helvetica', 7, 'bold'),
                       command=lambda: self.synth.set_param('osc_b_enabled', self._osc_b_en.get())
                       ).pack(side='right')

        kr = tk.Frame(f, bg=PANEL)
        kr.pack()
        specs = [
            ('OCT',   -3, 3, 1,    '{:.0f}', 'osc_b_oct'),
            ('SEMI',  -12, 12, 7,  '{:.0f}', 'osc_b_semi'),
            ('FINE',  -50, 50, -4, '{:.1f}', 'osc_b_fine'),
            ('LEVEL', 0,  1, 0.45, '{:.2f}', 'osc_b_level'),
        ]
        self._osc_b_knobs = self._make_knobs(kr, specs)

    def _build_env(self, grid):
        f = section(grid, 'ENVELOPE', col=2, row=0)
        kr = tk.Frame(f, bg=PANEL)
        kr.pack(pady=8)
        specs = [
            ('ATK',  0.001, 5.0, 0.06,  '{:.3f}', 'attack'),
            ('DEC',  0.001, 5.0, 0.4,   '{:.3f}', 'decay'),
            ('SUS',  0.0,   1.0, 0.55,  '{:.2f}', 'sustain'),
            ('REL',  0.01,  8.0, 2.8,   '{:.2f}', 'release'),
        ]
        self._env_knobs = self._make_knobs(kr, specs)

    def _build_filter(self, grid):
        f = section(grid, 'FILTER', col=3, row=0)
        self._filt_mode = OptionRow(f, 'MODE', ['lowpass', 'highpass', 'bandpass'],
                                    'lowpass',
                                    callback=lambda v: self.synth.set_param('filter_mode', v))
        self._filt_mode.pack(pady=(6, 2), padx=4)
        kr = tk.Frame(f, bg=PANEL)
        kr.pack()
        specs = [
            ('CUT',   80,  18000, 3800, '{:.0f}', 'filter_cutoff'),
            ('RES',   0.1, 4.0,   0.28, '{:.2f}', 'filter_resonance'),
            ('ENV',  -1.0, 1.0,   0.35, '{:.2f}', 'filter_env_amt'),
        ]
        self._filt_knobs = self._make_knobs(kr, specs)

    def _build_lfo(self, grid):
        f = section(grid, 'LFO', col=0, row=1)
        self._lfo_wave = OptionRow(f, 'WAVE', ['sine', 'triangle', 'sawtooth', 'square'],
                                   'sine', callback=lambda v: self.synth.set_param('lfo_wave', v))
        self._lfo_wave.pack(pady=(6, 2), padx=4)
        self._lfo_target = OptionRow(f, 'TARGET', ['pitch', 'filter', 'volume'],
                                     'pitch', callback=lambda v: self.synth.set_param('lfo_target', v))
        self._lfo_target.pack(pady=(0, 2), padx=4)
        kr = tk.Frame(f, bg=PANEL)
        kr.pack()
        specs = [
            ('RATE',  0.01, 20.0, 0.32, '{:.2f}', 'lfo_rate'),
            ('DEPTH', 0.0,  1.0,  0.18, '{:.2f}', 'lfo_depth'),
        ]
        self._lfo_knobs = self._make_knobs(kr, specs)

    def _build_reverb(self, grid):
        f = section(grid, 'REVERB', col=1, row=1)
        kr = tk.Frame(f, bg=PANEL)
        kr.pack(pady=8)
        specs = [
            ('SIZE', 0.0, 1.0, 0.88, '{:.2f}', 'reverb_size'),
            ('DAMP', 0.0, 1.0, 0.45, '{:.2f}', 'reverb_damp'),
            ('WET',  0.0, 1.0, 0.62, '{:.2f}', 'reverb_wet'),
        ]
        self._rev_knobs = self._make_knobs(kr, specs)

    def _build_delay(self, grid):
        f = section(grid, 'DELAY', col=2, row=1)
        kr = tk.Frame(f, bg=PANEL)
        kr.pack(pady=8)
        specs = [
            ('TIME', 0.02, 2.0, 0.375, '{:.3f}', 'delay_time'),
            ('FDBK', 0.0,  0.95, 0.38, '{:.2f}', 'delay_feedback'),
            ('WET',  0.0,  1.0,  0.24, '{:.2f}', 'delay_wet'),
        ]
        self._dly_knobs = self._make_knobs(kr, specs)

    def _build_chorus(self, grid):
        f = section(grid, 'CHORUS', col=3, row=1)
        kr = tk.Frame(f, bg=PANEL)
        kr.pack(pady=8)
        specs = [
            ('RATE',  0.1, 5.0,  0.45,   '{:.2f}', 'chorus_rate'),
            ('DEPTH', 0.0, 0.01, 0.0028, '{:.4f}', 'chorus_depth'),
            ('WET',   0.0, 1.0,  0.38,   '{:.2f}', 'chorus_wet'),
        ]
        self._cho_knobs = self._make_knobs(kr, specs)

    # ─── Helpers ─────────────────────────────────────────────────────────────

    def _make_knobs(self, parent, specs: list) -> dict:
        knobs = {}
        for i, (label, mn, mx, default, fmt, param_key) in enumerate(specs):
            def cb(val, k=param_key):
                self.synth.set_param(k, val)
            k = Knob(parent, label, mn, mx, default, fmt=fmt, callback=cb, size=52)
            k.grid(row=0, column=i, padx=5, pady=6)
            knobs[param_key] = k
        return knobs

    def _bind_keyboard(self):
        self.bind('<KeyPress>',   lambda e: self.piano.kb_press(e.char))
        self.bind('<KeyRelease>', lambda e: self.piano.kb_release(e.char))
        self.bind('<Escape>',     lambda _: self.synth.all_notes_off())

    def _on_preset(self, name: str):
        self.synth.load_preset_by_name(name)
        self.load_preset_to_ui(name)

    def load_preset_to_ui(self, name: str):
        p = self.synth.params

        self._osc_a_wave.set(p.get('osc_a_wave', 'sine'))
        self._osc_b_wave.set(p.get('osc_b_wave', 'soft'))
        self._osc_b_en.set(p.get('osc_b_enabled', True))
        self._filt_mode.set(p.get('filter_mode', 'lowpass'))
        self._lfo_wave.set(p.get('lfo_wave', 'sine'))
        self._lfo_target.set(p.get('lfo_target', 'pitch'))

        all_knobs = {
            **self._osc_a_knobs, **self._osc_b_knobs, **self._env_knobs,
            **self._filt_knobs,  **self._lfo_knobs,   **self._rev_knobs,
            **self._dly_knobs,   **self._cho_knobs,
        }
        for key, knob in all_knobs.items():
            if key in p:
                knob.set(float(p[key]))

    # ─── Device status ────────────────────────────────────────────────────────

    def _refresh_device_labels(self):
        try:
            import sounddevice as sd
            dev = sd.query_devices(sd.default.device[1], 'output')
            self._audio_label.config(text=dev['name'], fg='#556688')
        except Exception:
            pass

    def _poll_midi_ports(self):
        """Refresh MIDI port menu every 2 s (handles hot-plug)."""
        try:
            from engine.device_manager import list_midi_inputs
            ports = list_midi_inputs()
            self._midi_port_menu.delete(0, 'end')

            current = self.midi_mgr.current_name if self.midi_mgr else None

            self._midi_port_menu.add_command(
                label='Auto-detect (best device)',
                command=lambda: self._select_midi_port(None))

            if ports:
                self._midi_port_menu.add_separator()
                for p in ports:
                    self._midi_port_menu.add_command(
                        label=p,
                        command=lambda n=p: self._select_midi_port(n))
            else:
                self._midi_port_menu.add_command(
                    label='(no MIDI devices found)', state='disabled')

            # Update label
            if current:
                short = current[:30] + '...' if len(current) > 30 else current
                self._midi_port_var.set(short)
                self._midi_menu_btn.config(fg='#7b9fc8')
            else:
                self._midi_port_var.set('no MIDI detected')
                self._midi_menu_btn.config(fg='#443355')
        except Exception:
            pass

        self.after(2000, self._poll_midi_ports)

    def _select_midi_port(self, name):
        if self.midi_mgr:
            self.midi_mgr.set_preferred(name)

    def _on_midi_device_change(self, name):
        """Called from MidiManager thread — schedule GUI update on main thread."""
        self.after(0, lambda: self._update_midi_label(name))

    def _update_midi_label(self, name):
        if name:
            short = name[:30] + '...' if len(name) > 30 else name
            self._midi_port_var.set(short)
            self._midi_menu_btn.config(fg='#7b9fc8')
        else:
            self._midi_port_var.set('no MIDI detected')
            self._midi_menu_btn.config(fg='#443355')

    def _show_routing_hint(self):
        from engine.device_manager import routing_hint, print_audio_devices, print_midi_devices
        win = tk.Toplevel(self)
        win.title('DAW Routing')
        win.configure(bg=BG)
        win.resizable(False, False)

        txt = tk.Text(win, bg='#0e0e20', fg=TEXT, font=('Courier', 9),
                      relief='flat', bd=0, width=72, height=20,
                      insertbackground=TEXT)
        txt.pack(padx=12, pady=12)

        import io, contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            print_audio_devices()
            print_midi_devices()
        hint_text = routing_hint() + '\n' + buf.getvalue()
        txt.insert('1.0', hint_text)
        txt.config(state='disabled')

        tk.Button(win, text='Close', bg=PANEL, fg=TEXT, relief='flat',
                  command=win.destroy).pack(pady=(0, 10))
