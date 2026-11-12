# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the FDO2

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the FDO2: its function blocks, how they connect and which package pins reach each
    block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/fdo2_uart_interface.hpp`](../../inc/fdo2_uart_interface.hpp)

## FDO2: Direct connection to a 3.3-volt controller

<!-- figure: chip-circuit-direct -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-direct.dark.svg">
  <img src="chip-circuit-direct.svg"
    alt="Schematic of the FDO2 direct connection to a 3.3-volt controller: the chip with U2 MCU (3.3 V), C1
    capacitor 100 nF; the MCU wires carry the port hooks GpioSet(CtrlPin::Power), Read(), Write().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/fdo2_uart_interface.hpp`](../../inc/fdo2_uart_interface.hpp)

## FDO2: Connection to a 5-volt controller through a divider

<!-- figure: chip-circuit-shifted -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-shifted.dark.svg">
  <img src="chip-circuit-shifted.svg"
    alt="Schematic of the FDO2 connection to a 5-volt controller through a divider: the chip with U2 MCU (5
    V), R1 resistor 10 kΩ, R2 resistor 12 kΩ, C1 capacitor 100 nF; the MCU wires carry the port hooks
    GpioSet(CtrlPin::Power), Read(), Write().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/fdo2_uart_interface.hpp`](../../inc/fdo2_uart_interface.hpp)

## FDO2 package, Module, 4-pin

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-outline.dark.svg">
  <img src="chip-outline.svg"
    alt="Outline of the FDO2 in Module, 4-pin drawn to scale from above: body 28.5 x 28.5 x 27.5 mm, 4 pins.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/fdo2_uart_interface.hpp`](../../inc/fdo2_uart_interface.hpp)

## FDO2 pinout, Module, 4-pin

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the FDO2 in Module, 4-pin, top view: 4 pins, each with its name, its electrical type as a
    colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/fdo2_uart_interface.hpp`](../../inc/fdo2_uart_interface.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the FDO2 core, which sends register reads and writes through
    the UART port contract to the port class you write, which drives the chip; the core and the contract
    are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/fdo2.hpp`](../../inc/fdo2.hpp)
- `UartInterface` in [`inc/fdo2_uart_interface.hpp`](../../inc/fdo2_uart_interface.hpp)

## The driver's states

<!-- figure: state-machine -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="state-machine.dark.svg">
  <img src="state-machine.svg"
    alt="State diagram of fdo2::FDO2: Uninitialized, PoweringUp, Idle, Waiting, Streaming, Draining and
    StateUnknown, with the calls and events that move between them.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/state-machine.py`](../../docs/diagrams/figures/state-machine.py)

Drawn from:

- `State` in [`inc/fdo2_types.hpp`](../../inc/fdo2_types.hpp)
- `State transitions` in [`src/fdo2.ipp`](../../src/fdo2.ipp)

## One command and its reply

<!-- figure: transaction-timing -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="transaction-timing.dark.svg">
  <img src="transaction-timing.svg"
    alt="Five steps of a transaction: flush the receive buffer, write the command, wait for the module, read
    the echo and values, end at the carriage return; the allowance of each kind of command is listed
    under the wait.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/transaction-timing.py`](../../docs/diagrams/figures/transaction-timing.py)

Drawn from:

- `Config` in [`inc/fdo2_types.hpp`](../../inc/fdo2_types.hpp)
