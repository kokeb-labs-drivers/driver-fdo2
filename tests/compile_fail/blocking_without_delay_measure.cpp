/**
 * @file blocking_without_delay_measure.cpp
 * @brief Must NOT compile: a port without DelayUs() cannot call the blocking Measure; only the
 * Start* and Poll calls compile.
 *
 * EXPECT: DelayUs
 */
#include "fdo2.hpp"
#include "mock_fdo2.hpp"

int main() {
  fdo2_test::PollOnlyPort port;
  fdo2::FDO2<fdo2_test::PollOnlyPort> sensor(port);
  return static_cast<int>(sensor.MeasureOxygen().error);
}
