# Porting

How to write the port class that connects the driver to your MCU's UART, clock and pins.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    - UART framing, echo and terminator: PyroScience Datasheet_FDO2-G2 v5, 4.1 General Definitions, [p. 10](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=10)
    - #BAUD: the eleven baud rates: PyroScience Datasheet_FDO2-G2 v5, 4.3 Commands, #BAUD, [p. 14](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=14)
    - Connector, pins and signal levels: PyroScience Datasheet_FDO2-G2 v5, 7 Mechanical Dimensions and Electrical Interface, drawing and pin table, [p. 20](https://www.pyroscience.com/en/products/all-meters/fdo2-g2?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2-G2.pdf&cid=24457#page=20)
    - UART levels: 3.0 V, 3.3 V maximum on RXD: PyroScience Datasheet_FDO2 v21, 6.2 Voltage Tolerance of UART RX Pin (ERRATA), [p. 19](https://www.pyroscience.com/en/downloads/legacy?file=files/website_data/Downloads/Manuals/Meters/Datasheet_FDO2.pdf&cid=24471#page=19)

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver never calls your SDK. It calls hooks on a class you derive from `fdo2::UartInterface<Self>` (in
[`inc/fdo2_uart_interface.hpp`](../inc/fdo2_uart_interface.hpp)). The base uses CRTP and declares no hook itself;
the driver template is constrained by the `fdo2::UartPort` concept, which checks each hook's exact signature and
`noexcept`. A missing or misdeclared hook is a compile error at the point of use.

## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the UART up on the first call, 19200 8N1; return `true` at once afterwards |
| `bool Write(const uint8_t* data, std::size_t len) noexcept` | yes | Send all `len` bytes; `false` on an error |
| `std::size_t Read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept` | yes | Return the bytes received, 0 to `max`, waiting up to `timeout_ms` for the first one |
| `bool FlushInput() noexcept` | yes | Drop every received byte not yet returned by `Read()` |
| `void DelayUs(uint32_t us) noexcept` | for the blocking calls | Wait at least `us` microseconds |
| `uint32_t NowUs() noexcept` | no | A free-running microsecond counter, wrap-safe; `Poll()` then takes no argument |
| `bool SetBaudRate(uint32_t baud) noexcept` | no | Reconfigure the UART; without it `SetBaudRate()` returns `Unsupported` |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | `CtrlPin::Power`: `ACTIVE` powers the module (an external load switch) |
| `void Log(LogLevel, const char*) noexcept` | no | Print one line; the message is plain text, never a format string |

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

## Rules

- **Raw bytes only.** The driver builds the lines and checks the checksum; the port does not parse or retry.
- **`Read()` returns a count.** The reply length is not known in advance, so it is not an exact-length read. It
  waits for the first byte up to `timeout_ms` and returns what is in the FIFO. A port whose SDK returns only a
  status polls the FIFO level first.
- **The receive buffer holds at least 800 bytes** (`kRxBufferBytes`): the longest reply. A smaller buffer loses
  bytes while the driver is busy with something else.
- **`FlushInput()` really flushes.** The driver calls it before each request so a stale byte cannot shift a reply.
- **19200 baud, 8N1, no flow control** at start; the module starts there after every power-up.
- **Without `NowUs`**, call `Poll(elapsed_us)`; with it, `Poll()` ([Non-blocking use](non-blocking.md)).
- **A hook that returns false** is reported as `Bus`.
- **`DelayUs` should yield** for a millisecond or more on an RTOS (the ESP32 port calls `vTaskDelay`), so the
  other tasks run during the 1.5 s power-up wait.
- **Loopback.** A TX line wired to RX echoes every command. The driver does not take an echo of its own `#VERS`
  as an answer: the identity must match, so the start-up fails with `DeviceIdMismatch` or `InvalidReply`.
- **No power pin.** A port whose power pin is not fitted returns `true` from `GpioSet` for `CtrlPin::Power`, a valid
  no-op (the ESP32 example does so for pin -1).

Idle level of the supply switch: `INACTIVE` (module off) before the output is enabled and before the first line is
sent; the port maps `ACTIVE` to the polarity of your switch.

## Bus requirements

The driver states what the module needs as `constexpr` values; check your port against them with
`static_assert`, as both example ports do:

```cpp title="ports.cpp"
--8<-- "tests/docs/ports.cpp:port-constants"
```

| Constant | Value | Meaning |
| --- | --- | --- |
| `kPortDefaultBaud` | 19200 | The rate after every power-up |
| `kLogicLevelMv` | 3000 | The module's signal level |
| `kMaxLogicLevelMv` | 3300 | Absolute maximum on RXD |
| `kSupplyMinMv`, `kSupplyMaxMv` | 3300, 5000 | The supply range |
| `kPowerUpMs` | 1000 | The module's power-up time |
| `kRxBufferBytes` | 800 | The longest reply |

## A port with every hook

```cpp title="ports.cpp"
--8<-- "tests/docs/ports.cpp:full-port"
```

The [ESP-IDF port](../examples/esp32/main/esp32_fdo2_port.hpp) and the
[host port](../examples/host/host_fdo2_port.hpp) are complete examples.
