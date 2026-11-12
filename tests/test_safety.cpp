/**
 * @file test_safety.cpp
 * @brief The safety contracts: start-up never ends in `Idle` without its identity check (B1), no
 * power cut during a flash write (M2), `Resync()` in both CRC modes (M4), no flash write after an
 * unseen reset or on a line that only echoes (the guard), identity re-checks, results that do not
 * outlive a failed call, and the stream liveness.
 */
#include <algorithm>
#include <array>

#include "fixtures.hpp"

using fdo2::BroadcastPolicy;
using fdo2::Error;
using fdo2::State;
using fdo2_test::FaultKind;

namespace {

/// Poll with the model's clock until `done()` or the step limit; true when `done()` held.
template <typename F>
bool PumpUntil(fx::Rig& r, F done, int max_steps = 30000) {
  for (int i = 0; i < max_steps; ++i) {
    if (done()) {
      return true;
    }
    r.port.module.Elapse(1000);
    (void)r.drv.Poll();
  }
  return done();
}

/// Poll until the operation ends; the result of the last poll.
fdo2::Result<bool> Finish(fx::Rig& r, int max_steps = 60000) {
  fdo2::Result<bool> p = r.drv.Poll();
  for (int i = 0; (i < max_steps) && p.ok() && !p.value; ++i) {
    r.port.module.Elapse(1000);
    p = r.drv.Poll();
  }
  return p;
}

uint32_t Sent(const fx::Rig& r, const char* header) {
  return r.port.module.CountCommands(header);
}

/// What an abort must leave behind in start-up: not initialised, and the next start sends #VERS
/// and #IDNR again (no `Ok` without traffic).
void CheckRestarts(fx::Rig& r) {
  CHECK(!r.drv.IsInitialized());
  CHECK(r.drv.GetState() != State::Idle);
  const uint32_t vers = Sent(r, "#VERS");
  const uint32_t idnr = Sent(r, "#IDNR");
  r.port.module.knobs.power_up_us = 100'000;
  CHECK(r.drv.EnsureInitialized().ok());
  CHECK(r.drv.IsInitialized());
  CHECK(Sent(r, "#VERS") > vers);
  CHECK(Sent(r, "#IDNR") > idnr);
  CHECK(r.drv.MeasureOxygen().ok());
}

/// A clock that stops at `freeze_us` and reads that return nothing: a start-up that can only end
/// through the bound of the blocking call.
class StuckPort : public fdo2_test::PortBase<StuckPort> {
public:
  uint64_t freeze_us{1'600'000};
  void DelayUs(uint32_t us) noexcept {
    module.Elapse(us);
  }
  uint32_t NowUs() noexcept {
    return static_cast<uint32_t>(module.now_us < freeze_us ? module.now_us : freeze_us);
  }
};

} // namespace

// ---- B1: abort or timeout during Initialize --------------------------------------------------

TEST_CASE("B1: abort while powering up") {
  fx::Rig r;
  CHECK(r.drv.BeginInitialize().ok());
  CHECK(r.drv.AbortOperation().ok());
  CheckRestarts(r);
}

TEST_CASE("B1: abort while listening for a stream") {
  fdo2::Config c{};
  c.broadcast_listen_ms = 600;
  fx::Rig r(c);
  r.port.module.Reset();
  CHECK(r.drv.BeginInitialize().ok());
  CHECK(PumpUntil(r, [&] { return r.port.module.now_us > 1'700'000U; }));
  CHECK(Sent(r, "#VERS") == 0U);
  CHECK(r.drv.AbortOperation().ok());
  CheckRestarts(r);
}

TEST_CASE("B1: abort while #VERS is pending") {
  fx::Rig r;
  r.port.module.Reset();
  CHECK(r.drv.BeginInitialize().ok());
  CHECK(PumpUntil(r, [&] { return Sent(r, "#VERS") == 1U; }));
  CHECK(r.drv.GetState() == State::Waiting);
  CHECK(r.drv.AbortOperation().ok());
  CHECK(r.drv.GetState() == State::Uninitialized);
  CHECK(!r.drv.IsInitialized());
  CheckRestarts(r);
}

TEST_CASE("B1: abort while #IDNR is pending") {
  fx::Rig r;
  r.port.module.Reset();
  CHECK(r.drv.BeginInitialize().ok());
  CHECK(PumpUntil(r, [&] { return Sent(r, "#IDNR") == 1U; }));
  CHECK(r.drv.AbortOperation().ok());
  CHECK(r.drv.GetState() == State::Uninitialized);
  CheckRestarts(r);
}

TEST_CASE("B1: abort while the stream is being switched off") {
  fdo2::Config c = fx::Permissive();
  c.broadcast = BroadcastPolicy::Disable;
  fx::Rig r(c);
  r.port.module.broadcast_ms = 300;
  r.port.module.Reset();
  CHECK(r.drv.BeginInitialize().ok());
  CHECK(PumpUntil(r, [&] { return Sent(r, "#BCST") == 1U; }));
  CHECK(r.drv.AbortOperation().ok());
  CHECK(!r.drv.IsInitialized());
  CHECK(r.drv.GetState() != State::Idle);
  CHECK(r.drv.FlashWritePending()); // M2: the flash write may still run
  CHECK(r.drv.Deinitialize().error == Error::Busy);
  CHECK(r.port.gpio_log.back().second == fdo2::GpioSignal::ACTIVE); // the supply stayed on
  CHECK(!r.port.module.flash_corrupted);
  CHECK(Finish(r).ok()); // nothing pending: the poll just passes the time
  CHECK(r.drv.EnsureInitialized().ok());
  CHECK(r.drv.IsInitialized());
  CHECK(!r.port.module.flash_corrupted);
}

TEST_CASE("B1: abort while a failed read is drained before its repeat") {
  fx::Rig r;
  r.port.module.Reset();
  r.port.module.Arm(FaultKind::Silent);
  CHECK(r.drv.BeginInitialize().ok());
  CHECK(PumpUntil(r, [&] { return r.drv.GetState() == State::Draining; }));
  CHECK(r.drv.AbortOperation().ok());
  CHECK(!r.drv.IsInitialized());
  CHECK(r.drv.GetState() == State::Uninitialized);
  // The drain must not carry the aborted start-up into Idle.
  (void)Finish(r);
  CHECK(r.drv.GetState() == State::Uninitialized);
  CheckRestarts(r);
}

TEST_CASE("B1: the bound of the blocking call ends in Uninitialized, never in Idle") {
  StuckPort port;
  port.read_zero_forever = true;
  fdo2::FDO2<StuckPort> d(port);
  CHECK(d.EnsureInitialized().error == fdo2::Error::Timeout);
  CHECK(!d.IsInitialized());
  CHECK(d.GetState() == State::Uninitialized);
  CHECK(d.EnsureInitialized().error == fdo2::Error::Timeout); // asks again, never `Ok` unseen
  CHECK(!d.IsInitialized());
  port.read_zero_forever = false;
  port.freeze_us = 0xFFFF'FFFFU;
  const auto sent = port.module.CountCommands("#VERS");
  CHECK(d.EnsureInitialized().ok());
  CHECK(port.module.CountCommands("#VERS") > sent);
  CHECK(d.MeasureOxygen().ok());
}

TEST_CASE("B1: a timeout in each start-up command fails it and the next start asks again") {
  for (int which = 0; which < 2; ++which) {
    fdo2::Config c{};
    c.retries = 0;
    fx::Rig r(c);
    r.port.module.Reset();
    r.port.module.Arm(FaultKind::None, 0, which); // let #VERS through for the second case
    r.port.module.Arm(FaultKind::Silent);
    CHECK(r.drv.EnsureInitialized().error == Error::Timeout);
    CHECK(!r.drv.IsInitialized());
    CHECK(r.drv.GetState() == State::Uninitialized);
    CheckRestarts(r);
  }
}

// ---- M2: no power cut during a flash write ---------------------------------------------------

namespace {
/// Start a flash write and run it until the write command itself is on the wire.
void SendWrite(fx::Rig& r) {
  CHECK(r.drv.StartSetMeasurementMode(1).ok());
  CHECK(PumpUntil(r, [&] { return Sent(r, "#SETM") == 1U; }));
}
} // namespace

TEST_CASE("M2: Deinitialize is refused while a flash write is pending") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.knobs.flash_write_us = 1'500'000; // a slow write
  SendWrite(r);
  CHECK(r.drv.FlashWritePending());
  CHECK(r.drv.AbortOperation().ok());
  CHECK(r.drv.GetState() == State::StateUnknown);
  const auto gpio = r.port.gpio_log.size();
  CHECK(r.drv.Deinitialize().error == Error::Busy);
  CHECK_EQ(r.port.gpio_log.size(), gpio); // the supply was not touched
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(!r.port.module.flash_corrupted);
  // Time passes through Poll(); after the allowance the write is over for good.
  r.port.module.Elapse(5'000'000);
  CHECK(r.drv.Poll().ok());
  CHECK(!r.drv.FlashWritePending());
  CHECK(r.drv.Deinitialize().ok());
  CHECK(!r.port.module.flash_corrupted);
  CHECK(r.port.gpio_log.back().second == fdo2::GpioSignal::INACTIVE);
}

TEST_CASE("M2: PowerCycle waits out a pending flash write before it cuts the supply") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.knobs.flash_write_us = 1'500'000;
  SendWrite(r);
  CHECK(r.drv.AbortOperation().ok());
  CHECK(r.drv.PowerCycle().ok());
  CHECK(!r.port.module.flash_corrupted);
  CHECK(!r.drv.FlashWritePending());
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("M2: the echo ends the pending state at once") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  CHECK(!r.drv.FlashWritePending());
  CHECK(r.drv.StartSetMeasurementMode(1).ok());
  CHECK(Finish(r).ok());
  CHECK(!r.drv.FlashWritePending());
  CHECK(r.drv.Deinitialize().ok());
}

TEST_CASE("M2: a cut after a lost echo is refused until the allowance is over") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  SendWrite(r);
  CHECK(r.drv.FlashWritePending());
  CHECK(r.drv.Deinitialize().error == Error::Busy);
  CHECK(r.drv.PowerCycle().ok()); // waits, then cycles
  CHECK(!r.port.module.flash_corrupted);
}

// ---- M4 and m13: Resync ------------------------------------------------------------------

TEST_CASE("M4: Resync recovers from a lost #CRCE 0 echo") {
  fx::Rig r(fx::Permissive());
  r.port.module.crc_on = true;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.CrcActive());
  r.port.module.ArmWrite(FaultKind::Silent);
  CHECK(r.drv.SetCrcEnabled(false).error == Error::Timeout);
  CHECK(!r.port.module.crc_on); // the module did switch the CRC off
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(r.drv.Resync().ok());
  CHECK(!r.drv.CrcActive());
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("M4: Resync recovers from a lost #CRCE 1 echo") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.ArmWrite(FaultKind::Silent);
  CHECK(r.drv.SetCrcEnabled(true).error == Error::Timeout);
  CHECK(r.port.module.crc_on);
  CHECK(r.drv.Resync().ok());
  CHECK(r.drv.CrcActive());
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("M4: with CrcMode::Required Resync does not try the other mode") {
  fdo2::Config c = fx::Permissive();
  c.crc = fdo2::CrcMode::Required;
  fx::Rig r(c);
  r.port.module.crc_on = true;
  CHECK(r.Start() == Error::Ok);
  r.port.module.crc_on = false; // a module that dropped the CRC is not accepted
  r.port.module.UnseenReset();
  r.port.module.Elapse(2'000'000);
  CHECK(!r.drv.Resync().ok());
  CHECK(r.drv.GetState() == State::StateUnknown);
}

TEST_CASE("m13: Resync works in StateUnknown while the module streams") {
  fdo2::Config c = fx::Permissive();
  c.broadcast = BroadcastPolicy::Stream;
  fx::Rig r(c);
  r.port.module.broadcast_ms = 300;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.GetState() == State::Streaming);
  r.port.module.ArmWrite(FaultKind::Silent);
  CHECK(r.drv.DisableBroadcast().error == Error::Timeout);
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(r.drv.Resync().ok()); // not Busy: this is when it is needed
  CHECK(r.drv.GetState() == State::Idle);
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("m13: Resync is still Busy while an operation is pending or the stream is known") {
  fdo2::Config c{};
  c.broadcast = BroadcastPolicy::Stream;
  fx::Rig r(c);
  r.port.module.broadcast_ms = 300;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.Resync().error == Error::Busy); // Streaming, not StateUnknown
}

// ---- The guard: no flash write after an unseen reset or on an echoing line
// -----------------------

TEST_CASE("guard: a module that reset unseen gets no flash write") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.SetBaudRate(57'600).ok());
  r.port.module.UnseenReset(); // back at 19200, silent for the power-up time
  const auto before = r.port.module.flash_cycles;
  CHECK(!r.drv.SetMeasurementMode(1).ok());
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK_EQ(Sent(r, "#SETM"), 0U);
  CHECK_EQ(r.port.module.flash_cycles, before);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK(r.drv.Resync().ok());
  CHECK(r.drv.SetMeasurementMode(1).ok());
  CHECK_EQ(Sent(r, "#SETM"), 1U);
}

TEST_CASE("guard: every flash-writing call asks the module first") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 0.0);
  const std::array<int32_t, 1> word{7};
  CHECK(r.drv.SetCrcEnabled(true).ok());
  CHECK(r.drv.SetCrcEnabled(false).ok());
  CHECK(r.drv.SetMeasurementMode(3).ok());
  CHECK(r.drv.WriteUserMemory(0, word).ok());
  CHECK(r.drv.SetBroadcast(200).ok());
  CHECK(r.drv.DisableBroadcast().ok());
  CHECK(r.drv.CalibrateZero(300).ok());
  r.port.module.scenario.o2_hpa = 209.5;
  CHECK(r.drv.CalibrateHigh(209'500, 300).ok());
  const auto& c = r.port.module.commands;
  const auto is_flash = [](const std::string& s) {
    const std::array<const char*, 6> heads{"#CRCE", "#WRUM", "#CALO", "#CAHI", "#BCST", "#SETM"};
    return std::ranges::any_of(heads, [&s](const char* h) { return s.starts_with(h); });
  };
  uint32_t checked = 0;
  for (std::size_t i = 2U; i < c.size(); ++i) {
    if (!is_flash(c[i])) {
      continue;
    }
    ++checked;
    if (c[i] == "#BCST 0") {
      continue; // switching a running stream off: the frames prove the module is alive
    }
    const bool calibration = c[i].starts_with("#CALO") || c[i].starts_with("#CAHI");
    CHECK(c[i - 1U] == (calibration ? "#MOXY" : "#VERS"));
  }
  CHECK_EQ(checked, 8U);
}

TEST_CASE("guard: a line that only echoes cannot acknowledge a flash write (m15)") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 0.0);
  r.port.module.loopback = true; // TX shorted to RX
  CHECK(!r.drv.SetMeasurementMode(1).ok());
  CHECK(!r.drv.CalibrateZero(300).ok());
  CHECK(!r.drv.SetCrcEnabled(true).ok());
  CHECK_EQ(r.port.module.flash_cycles, 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK_EQ(r.port.module.last_cahi, 0LL);
}

TEST_CASE("guard: another module on the line gets no flash write") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.knobs.firmware = 358;
  CHECK(r.drv.SetMeasurementMode(1).error == Error::DeviceIdMismatch);
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK_EQ(Sent(r, "#SETM"), 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
}

TEST_CASE("guard: the module answering -22 in the guard (not ready) is a refusal, not a write") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.module.Arm(FaultKind::ErrorCode, -22); // the guard's #VERS
  CHECK(r.drv.SetMeasurementMode(1).error == Error::DeviceError);
  CHECK(r.drv.GetState() == State::Idle);
  CHECK_EQ(Sent(r, "#SETM"), 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK(r.drv.SetMeasurementMode(1).ok());
}

TEST_CASE("guard: a write error of the port after the guard read counts the write") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.port_faults.write_fail_after = r.port.write_calls + 1U;
  CHECK(r.drv.SetMeasurementMode(1).error == Error::Bus);
  CHECK_EQ(r.drv.FlashWritesUsed(), 1U);
  CHECK(r.drv.GetState() == State::StateUnknown);
}

TEST_CASE("guard: the write that fails to leave the port in the guard is a bus error") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  r.port.port_faults.write_fail_after = r.port.write_calls;
  CHECK(r.drv.SetMeasurementMode(1).error == Error::Bus);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK_EQ(Sent(r, "#SETM"), 0U);
}

// ---- m16: identity ---------------------------------------------------------------------------

TEST_CASE("m16: ReadVersion and ReadUniqueId notice another module") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.knobs.id = 42;
  CHECK(r.drv.ReadUniqueId().error == Error::DeviceIdMismatch);
  CHECK(r.drv.GetState() == State::StateUnknown);
  r.port.module.knobs.id = 18446744073709551615ULL;
  CHECK(r.drv.Resync().ok());
  CHECK(r.drv.ReadUniqueId().ok());
  r.port.module.knobs.firmware = 358;
  CHECK(r.drv.ReadVersion().error == Error::DeviceIdMismatch);
  CHECK(r.drv.GetState() == State::StateUnknown);
  CHECK(!r.drv.Resync().ok());
  CHECK(r.drv.GetState() == State::StateUnknown);
  r.port.module.knobs.firmware = 357;
  CHECK(r.drv.Resync().ok());
  CHECK(r.drv.ReadVersion().ok());
}

TEST_CASE("m16: the verification read after #CRCE checks the identity as well") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  // The module changes between the guard and the verification: the second #VERS is another one.
  CHECK(r.drv.StartSetCrcEnabled(true).ok());
  CHECK(PumpUntil(r, [&] { return Sent(r, "#CRCE") == 1U; }));
  r.port.module.knobs.firmware = 358;
  const auto p = Finish(r);
  CHECK(!p.ok());
  CHECK(p.error == Error::DeviceIdMismatch);
  CHECK(r.drv.GetState() == State::StateUnknown);
}

// ---- m12: results do not outlive a failed call --------------------------------------------------

TEST_CASE("m12: a failed call leaves no stale result behind") {
  fdo2::Config c{};
  c.retries = 0;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.MeasureOxygen().ok());
  CHECK(r.drv.GetOxygen().ok());
  r.port.module.Arm(FaultKind::Silent);
  CHECK(!r.drv.MeasureOxygen().ok());
  CHECK(r.drv.GetOxygen().error == Error::InvalidArgument);
  CHECK(r.drv.MeasureRaw().ok());
  r.port.module.Arm(FaultKind::Silent);
  CHECK(!r.drv.MeasureRaw().ok());
  CHECK(r.drv.GetRaw().error == Error::InvalidArgument);
  CHECK(r.drv.ReadVersion().ok());
  r.port.module.Arm(FaultKind::Silent);
  CHECK(!r.drv.ReadVersion().ok());
  CHECK(r.drv.GetVersion().error == Error::InvalidArgument);
  CHECK(r.drv.ReadUniqueId().ok());
  r.port.module.Arm(FaultKind::Silent);
  CHECK(!r.drv.ReadUniqueId().ok());
  CHECK(r.drv.GetUniqueId().error == Error::InvalidArgument);
  std::array<int32_t, 2> words{};
  CHECK(r.drv.ReadUserMemory(0, words).ok());
  r.port.module.Arm(FaultKind::Silent);
  CHECK(!r.drv.ReadUserMemory(0, words).ok());
  CHECK(r.drv.GetUserMemory().error == Error::InvalidArgument);
}

// ---- m17: stream liveness
// ------------------------------------------------------------------------

TEST_CASE("m17: the silence of a stream is measurable") {
  fdo2::Config c{};
  c.broadcast = BroadcastPolicy::Stream;
  fx::Rig r(c);
  r.port.module.broadcast_ms = 200;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.GetState() == State::Streaming);
  CHECK(PumpUntil(r, [&] { return r.drv.GetCounters().broadcast_lines >= 3U; }));
  CHECK(r.drv.StreamSilentMs() < 1000U);
  r.port.module.broadcast_ms = 0; // the module stops sending (dead or reset)
  r.port.module.Elapse(5'000'000);
  CHECK(r.drv.Poll().ok());
  CHECK(r.drv.StreamSilentMs() >= 4000U);
  fx::Rig q;
  CHECK(q.Start() == Error::Ok);
  CHECK_EQ(q.drv.StreamSilentMs(), 0U); // not streaming
}
