/**
 * @file non_blocking.cpp
 * @brief The non-blocking page's examples (a super-loop and a port without a clock), compiled by
 * the host build (never run).
 */
#include "fdo2.hpp"
#include "my_port.hpp"

// A second port type: no clock and no DelayUs, so only Poll(elapsed_us) works.
class TickPort : public fdo2::UartInterface<TickPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(const uint8_t* data, std::size_t len) noexcept {
    return platform_uart_write(data, len);
  }
  std::size_t Read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept {
    return platform_uart_read(data, max, timeout_ms);
  }
  bool FlushInput() noexcept {
    return platform_uart_flush_input();
  }
};

uint32_t platform_ticks_us() noexcept; // microseconds since the last call, from your timer

// --8<-- [start:super-loop]
// Start-up and one measurement per second, without blocking the loop.
class OxygenTask {
public:
  explicit OxygenTask(TickPort& port) : sensor_(port) {}

  // Call from the super-loop every few milliseconds.
  void Step() {
    const uint32_t elapsed_us = platform_ticks_us();
    if (state_ == Phase::Start) {
      if (sensor_.BeginInitialize()) {
        state_ = Phase::Starting;
      }
      return;
    }
    const auto p = sensor_.Poll(elapsed_us);
    if (!p) {
      state_ = Phase::Start; // the operation failed: p.error; start again
      return;
    }
    if (!p.value) {
      return; // still waiting for the module
    }
    if (state_ == Phase::Starting) {
      state_ = Phase::Idle;
    } else if (state_ == Phase::Measuring) {
      last_o2_mhpa_ = sensor_.GetOxygen().value.o2_mhpa;
      state_ = Phase::Idle;
    } else if (sensor_.StartMeasureOxygen()) {
      state_ = Phase::Measuring;
    } else {
      // the start was refused (busy): try again at the next poll
    }
  }

  [[nodiscard]] int32_t last_o2_mhpa() const {
    return last_o2_mhpa_;
  }

private:
  enum class Phase : uint8_t { Start, Starting, Idle, Measuring };
  fdo2::FDO2<TickPort> sensor_;
  Phase state_ = Phase::Start;
  int32_t last_o2_mhpa_ = 0;
};
// --8<-- [end:super-loop]

// --8<-- [start:with-clock]
// With a clock in the port (NowUs), Poll() takes no argument.
bool Tick(fdo2::FDO2<MyPort>& sensor) {
  if (sensor.IsInitialized() && (sensor.Pending() == fdo2::OperationKind::None)) {
    return sensor.StartMeasureRaw().ok();
  }
  return false;
}
// --8<-- [end:with-clock]
