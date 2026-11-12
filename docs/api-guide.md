# API guide

The driver's calls by task: start, measure, read the identity and handle errors.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From PyroScience Datasheet_FDO2-G2 v5:

    - Volume fraction from partial pressure: 2 Specifications, note on % O2 before Environment, [p. 6](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=6)
    - #VERS: device, channels, firmware, sensors: 4.3 Commands, #VERS, [p. 11](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=11)
    - #IDNR: the unique id: 4.3 Commands, #IDNR, [p. 11](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=11)
    - #MOXY: oxygen, temperature, status: 4.3 Commands, #MOXY, [p. 11](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=11)
    - #MRAW: phase, signals, pressure, humidity: 4.3 Commands, #MRAW, [p. 12](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=12)
    - #LOGO: the status LED: 4.3 Commands, #LOGO, [p. 13](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=13)
    - #RDUM and #WRUM: 64 user words: 4.3 Commands, #RDUM, #WRUM, [p. 14](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=14)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Start

`FDO2<YourPort> sensor(port)` stores the port and the `Config` and does no I/O. `EnsureInitialized()` is the
only start-up call: it powers the module if your port has the supply hook, waits for the power-up, reads `#VERS` and
`#IDNR`, checks the device and learns what the firmware offers. Every other I/O call returns `NotInitialized`
before it has succeeded. `Deinitialize()` goes back. `Resync()` recovers a line in an unknown state and
`PowerCycle()` restarts the module.

## Measure oxygen

`MeasureOxygen()` returns an `Oxygen`: `o2_mhpa` (oxygen partial pressure in 0.001 hPa), `temperature_mc` (0.001
degC), the `Status`, its `Quality` and `warming_up`. The call fails with `SensorFault` when the status word is not
acceptable under the [status policy](status-and-errors.md#status-policy), and with `OutOfRange` when oxygen or
temperature is outside the windows in `Config`.

```cpp title="quickstart.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

## Oxygen as a fraction

```cpp title="api_guide.cpp"
--8<-- "tests/docs/api_guide.cpp:volume"
```

`MeasureVolumePercentMilli()` returns the oxygen fraction in 0.001 percent: the partial pressure divided by the
total pressure, from `#MRAW`. It needs a good status and a pressure inside
`Config::pressure_min_ubar`..`pressure_max_ubar`. Set `pressure_reference_ubar` to use your own pressure instead of
the module's. It is a dry-gas fraction.

!!! warning "Venting capillary"
    The module's pressure sensor measures through the venting capillary at the back of the housing. The fraction is
    right only when the pressure at the capillary equals the pressure at the sensing membrane (datasheet section 2).
    In a sealed or pressurised vessel with the capillary outside, they differ: set `pressure_reference_ubar` to the
    pressure at the membrane, or use the oxygen partial pressure itself.

## Raw values

```cpp title="api_guide.cpp"
--8<-- "tests/docs/api_guide.cpp:raw"
```

`MeasureRaw()` adds the phase shift (`dphi_mdeg`), the signal and ambient light (microvolts), the pressure
(microbar) and the humidity (0.001 %RH). The pressure is not a gate of this call; it is checked when you ask for a
fraction or a calibration.

## Identity

```cpp title="api_guide.cpp"
--8<-- "tests/docs/api_guide.cpp:identity"
```

`GetIdentity()` returns what start-up read: `Version` (device id, channels, firmware, sensors), the unique id and
`Capabilities`. `ReadVersion()`, `ReadUniqueId()` and `Identify()` ask the module again.

## Handle errors

```cpp title="api_guide.cpp"
--8<-- "tests/docs/api_guide.cpp:errors"
```

Every call returns a `Result<T>` (`value` and `error`); check `ok()` before you use the value. The full list is on
[Status and errors](status-and-errors.md). Reads are repeated up to `Config::retries` times when the failure may be
transient; writes to flash never are.

## Next

- [Calibration](calibration.md), [Settings](settings.md), [Non-blocking use](non-blocking.md).
- [API reference](api-reference.md): every class and member.
