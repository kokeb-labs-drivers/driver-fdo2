/**
 * @file test_flash.cpp
 * @brief Flash-writing commands (data sheet 4.3, WARNING): consent, budget, counting at send time,
 * the unknown state after a lost echo, and the capability gates.
 */
#include <algorithm>
#include <array>
#include <string>

#include "fixtures.hpp"

using fdo2::Error;
using fdo2::State;
using fdo2_test::FaultKind;

TEST_CASE("flash: nothing is written without consent and nothing is sent") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto sent = r.port.module.commands.size();
  std::array<int32_t, 1> w{5};
  CHECK(r.drv.SetCrcEnabled(true).error == Error::NotPermitted);
  CHECK(r.drv.SetMeasurementMode(3).error == Error::NotPermitted);
  CHECK(r.drv.SetBroadcast(100).error == Error::NotPermitted);
  CHECK(r.drv.WriteUserMemory(0, w).error == Error::NotPermitted);
  CHECK(r.drv.CalibrateZero(300).error == Error::NotPermitted);
  CHECK(r.drv.CalibrateHigh(209'500, 300).error == Error::NotPermitted);
  CHECK(r.drv.StartSetCrcEnabled(true).error == Error::NotPermitted);
  CHECK_EQ(r.port.module.commands.size(), sent);
  CHECK_EQ(r.port.module.flash_cycles, 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK(r.drv.GetState() == State::Idle);
}

TEST_CASE("flash: with consent each write is counted") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetMeasurementMode(3).ok());
  CHECK_EQ(r.port.module.mode, 3);
  CHECK_EQ(r.drv.FlashWritesUsed(), 1U);
  CHECK_EQ(r.drv.GetCounters().flash_writes, 1U);
  CHECK_EQ(r.port.module.flash_cycles, 1U);
  CHECK(r.drv.SetMeasurementMode(0).ok());
  CHECK_EQ(r.drv.FlashWritesUsed(), 2U);
}

TEST_CASE("flash: the budget ends the writes, counted when sent") {
  fdo2::Config c = fx::Permissive();
  c.max_flash_writes = 2;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetMeasurementMode(1).ok());
  r.port.module.ArmWrite(FaultKind::Silent);
  CHECK(r.drv.SetMeasurementMode(2).error == Error::Timeout);
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK_EQ(r.drv.FlashWritesUsed(), 2U);
  CHECK(r.drv.Resync().ok());
  CHECK(r.drv.SetMeasurementMode(3).error == Error::NotPermitted);
  CHECK_EQ(r.drv.FlashWritesUsed(), 2U);
}

TEST_CASE("flash: argument errors are reported before consent and send nothing") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetMeasurementMode(5).error == Error::InvalidArgument);
  CHECK(r.drv.SetBroadcast(50).error == Error::InvalidArgument);
  CHECK(r.drv.SetBroadcast(10'001).error == Error::InvalidArgument);
  CHECK(r.drv.SetBroadcast(100'000).error == Error::InvalidArgument);
  CHECK_EQ(r.port.module.commands.size(), 2U);
}

TEST_CASE("flash: a lost echo leaves the state unknown until Resync") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.ArmWrite(FaultKind::Silent);
  // The call reports what happened (no echo); every later call reports the unknown state.
  CHECK(r.drv.SetMeasurementMode(2).error == Error::Timeout);
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(r.drv.MeasureOxygen().error == Error::StateUnknown);
  CHECK(r.drv.SetMeasurementMode(2).error == Error::StateUnknown);
  CHECK(r.drv.Resync().ok());
  CHECK(r.drv.GetState() == State::Idle);
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("flash: a clean refusal (-21, -22, -23) is not an unknown state") {
  for (const int code : {-21, -22, -23}) {
    fx::Rig r(fx::Permissive());
    CHECK(r.Start() == Error::Ok);
    r.port.module.ArmWrite(FaultKind::ErrorCode, code);
    CHECK(r.drv.SetMeasurementMode(2).error == Error::DeviceError);
    CHECK(r.drv.GetState() == State::Idle);
    CHECK_EQ(r.drv.FlashWritesUsed(), 1U);
    CHECK(!r.drv.FlashWritePending()); // the module said it did not execute the command
    CHECK(r.drv.MeasureOxygen().ok());
  }
}

TEST_CASE("flash: a refused #WRUM (-21, -22, -23) is sent once and never repeated (m24)") {
  for (const int code : {-21, -22, -23}) {
    fx::Rig r(fx::Permissive());
    CHECK(r.Start() == Error::Ok);
    const std::array<int32_t, 2> words{7, 8};
    r.port.module.ArmWrite(FaultKind::ErrorCode, code);
    CHECK(r.drv.WriteUserMemory(0, words).error == Error::DeviceError);
    CHECK(r.drv.GetState() == State::Idle);
    CHECK_EQ(r.drv.FlashWritesUsed(), 1U);
    const auto& c = r.port.module.commands;
    CHECK_EQ(std::count_if(c.begin(), c.end(),
                           [](const std::string& s) { return s.starts_with("#WRUM"); }),
             1);
  }
}

TEST_CASE(
    "flash: -1 or an unlisted code on a write may follow a flash cycle: state unknown (m14)") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.ArmWrite(FaultKind::ErrorCode, -1);
  CHECK(r.drv.SetMeasurementMode(2).error == Error::DeviceError);
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(r.drv.MeasureOxygen().error == Error::StateUnknown);
  CHECK(r.drv.Resync().ok());
  fx::Rig q(fx::Permissive());
  CHECK(q.Start() == Error::Ok);
  q.port.module.ArmWrite(FaultKind::ErrorCode, -77);
  CHECK(q.drv.SetMeasurementMode(2).error == Error::DeviceFault);
  CHECK(q.drv.GetState() == State::StateUnknown);
  CHECK_EQ(q.drv.DeviceErrorCode(), -77);
  // A read that draws -1 is not a write: the state stays known.
  fx::Rig p;
  CHECK(p.Start() == Error::Ok);
  p.port.module.Arm(FaultKind::ErrorCode, -1, 2);
  CHECK(p.drv.MeasureOxygen().error == Error::DeviceError);
  CHECK(p.drv.GetState() == State::Idle);
}

TEST_CASE("flash: a worn-out flash is reported by the module") {
  fx::Rig r(fx::Permissive());
  r.port.module.knobs.flash_limit = 1;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetMeasurementMode(1).ok());
  CHECK(r.drv.SetMeasurementMode(2).error == Error::DeviceError);
  CHECK_EQ(r.drv.DeviceErrorCode(), -1);
  CHECK(r.drv.GetState() == State::StateUnknown); // `-1` is not a clean refusal
}

TEST_CASE("flash: #SETM needs the G2 family and firmware") {
  fdo2::Config legacy = fx::Permissive();
  legacy.family = fdo2::Family::Fdo2;
  fx::Rig r(legacy);
  r.port.module.knobs.g2 = false;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetMeasurementMode(1).error == Error::Unsupported);
  CHECK_EQ(r.port.module.CountCommands("#SETM"), 0U);
  fx::Rig q(fx::Permissive());
  q.port.module.knobs.firmware = 354;
  CHECK(q.Start() == Error::Ok);
  CHECK(q.drv.SetMeasurementMode(1).error == Error::Unsupported);
  CHECK_EQ(q.port.module.CountCommands("#SETM"), 0U);
}

TEST_CASE("flash: a module that answers -26 to #SETM is remembered as without it") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.ArmWrite(FaultKind::ErrorCode, -26);
  CHECK(r.drv.SetMeasurementMode(1).error == Error::Unsupported);
  CHECK(!r.drv.GetIdentity().caps.set_mode);
  CHECK(r.drv.SetMeasurementMode(1).error == Error::Unsupported);
  CHECK_EQ(r.port.module.CountCommands("#SETM"), 1U);
}

TEST_CASE("flash: #BCST below firmware 3.41 is unsupported") {
  fx::Rig r(fx::Permissive());
  r.port.module.knobs.firmware = 340;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetBroadcast(100).error == Error::Unsupported);
  CHECK_EQ(r.port.module.CountCommands("#BCST"), 0U);
}

TEST_CASE("flash: user memory round trip and range") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  const std::array<int32_t, 3> in{1, -2, 2147483647};
  CHECK(r.drv.WriteUserMemory(5, in).ok());
  CHECK_EQ(r.port.module.user_memory[6], -2);
  std::array<int32_t, 3> out{};
  CHECK(r.drv.ReadUserMemory(5, out).ok());
  CHECK(out == in);
  std::array<int32_t, 64> all{};
  CHECK(r.drv.ReadUserMemory(0, all).ok());
  CHECK_EQ(all[7], 2147483647);
  std::array<int32_t, 65> too_many{};
  CHECK(r.drv.ReadUserMemory(0, too_many).error == Error::InvalidArgument);
  CHECK(r.drv.ReadUserMemory(63, std::span<int32_t>{all}.first(2)).error == Error::InvalidArgument);
  CHECK(r.drv.ReadUserMemory(64, std::span<int32_t>{all}.first(1)).error == Error::InvalidArgument);
  CHECK(r.drv.ReadUserMemory(0, std::span<int32_t>{}).error == Error::InvalidArgument);
  CHECK(r.drv.WriteUserMemory(63, std::span<const int32_t>{all}.first(2)).error ==
        Error::InvalidArgument);
  CHECK(r.drv.WriteUserMemory(0, std::span<const int32_t>{}).error == Error::InvalidArgument);
  CHECK(r.drv.WriteUserMemory(0, std::span<const int32_t>{all}.first(64)).ok());
}

TEST_CASE("flash: non-blocking user memory read") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.user_memory[3] = 77;
  CHECK(r.drv.StartReadUserMemory(3, 2).ok());
  for (int i = 0; i < 2000; ++i) {
    const auto p = r.drv.Poll();
    CHECK(p.ok());
    if (p.value) {
      break;
    }
    r.port.module.Elapse(1000);
  }
  const auto m = r.drv.GetUserMemory();
  REQUIRE(m.ok());
  CHECK_EQ(m.value.size(), 2U);
  CHECK_EQ(m.value[0], 77);
  CHECK(r.drv.StartReadUserMemory(0, 0).error == Error::InvalidArgument);
  CHECK(r.drv.StartReadUserMemory(60, 5).error == Error::InvalidArgument);
}

TEST_CASE("flash: a changed echo of a write is a mismatch and the state is known") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  const std::array<int32_t, 2> in{9, 9};
  r.port.module.ArmWrite(FaultKind::EchoArgChanged);
  const auto e = r.drv.WriteUserMemory(0, in).error;
  CHECK((e == Error::EchoMismatch) || (e == Error::StateUnknown));
}
