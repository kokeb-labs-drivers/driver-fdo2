# Calibration

How to calibrate the module in air or with two points, and why every calibration call needs your consent first.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From PyroScience Datasheet_FDO2-G2 v5:

    - #CALO: zero calibration: 4.3 Commands, #CALO, [p. 14](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=14)
    - #CAHI: high calibration: 4.3 Commands, #CAHI, [p. 15](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=15)
    - Flash endurance and interrupted writes: 4.3 Commands, WARNING, [p. 16](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=16)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Consent and the write budget

`#CALO` and `#CAHI` write the module's flash, and so do `#CRCE`, `#WRUM`, `#BCST` and `#SETM`. The flash
survives only a limited number of writes (the module's own limit, about 20000 cycles in all). So the driver sends
these commands only when you ask for it in the `Config`:

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:consent"
```

- `allow_persistent_writes` is false by default. A flash-writing call returns `NotPermitted` and sends nothing.
- `max_flash_writes` (1 to 64, default 8) is the number of such calls the driver sends in its lifetime.
  `FlashWritesUsed()` shows the count. A write counts when it is sent, even if the port fails, because the module may
  have received it.
- A flash write is never repeated. If the reply is lost, the call returns the original error (for example
  `Timeout`) and the state becomes `StateUnknown`; call `Resync()` and read the module back before you try again.

## Vendor warnings

The datasheet (section 4.3, Commands, and the warning that follows it) says, and the driver follows:

- Flash endurance is about 20,000 flash cycles in all, triggered by `#CALO`, `#CAHI`, `#CRCE`, `#SETM` and `#BCST`
  (and by `#WRUM`, which the datasheet describes as one cycle per call). Use them only when needed.
  `max_flash_writes` is a guard against a loop in your code, not the vendor figure: its limit of 64 per driver
  lifetime is far below 20,000.
- A power loss during a flash cycle can corrupt the flash. Keep the supply stable while one runs. The driver
  refuses `Deinitialize()` with `Busy` while a write is pending, and `PowerCycle()` waits for it to end
  ([Settings](settings.md#power-and-pending-flash-writes)).
- Always evaluate the reply. A write without its echo may have corrupted the flash: the driver reports the
  original error and the state `StateUnknown`.
- `#CALO` and `#CAHI` replace the factory calibration. The datasheet asks for at least 5 minutes of steady
  measuring in the reference medium before the command, and the command takes up to 10 seconds.

## The guard before every flash write

A wrong calibration is stored in the module for good, so the driver checks before it sends `#CALO` or `#CAHI`:

1. It reads `#VERS` and compares it with the identity found at start-up. A different module on the line, or a
   module that has reset without the driver seeing it (a power glitch, for example), ends in `StateUnknown` or
   `DeviceIdMismatch` and nothing is written.
2. It reads `#MOXY`. The status must be acceptable, and the sensor must have been powered for the warm-up time of
   3 minutes (datasheet section 2): earlier the call returns `NotReady`. The measured oxygen partial pressure must
   lie within `Config::cal_window_pct` percent (default 20, at least `cal_window_min_mhpa`, default 5000
   milli-hPa) of the value you are about to store. A sensor still in air cannot be calibrated as nitrogen, and the
   reverse: the call returns `OutOfRange` and nothing is written.
3. Only then does it send the write, and it counts it.

The guard reads `#MOXY` but not `#VERS` before a calibration, and its warm-up test counts the time since the
driver powered the module, using the host's clock (the datasheet gives no uptime value to read). A module that was
swapped or reset after start-up is therefore seen only through the plausibility window and the status word.

The other flash commands (`#CRCE`, `#WRUM`, `#BCST`, `#SETM`) get the identity check (1). The one exception is
`#BCST 0` while the module streams, which is sent as it is because no reply can be read in a stream.
`CalibrateHigh()` accepts 10 to 2000 hPa (10000 to 2000000 in 0.001 hPa): a standard outside the range of a
real calibration gas returns `InvalidArgument`.

## Before you calibrate

- Keep the sensor in the reference medium until the readings are steady. The datasheet asks for five minutes.
  `steady_s` is the time you have waited; a value below `Config::min_steady_s` (default 300) returns
  `InvalidArgument` and sends nothing.
- The status must be good. `CalibrateAir()` returns `SensorFault` otherwise.
- The sensor must be warmed up (3 minutes after power-up), or the call returns `NotReady`.
- Calibration changes the module's stored calibration. Not verified on hardware: nobody has run these calls
  against a module yet.

## One point in air

```cpp title="calibration.cpp"
--8<-- "tests/docs/calibration.cpp:air"
```

`CalibrateAir()` reads `#MRAW` for the pressure, computes the oxygen partial pressure of dry air at that
pressure (20.95 percent by default, `kDryAirO2Ppm`) and sends `#CAHI`. It is correct only when the sensor sees
dry air at the pressure of the module's own sensor, which sees the air through the venting capillary at the back of
the housing (datasheet section 2): keep the capillary open to the same air as the membrane. Humid air has a lower
oxygen partial pressure; the datasheet does not give a correction, so the driver applies none. You can see the
value before anything is written:

```cpp title="calibration.cpp"
--8<-- "tests/docs/calibration.cpp:air-value"
```

## Two points

```cpp title="calibration.cpp"
--8<-- "tests/docs/calibration.cpp:two-point"
```

`CalibrateZero()` sends `#CALO` with the sensor in an oxygen-free medium. `CalibrateHigh()` sends `#CAHI` with
the oxygen partial pressure of your standard in 0.001 hPa. A call allows `Config::calibration_ms` (10 s by default)
for the reply.

## Errors

| Error | Meaning |
| --- | --- |
| `NotPermitted` | No consent, or the write budget is used |
| `InvalidArgument` | `steady_s` too short, or the value is out of range (10 to 2000 hPa) |
| `NotReady` | The sensor is still warming up (3 minutes after power-up) |
| `OutOfRange` | The measured value is far from the calibration value (the guard) |
| `DeviceIdMismatch` | The module on the line is not the one found at start-up (the guard) |
| `CalibrationRejected` | `#ERRO -60`: the module judged the value outside its expected range (firmware 3.51 and later) |
| `SensorFault` | The status word was not good (`CalibrateAir()`) |
| `StateUnknown` | The reply was lost; `Resync()` |

Next: [Settings](settings.md).
