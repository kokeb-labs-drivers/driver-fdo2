# Settings

The module's settings that the driver can change: the checksum, the baud rate, the broadcast stream, the
measurement mode and the user memory.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - CRC-16/MODBUS suffix: PyroScience Datasheet_FDO2-G2 v5, 4.2 Detecting communication errors and optional CRC, [p. 11](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=11)
    - #CRCE: the CRC switch: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #CRCE, [p. 14](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=14)
    - #RDUM and #WRUM: 64 user words: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #RDUM, #WRUM, [p. 14](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=14)
    - #BAUD: the eleven baud rates: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #BAUD, [p. 14](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=14)
    - #BCST: broadcast mode: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #BCST, [p. 15](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=15)
    - #SETM: measurement modes: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #SETM, [p. 15](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=15)
    - Errata: noise at two temperatures: PyroScience Datasheet_FDO2-G2 v5, 6 ERRATA, 6.1 Increased Noise at Specific Temperatures, [p. 19](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=19)
    - Errata: broadcast send and receive conflict (firmware 3.51): PyroScience Datasheet_FDO2 v21, 6.1 Firmware Bug Broadcast Send and Receive Conflict (FW 3.51), [p. 19](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=19)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Checksum

```cpp title="settings.cpp"
--8<-- "tests/docs/settings.cpp:crc"
```

`SetCrcEnabled()` sends `#CRCE`, a flash write, then a `#VERS` that must come back as the setting says. With
`Config::crc` set to `Auto`, the driver follows. See [Protocol](protocol.md#checksum).

## Baud rate

```cpp title="settings.cpp"
--8<-- "tests/docs/settings.cpp:baud"
```

`SetBaudRate()` needs the port's `SetBaudRate` hook; without it the call returns `Unsupported`. The driver sends
`#BAUD`, switches the port after two character times, and checks with a `#VERS`. The module returns to 19200 baud
at every power-up. If the module does not answer at the new rate, the state is `StateUnknown`;
`Resync()` tries the new rate, the old one and 19200. `GetConfig().baud` follows a successful change.

## Broadcast

The module can send `#MRAW` lines by itself at an interval of 100 ms to 10 s (`#BCST`).

```cpp title="settings.cpp"
--8<-- "tests/docs/settings.cpp:broadcast"
```

- `SetBroadcast(interval_ms)` is a flash write and puts the driver in `Streaming`. In that state every call that
  sends a command returns `Busy` until `DisableBroadcast()`.
- `DisableBroadcast()` waits for the end of a frame before it sends `#BCST 0`, as the vendor advises, because a
  command sent in the middle of a frame is lost.
- A stream left on from an earlier session is found at start-up only if a frame arrives while the driver waits. With
  `broadcast_listen_ms` at 0 this covers intervals up to about 250 ms. For a slower stream, set
  `broadcast_listen_ms` to at least the interval (up to 12000), or call `ProbeBroadcast()`.
- `Config::broadcast` decides what start-up does when it finds a stream: `Refuse` returns
  `BroadcastActive`, `Stream` keeps listening, `Disable` turns it off (a flash write, so it needs consent).

## Measurement mode

`SetMeasurementMode(mode)` sends `#SETM` with a mode of 0 to 4 (a flash write). The FDO2-G2 documents it; the
legacy FDO2's datasheet does not, so with `Config::family = Family::Fdo2` the driver does not try it. A module
that answers `#ERRO -26` teaches the driver to stop trying. Modes 0 to 2 answer within 150 ms and every mode
within 300 ms (`Config::measurement_ms`).

## User memory

```cpp title="settings.cpp"
--8<-- "tests/docs/settings.cpp:user-memory"
```

The module keeps 64 signed 32-bit words that survive power-off. `ReadUserMemory()` does not write flash.
`WriteUserMemory()` does: it needs consent and counts against the budget
([Calibration](calibration.md#consent-and-the-write-budget)).

## Power and pending flash writes

A power loss during a flash cycle can corrupt the module's flash (datasheet section 4.3, warning). From the moment
the driver sends a flash command until its echo arrives, and for the flash allowance after a lost echo,
`FlashWritePending()` is true. In that time:

- `Deinitialize()` returns `Busy`, because it switches the supply off through the port's power hook. Wait, or
  call `Resync()` first.
- `PowerCycle()` waits for the flash allowance before it cuts the supply.
- A failed `EnsureInitialized()` keeps the supply on.

Without the port's power hook the driver cannot cut the supply, and your own switch must follow the same rule.

## Resynchronise

`Resync()` recovers a line in an unknown state. It tries each baud rate (the configured one, the one of a lost
`#BAUD` and 19200) with and without the checksum, so a lost `#CRCE` echo is found whichever setting the module
kept. It is allowed while a stream runs (it ends the stream state), and it does not change the identity the driver
stored. `StreamSilentMs()` returns the time since the last stream line, so you can see a stream that has stopped.

## Identify

`Identify()` sends `#LOGO`, which makes the module flash its LED for about a second
(`Config::logo_ms`), so you can tell which module on a bench is the one you talk to.

Next: [Non-blocking use](non-blocking.md).
