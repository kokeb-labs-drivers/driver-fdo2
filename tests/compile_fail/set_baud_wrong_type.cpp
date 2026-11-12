/**
 * @file set_baud_wrong_type.cpp
 * @brief Must NOT compile: a SetBaudRate hook that returns void hides a failed rate change.
 *
 * EXPECT: SetBaudRate hook must be
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

class BadBaudPort : public fdo2::UartInterface<BadBaudPort> {
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
  void SetBaudRate(uint32_t) noexcept {}
};

int main() {
  BadBaudPort port;
  fdo2::FDO2<BadBaudPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
