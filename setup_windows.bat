@echo off
:: OpenOmni Windows Setup
:: Run as normal user (no admin needed for pip)

echo ============================================
echo  OpenOmni Windows Setup
echo ============================================
echo.

:: Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found.
    echo Install from https://www.python.org/downloads/
    echo Make sure to check "Add Python to PATH" during install.
    pause
    exit /b 1
)

echo [1/3] Installing Python packages...
pip install numpy scipy sounddevice mido python-rtmidi JACK-Client
if errorlevel 1 (
    echo ERROR: pip install failed.
    pause
    exit /b 1
)

echo.
echo [2/3] Python packages installed OK.
echo.
echo [3/3] Manual installs needed for DAW routing:
echo.
echo   VB-Cable (virtual audio device - FREE):
echo     https://vb-audio.com/Cable/
echo     After install, select "CABLE Input" in OpenOmni and
echo     "CABLE Output" as audio input in Ableton.
echo.
echo   loopMIDI (virtual MIDI cable - FREE):
echo     https://www.tobias-erichsen.de/software/loopmidi.html
echo     Create a port named "OpenOmni MIDI", then set
echo     Ableton MIDI output to "OpenOmni MIDI".
echo.
echo   ASIO4ALL (low-latency driver - optional):
echo     https://www.asio4all.org/
echo.
echo ============================================
echo  How to run
echo ============================================
echo.
echo   Standalone (speakers only):
echo     python main.py
echo.
echo   With VB-Cable + loopMIDI:
echo     python main.py --device "CABLE Input" --midi "OpenOmni MIDI"
echo.
echo   Show all available devices:
echo     python main.py --list-devices
echo.
echo   Show DAW routing instructions:
echo     python main.py --hint
echo.
pause
