/**
 * @file hook_not_noexcept.cpp
 * @brief Must NOT compile: a Write hook that is not noexcept breaks the port contract (handbook
 * KB-CPP20-006).
 *
 * EXPECT: UartPort
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

class ThrowingPort : public fdo2::UartInterface<ThrowingPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(const uint8_t*, std::size_t) {
    return true;
  }
  std::size_t Read(uint8_t*, std::size_t, uint32_t) noexcept {
    return 0;
  }
  bool FlushInput() noexcept {
    return true;
  }
};

int main() {
  ThrowingPort port;
  fdo2::FDO2<ThrowingPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
