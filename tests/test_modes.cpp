/**
 * @file test_modes.cpp
 * @brief The CRC mode (A-8, A-9), the baud rate (A-10), the broadcast policies (A-13 to A-16) and
 * Resync.
 */
#include "fixtures.hpp"

using fdo2::BroadcastPolicy;
using fdo2::CrcMode;
using fdo2::Error;
using fdo2::State;
using fdo2_test::FaultKind;

namespace {

/// Let the model stream every `ms`, before the driver starts.
fx::Rig* Streaming(fx::Rig& r, uint32_t ms) {
  r.port.module.broadcast_ms = ms;
  r.port.module.Reset();
  return &r;
}

} // namespace

TEST_CASE("crc: enabling and disabling is verified by the next #VERS") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  CHECK(!r.drv.CrcActive());
  CHECK(r.drv.SetCrcEnabled(true).ok());
  CHECK(r.drv.CrcActive());
  CHECK(r.port.module.crc_on);
  CHECK(r.drv.MeasureOxygen().ok());
  CHECK(r.drv.SetCrcEnabled(false).ok());
  CHECK(!r.drv.CrcActive());
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("crc: the reply to #CRCE may or may not carry a suffix") {
  for (const bool crc_enable : {true, false}) {
    for (const bool crc_disable : {true, false}) {
      fx::Rig r(fx::Permissive());
      r.port.module.knobs.crce_reply_crc_enable = crc_enable;
      r.port.module.knobs.crce_reply_crc_disable = crc_disable;
      CHECK(r.Start() == Error::Ok);
      CHECK(r.drv.SetCrcEnabled(true).ok());
      CHECK(r.drv.MeasureOxygen().ok());
      CHECK(r.drv.SetCrcEnabled(false).ok());
      CHECK(r.drv.MeasureOxygen().ok());
    }
  }
}

TEST_CASE("crc: Auto learns from #VERS") {
  fx::Rig r;
  r.port.module.crc_on = true;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.CrcActive());
  CHECK(r.drv.MeasureRaw().ok());
}

TEST_CASE("crc: Required refuses a module without it, Off refuses one with it") {
  fdo2::Config need{};
  need.crc = CrcMode::Required;
  fx::Rig r(need);
  CHECK(r.Start() == Error::Crc);
  CHECK(!r.drv.IsInitialized());
  fx::Rig q(need);
  q.port.module.crc_on = true;
  CHECK(q.Start() == Error::Ok);
  fdo2::Config off{};
  off.crc = CrcMode::Off;
  fx::Rig o(off);
  o.port.module.crc_on = true;
  const auto e = o.Start();
  CHECK((e == Error::InvalidReply) || (e == Error::Timeout));
}

TEST_CASE("crc: the mode cannot be switched against the configuration") {
  fdo2::Config off = fx::Permissive();
  off.crc = CrcMode::Off;
  fx::Rig r(off);
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetCrcEnabled(true).error == Error::InvalidArgument);
  fdo2::Config need = fx::Permissive();
  need.crc = CrcMode::Required;
  fx::Rig q(need);
  q.port.module.crc_on = true;
  CHECK(q.Start() == Error::Ok);
  CHECK(q.drv.SetCrcEnabled(false).error == Error::InvalidArgument);
}

TEST_CASE("crc: a module that ignores #CRCE is reported, not believed") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::Silent);
  const auto e = r.drv.SetCrcEnabled(true).error;
  CHECK(e != Error::Ok);
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(r.drv.Resync().ok());
  CHECK(r.drv.CrcActive() == r.port.module.crc_on);
}

TEST_CASE("crc: #CRCE echoed but not applied is found by the #VERS that follows (m24b)") {
  for (const bool target : {true, false}) {
    fx::Rig r(fx::Permissive());
    CHECK(r.Start() == Error::Ok);
    if (!target) {
      CHECK(r.drv.SetCrcEnabled(true).ok());
    }
    r.port.module.knobs.crce_not_applied = true;
    CHECK(r.drv.SetCrcEnabled(target).error == Error::StateUnknown);
    CHECK(r.drv.GetState() == State::StateUnknown);
    CHECK(r.drv.Resync().ok());
    CHECK(r.drv.CrcActive() == r.port.module.crc_on);
  }
}

TEST_CASE("baud: switching and the new rate in use") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetBaudRate(115200).ok());
  CHECK_EQ(r.port.current_baud, 115200U);
  CHECK_EQ(r.port.module.module_baud, 115200U);
  CHECK(r.drv.MeasureOxygen().ok());
  CHECK(r.drv.SetBaudRate(9600).ok());
  CHECK(r.drv.MeasureOxygen().ok());
  CHECK_EQ(r.drv.GetConfig().baud, 9600U);
}

TEST_CASE("baud: an unsupported rate sends nothing") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto n = r.port.module.commands.size();
  CHECK(r.drv.SetBaudRate(9601).error == Error::InvalidArgument);
  CHECK(r.drv.SetBaudRate(0).error == Error::InvalidArgument);
  CHECK_EQ(r.port.module.commands.size(), n);
}

TEST_CASE("baud: a module slower to listen than two character times needs Resync") {
  fx::Rig r;
  r.port.module.knobs.baud_switch_us = 300'000;
  CHECK(r.Start() == Error::Ok);
  const auto e = r.drv.SetBaudRate(57600).error;
  if (e != Error::Ok) {
    CHECK(r.drv.GetState() == State::StateUnknown);
    CHECK(r.drv.Resync().ok());
  }
  CHECK(r.drv.MeasureOxygen().ok());
  fx::Rig q;
  CHECK(q.Start() == Error::Ok);
  q.port.module.knobs.baud_switch_us = 1'000;
  CHECK(q.drv.SetBaudRate(57600).ok());
}

TEST_CASE("baud: a failing hook while switching is a bus error") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.port_faults.set_baud_fails = true;
  CHECK(r.drv.SetBaudRate(57600).error == Error::Bus);
}

TEST_CASE("baud: a lost echo is resolved by Resync at the candidate rates") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::Silent);
  const auto e = r.drv.SetBaudRate(57600).error;
  CHECK(e != Error::Ok);
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(r.drv.Resync().ok());
  CHECK(r.drv.GetState() == State::Idle);
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("baud: a power cycle returns the module to 19200") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetBaudRate(115200).ok());
  CHECK(r.drv.PowerCycle().ok());
  CHECK_EQ(r.port.module.module_baud, 19200U);
  CHECK_EQ(r.port.current_baud, 19200U);
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("resync: from Idle it re-reads the version") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto n = r.port.module.CountCommands("#VERS");
  CHECK(r.drv.Resync().ok());
  CHECK_EQ(r.port.module.CountCommands("#VERS"), n + 1U);
}

TEST_CASE("resync: nothing answering is a timeout at every candidate") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.port_faults.write_fails = false;
  r.port.module.Arm(FaultKind::Silent, 0, 20);
  CHECK(r.drv.Resync().error == Error::Timeout);
}

TEST_CASE("resync: before start-up it is NotInitialized") {
  fx::Rig r;
  CHECK(r.drv.Resync().error == Error::NotInitialized);
}

TEST_CASE("broadcast: Refuse is the default and leaves the driver uninitialised") {
  fx::Rig r;
  Streaming(r, 200);
  CHECK(r.Start() == Error::BroadcastActive);
  CHECK(!r.drv.IsInitialized());
  CHECK(r.drv.MeasureOxygen().error == Error::NotInitialized);
}

TEST_CASE("broadcast: Stream mode decodes the frames") {
  fdo2::Config c{};
  c.broadcast = BroadcastPolicy::Stream;
  fx::Rig r(c);
  Streaming(r, 200);
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.GetState() == State::Streaming);
  CHECK(r.drv.MeasureOxygen().error == Error::Busy);
  bool got = false;
  for (int i = 0; (i < 3000) && !got; ++i) {
    const auto p = r.drv.Poll();
    CHECK(p.ok());
    got = p.value && r.drv.GetRaw().ok();
    r.port.module.Elapse(1000);
  }
  CHECK(got);
  CHECK(r.drv.GetCounters().broadcast_lines > 0U);
}

TEST_CASE("broadcast: Disable needs consent and writes #BCST 0") {
  fdo2::Config c{};
  c.broadcast = BroadcastPolicy::Disable;
  fx::Rig r(c);
  Streaming(r, 200);
  CHECK(r.Start() == Error::NotPermitted);
  CHECK_EQ(r.port.module.CountCommands("#BCST"), 0U);
  fdo2::Config d = fx::Permissive();
  d.broadcast = BroadcastPolicy::Disable;
  fx::Rig q(d);
  Streaming(q, 200);
  CHECK(q.Start() == Error::Ok);
  CHECK_EQ(q.port.module.broadcast_ms, 0U);
  CHECK(q.drv.GetState() == State::Idle);
  CHECK(q.drv.MeasureOxygen().ok());
}

TEST_CASE("broadcast: setting an interval and probing") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  const auto none = r.drv.ProbeBroadcast(1200);
  CHECK(none.ok());
  CHECK(!none.value);
  CHECK(r.drv.SetBroadcast(500).ok());
  CHECK_EQ(r.port.module.broadcast_ms, 500U);
  CHECK(r.drv.GetState() == State::Streaming);
  CHECK(r.drv.ProbeBroadcast(1200).error == Error::Busy);
  fx::Rig q;
  CHECK(q.Start() == Error::Ok);
  q.port.module.broadcast_ms = 200;
  q.port.module.Reset();
  const auto some = q.drv.ProbeBroadcast(2500);
  CHECK(some.ok());
  CHECK(some.value);
  CHECK(q.drv.ProbeBroadcast(0).error == Error::InvalidArgument);
  CHECK(q.drv.ProbeBroadcast(12'001).error == Error::InvalidArgument);
}

TEST_CASE("broadcast: the vendor workaround waits for a frame before #BCST 0") {
  fdo2::Config c = fx::Permissive();
  c.broadcast = BroadcastPolicy::Stream;
  fx::Rig r(c);
  r.port.module.knobs.bug351 = true;
  Streaming(r, 200);
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.DisableBroadcast().ok());
  CHECK_EQ(r.port.module.broadcast_ms, 0U);
  CHECK(r.drv.GetState() == State::Idle);
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("broadcast: the listening window at start-up finds a stream") {
  fdo2::Config c{};
  c.broadcast_listen_ms = 1500;
  fx::Rig r(c);
  Streaming(r, 1000);
  CHECK(r.Start() == Error::BroadcastActive);
  // Without the window a slow stream goes unseen by the start-up (docs/configuration.md).
  fx::Rig q;
  Streaming(q, 5000);
  CHECK(q.Start() == Error::Ok);
}
