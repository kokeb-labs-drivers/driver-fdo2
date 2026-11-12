# Configuration

Every `Config` field, its default and its bounds, and how to check a configuration before it reaches the module.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From PyroScience Datasheet_FDO2-G2 v5:

    - Measuring range, accuracy and warm-up: 2 Specifications, Analytical Performance, [p. 5](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=5)
    - Temperature and pressure limits, humidity: 2 Specifications, Environment, [p. 6](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=6)
    - Volume fraction from partial pressure: 2 Specifications, note on % O2 before Environment, [p. 6](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=6)
    - Status word bits: 4.3 Commands, #MOXY, the status word, [p. 12](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=12)
    - Measurement duration: 4.3 Commands, #SETM, modes, [p. 16](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=16)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The Config

`Config{}` holds the documented defaults. Pass a `Config` to the constructor; it is stored without I/O and
checked by `EnsureInitialized()`, which returns `InvalidArgument` for a value out of range.

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:custom"
```

| Field | Default | Bounds | Meaning |
| --- | --- | --- | --- |
| `family` | `Fdo2G2` | enum | `Fdo2` for the legacy module: no `#SETM` |
| `baud` | 19200 | one of the eleven rates | The rate the module runs at now |
| `power_up_ms` | 1500 | 1000 to 30000 | Wait after power before `#VERS` |
| `command_ms` | 100 | 10 to 5000 | Allowance of `#VERS #IDNR #RDUM #BAUD` |
| `measurement_ms` | 300 | 50 to 5000 | Allowance of `#MOXY` and `#MRAW` |
| `calibration_ms` | 10000 | 10000 to 60000 | Allowance of `#CALO` and `#CAHI` |
| `flash_ms` | 2000 | 100 to 30000 | Allowance of `#CRCE #SETM #BCST #WRUM` |
| `logo_ms` | 1100 | 100 to 10000 | Allowance of `#LOGO` |
| `margin_ms` | 50 | 0 to 1000 | Added to every deadline |
| `power_off_ms` | 500 | 10 to 10000 | Off time of `PowerCycle()` |
| `retries` | 1 | 0 to 3 | Extra attempts of an idempotent read |
| `crc` | `Auto` | `Off`, `Auto`, `Required` | Expectation for the checksum |
| `status_policy` | `Datasheet` | `Datasheet`, `Documented` | [Status policy](status-and-errors.md#status-policy) |
| `strict_spacing` | true | bool | One space between tokens |
| `ignore_lf` | true | bool | Drop a line feed at the start of a line |
| `erro_without_crc` | true | bool | Accept `#ERRO` without a checksum |
| `broadcast` | `Refuse` | `Refuse`, `Stream`, `Disable` | Start-up on a streaming module |
| `broadcast_listen_ms` | 0 | 0 to 12000 | Passive listen at start-up |
| `allow_persistent_writes` | false | bool | [Consent for flash writes](calibration.md#consent-and-the-write-budget) |
| `max_flash_writes` | 8 | 1 to 64 | Flash-writing calls per driver lifetime |
| `min_steady_s` | 300 | 0 to 3600 | Steady seconds a calibration needs |
| `cal_window_pct` | 20 | 1 to 100 | Percent the measured pO2 may differ from a calibration value |
| `cal_window_min_mhpa` | 5000 | 1000 to 50000 | Smallest window of that guard, in 0.001 hPa |
| `o2_min_mhpa`, `o2_max_mhpa` | -5000, 2100000 | ordered | Plausible oxygen, 0.001 hPa |
| `temp_min_mc`, `temp_max_mc` | -20000, 70000 | ordered | Plausible temperature, 0.001 degC |
| `pressure_min_ubar`, `pressure_max_ubar` | 250000, 1200000 | ordered | Plausible pressure, microbar |
| `humidity_max_mpct` | 100000 | positive | Plausible humidity, 0.001 %RH |
| `pressure_reference_ubar` | 0 | 0 or inside the pressure window | 0 uses the module's pressure |

The allowances are the datasheet's maxima plus room for the line; the datasheet is silent on some of them and the
build report lists which.

## Check a configuration

`Validate(config)` checks a `Config` without touching the module. `CheckedConfig` does it at compile time:

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:checked"
```

## Kconfig

Under ESP-IDF, `fdo2::KconfigDefaults()` builds a `Config` from `menuconfig`; everywhere else the same defaults
come from `Config{}`. Only `inc/fdo2_kconfig.hpp` reads the `CONFIG_` values.

```cpp title="configuration.cpp"
--8<-- "tests/docs/configuration.cpp:kconfig"
```
