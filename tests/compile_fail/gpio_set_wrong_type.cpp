/**
 * @file gpio_set_wrong_type.cpp
 * @brief Must NOT compile: a GpioSet hook that returns nothing hides a failed pin write.
 *
 * EXPECT: GpioSet hook must be
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

class BadGpioPort : public fdo2::UartInterface<BadGpioPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(const uint8_t*, std::size_t) noexcept {
    return true;
  }
  std::size_t Read(uint8_t*, std::size_t, uint32_t) noexcept {
    return 0;
  }
  bool FlushInput() noexcept {
    return true;
  }
  void GpioSet(fdo2::CtrlPin, fdo2::GpioSignal) noexcept {}
  void DelayUs(uint32_t) noexcept {}
};

int main() {
  BadGpioPort port;
  fdo2::FDO2<BadGpioPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
