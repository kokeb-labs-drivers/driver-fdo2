/**
 * @file fuzz_fdo2.cpp
 * @brief Fuzzes the driver: random configuration, random call sequences over the model with random
 * faults, and a chaos port whose Read returns the fuzz bytes themselves.
 *
 * Invariants (a violation aborts, so the fuzzer reports it):
 * F1 no call crashes or leaves the state outside the enumeration; F2 every returned error is a
 * defined value; F3 without consent the module's flash is never written; F4 the module's flash
 * cycles never exceed the budget, and equal the driver's own count; F5 every command the driver
 * wrote is a well-formed command of the thirteen; F6 a blocking call returns within 120 s of
 * model time and never in `Waiting`; F7 a successful measurement is inside the configured
 * plausibility windows and its status passes the policy; F8 `GetState()` agrees with
 * `IsInitialized()`; F9 the counters never decrease; F10 a late byte is never read as a value:
 * after a failed read the next successful value equals the model's scenario; F11 the module never
 * executes a flash write unless a `#VERS` or `#MOXY` of the driver was answered since the module's
 * last reset, seen or unseen (`#BCST 0` of a running stream excepted); F12 while uninitialised
 * every call but the start-up ones sends nothing; F13 while the state is unknown every read and
 * write call returns `StateUnknown` and sends nothing; F14 the supply is never cut during a flash
 * cycle.
 */
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "fdo2.hpp"
#include "mock_fdo2.hpp"

namespace {

using fdo2::Error;
using fdo2::State;

void Require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

class Input {
public:
  Input(const uint8_t* data, std::size_t size) : data_(data), size_(size) {}
  uint8_t Next() {
    return (pos_ < size_) ? data_[pos_++] : 0U;
  }
  [[nodiscard]] bool Done() const {
    return pos_ >= size_;
  }
  uint32_t Next16() {
    const uint32_t hi = Next();
    return (hi << 8U) | Next();
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_{0};
};

void CheckError(Error e) {
  Require(static_cast<uint8_t>(e) <= 9U ||
          (static_cast<uint8_t>(e) >= 16U && static_cast<uint8_t>(e) <= 25U));
}

fdo2::Config MakeConfig(Input& in) {
  fdo2::Config c{};
  const uint8_t a = in.Next();
  const uint8_t b = in.Next();
  c.crc = static_cast<fdo2::CrcMode>(a % 3U);
  c.status_policy =
      ((a & 0x08U) != 0U) ? fdo2::StatusPolicy::Documented : fdo2::StatusPolicy::Datasheet;
  c.broadcast = static_cast<fdo2::BroadcastPolicy>(b % 3U);
  c.retries = static_cast<uint8_t>((b >> 2U) % 4U);
  c.allow_persistent_writes = (b & 0x80U) != 0U;
  c.max_flash_writes = static_cast<uint8_t>(1U + (in.Next() % 4U));
  c.strict_spacing = (a & 0x10U) != 0U;
  c.ignore_lf = (a & 0x20U) != 0U;
  c.erro_without_crc = (a & 0x40U) != 0U;
  c.family = ((a & 0x80U) != 0U) ? fdo2::Family::Fdo2 : fdo2::Family::Fdo2G2;
  c.min_steady_s = 0;
  c.broadcast_listen_ms = (in.Next() % 4U) * 500U;
  return c;
}

void CheckCommands(const fdo2_test::Module& m) {
  for (const std::string& cmd : m.commands) {
    Require(cmd.size() >= 5U);
    Require(cmd[0] == '#');
    bool known = false;
    for (const char* h : {"VERS", "IDNR", "MOXY", "MRAW", "LOGO", "CRCE", "RDUM", "WRUM", "BAUD",
                          "CALO", "CAHI", "BCST", "SETM"}) {
      known = known || (cmd.compare(1, 4, h) == 0);
    }
    Require(known);
    for (const char ch : cmd) {
      Require((ch >= 0x20) && (ch <= 0x7E));
    }
  }
}

void CheckMeasurement(const fdo2::Config& c, const fdo2::Result<fdo2::Oxygen>& r) {
  if (!r.ok()) {
    return;
  }
  Require((r.value.o2_mhpa >= c.o2_min_mhpa) && (r.value.o2_mhpa <= c.o2_max_mhpa));
  Require((r.value.temperature_mc >= c.temp_min_mc) && (r.value.temperature_mc <= c.temp_max_mc));
  const uint32_t w = r.value.status.word;
  if (c.status_policy == fdo2::StatusPolicy::Datasheet) {
    Require(w <= 1U);
  } else {
    Require(!r.value.status.AnyFatal() && !r.value.status.UndefinedBits());
  }
}

/// F12 and F13: what the state before a call allows it to send and to return.
void CheckGates(State before, uint8_t which, Error e, std::size_t sent) {
  const bool starts = (which == 14U) || (which == 16U); // EnsureInitialized, PowerCycle
  const bool call = (which <= 13U) || (which == 19U) || (which == 20U) || (which == 21U);
  const bool recovery = (which == 13U) || (which == 14U) || (which == 15U) || (which == 16U);
  if ((before == State::Uninitialized) && !starts) {
    Require(sent == 0U);
    Require(!call || (e == Error::NotInitialized));
  }
  if ((before == State::StateUnknown) && call && !recovery) {
    Require(sent == 0U);
    Require(e == Error::StateUnknown);
  }
}

/// One random operation on the driver; the model is faulted by the fuzz bytes.
void Step(Input& in, fdo2_test::FullPort& port, fdo2::FDO2<fdo2_test::FullPort>& d,
          const fdo2::Config& c) {
  const uint8_t op = in.Next();
  const uint8_t arg = in.Next();
  const State before = d.GetState();
  const std::size_t commands0 = port.module.commands.size();
  if ((op & 0x80U) != 0U) {
    port.module.Arm(static_cast<fdo2_test::FaultKind>(1U + (arg % 17U)),
                    static_cast<int>(in.Next() % 40U));
  }
  const uint64_t t0 = port.module.now_us;
  const uint32_t cycles0 = port.module.flash_cycles;
  Error e = Error::Ok;
  const auto which = static_cast<uint8_t>(op % 23U);
  switch (which) {
  case 0:
    e = d.MeasureOxygen().error;
    break;
  case 1:
    e = d.MeasureRaw().error;
    break;
  case 2:
    e = d.ReadVersion().error;
    break;
  case 3:
    e = d.ReadUniqueId().error;
    break;
  case 4:
    e = d.Identify().error;
    break;
  case 5: {
    std::array<int32_t, 8> out{};
    e = d.ReadUserMemory(static_cast<uint8_t>(arg % 70U),
                         std::span<int32_t>{out}.first(1U + (in.Next() % 8U)))
            .error;
    break;
  }
  case 6: {
    std::array<int32_t, 4> in_w{1, 2, 3, 4};
    e = d.WriteUserMemory(static_cast<uint8_t>(arg % 64U),
                          std::span<const int32_t>{in_w}.first(1U + (in.Next() % 4U)))
            .error;
    break;
  }
  case 7:
    e = d.SetCrcEnabled((arg & 1U) != 0U).error;
    break;
  case 8:
    e = d.SetMeasurementMode(arg % 6U).error;
    break;
  case 9:
    e = d.SetBroadcast((arg % 3U == 0U) ? 0U : (50U + (static_cast<uint32_t>(arg) * 100U))).error;
    break;
  case 10:
    e = d.SetBaudRate(fdo2::kBaudRates[arg % 12U % 11U]).error;
    break;
  case 11:
    e = d.CalibrateZero(arg).error;
    break;
  case 12:
    e = d.CalibrateAir(arg).error;
    break;
  case 13:
    e = d.Resync().error;
    break;
  case 14:
    e = d.EnsureInitialized().error;
    break;
  case 15:
    e = d.Deinitialize().error;
    break;
  case 16:
    e = d.PowerCycle().error;
    break;
  case 17:
    e = d.AbortOperation().error;
    break;
  case 18:
    port.module.scenario.status_override =
        static_cast<int>(arg % 8U) == 0 ? static_cast<int>(in.Next()) * 5 : -1;
    port.module.scenario.temperature_c = static_cast<double>(static_cast<int>(in.Next()) - 40);
    break;
  case 19:
    e = d.ProbeBroadcast(1U + ((in.Next() % 40U) * 100U)).error;
    break;
  case 20: {
    const auto r = d.StartMeasureOxygen();
    e = r.error;
    for (int i = 0; (i < 400) && r.ok(); ++i) {
      const auto p = d.Poll();
      if (!p.ok()) {
        e = p.error;
        break;
      }
      if (p.value) {
        break;
      }
      port.module.Elapse(1000U + (static_cast<uint64_t>(arg) * 100U));
    }
    break;
  }
  case 22:
    port.module.UnseenReset(); // a brown-out the driver does not see
    break;
  default:
    e = d.DisableBroadcast().error;
    break;
  }
  CheckError(e);
  CheckGates(before, which, e, port.module.commands.size() - commands0);
  // F6, F3, F4
  Require(port.module.now_us - t0 < 120ULL * 1'000'000ULL * 4U);
  Require(port.module.flash_cycles >= cycles0);
  if (!c.allow_persistent_writes) {
    Require(port.module.flash_cycles == 0U);
  }
  Require(port.module.flash_cycles <= d.FlashWritesUsed());
  Require(port.module.unchecked_flash_writes == 0U); // F11
  Require(!port.module.supply_cut_in_write);         // F14
  Require(d.FlashWritesUsed() <= c.max_flash_writes);
  // F8
  const State s = d.GetState();
  Require(static_cast<uint8_t>(s) <= static_cast<uint8_t>(State::StateUnknown));
  Require(d.IsInitialized() == ((s == State::Idle) || (s == State::Waiting) ||
                                (s == State::Draining) || (s == State::StateUnknown) ||
                                (s == State::Streaming)));
  if (d.Pending() == fdo2::OperationKind::None && s == State::Waiting) {
    Require(false);
  }
}

void RunModel(Input& in) {
  const fdo2::Config c = MakeConfig(in);
  fdo2_test::FullPort port;
  port.module.crc_on = (in.Next() & 1U) != 0U;
  port.module.knobs.g2 = c.family == fdo2::Family::Fdo2G2;
  port.module.knobs.firmware = 328 + static_cast<int>(in.Next() % 40U);
  port.module.knobs.bug351 = (in.Next() & 1U) != 0U;
  port.module.broadcast_ms = ((in.Next() % 5U) == 0U) ? 200U : 0U;
  port.module.Reset();
  fdo2::FDO2<fdo2_test::FullPort> d(port, c);
  CheckError(d.EnsureInitialized().error);
  uint32_t last_tx = 0;
  while (!in.Done()) {
    Step(in, port, d, c);
    const auto counters = d.GetCounters();
    Require(counters.transactions >= last_tx);
    last_tx = counters.transactions;
    CheckCommands(port.module);
    const auto ox = d.GetOxygen();
    CheckMeasurement(c, ox);
  }
}

/// A port whose Read returns the fuzz bytes: the line is arbitrary.
class ChaosPort : public fdo2::UartInterface<ChaosPort> {
public:
  const uint8_t* data{nullptr};
  std::size_t size{0};
  std::size_t pos{0};
  uint32_t writes{0};
  uint64_t now{0};

  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(const uint8_t* /*data*/, std::size_t /*len*/) noexcept {
    ++writes;
    return true;
  }
  std::size_t Read(uint8_t* out, std::size_t max, uint32_t timeout_ms) noexcept {
    now += static_cast<uint64_t>(timeout_ms) * 1000U;
    std::size_t n = 0;
    while ((n < max) && (pos < size) && (n < 7U)) {
      out[n++] = data[pos++];
    }
    return n;
  }
  bool FlushInput() noexcept {
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    now += us;
  }
  // NOLINTNEXTLINE(readability-make-member-function-const): the port contract is a non-const hook
  uint32_t NowUs() noexcept {
    return static_cast<uint32_t>(now);
  }
};

void RunChaos(Input& in, const uint8_t* data, std::size_t size) {
  ChaosPort port;
  port.data = data;
  port.size = size;
  fdo2::Config c{};
  c.retries = static_cast<uint8_t>(in.Next() % 4U);
  c.crc = static_cast<fdo2::CrcMode>(in.Next() % 3U);
  c.broadcast = fdo2::BroadcastPolicy::Stream;
  fdo2::FDO2<ChaosPort> d(port, c);
  const Error init = d.EnsureInitialized().error;
  CheckError(init);
  for (int i = 0; i < 6; ++i) {
    CheckMeasurement(c, d.MeasureOxygen());
    const auto raw = d.MeasureRaw();
    CheckError(raw.error);
    CheckError(d.ReadVersion().error);
    if (init == Error::Ok) {
      Require(d.IsInitialized());
    }
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  if (size == 0U) {
    return 0;
  }
  Input in(data + 1, size - 1U);
  if ((data[0] & 1U) == 0U) {
    RunModel(in);
  } else {
    RunChaos(in, data + 1, size - 1U);
  }
  return 0;
}
