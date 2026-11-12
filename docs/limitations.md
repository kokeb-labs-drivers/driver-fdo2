# Limitations

What the driver does not do, what it cannot verify and where the documentation has a known gap.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Cross-sensitivity and chemical compatibility: PyroScience Datasheet_FDO2-G2 v5, 3 Cross-Sensitivity and Chemical Compatibility, [p. 8](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=8)
    - Flash endurance and interrupted writes: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, WARNING, [p. 16](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=16)
    - Firmware revision history of the G2: PyroScience Datasheet_FDO2-G2 v5, 5 Released Firmware Revisions Changes, [p. 18](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=18)
    - Errata: noise at two temperatures: PyroScience Datasheet_FDO2-G2 v5, 6 ERRATA, 6.1 Increased Noise at Specific Temperatures, [p. 19](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=19)
    - Warnings: no medical or safety use: PyroScience Datasheet_FDO2-G2 v5, 8 Warnings, [p. 21](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=21)
    - Supply below 3.3 V and readings: PyroScience Datasheet_FDO2 v21, 2 Specifications, Interface, WARNING, [p. 6](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=6)
    - Firmware revision history: PyroScience Datasheet_FDO2 v21, 5 Released Firmware Revisions Changes, [p. 17](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=17)
    - Errata: broadcast send and receive conflict (firmware 3.51): PyroScience Datasheet_FDO2 v21, 6.1 Firmware Bug Broadcast Send and Receive Conflict (FW 3.51), [p. 19](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=19)
    - Errata: error handling of the internal temperature sensor: PyroScience Datasheet_FDO2 v21, 6.3 Error Handling of Internal Temperature Sensor, [p. 20](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=20)
    - Low-pass filter change and the noise erratum: PyroScience PCN-FDO2-G2-001 n/a, Product change notification, text, [p. 1](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Product%20Change%20Notifications/PCN-FDO2-G2-001.pdf&cid=25762#page=1)
    - Replacement of the FDO2 by the FDO2-G2: PyroScience PCN-FDO2-002 n/a, Product change notification, text, [p. 1](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Product%20Change%20Notifications/PCN-FDO2-002.pdf&cid=24463#page=1)
    - Modbus extension module (a different interface): PyroScience Datasheet_FDO2-EXT v5, 1 INTRODUCTION, [p. 1](https://www.pyroscience.com/en/products/oem-solutions/modbus-interface-adapter/fdo2-ext?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-EXT.pdf&cid=19827#page=1)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## What the driver does not do

- **No compensation inputs.** The module's firmware has no command to set salinity, pressure or temperature
  for a reading. The driver reports what the module reports. `MeasureVolumePercentMilli()` computes a fraction
  from the module's own pressure; it is a dry-gas fraction and does not model humidity. It is right only when the
  venting capillary at the back of the housing sees the same pressure as the membrane; otherwise set
  `pressure_reference_ubar`.
- **No supply check.** Below 3.3 V the readings can be wrong (datasheet section 2). The module has no supply
  reading, so the driver cannot warn you.
- **No wiring check.** Over-voltage on RXD is not detectable from software.
- **Late replies.** A reply that arrives after the deadline is drained before the next request, but a module that
  answers later than the allowance plus the margin is reported as `Timeout`. Raise `Config::command_ms` or the
  matching allowance if your module is slower.
- **One module per line.** The protocol has no addressing. Use one UART per module.
- **No unit conversion beyond what the module gives.** Values are integers: 0.001 hPa, 0.001 degC, microvolts,
  microbar. `inc/fdo2_float.hpp` does not exist; convert in your own code.

## Not verified on hardware

No module has been connected to this driver. The behaviour comes from the datasheets and from a model of the
module that the tests use. The model is written from the datasheets and not from the driver, but both can share a
misreading. Every behaviour the datasheets leave open is a setting in `Config` or a documented choice; the
[Calibration](calibration.md), [Settings](settings.md) and [Status](status-and-errors.md) pages say which. The
bench steps that confirm them are listed in the build report.

## The module figures

The module is round with a four-pin connector, and the chip-diagram renderer draws packages with pins on the sides
of a rectangle. So the pinout figure puts the four contacts in one row, as the datasheet draws them, and the
outline figure draws a square of the module's diameter. A module view with a round body is a follow-up for the
shared tooling.

## Firmware differences

The driver covers firmware 3.28 to 3.57. `GetIdentity().caps` says what the connected firmware offers: user memory,
broadcast, a stored checksum setting, calibration errors, `#SETM` and a reliable temperature flag. Firmware
affected by the low-temperature noise erratum (8 to 15 degC) is flagged in `Identity::noise_erratum`; the driver
does not change readings for it.
