/**
 * @file poll_with_elapsed_on_clock_port.cpp
 * @brief Must NOT compile: a port with a clock polls without an argument; passing the elapsed time
 * is for ports without one.
 *
 * EXPECT: Poll|constraints
 */
#include "fdo2.hpp"
#include "mock_fdo2.hpp"

int main() {
  fdo2_test::ClockPort port;
  fdo2::FDO2<fdo2_test::ClockPort> sensor(port);
  return static_cast<int>(sensor.Poll(1000U).ok());
}
