# FDO2 ESP32 examples

ESP-IDF applications that run the FDO2 driver on an ESP32, built by CI on every pull request.

## Wiring

| ESP32 | FDO2 | Notes |
| --- | --- | --- |
| GPIO4 (TX) | RXD (pin 3) | UART1, 19200 8N1; RXD tolerates 3.3 V at most |
| GPIO5 (RX) | TXD (pin 2) | The module's output is 3.0 V |
| 3V3 or 5 V | VCC (pin 4) | 3.3 V to 5.0 V; below 3.3 V the readings can be wrong |
| GND | GND (pin 1) | |

The pins are in `Esp32Fdo2Pins` in [`main/esp32_fdo2_port.hpp`](main/esp32_fdo2_port.hpp). The optional supply
switch (`power`) is off by default (-1). The mating plug is the vendor's to name: see the datasheet, section 7.

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh fdo2_basic_example Debug
./scripts/flash_app.sh flash_monitor fdo2_basic_example Debug
```

`idf.py menuconfig` shows "FDO2 oxygen sensor": the family, the status policy, the power-up wait, the measurement
allowance, retries, the CRC and broadcast policies and the flash-write consent that `fdo2::KconfigDefaults()`
returns.

## Expected output

```text
I (312) fdo2_example: FDO2 driver 0.1.0
I (1830) fdo2_example: initialised, firmware 357
I (2140) fdo2_example: pO2 209.500 hPa, 23.450 degC
```

These numbers are an example of the format; they are not from a measurement. An `EnsureInitialized failed: Timeout`
line means the module did not answer `#VERS`: check the wiring, the supply and the 3.0 V levels.

Not verified on hardware: these applications are built in CI and not run against a module.

## Files

| File | What it is |
| --- | --- |
| [`main/fdo2_basic_example.cpp`](main/fdo2_basic_example.cpp) | Blocking calls, once a second |
| [`main/fdo2_nonblocking_example.cpp`](main/fdo2_nonblocking_example.cpp) | `Start...()` and `Poll()` |
| [`main/fdo2_cxx20_check.cpp`](main/fdo2_cxx20_check.cpp) | The public headers as ISO C++20 |
| [`main/esp32_fdo2_port.hpp`](main/esp32_fdo2_port.hpp) | The port: UART, clock, delay, baud rate, supply switch, log |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; the repository root is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
