# Quickstart

Run the driver on your PC first, with no hardware; then read the oxygen partial pressure from your MCU: wire the
module, write a small port class, call two driver functions.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - Power-up time: PyroScience Datasheet_FDO2-G2 v5, 4 Communication Interface, [p. 10](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=10)
    - Connector, pins and signal levels: PyroScience Datasheet_FDO2-G2 v5, 7 Mechanical Dimensions and Electrical Interface, drawing and pin table, [p. 20](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=20)
    - Start-up flashes of the status LED: PyroScience Quick_Start_FDO2-G2 v1.00, Quick start, connecting the module, [p. 2](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Quick_Start_FDO2-G2.pdf&cid=24459#page=2)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Try it on your PC

[`examples/host/`](../examples/host/README.md) runs the real driver against a simulated module, through a host
port. From the repository root, with CMake and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/fdo2_host_example
```

```text
FDO2 driver on the host
fdo2: initialised
module: device 8, firmware 3.57, sensors 0xF
oxygen 211.717 hPa
temperature 21.000 degC
oxygen fraction 20.900 %
```

The full output is in [`expected_output.txt`](../examples/host/expected_output.txt); CI runs the example and compares
it. The numbers come from the simulated module, not from a measurement.

## 1. Wire it

| MCU | FDO2 | Notes |
| --- | --- | --- |
| UART TX | RXD (pin 3) | 19200 baud, 8N1; 3.3 V at most |
| UART RX | TXD (pin 2) | The module's output is 3.0 V |
| 3.3 V to 5 V | VCC (pin 4) | Below 3.3 V the readings can be wrong |
| GND | GND (pin 1) | |

The [Hardware](hardware.md) page has the pin figure and the 5 V circuit.

## 2. Add the driver

Add the repository as a submodule and link the `fdo2::fdo2` CMake target. [Installation](installation.md) covers
CMake on any MCU, the installed package and ESP-IDF.

```bash
git submodule add https://github.com/kokeb-labs-drivers/driver-fdo2.git external/driver-fdo2
```

## 3. Write the port

The port is the only platform code: it brings the UART up and moves bytes. Replace the `platform_*` calls with
your SDK's.

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

[Porting](porting.md) explains each method and the optional hooks.

## 4. Read the oxygen

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. `EnsureInitialized()` calls your port's `EnsureInitialized()`, waits for the module's power-up, reads `#VERS`
   and `#IDNR` and checks that the device is an FDO2 with an oxygen sensor.
2. `MeasureOxygen()` sends `#MOXY` and checks the status word and the plausibility windows.
3. The values are integers: oxygen in 0.001 hPa, temperature in 0.001 degC.

This example is built by the host CI, so it compiles against the current headers.

## If it does not work

| Symptom | First thing to check |
| --- | --- |
| `EnsureInitialized()` returns `Bus` | Your port's `EnsureInitialized()`: the UART did not come up |
| `EnsureInitialized()` returns `Timeout` | Supply, wiring, the TX and RX swap, the baud rate |
| `EnsureInitialized()` returns `DeviceIdMismatch` | The device is not an FDO2 ([Troubleshooting](troubleshooting.md)) |
| `MeasureOxygen()` returns `SensorFault` | The status word: [Status and errors](status-and-errors.md) |

## Next

- [API guide](api-guide.md): the measurements, identity, errors.
- [Configuration](configuration.md): every `Config` field.
- [Examples](examples.md): the host and ESP32 examples.
