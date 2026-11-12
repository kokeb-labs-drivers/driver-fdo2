/**
 * @file poll_without_elapsed_on_noclock_port.cpp
 * @brief Must NOT compile: a port without a clock must pass the elapsed time to Poll().
 *
 * EXPECT: Poll|constraints
 */
#include "fdo2.hpp"
#include "mock_fdo2.hpp"

int main() {
  fdo2_test::NoClockPort port;
  fdo2::FDO2<fdo2_test::NoClockPort> sensor(port);
  return static_cast<int>(sensor.Poll().ok());
}
