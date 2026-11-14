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

## What the FDO2 does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the FDO2: Oxygen and temperature (supported): Measures the oxygen partial pressure
    and the temperature, as hPa or as a volume fraction; Raw values and housing sensors (supported):
    Adds the phase shift, signal and ambient light, and the pressure and humidity inside the housing;
    Status word (supported): Classifies each reading's warning and fatal bits and holds back a faulty
    measurement; Identity and start-up (supported): Waits out the power-up, reads the version and the
    unique ID, and flashes the status LED to find a module; Calibration (guarded): Calibrates at zero
    oxygen, at a known partial pressure or in dry air; the factory calibration is lost; Broadcast stream
    (guarded): Makes the module measure by itself at a set interval and send each result; Measurement
    mode, CRC and baud rate (guarded): Trades drift for noise, adds a CRC-16 to every reply, and changes
    the baud rate; User memory (guarded): Reads and writes 64 words that survive a power cycle; each
    write costs a flash cycle.">
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

## FDO2

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the FDO2, an optical oxygen sensor module with a UART: it excites a sensor dye with red
    light, measures the luminescence that oxygen quenches, and reports the oxygen partial pressure with
    its temperature, compensated and factory calibrated. A MCU talks to it over UART, 3.0 V; it reads
    back Oxygen, Temperature, pressure, humidity. Headline figures: 0 to 1000 hPa typical oxygen partial
    pressure range; < 2 s response time (t63); 3.3 to 5.0 V supply; 19200 baud UART at power-up, 8N1,
    3.0 V.">
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

## Wiring: Direct connection to a 3.3-volt controller

<!-- figure: chip-wiring-direct -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-direct.dark.svg">
  <img src="chip-wiring-direct.svg"
    alt="Wiring of the FDO2 for the Direct connection to a 3.3-volt controller: MCU (3.3 V) UART TX to RXD
    (Write()); MCU (3.3 V) UART RX to TXD (Read()). Supplies, grounds and the support parts are listed
    under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/fdo2_uart_interface.hpp`](../../inc/fdo2_uart_interface.hpp)

## Wiring: Connection to a 5-volt controller through a divider

<!-- figure: chip-wiring-shifted -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-shifted.dark.svg">
  <img src="chip-wiring-shifted.svg"
    alt="Wiring of the FDO2 for the Connection to a 5-volt controller through a divider: MCU (5 V) UART RX to
    TXD (Read()); MCU (5 V) UART TX to RXD through R1 10 kΩ (Write()). Supplies, grounds and the support
    parts are listed under the drawing.">
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

## One call through the layers

<!-- figure: one-call-through-the-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="one-call-through-the-layers.dark.svg">
  <img src="one-call-through-the-layers.svg"
    alt="One call through the layers: your code calls MeasureOxygen(), the core exchanges frames with the
    chip through your port and returns the result to your code.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/one-call-through-the-layers.seq.yml`](../model/one-call-through-the-layers.seq.yml)

Drawn from:

- [`docs/model/one-call-through-the-layers.seq.yml`](../model/one-call-through-the-layers.seq.yml)

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
