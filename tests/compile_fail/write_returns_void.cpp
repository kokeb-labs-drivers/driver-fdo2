/**
 * @file write_returns_void.cpp
 * @brief Must NOT compile: a Write hook that returns nothing hides a failed transmit, so the
 * concept rejects it.
 *
 * EXPECT: UartPort
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

class VoidWritePort : public fdo2::UartInterface<VoidWritePort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  void Write(const uint8_t*, std::size_t) noexcept {}
  std::size_t Read(uint8_t*, std::size_t, uint32_t) noexcept {
    return 0;
  }
  bool FlushInput() noexcept {
    return true;
  }
};

int main() {
  VoidWritePort port;
  fdo2::FDO2<VoidWritePort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
