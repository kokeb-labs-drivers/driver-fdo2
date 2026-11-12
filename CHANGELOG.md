# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Added

- First version: a C++20 driver for the PyroScience FDO2-G2 and the legacy FDO2 optical oxygen modules on UART.
  - Start-up, `#MOXY` and `#MRAW` measurements, the oxygen fraction from the module's pressure, identity and
    capabilities by firmware revision.
  - A status policy (`Datasheet` by default, `Documented`) and plausibility windows.
  - Calibration (zero, high, air), the checksum, the baud rate, the broadcast stream, the measurement mode and the
    user memory. Every command that writes flash needs `allow_persistent_writes` and is counted against
    `max_flash_writes`; none is ever repeated.
  - Blocking calls and start calls with `Poll()`, with or without a clock in the port.
  - A host example and ESP-IDF examples (blocking and non-blocking).
- Flash-write guard: every flash command is preceded by a `#VERS` identity check, and `#CALO` and `#CAHI` by a
  `#MOXY` reading that must be good, warmed up and near the value to store (`Config::cal_window_pct`,
  `cal_window_min_mhpa`, `Error::NotReady`). `FlashWritePending()` and `StreamSilentMs()` observers.
- `Resync()` tries both checksum settings at each rate.
- A host test of the ESP32 example port against stubs of the ESP-IDF headers.
- Not verified on hardware.

### Changed

- `CalibrateHigh()` accepts 10 to 2000 hPa.
- `Deinitialize()` returns `Busy` while a flash write is pending; `PowerCycle()` waits for it; an aborted or failed
  `EnsureInitialized()` leaves the driver uninitialised so the next call identifies the module again.
- An unexpected `#ERRO` on a flash write leaves the state `StateUnknown`.
- The reference circuit for a 5 V controller uses a 10 kOhm and 12 kOhm divider (3.0 V at 5.5 V on RXD, below the
  3.3 V maximum).

### Fixed

- `#MOXY` is documented without an argument (the examples showed `#MOXY 1`).
- Getters no longer return the value of an earlier call after a failed one; `Resync()` no longer overwrites the stored
  identity and works in `StateUnknown` while streaming.
- `EncodeCommand()` and `FindCommand()` no longer read outside their tables.
- The ESP32 example runs with the default power pin -1.
- The ESP32 examples list the `esp_timer` component they include.
- Characters are no longer used as numbers in the protocol layer (MISRA C++:2023 Rule 7.0.3).
