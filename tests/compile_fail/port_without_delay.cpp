/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: start-up waits for the power-up time and the replies, so a blocking call
 * needs DelayUs(); use BeginInitialize() and Poll() on a port without it.
 *
 * EXPECT: DelayUs
 */
#include "fdo2.hpp"
#include "mock_fdo2.hpp"

int main() {
  fdo2_test::BarePort port;
  fdo2::FDO2<fdo2_test::BarePort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
