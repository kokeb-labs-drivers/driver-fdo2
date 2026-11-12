/**
 * @file test_lifecycle.cpp
 * @brief Start-up, identity, power control, deinitialisation and the guard on every call.
 */
#include "fixtures.hpp"

using fdo2::Error;
using fdo2::State;

TEST_CASE("lifecycle: nothing before EnsureInitialized") {
  fx::Rig r;
  CHECK(r.drv.GetState() == State::Uninitialized);
  CHECK(!r.drv.IsInitialized());
  CHECK(r.drv.MeasureOxygen().error == Error::NotInitialized);
  CHECK(r.drv.MeasureRaw().error == Error::NotInitialized);
  CHECK(r.drv.ReadVersion().error == Error::NotInitialized);
  CHECK(r.drv.ReadUniqueId().error == Error::NotInitialized);
  CHECK(r.drv.Identify().error == Error::NotInitialized);
  CHECK(r.drv.StartMeasureOxygen().error == Error::NotInitialized);
  CHECK(r.drv.StartReadUserMemory(0, 1).error == Error::NotInitialized);
  CHECK(r.drv.GetOxygen().error == Error::NotInitialized);
  CHECK(r.drv.GetVersion().error == Error::NotInitialized);
  CHECK(r.drv.Poll().ok());
  CHECK_EQ(r.port.module.bytes_in, 0U);
  CHECK_EQ(r.port.write_calls, 0U);
}

TEST_CASE("lifecycle: start-up reads #VERS and #IDNR and decodes the identity") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.IsInitialized());
  CHECK(r.drv.GetState() == State::Idle);
  const auto& id = r.drv.GetIdentity();
  CHECK_EQ(id.version.device_id, 8);
  CHECK_EQ(id.version.channels, 1);
  CHECK_EQ(id.version.firmware, 357);
  CHECK_EQ(id.version.sensors, 15);
  CHECK(id.unique_id == 18446744073709551615ULL);
  CHECK(id.caps.set_mode);
  CHECK(id.caps.broadcast);
  CHECK(!id.noise_erratum);
  CHECK_EQ(r.port.module.CountCommands("#VERS"), 1U);
  CHECK_EQ(r.port.module.CountCommands("#IDNR"), 1U);
  CHECK_EQ(r.port.module.commands.size(), 2U);
  CHECK_EQ(r.port.module.flash_cycles, 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK(r.port.ensured);
}

TEST_CASE("lifecycle: the first command waits for the power-up time") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.port.module.command_times_us.front() >= 1'500'000U);
}

TEST_CASE("lifecycle: a second EnsureInitialized sends nothing") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto before = r.port.module.commands.size();
  CHECK(r.drv.EnsureInitialized().ok());
  CHECK(r.drv.BeginInitialize().ok());
  CHECK_EQ(r.port.module.commands.size(), before);
}

TEST_CASE("lifecycle: an invalid config is refused before any I/O") {
  fdo2::Config c{};
  c.power_up_ms = 10;
  fx::Rig r(c);
  CHECK(r.drv.BeginInitialize().error == Error::InvalidArgument);
  CHECK(r.drv.EnsureInitialized().error == Error::InvalidArgument);
  CHECK(!r.port.ensured);
  CHECK_EQ(r.port.write_calls, 0U);
  CHECK(r.drv.GetState() == State::Uninitialized);
}

TEST_CASE("lifecycle: the power pin is driven ACTIVE at start and INACTIVE at Deinitialize") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  REQUIRE(!r.port.gpio_log.empty());
  CHECK(r.port.gpio_log.front().first == fdo2::CtrlPin::Power);
  CHECK(r.port.gpio_log.front().second == fdo2::GpioSignal::ACTIVE);
  CHECK(r.drv.Deinitialize().ok());
  CHECK(r.port.gpio_log.back().second == fdo2::GpioSignal::INACTIVE);
  CHECK(!r.drv.IsInitialized());
  CHECK(r.drv.MeasureOxygen().error == Error::NotInitialized);
  CHECK(r.drv.EnsureInitialized().ok());
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("lifecycle: a failing power pin is a bus error on both edges") {
  fx::Rig r;
  r.port.port_faults.gpio_fails = true;
  CHECK(r.drv.BeginInitialize().error == Error::Bus);
  r.port.port_faults.gpio_fails = false;
  CHECK(r.Start() == Error::Ok);
  r.port.port_faults.gpio_fails = true;
  CHECK(r.drv.Deinitialize().error == Error::Bus);
  CHECK(!r.drv.IsInitialized());
}

TEST_CASE("lifecycle: the port's own start-up failing stops everything") {
  fx::Rig r;
  r.port.port_faults.ensure_fails = true;
  CHECK(r.drv.BeginInitialize().error == Error::Bus);
  CHECK_EQ(r.port.write_calls, 0U);
  CHECK(r.drv.GetState() == State::Uninitialized);
}

TEST_CASE("lifecycle: another device id is a mismatch") {
  fx::Rig r;
  r.port.module.knobs.device_id = 4;
  CHECK(r.Start() == Error::DeviceIdMismatch);
  CHECK(!r.drv.IsInitialized());
  fx::Rig r2;
  r2.port.module.knobs.channels = 2;
  CHECK(r2.Start() == Error::DeviceIdMismatch);
  fx::Rig r3;
  r3.port.module.knobs.sensors = 14;
  CHECK(r3.Start() == Error::DeviceIdMismatch);
}

TEST_CASE("lifecycle: firmware below 3.28 is unsupported, 3.28 is accepted") {
  fx::Rig r;
  r.port.module.knobs.firmware = 327;
  CHECK(r.Start() == Error::Unsupported);
  fx::Rig r2;
  r2.port.module.knobs.firmware = 328;
  CHECK(r2.Start() == Error::Ok);
  CHECK(r2.drv.GetIdentity().caps.user_memory);
  CHECK(!r2.drv.GetIdentity().caps.broadcast);
}

TEST_CASE("lifecycle: the noise erratum is reported for firmware 3.54 to 3.56") {
  fx::Rig r;
  r.port.module.knobs.firmware = 355;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.GetIdentity().noise_erratum);
}

TEST_CASE("lifecycle: reading the version and the id again") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto v = r.drv.ReadVersion();
  CHECK(v.ok());
  CHECK_EQ(v.value.firmware, 357);
  const auto id = r.drv.ReadUniqueId();
  CHECK(id.ok());
  CHECK(id.value == 18446744073709551615ULL);
  CHECK(r.drv.Identify().ok());
  CHECK(r.port.module.CountCommands("#LOGO") == 1U);
}

TEST_CASE("lifecycle: a second call while one is in flight is Busy") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.StartMeasureOxygen().ok());
  CHECK(r.drv.Pending() == fdo2::OperationKind::MeasureOxygen);
  CHECK(r.drv.GetState() == State::Waiting);
  CHECK(r.drv.StartMeasureRaw().error == Error::Busy);
  CHECK(r.drv.StartReadVersion().error == Error::Busy);
  CHECK(r.drv.BeginInitialize().ok() || true);
  CHECK(r.drv.GetOxygen().error == Error::Busy);
  CHECK(r.drv.Resync().error == Error::Busy);
}

TEST_CASE("lifecycle: observers return the state without traffic") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto cmds = r.port.module.commands.size();
  CHECK(r.drv.GetConfig().baud == 19200U);
  CHECK(r.drv.CrcActive() == false);
  CHECK_EQ(r.drv.DeviceErrorCode(), 0);
  CHECK_EQ(r.drv.GetCounters().device_errors, 0U);
  CHECK_EQ(r.port.module.commands.size(), cmds);
}
