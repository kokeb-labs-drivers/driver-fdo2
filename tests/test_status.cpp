/**
 * @file test_status.cpp
 * @brief The status word (data sheet 4.3): bits, classes and the two policies' boundaries.
 */
#include "fixtures.hpp"

using fdo2::Quality;
using fdo2::Status;

TEST_CASE("status: every bit has its accessor") {
  CHECK(Status{0x1U}.AmplificationReduced());
  CHECK(Status{0x2U}.SignalTooLow());
  CHECK(Status{0x4U}.SignalTooHigh());
  CHECK(Status{0x8U}.ReferenceTooLow());
  CHECK(Status{0x10U}.ReferenceTooHigh());
  CHECK(Status{0x20U}.TemperatureSensorFailed());
  CHECK(Status{0x80U}.HumidityHigh());
  CHECK(Status{0x200U}.PressureSensorFailed());
  CHECK(Status{0x400U}.HumiditySensorFailed());
  CHECK(!Status{0U}.AmplificationReduced());
  CHECK(!Status{0xFFFFFBFEU}.AmplificationReduced());
  CHECK(!Status{0xFFFFFFFDU}.SignalTooLow());
  CHECK(!Status{0xFFFFFFFBU}.SignalTooHigh());
  CHECK(!Status{0xFFFFFFF7U}.ReferenceTooLow());
  CHECK(!Status{0xFFFFFFEFU}.ReferenceTooHigh());
  CHECK(!Status{0xFFFFFFDFU}.TemperatureSensorFailed());
  CHECK(!Status{0xFFFFFF7FU}.HumidityHigh());
  CHECK(!Status{0xFFFFFDFFU}.PressureSensorFailed());
  CHECK(!Status{0xFFFFFBFFU}.HumiditySensorFailed());
}

TEST_CASE("status: classes") {
  CHECK(Status{0U}.Classify() == Quality::Good);
  CHECK(Status{1U}.Classify() == Quality::Warning);
  for (const uint32_t bit : {0x2U, 0x4U, 0x8U, 0x10U, 0x20U}) {
    CHECK(Status{bit}.Classify() == Quality::Invalid);
    CHECK(Status{bit}.AnyFatal());
  }
  for (const uint32_t bit : {0x80U, 0x200U, 0x400U}) {
    CHECK(Status{bit}.Classify() == Quality::Degraded);
    CHECK(Status{bit | 1U}.Classify() == Quality::Degraded);
  }
  for (const uint32_t bit : {0x40U, 0x100U, 0x800U, 0x80000000U, 0x1000U}) {
    CHECK(Status{bit}.UndefinedBits());
    CHECK(Status{bit}.Classify() == Quality::Invalid);
  }
  CHECK(Status{0x6BFU & ~0x3EU}.Classify() == Quality::Degraded);
  CHECK(!Status{0x6BFU}.UndefinedBits());
}
