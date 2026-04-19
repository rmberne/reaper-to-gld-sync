# ReaperSync

A lightweight macOS Menu Bar app designed for live drummers to synchronize Reaper project tempo with hardware and receive commands via an Arduino controller.

## Main Functionalities

1. **Allen & Heath GLD Sync**: Sends Reaper project tempo to a delay effect in an A&H GLD mixer via MIDI NRPN messages over TCP/IP.
2. **Soundbrenner Pulse Sync**: Sends Reaper project tempo to the Soundbrenner Pulse app/wearable using Ableton Link.
3. **Arduino Controller**: Receives serial commands from an Arduino to trigger Reaper actions (e.g., Play/Stop, Next/Previous Tab, Mute Backing Tracks).

## Features

- **macOS Menu Bar App**: Runs discreetly in the system menu bar (🥁 icon).
- **Native Preferences**: Configure all settings (IPs, Ports, MIDI Channels) through a native macOS Preferences window (using `NSUserDefaults`).
- **High Precision Sync**: Uses a sliding window and snapping algorithm to eliminate USB/OS jitter, ensuring a rock-solid BPM reading.
- **Auto-Reconnection**: Automatically attempts to reconnect to the mixer or Arduino in the background.
- **Universal Binary**: Supports both Apple Silicon (`arm64`) and Intel (`x86_64`) Macs.
- **Asynchronous Logging**: Integrated `spdlog` for efficient, rotating file logging.

## Control Methods

- **MIDI Clock**: Automatic BPM detection from Reaper's MIDI output.
- **MIDI CC 20**: Manual tempo override (60-187 BPM).
- **MIDI Notes**: Multiplier control (Note 60 = 1x, 61 = 0.5x, 62 = 2x).
- **Arduino Commands**:
  - `s`: Start / Stop (CC 100)
  - `m`: Mute Tracks Toggle (CC 102)
  - `n`: Next Tab (CC 103)
  - `p`: Previous Tab (CC 104)

## Setup & Configuration

1. **Launch**: Start `ReaperSync.app`. A drum icon (🥁) will appear in your menu bar.
2. **Preferences**: Click the 🥁 icon and select **Preferences...** to configure:
   - Arduino Port (auto-detection supported)
   - Mixer IP and Port
   - MIDI Channel and NRPN Parameter for the GLD delay
   - Enable/Disable specific modules (Arduino, Pulse, Mixer)
3. **Reaper Configuration**:
   - Go to `Preferences > MIDI Devices`.
   - Enable your MIDI output (e.g., the virtual "ReaperSync" port created by this app).
   - Right-click the output and check **"Send clock to this device"**.
   - (Optional) Map the CC messages from the Arduino to actions in Reaper.

## Requirements

- macOS (AppKit, CoreMIDI, CoreAudio frameworks)
- [Asio](https://think-async.com/Asio/) (installed via Homebrew: `brew install asio`)
- [RtMidi](https://github.com/thestk/rtmidi) (included as a submodule)
- [Ableton Link](https://github.com/Ableton/link) (included in `libs/link`)
- [spdlog](https://github.com/gabime/spdlog) (fetched automatically via CMake)

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

The build will produce a `ReaperSync.app` bundle in the `build` directory.
