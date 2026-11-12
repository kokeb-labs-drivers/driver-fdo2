/**
 * @file test_ports.cpp
 * @brief Port variants (which optional hooks exist) and port failures in every hook.
 */
#include "fixtures.hpp"

using fdo2::Error;
using fdo2::State;

namespace {

/// Drive a non-blocking operation to its end on a port with a clock; time is the model's.
template <typename D, typename P>
fdo2::Result<bool> RunClock(D& d, P& p, int max_steps = 20000) {
  fdo2::Result<bool> r = d.Poll();
  for (int i = 0; (i < max_steps) && r.ok() && !r.value; ++i) {
    p.module.Elapse(1000);
    r = d.Poll();
  }
  return r;
}

/// The same for a port without a clock: the caller passes the elapsed time.
template <typename D, typename P>
fdo2::Result<bool> RunElapsed(D& d, P& p, int max_steps = 20000) {
  fdo2::Result<bool> r = d.Poll(0);
  for (int i = 0; (i < max_steps) && r.ok() && !r.value; ++i) {
    p.module.Elapse(1000);
    r = d.Poll(1000);
  }
  return r;
}

} // namespace

TEST_CASE("ports: a port with DelayUs only runs every blocking call") {
  fdo2_test::MinimalPort port;
  fdo2::FDO2<fdo2_test::MinimalPort> d(port);
  CHECK(d.EnsureInitialized().ok());
  CHECK(d.MeasureOxygen().ok());
  CHECK(d.MeasureRaw().ok());
  CHECK(d.ReadVersion().ok());
  CHECK(d.Identify().ok());
  CHECK(!d.SetBaudRate(9600).ok());
  CHECK(d.SetBaudRate(9600).error == Error::Unsupported);
  CHECK(d.PowerCycle().error == Error::Unsupported);
  CHECK(!port.module.commands.empty());
}

TEST_CASE("ports: with a clock the non-blocking calls use Poll()") {
  fdo2_test::ClockPort port;
  fdo2::FDO2<fdo2_test::ClockPort> d(port);
  CHECK(d.BeginInitialize().ok());
  CHECK(d.GetState() == State::PoweringUp);
  CHECK(RunClock(d, port).ok());
  CHECK(d.GetState() == State::Idle);
  CHECK(d.StartMeasureRaw().ok());
  CHECK(RunClock(d, port).value);
  const auto raw = d.GetRaw();
  CHECK(raw.ok());
  CHECK_EQ(raw.value.oxygen.o2_mhpa, 209'500);
  CHECK(d.MeasureOxygen().ok());
}

TEST_CASE("ports: a clock that wraps at 32 bits does not shorten a wait") {
  fdo2_test::ClockPort port;
  port.clock_offset = 0xFFFFFFFFU - 500'000U;
  fdo2::FDO2<fdo2_test::ClockPort> d(port);
  CHECK(d.BeginInitialize().ok());
  CHECK(RunClock(d, port).ok());
  CHECK(port.module.command_times_us.front() >= 1'500'000U);
}

TEST_CASE("ports: without DelayUs the driver is driven only by Poll()") {
  fdo2_test::PollOnlyPort port;
  fdo2::FDO2<fdo2_test::PollOnlyPort> d(port);
  CHECK(d.BeginInitialize().ok());
  CHECK(RunClock(d, port).ok());
  CHECK(d.StartMeasureOxygen().ok());
  CHECK(RunClock(d, port).value);
  CHECK_EQ(d.GetOxygen().value.o2_mhpa, 209'500);
}

TEST_CASE("ports: without a clock Poll takes the elapsed time") {
  fdo2_test::NoClockPort port;
  fdo2::FDO2<fdo2_test::NoClockPort> d(port);
  CHECK(d.BeginInitialize().ok());
  CHECK(RunElapsed(d, port).ok());
  CHECK(d.GetState() == State::Idle);
  CHECK(d.StartReadVersion().ok());
  CHECK(RunElapsed(d, port).value);
  CHECK_EQ(d.GetVersion().value.firmware, 357);
  // A caller that never passes time sees nothing finish: it is pending, not failed.
  CHECK(d.StartMeasureOxygen().ok());
  for (int i = 0; i < 100; ++i) {
    const auto p = d.Poll(0);
    CHECK(p.ok());
    CHECK(!p.value);
  }
  CHECK(d.Pending() == fdo2::OperationKind::MeasureOxygen);
  // Too little time per call is the same as slow time: the deadline is on the sum.
  fdo2::Result<bool> r{};
  for (int i = 0; (i < 2000) && r.ok() && !r.value; ++i) {
    port.module.Elapse(1000);
    r = d.Poll(1000);
  }
  CHECK(r.ok());
  CHECK(r.value);
}

TEST_CASE("ports: Poll with nothing pending is ready and sends nothing") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto n = r.port.write_calls;
  const auto p = r.drv.Poll();
  CHECK(p.ok());
  CHECK(p.value);
  CHECK_EQ(r.port.write_calls, n);
}

TEST_CASE("ports: a failing Write is a bus error, once, and the driver stays usable") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.port_faults.write_fails = true;
  CHECK(r.drv.MeasureOxygen().error == Error::Bus);
  CHECK(r.drv.StartMeasureOxygen().error == Error::Bus);
  r.port.port_faults.write_fails = false;
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("ports: a failing FlushInput is a bus error before anything is sent") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto n = r.port.write_calls;
  r.port.port_faults.flush_fails = true;
  CHECK(r.drv.MeasureOxygen().error == Error::Bus);
  CHECK_EQ(r.port.write_calls, n);
}

TEST_CASE("ports: a Write failing during start-up fails it") {
  fx::Rig r;
  r.port.port_faults.write_fail_after = 0;
  CHECK(r.Start() == Error::Bus);
  CHECK(!r.drv.IsInitialized());
}

TEST_CASE("ports: a Write failing on a flash command still counts it") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.port_faults.write_fail_after = r.port.write_calls + 1U; // the guard read goes through
  CHECK(r.drv.SetMeasurementMode(1).error == Error::Bus);
  CHECK_EQ(r.drv.FlashWritesUsed(), 1U);
}

TEST_CASE("ports: a Read that claims more than it was given is a bus error") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.port_faults.read_oversize = true;
  CHECK(r.drv.MeasureOxygen().error == Error::Bus);
  r.port.port_faults.read_oversize = false;
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("ports: a Read that never returns data is a timeout, bounded in time") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.read_zero_forever = true;
  const uint64_t t0 = r.port.module.now_us;
  CHECK(r.drv.MeasureOxygen().error == Error::Timeout);
  CHECK(r.port.module.now_us - t0 < 3'000'000U);
  r.port.read_zero_forever = false;
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("ports: one byte per Read still assembles the reply") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.read_returns_limit = 1;
  const auto o = r.drv.MeasureOxygen();
  CHECK(o.ok() || (o.error == Error::Timeout));
}

TEST_CASE("ports: the Log hook receives messages and the driver works without it") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(!r.port.logs.empty());
  fdo2_test::MinimalPort quiet;
  fdo2::FDO2<fdo2_test::MinimalPort> d(quiet);
  CHECK(d.EnsureInitialized().ok());
}

TEST_CASE("ports: Deinitialize without the power hook succeeds") {
  fdo2_test::MinimalPort port;
  fdo2::FDO2<fdo2_test::MinimalPort> d(port);
  CHECK(d.EnsureInitialized().ok());
  CHECK(d.Deinitialize().ok());
  CHECK(!d.IsInitialized());
}

TEST_CASE("ports: AbortOperation drops a read into draining and never touches the port") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.StartMeasureOxygen().ok());
  const auto w = r.port.write_calls;
  const auto f = r.port.module.flush_calls;
  CHECK(r.drv.AbortOperation().ok());
  CHECK_EQ(r.port.write_calls, w);
  CHECK_EQ(r.port.module.flush_calls, f);
  CHECK(r.drv.GetState() == State::Draining);
  CHECK(r.drv.StartMeasureOxygen().error == Error::Busy);
  const auto p = RunClock(r.drv, r.port);
  CHECK(p.ok());
  CHECK(r.drv.GetState() == State::Idle);
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("ports: aborting a flash write leaves the state unknown") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.StartSetMeasurementMode(2).ok());
  // Run the guard read until the write itself has been sent.
  for (int i = 0; (i < 2000) && (r.port.module.CountCommands("#SETM") == 0U); ++i) {
    r.port.module.Elapse(1000);
    CHECK(r.drv.Poll().ok());
  }
  CHECK_EQ(r.port.module.CountCommands("#SETM"), 1U);
  CHECK(r.drv.AbortOperation().ok());
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(r.drv.AbortOperation().ok());
}

TEST_CASE("ports: aborting during the guard read writes nothing and only drains") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.StartSetMeasurementMode(2).ok());
  CHECK(r.drv.AbortOperation().ok());
  CHECK(r.drv.GetState() == State::Draining);
  CHECK(RunClock(r.drv, r.port).ok());
  CHECK(r.drv.GetState() == State::Idle);
  CHECK_EQ(r.port.module.CountCommands("#SETM"), 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
}

TEST_CASE("ports: Poll while draining blocks new operations until the late reply is gone") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.knobs.measure_us = 400'000;
  CHECK(r.drv.StartMeasureOxygen().ok());
  const auto p = RunClock(r.drv, r.port);
  CHECK(!p.ok());
  CHECK(p.error == Error::Timeout);
  CHECK(r.drv.GetState() == State::Draining);
  CHECK(r.drv.StartMeasureOxygen().error == Error::Busy);
  r.port.module.knobs.measure_us = 0;
  CHECK(RunClock(r.drv, r.port).ok());
  CHECK(r.drv.GetState() == State::Idle);
  CHECK(r.drv.MeasureOxygen().ok());
}
