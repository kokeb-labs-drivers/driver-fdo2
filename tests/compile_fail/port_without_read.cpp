/**
 * @file port_without_read.cpp
 * @brief Must NOT compile: a port with no Read hook is not a UartPort.
 *
 * EXPECT: UartPort
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

class NoReadPort : public fdo2::UartInterface<NoReadPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(const uint8_t*, std::size_t) noexcept {
    return true;
  }
  bool FlushInput() noexcept {
    return true;
  }
};

int main() {
  NoReadPort port;
  fdo2::FDO2<NoReadPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
