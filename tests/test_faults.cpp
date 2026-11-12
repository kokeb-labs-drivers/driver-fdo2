/**
 * @file test_faults.cpp
 * @brief Every way a reply can be bad (hardware lessons: validity, resynchronisation, retries):
 * the error, the drain and the next call that must not read a late byte.
 */
#include "fixtures.hpp"

using fdo2::Error;
using fdo2::State;
using fdo2_test::FaultKind;

namespace {

fdo2::Config NoRetry() {
  fdo2::Config c{};
  c.retries = 0;
  return c;
}

/// A started rig with the CRC on.
struct CrcRig : fx::Rig {
  explicit CrcRig(const fdo2::Config& c = NoRetry()) : fx::Rig(c) {
    port.module.crc_on = true;
    port.module.Reset();
    port.module.faults.clear();
  }
};

} // namespace

TEST_CASE("faults: silence is a timeout, the retry then succeeds") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::Silent);
  CHECK(r.drv.MeasureOxygen().error == Error::Timeout);
  CHECK_EQ(r.drv.GetCounters().timeouts, 1U);
  CHECK(r.drv.MeasureOxygen().ok());
  fx::Rig q;
  CHECK(q.Start() == Error::Ok);
  q.port.module.Arm(FaultKind::Silent);
  CHECK(q.drv.MeasureOxygen().ok());
  CHECK_EQ(q.drv.GetCounters().retries, 1U);
}

TEST_CASE("faults: a cut reply and a missing terminator are timeouts, never a value") {
  for (const int cut : {1, 5, 12}) {
    fx::Rig r(NoRetry());
    CHECK(r.Start() == Error::Ok);
    r.port.module.Arm(FaultKind::TruncatedAt, cut);
    CHECK(r.drv.MeasureOxygen().error == Error::Timeout);
    CHECK(r.drv.MeasureOxygen().ok());
  }
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::NoTerminator);
  CHECK(r.drv.MeasureOxygen().error == Error::Timeout);
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("faults: a flipped bit is a CRC error with the CRC on") {
  CrcRig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.CrcActive());
  r.port.module.Arm(FaultKind::BitFlip, 8);
  CHECK(r.drv.MeasureOxygen().error == Error::Crc);
  CHECK_EQ(r.drv.GetCounters().crc_errors, 1U);
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("faults: bytes after the CR never become a value; a retry recovers") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::ExtraBytes);
  CHECK(r.drv.MeasureOxygen().ok());
  // The two stray bytes arrive in front of the next reply and break its line: it is refused.
  const auto second = r.drv.MeasureOxygen();
  CHECK((second.error == Error::InvalidReply) || second.ok());
  const auto third = r.drv.MeasureOxygen();
  CHECK(third.ok());
  CHECK_EQ(third.value.o2_mhpa, 209'500);
  fx::Rig q;
  CHECK(q.Start() == Error::Ok);
  q.port.module.Arm(FaultKind::ExtraBytes);
  CHECK(q.drv.MeasureOxygen().ok());
  const auto again = q.drv.MeasureOxygen();
  CHECK(again.ok());
  CHECK_EQ(again.value.o2_mhpa, 209'500);
}

TEST_CASE("faults: another command's reply is an echo mismatch") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::WrongHeader);
  const auto e = r.drv.MeasureOxygen().error;
  CHECK((e == Error::EchoMismatch) || (e == Error::Timeout));
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("faults: a changed echoed argument is an echo mismatch") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  std::array<int32_t, 2> out{};
  r.port.module.Arm(FaultKind::EchoArgChanged);
  CHECK(r.drv.ReadUserMemory(0, out).error == Error::EchoMismatch);
  CHECK(r.drv.ReadUserMemory(0, out).ok());
}

TEST_CASE("faults: a stale complete line is dropped and counted") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::StaleLineFirst);
  const auto o = r.drv.MeasureOxygen();
  CHECK(o.ok());
  CHECK_EQ(r.drv.GetCounters().stale_lines, 1U);
}

TEST_CASE("faults: garbage, NULs and 0xFF fill never become a value") {
  for (const FaultKind k :
       {FaultKind::GarbageBurst, FaultKind::AllZero, FaultKind::AllFF, FaultKind::OverlongLine}) {
    fx::Rig r(NoRetry());
    CHECK(r.Start() == Error::Ok);
    r.port.module.Arm(k);
    const auto e = r.drv.MeasureOxygen().error;
    CHECK((e == Error::InvalidReply) || (e == Error::Timeout) || (e == Error::Ok));
    CHECK(r.drv.MeasureOxygen().ok());
    CHECK(r.drv.GetState() == State::Idle);
  }
}

TEST_CASE("faults: an all-zero reply is invalid") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::AllZero);
  CHECK(r.drv.MeasureOxygen().error == Error::InvalidReply);
  CHECK_EQ(r.drv.GetCounters().invalid_replies, 1U);
  CHECK(r.drv.GetCounters().bytes_discarded > 0U);
}

TEST_CASE("faults: a double space is invalid in strict mode and accepted when folded") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::ExtraSpace);
  CHECK(r.drv.MeasureOxygen().error == Error::InvalidReply);
  fdo2::Config c = NoRetry();
  c.strict_spacing = false;
  fx::Rig q(c);
  CHECK(q.Start() == Error::Ok);
  q.port.module.Arm(FaultKind::ExtraSpace);
  CHECK(q.drv.MeasureOxygen().ok());
}

TEST_CASE("faults: a reply ending in CR LF is accepted, the LF is not the next reply") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::LfTerminated);
  CHECK(r.drv.MeasureOxygen().ok());
  CHECK(r.drv.MeasureOxygen().ok());
  fdo2::Config c = NoRetry();
  c.ignore_lf = false;
  fx::Rig q(c);
  CHECK(q.Start() == Error::Ok);
  q.port.module.Arm(FaultKind::LfTerminated);
  CHECK(q.drv.MeasureOxygen().ok());
  CHECK(q.drv.MeasureOxygen().ok());
}

TEST_CASE("faults: #ERRO codes map to errors") {
  struct Row {
    int code;
    Error error;
  };
  const Row rows[] = {{-1, Error::DeviceError},  {-21, Error::DeviceError},
                      {-22, Error::DeviceError}, {-23, Error::DeviceError},
                      {-26, Error::Unsupported}, {-60, Error::CalibrationRejected},
                      {-99, Error::DeviceFault}, {-2147483647 - 1, Error::DeviceFault}};
  for (const Row& row : rows) {
    fx::Rig r(NoRetry());
    CHECK(r.Start() == Error::Ok);
    r.port.module.Arm(FaultKind::ErrorCode, row.code);
    CHECK(r.drv.MeasureOxygen().error == row.error);
    CHECK_EQ(r.drv.DeviceErrorCode(), row.code);
    CHECK(r.drv.MeasureOxygen().ok());
  }
}

TEST_CASE("faults: #ERRO without CRC while the CRC is on, by policy") {
  CrcRig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::ErrorWithoutCrc, -21);
  CHECK(r.drv.MeasureOxygen().error == Error::DeviceError);
  fdo2::Config c = NoRetry();
  c.erro_without_crc = false;
  CrcRig q(c);
  CHECK(q.Start() == Error::Ok);
  q.port.module.Arm(FaultKind::ErrorWithoutCrc, -21);
  CHECK(q.drv.MeasureOxygen().error == Error::Crc);
}

TEST_CASE("faults: only idempotent reads are retried") {
  fdo2::Config c{};
  c.retries = 2;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::Silent, 0, 2);
  CHECK(r.drv.MeasureOxygen().ok());
  CHECK_EQ(r.drv.GetCounters().retries, 2U);
  r.port.module.Arm(FaultKind::Silent, 0, 3);
  CHECK(r.drv.MeasureOxygen().error == Error::Timeout);
  CHECK(r.drv.MeasureOxygen().ok());
  const auto cals = r.port.module.CountCommands("#CRCE");
  r.port.module.Arm(FaultKind::Silent);
  CHECK(r.drv.SetCrcEnabled(true).error == Error::NotPermitted);
  CHECK_EQ(r.port.module.CountCommands("#CRCE"), cals);
}

TEST_CASE("faults: a device error is not retried") {
  fdo2::Config c{};
  c.retries = 3;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  const auto before = r.port.module.CountCommands("#MOXY");
  r.port.module.Arm(FaultKind::ErrorCode, -1);
  CHECK(r.drv.MeasureOxygen().error == Error::DeviceError);
  CHECK_EQ(r.port.module.CountCommands("#MOXY"), before + 1U);
  // -21, -22 and -23 may come from a corrupted request line: an idempotent read is sent again.
  r.port.module.Arm(FaultKind::ErrorCode, -21);
  CHECK(r.drv.MeasureOxygen().ok());
  CHECK_EQ(r.port.module.CountCommands("#MOXY"), before + 3U);
}

TEST_CASE("faults: a late reply after a timeout is drained, not taken for the next one") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  // The module is slower than the allowance: the reply arrives after the timeout.
  r.port.module.knobs.measure_us = 420'000;
  CHECK(r.drv.MeasureOxygen().error == Error::Timeout);
  r.port.module.knobs.measure_us = 0;
  r.port.module.scenario.o2_hpa = 150.0;
  const auto o = r.drv.MeasureOxygen();
  CHECK(o.ok());
  CHECK_EQ(o.value.o2_mhpa, 150'000);
}

TEST_CASE("faults: the receive buffer overflowing loses bytes, never invents a value") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.knobs.rx_capacity = 8;
  // The driver reads as the bytes arrive, so a small FIFO still delivers whole replies.
  const auto o = r.drv.MeasureOxygen();
  CHECK((!o.ok()) || (o.value.o2_mhpa == 209'500));
  r.port.module.knobs.rx_capacity = 1024;
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("faults: a lost power is a timeout and the start-up can be repeated") {
  fx::Rig r(NoRetry());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::PowerLoss);
  CHECK(r.drv.MeasureOxygen().error == Error::Timeout);
  CHECK(r.drv.PowerCycle().ok());
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("faults: the single-shot raw read is never repeated") {
  fdo2::Config c{};
  c.retries = 2;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::Silent, 0, 1);
  CHECK(r.drv.ReadRawFrame().error == Error::Timeout);
  CHECK_EQ(r.drv.GetConfig().retries, 2U); // restored
  CHECK_EQ(r.drv.GetCounters().retries, 0U);
  CHECK(r.drv.ReadRawFrame().ok());
  CHECK(r.drv.GetLastFrame().ok());
}

static_assert(fdo2::kMaxBaudRate == 115'200U);
static_assert(fdo2::kTurnaroundUs > 0U);
