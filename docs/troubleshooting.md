# Troubleshooting

What to check when a call fails, by the error it returns.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Errata: noise at two temperatures: PyroScience Datasheet_FDO2-G2 v5, 6 ERRATA, 6.1 Increased Noise at Specific Temperatures, [p. 19](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=19)
    - UART levels: 3.0 V, 3.3 V maximum on RXD: PyroScience Datasheet_FDO2 v21, 6.2 Voltage Tolerance of UART RX Pin (ERRATA), [p. 19](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=19)
    - Errata: broadcast send and receive conflict (firmware 3.51): PyroScience Datasheet_FDO2 v21, 6.1 Firmware Bug Broadcast Send and Receive Conflict (FW 3.51), [p. 19](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=19)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## EnsureInitialized returns Bus

A port hook reported a failure.

1. Check that your port's `EnsureInitialized()` returns `true` on its own (log it).
2. Check the UART pins in your port against the wiring.
3. Check that `Write()` returns `true` and sends all the bytes.

## EnsureInitialized returns Timeout

The module did not answer `#VERS`.

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| Nothing on TXD | No power, or below 3.3 V | Check VCC (3.3 V to 5.0 V) and GND |
| Nothing on TXD | TX and RX swapped | Cross them: MCU TX to RXD, MCU RX to TXD |
| Garbage on TXD | The module is at another baud rate | Power-cycle it: it starts at 19200 |
| The module streams lines | A broadcast is on | See below |
| Answers only after a wait | A short `power_up_ms` | Raise `Config::power_up_ms` |

## EnsureInitialized returns DeviceIdMismatch

`#VERS` answered, but not as an FDO2 with an oxygen sensor: the device id is not 8 or the sensor bits lack oxygen.
Check that the line is not shared with another device.

## EnsureInitialized returns BroadcastActive

The module streams `#MRAW` lines (`#BCST` is on) and `Config::broadcast` is `Refuse`. Set it to `Stream` to
read the frames, or to `Disable` to turn the stream off (a flash write: it needs
`allow_persistent_writes`). A slow stream is seen only when `broadcast_listen_ms` is at least its interval
([Settings](settings.md#broadcast)).

## Calls return Crc now and then

Noise on the line. Shorten the wires, keep them away from the supply and check the ground. The driver already
retried a read; `GetCounters().crc_errors` shows how often it happens.

## Calls return InvalidReply or EchoMismatch

Bytes that are not part of the reply, or a wrong baud rate. Check that `FlushInput()` really flushes, that the
receive buffer holds 800 bytes and that nothing else writes to the line.

## Calls return SensorFault

The status word is not acceptable under the status policy. [Status and errors](status-and-errors.md) lists the
bits. With the sensor in air and a signal too low, check that the membrane is clean and uncovered.

## Calls return StateUnknown

A flash write or a baud change ended with no echo. Call `Resync()`: it tries the configured rate, the new rate and
19200, each with and without the checksum, and reads `#VERS`. Then read the setting back before you write it again.

## Calls return NotPermitted

A flash write needs `allow_persistent_writes` and room in `max_flash_writes`
([Calibration](calibration.md#consent-and-the-write-budget)).

## Calls return Unsupported

The firmware answered `#ERRO -26` (it does not know the command), or your port has no hook the call needs
(`SetBaudRate`). `GetIdentity().caps` shows what the firmware offers.

## Readings look wrong

- Within the first three minutes after power-up the accuracy is reduced (`warming_up`).
- A supply below 3.3 V can make the readings wrong; the module has no supply reading.
- A calibration at the wrong pressure, humidity or temperature moves every later reading
  ([Calibration](calibration.md)).
