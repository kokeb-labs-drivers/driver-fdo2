/**
 * @file hook_narrowed_parameter.cpp
 * @brief Must NOT compile: a Read hook with a narrower timeout would truncate every wait, so the
 * port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: UartPort
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

class NarrowPort : public fdo2::UartInterface<NarrowPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(const uint8_t*, std::size_t) noexcept {
    return true;
  }
  std::size_t Read(uint8_t*, std::size_t, uint16_t) noexcept {
    return 0;
  }
  bool FlushInput() noexcept {
    return true;
  }
};

int main() {
  NarrowPort port;
  fdo2::FDO2<NarrowPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
