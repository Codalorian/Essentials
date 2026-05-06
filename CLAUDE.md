# OpenOmni — Codebase Guide

## What it is
A cross-platform software synthesizer written in pure Python. It runs standalone (with its own Tkinter GUI) and routes audio to a DAW via virtual cables (VB-Cable / BlackHole / PipeWire-JACK). No VST SDK — it IS the instrument.

## Tech stack
- **Audio output**: `sounddevice` (WASAPI/CoreAudio/ALSA) or JACK (`python-jack`)
- **MIDI input**: `mido` + `python-rtmidi`, hot-plugged via a background thread
- **Signal processing**: `numpy` + `scipy` (filter coefficients, lfilter, sosfilt)
- **GUI**: `tkinter` (dark theme, custom canvas Knob widget)
- **Python**: 3.10+, no async, threading via `threading.Lock` / `threading.Event`

## Architecture — signal flow

```
MIDI in ──► MidiManager ──► synth.note_on/note_off/set_param
                                        │
                              OpenOmni.render(n_frames)
                                        │
                              ┌─────────▼──────────┐
                              │  up to 16 Voice     │ ← voice stealing
                              │  ┌───────────────┐  │
                              │  │ WavetableOsc A│  │
                              │  │ WavetableOsc B│  │ ← 2 osc, unison N
                              │  │ ADSR amp_env  │  │
                              │  │ ADSR filt_env │  │
                              │  │ SVFilter L/R  │  │
                              │  └───────────────┘  │
                              └─────────┬──────────┘
                                        │ sum stereo
                              soft-clip (tanh × 0.65)
                                        │
                              Chorus ──► Delay ──► Reverb
                                        │
                              × master_volume ──► output
```

LFO ticks once per render block (single float value, not per-sample).

## File map

| File | Role |
|---|---|
| `main.py` | Entry point, CLI args, audio backend selection, GUI launch |
| `engine/synth.py` | `OpenOmni` — voice pool, preset store, render pipeline |
| `engine/voice.py` | `Voice` — per-note DSP: oscs, envelopes, filter, unison, glide |
| `engine/oscillator.py` | `WavetableOscillator` — wavetable lookup with linear interpolation |
| `engine/envelope.py` | `ADSR` — linear attack/decay, exponential release |
| `engine/filter.py` | `SVFilter` — TPT biquad (lowpass / highpass / bandpass) |
| `engine/lfo.py` | `LFO` — single-value tick per block (sine/tri/square/saw) |
| `engine/midi_manager.py` | Hot-plug MIDI, CC dispatch (mod wheel, cutoff, resonance, pitch bend, sustain) |
| `engine/jack_client.py` | JACK backend (optional; falls back to sounddevice) |
| `engine/device_manager.py` | Audio/MIDI device enumeration, DAW routing hints |
| `effects/reverb.py` | Freeverb (8 comb + 4 allpass per channel, numpy-vectorised) |
| `effects/delay.py` | `StereoDelay` — feedback delay line |
| `effects/chorus.py` | `Chorus` — LFO-modulated stereo delay |
| `gui/main_window.py` | `MainWindow(tk.Tk)` — full UI, preset picker, MIDI port menu |
| `gui/knob.py` | `Knob` (canvas), `OptionRow` (themed OptionMenu), color constants |
| `gui/piano.py` | `PianoKeyboard` — clickable/keyboard-playable 3-octave piano |
| `presets/__init__.py` | (empty — presets live in `engine/synth.py`) |

## Preset format

Presets are plain Python dicts returned by methods on `OpenOmni`. Every key maps directly to `synth.params` and is read by `Voice.render()` or `_apply_effects()` each block.

### All parameter keys

**Oscillator A/B**
- `osc_a_wave` / `osc_b_wave` — `'sine'|'soft'|'triangle'|'sawtooth'|'square'|'bell'|'gamelan'`
- `osc_a_level` / `osc_b_level` — 0–1
- `osc_a_oct` / `osc_b_oct` — int, −3–3
- `osc_a_semi` / `osc_b_semi` — int, −12–12
- `osc_a_fine` / `osc_b_fine` — cents, −50–50
- `osc_b_enabled` — bool

**Unison / Voice**
- `unison_voices` — 1–8 (int); 1 = no unison. Voices are spread L→R.
- `unison_detune` — total detune spread in cents (e.g. 12 = ±6 ¢ per edge voice)
- `glide_time` — portamento time in seconds (0 = off)

**Amplitude envelope**
- `attack`, `decay`, `sustain`, `release`

**Filter envelope**
- `fenv_attack`, `fenv_decay`, `fenv_sustain`, `fenv_release`

**Filter**
- `filter_mode` — `'lowpass'|'highpass'|'bandpass'`
- `filter_cutoff` — 20–18000 Hz
- `filter_resonance` — 0.1–8.0 (maps to Q)
- `filter_env_amt` — −1–1 (scales filt_env → cutoff modulation)
- `filter_keytrack` — 0–1 (1 = full keytracking; cutoff scales with note pitch)

**LFO**
- `lfo_wave` — `'sine'|'triangle'|'sawtooth'|'square'`
- `lfo_rate` — Hz
- `lfo_depth` — 0–1
- `lfo_target` — `'pitch'|'filter'|'volume'`

**Effects**
- `reverb_size`, `reverb_damp`, `reverb_wet`
- `delay_time` (s), `delay_feedback`, `delay_wet`
- `chorus_rate` (Hz), `chorus_depth` (s), `chorus_wet`

**Global**
- `master_volume`
- `pitch_bend_semi` — set by MIDI pitchwheel, ±2 semitones

### Adding a preset

Add a `_myname_preset()` method on `OpenOmni`, add the name to `get_preset_list()`, and add the mapping in `load_preset_by_name()`. The GUI preset dropdown is populated from `get_preset_list()` at startup.

### Adding a wavetable

Add a key to `WavetableOscillator._tables` inside `_build_tables()`. Tables are built once at class level (shared across all instances). Add the new name to the WAVE `OptionRow` options in `gui/main_window.py` for both OSC A and OSC B.

## MIDI CC mappings (hardcoded in midi_manager.py + jack_client.py)

| CC | Parameter |
|---|---|
| 1 | lfo_depth |
| 7, 11 | master_volume |
| 64 | sustain pedal |
| 71 | filter_resonance |
| 74 | filter_cutoff |
| 123 | all notes off |

Pitch bend: ±2 semitones.

## Running

```bash
python main.py                          # auto-detect everything
python main.py --list-devices           # show audio + MIDI devices
python main.py --device "CABLE Input"   # Windows DAW routing
pw-jack python3 main.py                 # Linux JACK
```

Keyboard play: `a w s e d f t g y h u j k o l p ;` → C4–E5. ESC = all notes off.

## Known gaps / future work

- LFO is block-rate (once per 1024 samples); could be per-sample for accuracy at high rates
- Filter envelope uses `np.mean()` across the block — an approximation
- Delay inner loop is a Python for-loop (slow for large block sizes — candidate for vectorization)
- No arpeggiator or step sequencer
- No per-voice panning (unison panning is the only stereo spread within a voice)
- Presets stored in code, not JSON files
