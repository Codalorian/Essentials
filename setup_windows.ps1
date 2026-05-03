#Requires -Version 5.1
# OpenOmni Windows Setup (PowerShell)
# Right-click → "Run with PowerShell"   OR   pwsh setup_windows.ps1

$ErrorActionPreference = 'Stop'

function Write-Step($n, $msg) {
    Write-Host "`n[$n] $msg" -ForegroundColor Cyan
}

Write-Host "`n=== OpenOmni Windows Setup ===" -ForegroundColor Magenta

# ── Python check ──────────────────────────────────────────────────────────────
Write-Step "1/4" "Checking Python..."
try {
    $ver = python --version 2>&1
    Write-Host "  $ver" -ForegroundColor Green
} catch {
    Write-Host "  ERROR: Python not found." -ForegroundColor Red
    Write-Host "  Download: https://www.python.org/downloads/" -ForegroundColor Yellow
    Write-Host "  Tick 'Add Python to PATH' during install." -ForegroundColor Yellow
    Read-Host "Press Enter to exit"; exit 1
}

# ── pip packages ──────────────────────────────────────────────────────────────
Write-Step "2/4" "Installing Python packages..."
pip install numpy scipy sounddevice mido python-rtmidi JACK-Client
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ERROR: pip install failed." -ForegroundColor Red
    Read-Host "Press Enter to exit"; exit 1
}
Write-Host "  Packages installed OK." -ForegroundColor Green

# ── List devices ──────────────────────────────────────────────────────────────
Write-Step "3/4" "Detected audio devices:"
python -c "from engine.device_manager import print_audio_devices, print_midi_devices; print_audio_devices(); print_midi_devices()"

# ── Download links ────────────────────────────────────────────────────────────
Write-Step "4/4" "Manual installs for DAW routing:"

Write-Host @"

  VB-Cable  (virtual audio — FREE)
    Download : https://vb-audio.com/Cable/
    Usage    : python main.py --device "CABLE Input"
    In Ableton set AUDIO INPUT to "CABLE Output (VB-Audio Virtual Cable)"

  loopMIDI  (virtual MIDI — FREE)
    Download : https://www.tobias-erichsen.de/software/loopmidi.html
    Create a port named "OpenOmni MIDI"
    Usage    : python main.py --midi "OpenOmni MIDI"
    In Ableton MIDI PREFERENCES → Outputs → enable "OpenOmni MIDI"
               then set MIDI track output to "OpenOmni MIDI"

  ASIO4ALL  (low-latency audio driver — optional, FREE)
    Download : https://www.asio4all.org/
    Usage    : python main.py --device "ASIO4ALL"

"@ -ForegroundColor Yellow

Write-Host "=== Run Commands ===" -ForegroundColor Magenta
Write-Host @"

  Standalone (plays through your speakers):
    python main.py

  With VB-Cable + loopMIDI routed to Ableton:
    python main.py --device "CABLE Input" --midi "OpenOmni MIDI"

  Show all devices:
    python main.py --list-devices

  Show routing instructions:
    python main.py --hint

"@ -ForegroundColor White

Read-Host "Setup complete. Press Enter to exit"
