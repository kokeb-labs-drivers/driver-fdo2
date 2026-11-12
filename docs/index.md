---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# FDO2 Driver

<p>A C++20 driver for the PyroScience FDO2-G2 and the legacy FDO2 optical oxygen modules on UART. It starts the
module, reads oxygen and temperature, checks the status word and guards the commands that write flash, on any
MCU.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-fdo2){ .md-button }
</div>

The driver has no platform code. You give it a small class that moves bytes on the UART; it does the rest: the
power-up wait, the command lines, the checksum, the status policy, retries and timeouts.

<div class="kb-chip" markdown>
<div class="kb-chip__head">
<span class="kb-chip__kicker">Optical oxygen sensor module</span>
<span class="kb-chip__name">FDO2-G2 and FDO2</span>
<span class="kb-chip__vendor">PyroScience</span>
</div>

Function
:   Oxygen partial pressure, temperature, pressure and humidity inside the housing

Interface
:   UART, 19200 baud, 8N1 at power-up, 3.0 V levels (3.3 V maximum on RXD); ASCII commands ended by a carriage return

Integrity
:   Optional CRC-16 on every reply; a status word on every measurement

Supply
:   3.3 V to 5.0 V

Package
:   Cylindrical module, 28.5 mm diameter, four-pin connector
</div>


## What you get

- **One object per module.** \`FDO2<YourPort>\` takes a reference to your port and does no I/O until
  \`EnsureInitialized()\`. See [API guide](api-guide.md).
- **Errors you can act on.** Every call returns \`Result<T>\` with an \`Error\` such as \`Timeout\`, \`SensorFault\` or
  \`NotPermitted\`. See [Status and errors](status-and-errors.md).
- **Flash writes need your consent.** Calibration and settings write the module's flash; the driver sends them only
  with an opt-in and inside a budget. See [Calibration](calibration.md).
- **Datasheet defaults.** \`Config{}\` follows the datasheets, with units in the field names. See
  [Configuration](configuration.md).
- **Blocking or not.** Every operation has a blocking call and a start call with \`Poll()\`. See
  [Non-blocking use](non-blocking.md).
- **Zero-overhead portability.** The port is a CRTP class checked by a concept. See [Porting](porting.md).
- **No heap, no exceptions, no RTTI.** See [Contract](contract.md).
- **Tested without hardware.** The host tests run the driver against a model of the module written from the
  datasheets; it has not been run on a real module ([Limitations](limitations.md)).

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: FDO2-G2 Optical Oxygen Sensor Data Sheet (PyroScience Datasheet_FDO2-G2) | v5, 2026-03 | [PDF](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457) · [vendor page](https://www.pyroscience.com/en/products/all-meters/fdo2-g2) |
| Datasheet: FDO2 Optical Oxygen Sensor Data Sheet (legacy part) (PyroScience Datasheet_FDO2) | v21, 2025-06 | [PDF](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471) · [vendor page](https://www.pyroscience.com/en/downloads/legacy) |
| Product change notification: Low-pass filter and noise erratum (PyroScience PCN-FDO2-G2-001) | n/a, 2026-03 | [PDF](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Product%20Change%20Notifications/PCN-FDO2-G2-001.pdf&cid=25762) · [vendor page](https://www.pyroscience.com/en/products/all-meters/fdo2-g2) |
| Product change notification: FDO2 replaced by FDO2-G2 (PyroScience PCN-FDO2-002) | n/a, 2025-05 | [PDF](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Product%20Change%20Notifications/PCN-FDO2-002.pdf&cid=24463) · [vendor page](https://www.pyroscience.com/en/products/all-meters/fdo2-g2) |
| Quick start guide: Quick Start FDO2-G2 evaluation kit (PyroScience Quick_Start_FDO2-G2) | v1.00, n/a | [PDF](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Quick_Start_FDO2-G2.pdf&cid=24459) · [vendor page](https://www.pyroscience.com/en/products/all-meters/fdo2-g2) |
| Datasheet: FDO2-FTC flow-through cell data sheet (accessory) (PyroScience Data Sheet_FDO2_FTC) | n/a, n/a | [PDF](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Data%20Sheet_FDO2_FTC.PDF&cid=24458) · [vendor page](https://www.pyroscience.com/en/products/all-meters/fdo2-g2) |
| Datasheet: FDO2-EXT Modbus and analog extension module data sheet (PyroScience Datasheet_FDO2-EXT) | v5, 2025-07 | [PDF](https://www.pyroscience.com/en/products/oem-solutions/modbus-interface-adapter/fdo2-ext?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-EXT.pdf&cid=19827) · [vendor page](https://www.pyroscience.com/en/products/oem-solutions/modbus-interface-adapter/fdo2-ext) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the module, write the port, read the oxygen in about 15 lines.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Connector, levels, supply and the 5 V circuit.

-   :material-file-document-check-outline: **[Contract](contract.md)**

    ---

    What the driver promises, what it needs and threading.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Start, measure, read the identity and handle errors, task by task.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the port for your UART.

-   :material-book-open-variant: **[API reference](api-reference.md)**

    ---

    Every class and member, generated from the headers.

</div>
