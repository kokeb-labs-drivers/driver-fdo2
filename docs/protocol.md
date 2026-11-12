# Protocol

The module's UART protocol as the driver speaks it: the line, the thirteen commands, the reply grammar and the
checksum.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - UART framing, echo and terminator: PyroScience Datasheet_FDO2-G2 v5, 4.1 General Definitions, [p. 10](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=10)
    - Numbers in a reply: PyroScience Datasheet_FDO2-G2 v5, 4.1 General Definitions, value formats, [p. 10](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=10)
    - CRC-16/MODBUS suffix: PyroScience Datasheet_FDO2-G2 v5, 4.2 Detecting communication errors and optional CRC, [p. 11](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=11)
    - #VERS: device, channels, firmware, sensors: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #VERS, [p. 11](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=11)
    - #IDNR: the unique id: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #IDNR, [p. 11](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=11)
    - #MOXY: oxygen, temperature, status: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #MOXY, [p. 11](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=11)
    - #MRAW: phase, signals, pressure, humidity: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #MRAW, [p. 12](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=12)
    - #LOGO: the status LED: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #LOGO, [p. 13](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=13)
    - #CRCE: the CRC switch: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #CRCE, [p. 14](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=14)
    - Firmware revision history of the G2: PyroScience Datasheet_FDO2-G2 v5, 5 Released Firmware Revisions Changes, [p. 18](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=18)
    - Firmware revision history: PyroScience Datasheet_FDO2 v21, 5 Released Firmware Revisions Changes, [p. 17](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=17)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The line

The module starts at 19200 baud, 8 data bits, no parity, 1 stop bit and no handshake after every power-up. Its
signals are 3.0 V; the RXD pin tolerates 3.3 V at most ([Hardware](hardware.md)). Every message is ASCII and ends
with one carriage return (CR, 0x0D). The driver ignores a line feed (LF) at the start of a line
(`Config::ignore_lf`).

A request is a `#`, a four-letter command and its arguments, separated by single spaces. The module answers with
an echo of the command and the values, ended by a CR:

```text
#MOXY <CR>                    request (no argument)
#MOXY 211717 21000 1 <CR>     reply: oxygen, temperature, status
```

With the checksum on, each reply ends with a colon, a space and a CRC-16 of the line
(see [Checksum](#checksum)).

## Commands

| Command | What it does | Writes flash | Repeated after a fault |
| --- | --- | --- | --- |
| `#VERS` | Device id, channels, firmware, sensors | no | yes |
| `#IDNR` | The 64-bit unique id (not the serial number) | no | yes |
| `#MOXY` | Oxygen partial pressure, temperature and the status word | no | yes |
| `#MRAW` | The same plus phase shift, signal, ambient light, pressure and humidity | no | yes |
| `#LOGO` | Flashes the module's LED so you can find it | no | yes |
| `#RDUM` | Reads user memory words | no | yes |
| `#WRUM` | Writes user memory words | yes | never |
| `#BAUD` | Changes the baud rate until the next power-up | no | never |
| `#CALO` | Calibrates the zero point | yes | never |
| `#CAHI` | Calibrates the high point | yes | never |
| `#CRCE` | Turns the checksum on or off | yes | never |
| `#BCST` | Sets the broadcast interval | yes | never |
| `#SETM` | Sets the measurement mode (FDO2-G2) | yes | never |

The driver sends a command that writes flash only when you opted in and the write budget has room
([Calibration](calibration.md#consent-and-the-write-budget)). A repeat of a write after an unclear failure could
write twice or hide a half-done change, so the driver never repeats one: it leaves the state as `StateUnknown`
and waits for `Resync()`.

## Replies and errors

A failed command answers `#ERRO` and a code:

| Code | Meaning | Driver error | Retried |
| --- | --- | --- | --- |
| -1 | General error | `DeviceError` | no |
| -21 | Command could not be parsed | `DeviceError` | yes |
| -22 | Receive error | `DeviceError` | yes |
| -23 | Header error | `DeviceError` | yes |
| -26 | Command not supported by this firmware | `Unsupported` | no |
| any other | A code the datasheet calls potentially fatal | `DeviceFault` | no |
| -60 | Calibration value rejected | `CalibrationRejected` | no |

`-26` also teaches the driver: the capability is switched off for the rest of the session
(`GetIdentity().caps`). The code itself stays in `DeviceErrorCode()`.

## Checksum

With `#CRCE 1` the module appends a colon, a space and the checksum to every reply. The checksum is the
CRC-16/MODBUS of the characters before the colon. `Config::crc` says what to expect: `Auto` learns it from
the first `#VERS` reply, `Off` never expects it, `Required` rejects a reply without it. An `#ERRO` line may
come without a checksum (`Config::erro_without_crc`). `SetCrcEnabled()` turns it on or off
([Settings](settings.md#checksum)). Firmware
older than 3.41 forgets the setting at a power cycle (`caps.crc_persistent`).

```text
#MOXY 203456 17892 0: 43291 <CR>     the checksum covers "#MOXY 203456 17892 0", 43291 in decimal
#VERS 8 1 357 15: 17525 <CR>         the checksum covers "#VERS 8 1 357 15"
```

Commands carry no checksum; only replies do. The checksum is written in decimal, without leading zeros, after one
colon and one space.

## Grammar the driver enforces

- One space between tokens (`Config::strict_spacing`).
- Numbers are decimal integers; the module sends no decimal point.
- A line that does not match the command that was asked is dropped and counted as stale
  (`Counters::stale_lines`).
- A line with an unknown header parses as `Ok` with `known=false`; the driver counts it as stale and keeps
  reading, and the fifth one in a request ends it with `InvalidReply`.
- A reply that is too long, too short or has the wrong token count is `InvalidReply`.

Next: [Status and errors](status-and-errors.md).
