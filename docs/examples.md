# Examples

One folder per platform under `examples/`, each with its own port, built by CI on every pull request. The driver
itself has no platform code; [Platforms](platforms.md) lists what exists.

## Host

[`examples/host/`](../examples/host/README.md) runs the driver on any PC against a simulated module, through the
host port [`host_fdo2_port.hpp`](../examples/host/host_fdo2_port.hpp). No hardware:

```bash
cmake -S examples/host -B build-example && cmake --build build-example && ./build-example/fdo2_host_example
```

The host tests build it with warnings as errors and run it (CTest `fdo2_host_example`).

## ESP32

ESP-IDF applications that run the driver on an ESP32; the wiring is in the
[example folder's README](../examples/esp32/README.md) and the port in
[`esp32_fdo2_port.hpp`](../examples/esp32/main/esp32_fdo2_port.hpp). The project points
`EXTRA_COMPONENT_DIRS` at the repository root, which is the driver's ESP-IDF component.

### The apps

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `fdo2_basic_example` :material-star:{ title="featured" } | Start the FDO2 from menuconfig values and log pO2 and temperature once a second | [`fdo2_basic_example.cpp`](../examples/esp32/main/fdo2_basic_example.cpp) | yes |
| `fdo2_nonblocking_example` | Measure without blocking: Start calls and Poll() from a task that does other work | [`fdo2_nonblocking_example.cpp`](../examples/esp32/main/fdo2_nonblocking_example.cpp) | yes |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh fdo2_basic_example Release
./scripts/flash_app.sh flash_monitor fdo2_basic_example Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

### Set up once

```bash
git submodule update --init examples/esp32/scripts   # the ESP-IDF build scripts
cd examples/esp32
. $IDF_PATH/export.sh                                 # ESP-IDF v5.5
./scripts/build_app.sh list                           # every app and build type
```

The scripts read [`app_config.yml`](../examples/esp32/app_config.yml) (the same file this page's table comes
from) and build each app as its own firmware image.

### What to read in an example

| You want to | Read |
| --- | --- |
| See the lifecycle and every `Result` checked | `fdo2_basic_example.cpp` |
| Measure without blocking | `fdo2_nonblocking_example.cpp` |
| Write an ESP-IDF port | `esp32_fdo2_port.hpp` |
| Use menuconfig values | `fdo2_kconfig.hpp` and [`_config/Kconfig`](../_config/Kconfig) |
