/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: UartPort
 */
#include "fdo2.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  fdo2::FDO2<NotAPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
