# OpenOmni VST3 — Build Instructions

## Requirements

| Tool | Version | Notes |
|------|---------|-------|
| CMake | 3.22+ | |
| Visual Studio | 2022 (Community is free) | Windows — needed for MSVC |
| Git | any | CMake fetches JUCE automatically |

JUCE 7.0.12 is downloaded automatically via CMake FetchContent on first configure.

---

## Windows (VST3 for Ableton)

```cmd
cd cpp
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The VST3 bundle lands at:

```
build\OpenOmni_artefacts\Release\VST3\OpenOmni.vst3
```

Copy the entire `OpenOmni.vst3` folder to one of Ableton's VST3 scan paths:

```
C:\Program Files\Common Files\VST3\
```

Then in Ableton: **Preferences → Plug-Ins → VST3 Plug-In Custom Folder** → add the path → **Rescan**.

---

## macOS

```bash
cd cpp
cmake -B build -G Xcode
cmake --build build --config Release
```

Output: `build/OpenOmni_artefacts/Release/VST3/OpenOmni.vst3`

Copy to `~/Library/Audio/Plug-Ins/VST3/`.

---

## Linux (standalone only — VST3 works in Reaper/Bitwig)

```bash
cd cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## Standalone app (all platforms)

The Standalone target is also built automatically. Find it at:

```
build\OpenOmni_artefacts\Release\Standalone\OpenOmni.exe   (Windows)
build/OpenOmni_artefacts/Release/Standalone/OpenOmni        (Linux/macOS)
```

Run the standalone with a MIDI keyboard and your audio interface — no DAW needed.

---

## Troubleshooting

- **"JUCE not found"** — delete `build/` and re-run cmake (FetchContent re-downloads).
- **Plugin not showing in Ableton** — make sure you copied the whole `.vst3` folder, not just the DLL inside it. Ableton requires the bundle.
- **Audio crackling** — increase buffer size to 256 or 512 samples in Ableton preferences.
