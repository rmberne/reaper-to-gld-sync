# Reaper to GLD Sync - Project Overview

## High-Level Architecture

The project is structured into modular components to handle MIDI input, configuration, and hardware communication independently.

### Components

1.  **MidiManager (rt::midi::MidiManager)**:
    *   Listens for MIDI input (Clock, Transport, CC, Notes).
    *   Calculates BPM using absolute timestamps and a snapping algorithm for stability.
    *   Provides two callbacks:
        *   `SyncCallback`: Reports calculated BPM and multiplier changes.
        *   `ClockCallback`: Forwards raw MIDI realtime messages (Clock, Start, Stop) with minimal latency.

2.  **Mixer (Mixer)**:
    *   Handles MIDI-over-TCP communication with the Allen & Heath GLD mixer.
    *   Manages an internal background thread for automatic reconnection.
    *   Converts BPM/ms into 14-bit MIDI NRPN messages.

3.  **PulseManager (rt::midi::PulseManager)**:
    *   Handles MIDI output to a SoundBrenner Pulse device.
    *   Sends MIDI Realtime messages (0xF8, 0xFA, 0xFC) to keep the device in sync.

4.  **ConfigLoader (config::ConfigLoader)**:
    *   Parses a simple `config.txt` file.
    *   Allows enabling/disabling Mixer and Pulse components.

5.  **Main (main.cpp)**:
    *   Orchestrates the components.
    *   Wires the low-latency `ClockCallback` from `MidiManager` directly to `PulseManager`.
    *   Wires the `SyncCallback` from `MidiManager` to `Mixer`.

### Threading Model

*   **MIDI Input Thread**: Created by `RtMidi`. Handles all incoming MIDI messages and executes callbacks.
*   **Mixer Reconnect Thread**: Background thread in `Mixer` that checks and restores the TCP connection every 3 seconds.
*   **Main Thread**: Initializes the system and then blocks on `asio::io_context::run()`.

## Todo List

- [ ] **Logging Improvement**: Implement a more structured logging system instead of raw `std::cout`.
- [ ] **Port Selection**: Allow selecting specific MIDI Input/Output ports by name via `config.txt`.
- [ ] **Soundbrenner Specifics**: Research if SoundBrenner requires specific SysEx messages for advanced features (e.g., color, haptic intensity).
- [ ] **Tests**: Expand unit tests to cover `MidiManager` calculation logic using a mock `RtMidiIn`.
- [ ] **Graceful Shutdown**: Implement a signal handler (SIGINT) to close sockets and MIDI ports cleanly.
- [ ] **GUI/Tray**: Consider a small system tray app to show connection status and current BPM.
