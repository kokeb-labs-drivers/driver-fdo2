/**
 * @file host_fdo2_port.hpp
 * @brief The host port: the driver's UART hooks over a simulated FDO2, so the example runs on any
 * PC.
 *
 * `fdo2_test::PortBase` (tests/mock_fdo2.hpp) supplies the required hooks over the module model
 * the host tests use. This class adds what a real port adds: `DelayUs()` (here it advances the
 * simulated clock instead of waiting), the power pin, the baud rate and `Log()` to the console.
 * A port for a real UART on a PC (Linux termios, a USB serial adapter) implements the same
 * hooks: see docs/porting.md. It has no `NowUs()`, so the non-blocking part of the example passes
 * the elapsed time to `Poll()`.
 */
#ifndef FDO2_HOST_FDO2_PORT_HPP
#define FDO2_HOST_FDO2_PORT_HPP

#include <cstdint>
#include <cstdio>

#include "mock_fdo2.hpp"

/// The host port: simulated module, real console.
class HostFdo2Port : public fdo2_test::PortBase<HostFdo2Port> {
public:
  /// The line settings a real port would use; checked against the driver's bus requirements
  /// like any port's.
  static constexpr uint32_t kBaudRate = fdo2::kPortDefaultBaud;
  static constexpr uint32_t kRxBufferBytes = 1024;
  static_assert(kBaudRate == 19'200U, "the module starts at 19200 baud");
  static_assert(kRxBufferBytes >= 800U, "the receive buffer must hold the longest reply");

  /// Time passes in the simulated module; nothing sleeps.
  void DelayUs(uint32_t us) noexcept {
    module.Elapse(us);
  }

  /// The simulated baud rate follows the driver's request.
  bool SetBaudRate(uint32_t baud) noexcept {
    module.host_baud = baud;
    return true;
  }

  /// The power pin of the module (a real port drives a GPIO or a load switch).
  bool GpioSet(fdo2::CtrlPin pin, fdo2::GpioSignal level) noexcept {
    (void)pin;
    module.SetPower(level == fdo2::GpioSignal::ACTIVE);
    return true;
  }

  /// Driver messages (they carry their own `fdo2:` prefix) go to the console, one line each.
  void Log(fdo2::LogLevel level, const char* message) noexcept {
    (void)level;
    (void)std::puts(message); // plain text: never pass it as a format string
  }
};

#endif // FDO2_HOST_FDO2_PORT_HPP
