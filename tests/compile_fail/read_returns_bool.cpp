/**
 * @file read_returns_bool.cpp
 * @brief Must NOT compile: a Read hook that returns a bool cannot say how many bytes arrived, so
 * the concept rejects it.
 *
 * EXPECT: UartPort
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

class BoolReadPort : public fdo2::UartInterface<BoolReadPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(const uint8_t*, std::size_t) noexcept {
    return true;
  }
  bool Read(uint8_t*, std::size_t, uint32_t) noexcept {
    return true;
  }
  bool FlushInput() noexcept {
    return true;
  }
};

int main() {
  BoolReadPort port;
  fdo2::FDO2<BoolReadPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
