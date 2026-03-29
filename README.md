# Reaper to GLD Sync

Sync MIDI clock from Reaper (or any DAW) to Allen & Heath GLD delay parameter.

This utility listens for MIDI Clock (0xF8) or specific MIDI CC/Note messages from your DAW and translates them into MIDI NRPN messages that the A&H GLD mixer understands to synchronize its delay effects.

## Features

- **High Precision Sync**: Uses absolute timestamps and a snapping algorithm to eliminate USB/OS jitter, ensuring a rock-solid BPM reading.
- **Auto-Reconnection**: Automatically attempts to connect to the mixer's IP and handles hardware power cycles or network interruptions gracefully in the background.
- **Hybrid Support**: Built as a macOS Universal Binary (`arm64` + `x86_64`) to run on both modern Apple Silicon and older Intel Macs.
- **Multiple Control Methods**:
  - **MIDI Clock**: Automatic BPM detection from Reaper's MIDI output.
  - **MIDI CC 20**: Manual tempo override (60-187 BPM).
  - **MIDI Notes**: Multiplier control (Note 60 = 1x, 61 = 0.5x, 62 = 2x).

## Setup

1. **Configuration**:
   Create a `config.txt` file in the same directory as the executable to customize settings. If no file is found, the application uses the defaults shown below:

   ```ini
   IP=192.168.1.10
   PORT=51325
   CHANNEL=0
   PARAMETER=8449
   ```

   - **IP/PORT**: The network address of your GLD mixer.
   - **CHANNEL**: MIDI Channel (0-15).
   - **PARAMETER**: The 14-bit NRPN parameter ID. (Default `8449` = FX Slot 3, Param 1/Time).

2. **DAW Configuration (Reaper)**:
   - Go to `Preferences > MIDI Devices`.
   - Enable your MIDI output (e.g., "IAC Driver" or the virtual "ReaperSync" port created by this app).
   - Right-click the output and check **"Send clock to this device"**.

3. **Build**:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

## Requirements

- macOS (CoreMIDI/CoreAudio frameworks)
- [Asio](https://think-async.com/Asio/) (header-only, included via Homebrew)
- [RtMidi](https://github.com/thestk/rtmidi) (included as a submodule)
