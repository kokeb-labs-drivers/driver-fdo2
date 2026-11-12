# FDO2 host example

The driver on your PC, with no hardware: the host port in [`host_fdo2_port.hpp`](host_fdo2_port.hpp) connects the
real driver to a simulated module (the model the host tests use), and [`main.cpp`](main.cpp) walks through
start-up, a measurement, a status warning, a sensor fault, a refused flash write, a non-blocking loop and a
broadcast stream.

## Build and run

From the repository root, with CMake 3.16 or later and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/fdo2_host_example
```

## Expected output

The exact text is in [`expected_output.txt`](expected_output.txt). The host tests build this folder, run the
example and compare its output with that file (CTest `fdo2_host_example`), so CI fails if it stops compiling or a
call changes.

## Files

| File | What it is |
| --- | --- |
| [`main.cpp`](main.cpp) | The walk-through; integer printing only |
| [`host_fdo2_port.hpp`](host_fdo2_port.hpp) | The port over the simulated module, with `static_assert`s |
| [`expected_output.txt`](expected_output.txt) | What the run prints |
| [`CMakeLists.txt`](CMakeLists.txt) | A stand-alone project for the example |
