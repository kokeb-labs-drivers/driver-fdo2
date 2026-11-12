/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would truncate long waits, so
 * the driver rejects it (handbook KB-CPP20-007).
 *
 * EXPECT: DelayUs hook must be
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

class ShortDelayPort : public fdo2::UartInterface<ShortDelayPort> {
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
  void DelayUs(uint16_t) noexcept {}
};

int main() {
  ShortDelayPort port;
  fdo2::FDO2<ShortDelayPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
