# Status and errors

How the driver turns the module's status word and error replies into values you can act on.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Status word bits: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #MOXY, the status word, [p. 12](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=12)
    - #ERRO codes: PyroScience Datasheet_FDO2-G2 v5, 4.4 Error Codes, Table 1, [p. 17](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=17)
    - Errata: error handling of the internal temperature sensor: PyroScience Datasheet_FDO2 v21, 6.3 Error Handling of Internal Temperature Sensor, [p. 20](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=20)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The status word

Every `#MOXY` and `#MRAW` reply ends with a status word `S`. The datasheet says S should be 0 or 1. The
bits:

| Bit | Level | Meaning | `Status` method |
| --- | --- | --- | --- |
| 0 | warning | Detector amplification reduced; the reading is valid | `AmplificationReduced()` |
| 1 | fatal | Oxygen signal too low (below 20 mV) | `SignalTooLow()` |
| 2 | fatal | Oxygen signal or ambient light too high | `SignalTooHigh()` |
| 3 | fatal | Reference signal too low (below 20 mV) | `ReferenceTooLow()` |
| 4 | fatal | Reference signal or ambient light too high (above 2400 mV) | `ReferenceTooHigh()` |
| 5 | fatal | Temperature sensor failure | `TemperatureSensorFailed()` |
| 7 | warning | Humidity inside the housing above 90 %RH | `HumidityHigh()` |
| 9 | error | Pressure sensor failure; the oxygen value is not affected | `PressureSensorFailed()` |
| 10 | error | Humidity sensor failure; the oxygen value is not affected | `HumiditySensorFailed()` |

Bits 6, 8 and 11 to 31 are not defined; `UndefinedBits()` reports them. `Status::Classify()` gives a
`Quality`: `Good` (S is 0), `Warning` (S is 1), `Degraded` (bit 7, 9 or 10) or `Invalid` (a fatal or undefined bit).

## Status policy

`Config::status_policy` decides which statuses a call accepts:

| Policy | Accepts | Everything else |
| --- | --- | --- |
| `Datasheet` (default) | S of 0 or 1 | `SensorFault` |
| `Documented` | Also bits 7, 9 and 10, as a degraded reading | `SensorFault` |

Use `Documented` when you need an oxygen value from a module whose humidity or pressure sensor failed. The
bits stay in `Oxygen::status` either way.

```cpp title="status.cpp"
--8<-- "tests/docs/status.cpp:status"
```

## Errors

Every call returns `Result<T>` with an `Error`. `ToString(error)` gives its name.

| Error | When | What to do |
| --- | --- | --- |
| `Timeout` | No complete reply in time | Wiring, 3.0 V levels, supply, baud rate |
| `Crc` | The reply's checksum is wrong | Noise on the line; the driver already retried a read |
| `InvalidReply` | A line the grammar refuses | Check the baud rate and stray bytes |
| `SensorFault` | The status word is not acceptable | Read the bits above |
| `OutOfRange` | A value outside the plausibility window | Check `Config` bounds and the medium |
| `DeviceError` | `#ERRO` -21, -22 or -23 (and -1 on a read) | `DeviceErrorCode()` has the code; -21 to -23 are retried on reads |
| `DeviceFault` | `#ERRO` with a code the datasheet calls potentially fatal | `DeviceErrorCode()` has the code |
| `EchoMismatch` | The reply echoes other values than were sent | Stray bytes or a wrong baud rate |
| `DeviceIdMismatch` | `#VERS` is not an FDO2 with an oxygen sensor | Wrong device or wiring |
| `BroadcastActive` | The module streams and the call needs it silent | `Config::broadcast` policy |
| `Unsupported` | `#ERRO -26`, or a missing port hook | Older firmware, or a port without the hook |
| `CalibrationRejected` | `#ERRO -60` | The value is outside the range the module accepts |
| `NotPermitted` | A flash write without consent or past the budget | [Calibration](calibration.md) |
| `StateUnknown` | A flash write or baud change ended with no echo, or an unexpected `#ERRO` on a flash write | Call `Resync()` |
| `NotReady` | A calibration while the sensor warms up (3 minutes) | Wait and call again |
| `Busy` | An operation is pending, or a stream is running | Poll until it ends |
| `NotInitialized` | `EnsureInitialized()` has not succeeded | Call it first |
| `InvalidArgument` | An argument or `Config` field out of range | `Validate(config)` |
| `Bus` | A port hook returned false | Your port |

```cpp title="api_guide.cpp"
--8<-- "tests/docs/api_guide.cpp:errors"
```

## Counters

`GetCounters()` returns counters that only grow: transactions, retries, timeouts, CRC errors, invalid replies,
device errors, stale lines, broadcast lines, flash writes and bytes discarded. Log them to see a noisy line before
it fails.

Next: [Troubleshooting](troubleshooting.md).
