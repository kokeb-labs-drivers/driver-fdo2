# Contract

What the FDO2 driver promises, what it needs and the rules a caller follows.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From PyroScience Datasheet_FDO2-G2 v5:

    - Flash endurance and interrupted writes: 4.3 Commands, WARNING, [p. 16](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=16)
    - #ERRO codes: 4.4 Error Codes, Table 1, [p. 17](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=17)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver inherits the [Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it. A change to anything here is a breaking change, listed under
**Breaking** in the [changelog](changelog.md).

## Promises

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail.
- `EnsureInitialized()` is the only way to start the driver. It checks the configuration (`InvalidArgument`,
  nothing sent), brings the UART up, waits for the power-up, reads `#VERS` and `#IDNR` and checks the device. On
  any failure the driver stays uninitialised and the next call starts again; once it has succeeded, later calls
  return `Ok` with no I/O.
- Every call that touches the module returns `NotInitialized` before `EnsureInitialized()` has succeeded.
- A command that writes flash is sent only with `Config::allow_persistent_writes` and inside
  `Config::max_flash_writes`; it is never repeated. A write that ends without an echo leaves the state
  `StateUnknown` until `Resync()` or a power cycle.
- Before every flash write the driver re-reads `#VERS` and checks the identity; before `#CALO` and `#CAHI` it also
  reads `#MOXY` and refuses a warming-up or implausible reading ([Calibration](calibration.md)).
- `Deinitialize()` returns `Busy` while a flash write is pending, and a failed `EnsureInitialized()` or an aborted
  one leaves the driver uninitialised, so the next call identifies the module again.
- Reads are repeated only for transient failures, at most `Config::retries` times.
- Every port failure propagates as `Bus`; no getter returns a stale or zero value after a failed read.
- Every wait is bounded by the allowance of its command plus the line time plus `Config::margin_ms`.
- `Config{}` follows the datasheets; `StatusPolicy::Datasheet` is the default.
- A missing `DelayUs()` is a compile error for the blocking calls; a missing `NowUs()` selects `Poll(elapsed_us)`;
  a missing `SetBaudRate()` gives `Unsupported`; a missing `Log()` logs nothing.

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `fdo2_version.h` |
| Your port | | The UART within the bus requirements, a delay, and optionally a clock, the baud rate, the supply switch and logging ([Porting](porting.md)) |

Nothing else: no RTOS, no SDK, no other repository of the organisation.

## Threading

The driver is not thread-safe and takes no lock. Use each driver object from one task at a time, or guard every
call on it with one mutex. Nothing may run in an interrupt handler: the blocking calls wait. Separate objects
share no state; each needs its own UART, because the protocol has no addressing.

## Errors

`fdo2::Error` inside `fdo2::Result<T>`. Values 0 to 9 are the shared base set; the chip-specific values start at
16. The full list, with what to do about each, is on [Status and errors](status-and-errors.md#errors).

## Memory

No heap, no exceptions, no RTTI, no `std::function`, no `std::string`, no `printf` and no static mutable state. A
driver object holds a reference to the port, the `Config`, the identity, a line buffer of 800 characters and a
64-byte receive window: about one kilobyte, mostly the line buffer.

## Platform

Platform-free: the same code runs on any target with a C++20 compiler, including the host tests. The ESP-IDF port in
[`examples/esp32`](examples.md) is an example, not part of the driver.

## Not verified on hardware

See [Limitations](limitations.md).

## Deviations from the standard

None.
