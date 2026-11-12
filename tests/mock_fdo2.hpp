/**
 * @file mock_fdo2.hpp
 * @brief An independent, byte-level, time-driven model of the FDO2 module and its UART, plus the
 * test ports that connect the driver to it.
 *
 * @details
 * The model shares no code with the driver: it has its own decimal formatting, its own bitwise
 * CRC-16, its own reading of the datasheet's command grammar (G2 data sheet, section 4) and its
 * own physics (any monotone function; it uses `double`). It runs on virtual time that moves only
 * through `DelayUs()` and `Read(timeout)`: a reply becomes readable one character time after the
 * previous one, so `Read(..., 0)` returns partial lines as a real UART does.
 *
 * Knobs (`Knobs`) cover every ambiguity of the datasheet that the driver has a setting for;
 * faults (`Fault`) cover everything a bad line can do.
 */
#ifndef FDO2_MOCK_FDO2_HPP
#define FDO2_MOCK_FDO2_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "fdo2.hpp"

namespace fdo2_test {

/// Independent bitwise CRC-16/MODBUS over a string (reflected 0xA001, start 0xFFFF).
inline uint16_t RefCrc16(const std::string& s) {
  unsigned crc = 0xFFFFU;
  for (const char c : s) {
    crc ^= static_cast<unsigned>(static_cast<unsigned char>(c));
    for (int i = 0; i < 8; ++i) {
      crc = ((crc & 1U) != 0U) ? ((crc >> 1U) ^ 0xA001U) : (crc >> 1U);
    }
  }
  return static_cast<uint16_t>(crc);
}

/// What a bad line can do (one fault is consumed by each reply it applies to).
enum class FaultKind : uint8_t {
  None,
  Silent,          ///< No reply at all.
  TruncatedAt,     ///< The reply is cut after `param` characters, with no CR.
  NoTerminator,    ///< The CR is missing.
  BitFlip,         ///< Bit 0 of character `param` is flipped after the CRC was computed.
  ExtraBytes,      ///< Bytes after the CR.
  WrongHeader,     ///< The reply carries another command's header.
  EchoArgChanged,  ///< The first echoed argument is changed.
  StaleLineFirst,  ///< A complete line of another command comes first.
  GarbageBurst,    ///< Garbage, then the reply.
  AllZero,         ///< The reply is 16 NUL bytes.
  AllFF,           ///< The reply is 16 bytes of 0xFF.
  ErrorCode,       ///< `#ERRO param` instead of the reply (with the CRC when it is on).
  ErrorWithoutCrc, ///< `#ERRO param` without the CRC while the CRC is on.
  OverlongLine,    ///< 900 characters, then the reply.
  PowerLoss,       ///< The module loses power while handling the command: no reply, reset.
  LfTerminated,    ///< The reply ends with CR LF.
  ExtraSpace,      ///< A second space after the header.
};

/// One queued fault.
struct Fault {
  FaultKind kind{FaultKind::None};
  int param{0};
  bool on_write{false}; ///< Applies only to the reply of a flash-writing command.
};

/// The module's observable settings and the model's timing knobs.
struct Knobs {
  bool g2{true};     ///< FDO2-G2 (has `#SETM`) or the legacy FDO2.
  int firmware{357}; ///< R of `#VERS`.
  int device_id{8};
  int channels{1};
  int sensors{15};
  uint64_t id{18446744073709551615ULL}; ///< The unique id.
  uint32_t power_up_us{1'000'000};      ///< Silent time after power.
  bool early_replies_22{false};         ///< During power-up answer `#ERRO -22` instead of silence.
  uint32_t measure_us{0};               ///< Measurement time; 0: the datasheet maximum of the mode.
  uint32_t vers_us{2000};               ///< Processing of `#VERS #IDNR #RDUM #BAUD`.
  uint32_t flash_write_us{50'000};      ///< Processing of `#CRCE #SETM #BCST #WRUM`.
  uint32_t logo_us{1'000'000};          ///< LED flashing time.
  bool logo_reply_first{false};         ///< Reply before the flashing instead of after.
  uint32_t cal_us{10'000'000};          ///< `#CALO` and `#CAHI`.
  uint32_t baud_switch_us{0};           ///< Deaf time after a baud change.
  bool crce_reply_crc_enable{true};     ///< `#CRCE 1` reply carries the CRC.
  bool crce_reply_crc_disable{false};   ///< `#CRCE 0` reply carries the CRC.
  bool crce_not_applied{false};         ///< `#CRCE` is echoed but the setting does not change.
  bool erro_has_crc{true};              ///< `#ERRO` lines carry the CRC while it is on.
  bool lf_only_silent{true};            ///< A command ended by LF only: silence (else `-22`).
  uint32_t rx_capacity{1024};           ///< Host receive FIFO, bytes.
  uint32_t flash_limit{20000};          ///< Flash cycles before `#ERRO -1`.
  bool bug351{false};                   ///< A command during a broadcast frame silences the module.
  bool collide_truncate{true};          ///< A command during a frame truncates the frame.
};

/// The measurement scenario.
struct Scenario {
  double o2_hpa{209.5};
  double temperature_c{21.0};
  double pressure_mbar{1013.0};
  double humidity_pct{40.0};
  double light_mv{12.0};
  int status_override{-1}; ///< >= 0: report this status word.
};

/// The module and its UART, on virtual time.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding): fields are ordered by topic
class Module {
public:
  Knobs knobs{};
  Scenario scenario{};
  std::deque<Fault> faults;

  // Host-side wire state.
  uint64_t now_us{0};
  uint32_t host_baud{19200};
  bool loopback{false};
  bool powered{true};

  // Counters for the tests.
  uint32_t bytes_in{0};
  uint32_t bytes_out{0};
  uint32_t flush_calls{0};
  uint32_t read_calls{0};
  uint32_t read_timeouts_requested{0};
  uint64_t delay_total_us{0};
  uint32_t flash_cycles{0};
  uint32_t resets{0};
  uint32_t unchecked_flash_writes{0}; ///< Flash writes executed with no #VERS/#MOXY answered since
                                      ///< the module's last reset (`#BCST 0` excepted).
  bool supply_cut_in_write{false};    ///< The host cut the supply during a flash cycle.
  uint32_t overflow_dropped{0};
  std::vector<std::string> commands;      ///< Every complete command line received, without CR.
  std::vector<uint64_t> command_times_us; ///< When each was completed.
  uint32_t last_read_max{0};

  // Persistent settings.
  bool crc_on{false};
  uint32_t broadcast_ms{0};
  int mode{2};
  std::vector<int32_t> user_memory = std::vector<int32_t>(64, 0);
  long long last_cahi{0};
  bool calo_done{false};
  bool flash_corrupted{false};
  uint32_t module_baud{19200};

  Module() : powerup_end_us_(knobs.power_up_us) {}

  /// Power switched on or off (a power cycle resets the baud rate and the power-up delay).
  void SetPower(bool on) {
    if (!on && (now_us < flash_busy_until_us_)) {
      flash_corrupted = true; // the supply was cut during a flash cycle (4.3, WARNING)
      supply_cut_in_write = true;
    }
    flash_busy_until_us_ = 0U;
    answered_since_reset_ = false;
    powered = on;
    if (on) {
      module_baud = 19200;
      powerup_end_us_ = now_us + knobs.power_up_us;
      line_.clear();
      tx_.clear();
      next_bcast_us_ = (broadcast_ms > 0U)
                           ? (powerup_end_us_ + (static_cast<uint64_t>(broadcast_ms) * 1000U))
                           : 0U;
    }
  }

  /// Apply knob changes that depend on the construction (call after editing `knobs`).
  void Reset() {
    powerup_end_us_ = now_us + knobs.power_up_us;
    if (broadcast_ms > 0U) {
      next_bcast_us_ = powerup_end_us_ + (static_cast<uint64_t>(broadcast_ms) * 1000U);
    }
  }

  /// Queue a fault for the next reply.
  void Arm(FaultKind kind, int param = 0, int count = 1) {
    for (int i = 0; i < count; ++i) {
      faults.push_back(Fault{kind, param, false});
    }
  }

  /// Queue a fault for the next reply to a flash-writing command (`#VERS` or `#MOXY` before it
  /// is answered normally).
  void ArmWrite(FaultKind kind, int param = 0) {
    faults.push_back(Fault{kind, param, true});
  }

  /// The module resets without the host noticing (brown-out): the baud rate returns to 19200 and
  /// it is silent for the power-up time; its flash settings stay.
  void UnseenReset() {
    ++resets;
    answered_since_reset_ = false;
    module_baud = 19200;
    powerup_end_us_ = now_us + knobs.power_up_us;
    line_.clear();
    tx_.clear();
    saw_cr_ = false;
    deaf_ = false;
  }

  /// Time to put one character on the wire at `baud`, microseconds (10 bits per character).
  static uint64_t CharUs(uint32_t baud) {
    return (10'000'000ULL + (static_cast<uint64_t>(baud) / 2U)) / baud;
  }

  /// The host writes bytes: they arrive at the host rate; each CR completes a command.
  void HostWrite(const uint8_t* data, std::size_t len) {
    bytes_in += static_cast<uint32_t>(len);
    if (loopback) {
      for (std::size_t i = 0; i < len; ++i) {
        push(now_us + ((i + 1U) * CharUs(host_baud)), data[i], false);
      }
      return;
    }
    uint64_t t = now_us;
    for (std::size_t i = 0; i < len; ++i) {
      t += CharUs(host_baud);
      onByte(data[i], t);
    }
  }

  /// Host `Read`: bytes readable now, waiting up to `timeout_ms` for the first one.
  std::size_t HostRead(uint8_t* out, std::size_t max, uint32_t timeout_ms) {
    ++read_calls;
    last_read_max = static_cast<uint32_t>(max);
    read_timeouts_requested += (timeout_ms != 0U) ? 1U : 0U;
    const uint64_t target = now_us + (static_cast<uint64_t>(timeout_ms) * 1000U);
    if (timeout_ms != 0U) {
      generateBroadcast(target);
      const uint64_t first = firstReady();
      if ((first != 0U) && (first <= target)) {
        now_us = (first > now_us) ? first : now_us;
      } else {
        now_us = target;
      }
    }
    generateBroadcast(now_us);
    limitOverflow();
    std::size_t n = 0;
    while ((n < max) && !tx_.empty() && (tx_.front().ready_us <= now_us)) {
      out[n] = tx_.front().byte;
      tx_.pop_front();
      ++n;
    }
    bytes_out += static_cast<uint32_t>(n);
    return n;
  }

  /// Advance time (the host waits).
  void Elapse(uint64_t us) {
    now_us += us;
    delay_total_us += us;
  }

  /// Drop every byte already received by the host.
  void FlushRx() {
    ++flush_calls;
    generateBroadcast(now_us);
    while (!tx_.empty() && (tx_.front().ready_us <= now_us)) {
      tx_.pop_front();
    }
  }

  /// Number of bytes readable now (for assertions).
  std::size_t Readable() {
    generateBroadcast(now_us);
    std::size_t n = 0;
    for (const auto& b : tx_) {
      n += (b.ready_us <= now_us) ? 1U : 0U;
    }
    return n;
  }

  /// Number of commands received with a header.
  [[nodiscard]] uint32_t CountCommands(const std::string& header) const {
    uint32_t n = 0;
    for (const auto& c : commands) {
      n += c.starts_with(header) ? 1U : 0U;
    }
    return n;
  }

  /// Set the baud rate the module listens at (the host's must follow for the line to work).
  void SetModuleBaud(uint32_t baud) {
    module_baud = baud;
  }

  /// Format an integer in decimal, independent of the driver.
  static std::string Dec(long long v) {
    return std::to_string(v);
  }

private:
  struct TxByte {
    uint64_t ready_us;
    uint8_t byte;
    bool broadcast;
  };

  std::deque<TxByte> tx_;
  std::string line_;
  bool saw_cr_{false};
  uint64_t powerup_end_us_;
  uint64_t next_bcast_us_{0};
  uint64_t busy_until_us_{0};
  uint64_t flash_busy_until_us_{0};
  bool answered_since_reset_{false};
  uint64_t last_byte_us_{0};
  bool deaf_{false};
  uint64_t deaf_until_us_{0};
  bool silenced_{false};

  void push(uint64_t at, uint8_t byte, bool broadcast) {
    tx_.push_back(TxByte{at, byte, broadcast});
  }

  [[nodiscard]] uint64_t firstReady() const {
    return tx_.empty() ? 0U : tx_.front().ready_us;
  }

  void limitOverflow() {
    std::size_t ready = 0;
    for (const auto& b : tx_) {
      ready += (b.ready_us <= now_us) ? 1U : 0U;
    }
    while (ready > knobs.rx_capacity) {
      // Drop the newest ready byte: a full FIFO loses what arrives.
      for (std::size_t i = tx_.size(); i > 0U; --i) {
        if (tx_[i - 1U].ready_us <= now_us) {
          tx_.erase(tx_.begin() + static_cast<std::ptrdiff_t>(i - 1U));
          ++overflow_dropped;
          --ready;
          break;
        }
      }
    }
  }

  void emit(const std::string& text, uint64_t start, bool broadcast) {
    uint64_t t = (start > last_byte_us_) ? start : last_byte_us_;
    for (const char c : text) {
      t += CharUs(module_baud);
      push(t, static_cast<uint8_t>(c), broadcast);
    }
    last_byte_us_ = t;
  }

  void onByte(uint8_t b, uint64_t t) {
    if (!powered) {
      return;
    }
    if (deaf_ && (t < deaf_until_us_)) {
      return;
    }
    if (b == 0x0DU) {
      saw_cr_ = true;
      handle(t);
      line_.clear();
      return;
    }
    if (b == 0x0AU) {
      if (saw_cr_) {
        saw_cr_ = false; // the LF of a CR LF
        return;
      }
      // LF alone ends nothing: it is a bad terminator.
      pending_lf_ = true;
      handleLfOnly(t);
      return;
    }
    saw_cr_ = false;
    line_.push_back(static_cast<char>(b));
  }

  bool pending_lf_{false};
  bool in_flash_cmd_{false};

  void handleLfOnly(uint64_t t) {
    pending_lf_ = false;
    if (!knobs.lf_only_silent) {
      reply(std::string("#ERRO -22"), t, false, false);
    }
    line_.clear();
  }

  // ---------------------------------------------------------------------------------------------
  // The module's reading of the command grammar (data sheet 4.1, 4.3, 4.4).
  // ---------------------------------------------------------------------------------------------
  static bool parseInt(const std::string& s, long long& out) {
    if (s.empty()) {
      return false;
    }
    std::size_t i = 0;
    bool neg = false;
    if (s[0] == '-') {
      neg = true;
      i = 1;
    }
    if (i >= s.size() || s.size() - i > 12U) {
      return false;
    }
    long long v = 0;
    for (; i < s.size(); ++i) {
      if (s[i] < '0' || s[i] > '9') {
        return false;
      }
      v = (v * 10) + (s[i] - '0');
    }
    out = neg ? -v : v;
    return true;
  }

  static std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (const char c : s) {
      if (c == ' ') {
        if (!cur.empty()) {
          out.push_back(cur);
          cur.clear();
        }
      } else {
        cur.push_back(c);
      }
    }
    if (!cur.empty()) {
      out.push_back(cur);
    }
    return out;
  }

  [[nodiscard]] std::string withCrc(const std::string& body, bool crc) const {
    return crc ? (body + ": " + std::to_string(RefCrc16(body))) : body;
  }

  void handle(uint64_t t) {
    commands.push_back(line_);
    command_times_us.push_back(t);
    if (t < powerup_end_us_) {
      if (knobs.early_replies_22) {
        reply(std::string("#ERRO -22"), t, false, false);
      }
      return;
    }
    if (host_baud != module_baud) {
      emit(std::string("\xFF\xFF\xE0", 3), t, false);
      return;
    }
    generateBroadcast(t);
    const bool in_frame = frameInProgress(t);
    if (in_frame && knobs.bug351) {
      silenced_ = true;
    }
    if (silenced_) {
      return;
    }
    if (in_frame && knobs.collide_truncate) {
      truncateBroadcast(t);
    }
    execute(t);
  }

  [[nodiscard]] bool frameInProgress(uint64_t t) const {
    return std::ranges::any_of(tx_,
                               [t](const TxByte& b) { return b.broadcast && (b.ready_us > t); });
  }

  void truncateBroadcast(uint64_t t) {
    for (std::size_t i = tx_.size(); i > 0U; --i) {
      if (tx_[i - 1U].broadcast && (tx_[i - 1U].ready_us > t)) {
        tx_.erase(tx_.begin() + static_cast<std::ptrdiff_t>(i - 1U));
      }
    }
    last_byte_us_ = t;
  }

  void execute(uint64_t t) {
    const std::string& ln = line_;
    in_flash_cmd_ = false;
    if (ln.size() < 5U || ln[0] != '#') {
      error(-22, t);
      return;
    }
    for (std::size_t i = 1; i < 5; ++i) {
      if (ln[i] < 'A' || ln[i] > 'Z') {
        error(-23, t);
        return;
      }
    }
    const std::string head = ln.substr(1, 4);
    in_flash_cmd_ = (head == "CRCE") || (head == "WRUM") || (head == "CALO") || (head == "CAHI") ||
                    (head == "BCST") || (head == "SETM");
    std::vector<std::string> tok = split(ln.substr(5));
    std::vector<long long> v;
    for (const auto& s : tok) {
      long long x = 0;
      if (!parseInt(s, x)) {
        error(-21, t);
        return;
      }
      v.push_back(x);
    }
    dispatch(head, v, t);
  }

  void error(int code, uint64_t t) {
    reply("#ERRO " + Dec(code), t, false, true);
  }

  void dispatch(const std::string& head, const std::vector<long long>& v, uint64_t t) {
    const bool fw_ok = knobs.firmware >= 328;
    if (!fw_ok) {
      error(-26, t);
      return;
    }
    if (head == "VERS" && v.empty()) {
      answered_since_reset_ = true;
      reply("#VERS " + Dec(knobs.device_id) + " " + Dec(knobs.channels) + " " +
                Dec(knobs.firmware) + " " + Dec(knobs.sensors),
            t + knobs.vers_us, false, true);
    } else if (head == "IDNR" && v.empty()) {
      reply("#IDNR " + std::to_string(knobs.id), t + knobs.vers_us, false, true);
    } else if (head == "MOXY" && v.empty()) {
      answered_since_reset_ = true;
      measure(false, t);
    } else if (head == "MRAW" && v.empty()) {
      measure(true, t);
    } else if (head == "LOGO" && v.empty()) {
      logo(t);
    } else {
      dispatchSettings(head, v, t);
    }
  }

  void dispatchSettings(const std::string& head, const std::vector<long long>& v, uint64_t t) {
    if (head == "CRCE" && v.size() == 1U && (v[0] == 0 || v[0] == 1)) {
      flashWrite(
          head + " " + Dec(v[0]), t,
          [&] {
            if (!knobs.crce_not_applied) {
              crc_on = (v[0] == 1);
            }
          },
          (v[0] == 1) ? knobs.crce_reply_crc_enable : knobs.crce_reply_crc_disable);
    } else if (head == "RDUM" && v.size() == 2U) {
      readMemory(v[0], v[1], t);
    } else if (head == "WRUM" && v.size() >= 3U) {
      writeMemory(v, t);
    } else if (head == "BAUD" && v.size() == 1U) {
      setBaud(v[0], t);
    } else if (head == "CALO" && v.empty()) {
      calibrate(head, 0, t, false);
    } else if (head == "CAHI" && v.size() == 1U) {
      calibrate(head + " " + Dec(v[0]), v[0], t, true);
    } else if (head == "BCST" && v.size() == 1U && (v[0] == 0 || (v[0] >= 100 && v[0] <= 10000)) &&
               knobs.firmware >= 341) {
      const long long interval = v[0];
      flashWrite(head + " " + Dec(interval), t, [&] { setBroadcast(interval, t); }, crc_on);
    } else if (head == "SETM" && v.size() == 1U && v[0] >= 0 && v[0] <= 4 && knobs.g2) {
      flashWrite(head + " " + Dec(v[0]), t, [&] { mode = static_cast<int>(v[0]); }, crc_on);
    } else if ((head == "BCST" && knobs.firmware < 341) || (head == "SETM" && !knobs.g2) ||
               !isKnown(head)) {
      error(-26, t);
    } else {
      error(-21, t);
    }
  }

  static bool isKnown(const std::string& h) {
    static const char* const kNames[] = {"VERS", "IDNR", "MOXY", "MRAW", "LOGO", "CRCE", "RDUM",
                                         "WRUM", "BAUD", "CALO", "CAHI", "BCST", "SETM"};
    return std::ranges::any_of(kNames, [&h](const char* n) { return h == n; });
  }

  void noteWrite(const std::string& echo) {
    if (!answered_since_reset_ && (echo != "BCST 0")) {
      ++unchecked_flash_writes;
    }
  }

  void setBroadcast(long long interval, uint64_t t) {
    broadcast_ms = static_cast<uint32_t>(interval);
    next_bcast_us_ = (interval > 0) ? (t + (static_cast<uint64_t>(interval) * 1000U)) : 0U;
  }

  template <typename Apply>
  void flashWrite(const std::string& echo, uint64_t t, Apply apply, bool crc_in_reply) {
    if (!faults.empty() && faults.front().kind == FaultKind::PowerLoss) {
      faults.pop_front();
      flash_corrupted = true;
      flash_busy_until_us_ = 0U; // the model's own fault, not the host's doing
      SetPower(false);
      SetPower(true);
      return;
    }
    if (flash_cycles >= knobs.flash_limit) {
      error(-1, t);
      return;
    }
    ++flash_cycles;
    noteWrite(echo);
    flash_busy_until_us_ = t + knobs.flash_write_us;
    apply();
    reply("#" + echo, t + knobs.flash_write_us, false, crc_in_reply);
  }

  void calibrate(const std::string& echo, long long p, uint64_t t, bool high) {
    if (high && (p <= 0 || p > 2'100'000) && knobs.firmware >= 351) {
      error(-60, t);
      return;
    }
    if (flash_cycles >= knobs.flash_limit) {
      error(-1, t);
      return;
    }
    ++flash_cycles;
    noteWrite(echo);
    flash_busy_until_us_ = t + knobs.cal_us;
    if (high) {
      last_cahi = p;
    } else {
      calo_done = true;
    }
    reply("#" + echo, t + knobs.cal_us, false, crc_on);
  }

  void readMemory(long long r, long long n, uint64_t t) {
    if (r < 0 || r > 63 || n < 1 || n > 64 || r + n > 64) {
      error(-21, t);
      return;
    }
    std::string out = "#RDUM " + Dec(r) + " " + Dec(n);
    for (long long i = 0; i < n; ++i) {
      out += " " + Dec(user_memory[static_cast<std::size_t>(r + i)]);
    }
    reply(out, t + knobs.vers_us, false, crc_on);
  }

  void writeMemory(const std::vector<long long>& v, uint64_t t) {
    const long long r = v[0];
    const long long n = v[1];
    if (r < 0 || r > 63 || n < 1 || n > 64 || r + n > 64 ||
        static_cast<long long>(v.size()) != n + 2) {
      error(-21, t);
      return;
    }
    std::string echo = "WRUM " + Dec(r) + " " + Dec(n);
    for (long long i = 0; i < n; ++i) {
      echo += " " + Dec(v[static_cast<std::size_t>(2 + i)]);
    }
    flashWrite(
        echo, t,
        [&] {
          for (long long i = 0; i < n; ++i) {
            user_memory[static_cast<std::size_t>(r + i)] =
                static_cast<int32_t>(v[static_cast<std::size_t>(2 + i)]);
          }
        },
        crc_on);
  }

  void setBaud(long long rate, uint64_t t) {
    static const long long kRates[] = {1200,  2400,  4800,  9600,  14400, 19200,
                                       28800, 38400, 56000, 57600, 115200};
    bool ok = false;
    for (const long long r : kRates) {
      ok = ok || (r == rate);
    }
    if (!ok) {
      error(-21, t);
      return;
    }
    reply("#BAUD " + Dec(rate), t + knobs.vers_us, false, crc_on);
    // The reply is sent at the old rate; the module listens at the new one afterwards.
    module_baud_after_ = static_cast<uint32_t>(rate);
    const uint64_t done = last_byte_us_;
    scheduled_baud_at_ = done;
    pending_baud_ = true;
    deaf_ = knobs.baud_switch_us > 0U;
    deaf_until_us_ = done + knobs.baud_switch_us;
    applyPendingBaud();
  }

  uint32_t module_baud_after_{19200};
  uint64_t scheduled_baud_at_{0};
  bool pending_baud_{false};

  void applyPendingBaud() {
    // Replies are already timestamped at the old rate, so the new rate can be applied at once.
    if (pending_baud_) {
      module_baud = module_baud_after_;
      pending_baud_ = false;
    }
  }

  [[nodiscard]] double dphiDeg(double o2) const {
    return 60.0 - (55.0 * (o2 / (o2 + 120.0)));
  }

  void measure(bool raw, uint64_t t) {
    uint32_t us = knobs.measure_us;
    if (us == 0U) {
      us = (mode >= 3) ? 300'000U : 150'000U; // the datasheet maximum of the mode
    }
    const uint64_t done = t + us;
    reply(measureText(raw, done), done, false, crc_on);
  }

  [[nodiscard]] std::string measureText(bool raw, uint64_t /*done*/) const {
    const long long o = std::llround(scenario.o2_hpa * 1000.0);
    const long long tm = std::llround(scenario.temperature_c * 1000.0);
    long long status = 0;
    if (scenario.status_override >= 0) {
      status = scenario.status_override;
    } else if (scenario.temperature_c < 5.0 && scenario.o2_hpa < 10.0) {
      status = 1;
    } else {
      status = 0;
    }
    std::string out =
        std::string(raw ? "#MRAW " : "#MOXY ") + Dec(o) + " " + Dec(tm) + " " + Dec(status);
    if (raw) {
      out += " " + Dec(std::llround(dphiDeg(scenario.o2_hpa) * 1000.0));
      out += " " + Dec(std::llround(200000.0 + (scenario.o2_hpa * 200.0)));
      out += " " + Dec(std::llround(scenario.light_mv * 1000.0));
      out += " " + Dec(std::llround(scenario.pressure_mbar * 1000.0));
      out += " " + Dec(std::llround(scenario.humidity_pct * 1000.0));
    }
    return out;
  }

  void logo(uint64_t t) {
    if (knobs.logo_reply_first) {
      reply("#LOGO", t, false, crc_on);
      busy_until_us_ = t + knobs.logo_us;
    } else {
      reply("#LOGO", t + knobs.logo_us, false, crc_on);
    }
  }

  // ---------------------------------------------------------------------------------------------
  // Replies and faults
  // ---------------------------------------------------------------------------------------------
  void reply(std::string body, uint64_t start, bool broadcast, bool crc) {
    Fault f{};
    if (!faults.empty() && !broadcast && (!faults.front().on_write || in_flash_cmd_)) {
      f = faults.front();
      faults.pop_front();
    }
    const bool is_erro = body.starts_with("#ERRO");
    bool with_crc = crc_on && crc;
    if (is_erro) {
      with_crc = crc_on && knobs.erro_has_crc;
    }
    applyFaultToBody(f, body, with_crc);
    std::string text = withCrc(body, with_crc);
    finishFault(f, text, with_crc, body);
    if (f.kind == FaultKind::Silent || f.kind == FaultKind::PowerLoss) {
      return;
    }
    emit(text, start, broadcast);
  }

  void applyFaultToBody(const Fault& f, std::string& body, bool& with_crc) {
    switch (f.kind) {
    case FaultKind::WrongHeader:
      body = "#IDNR 5";
      break;
    case FaultKind::EchoArgChanged: {
      const std::size_t sp = body.find(' ');
      if (sp != std::string::npos && sp + 1U < body.size()) {
        body[sp + 1U] = (body[sp + 1U] == '9') ? '8' : static_cast<char>(body[sp + 1U] + 1);
      }
      break;
    }
    case FaultKind::ErrorCode:
      body = "#ERRO " + Dec(f.param);
      break;
    case FaultKind::ErrorWithoutCrc:
      body = "#ERRO " + Dec(f.param);
      with_crc = false;
      break;
    case FaultKind::ExtraSpace:
      if (body.size() > 5U) {
        body.insert(5, " ");
      }
      break;
    case FaultKind::None:
    case FaultKind::Silent:
    case FaultKind::TruncatedAt:
    case FaultKind::NoTerminator:
    case FaultKind::BitFlip:
    case FaultKind::ExtraBytes:
    case FaultKind::StaleLineFirst:
    case FaultKind::GarbageBurst:
    case FaultKind::AllZero:
    case FaultKind::AllFF:
    case FaultKind::OverlongLine:
    case FaultKind::PowerLoss:
    case FaultKind::LfTerminated:
    default:
      break;
    }
  }

  void finishFault(const Fault& f, std::string& text, bool with_crc, const std::string& body) {
    (void)with_crc;
    (void)body;
    switch (f.kind) {
    case FaultKind::TruncatedAt:
      text = text.substr(0, static_cast<std::size_t>(f.param));
      return;
    case FaultKind::BitFlip:
      if (static_cast<std::size_t>(f.param) < text.size()) {
        text[static_cast<std::size_t>(f.param)] =
            static_cast<char>(static_cast<uint8_t>(text[static_cast<std::size_t>(f.param)]) ^ 1U);
      }
      text.push_back('\r');
      return;
    case FaultKind::NoTerminator:
      return;
    case FaultKind::AllZero:
      text = std::string(16, '\0');
      return;
    case FaultKind::AllFF:
      text = std::string(16, '\xFF');
      return;
    case FaultKind::OverlongLine:
      text = std::string(900, 'A') + "\r" + text + "\r";
      return;
    case FaultKind::StaleLineFirst:
      text = "#IDNR 7\r" + text + "\r";
      return;
    case FaultKind::GarbageBurst:
      text = std::string("\x80\x81\x82\r", 4) + text + "\r";
      return;
    case FaultKind::ExtraBytes:
      text += "\r\x01\x02";
      return;
    case FaultKind::LfTerminated:
      text += "\r\n";
      return;
    case FaultKind::None:
    case FaultKind::Silent:
    case FaultKind::WrongHeader:
    case FaultKind::EchoArgChanged:
    case FaultKind::ErrorCode:
    case FaultKind::ErrorWithoutCrc:
    case FaultKind::PowerLoss:
    case FaultKind::ExtraSpace:
    default:
      break;
    }
    text.push_back('\r');
  }

  void generateBroadcast(uint64_t until) {
    if (!powered || silenced_ || broadcast_ms == 0U || next_bcast_us_ == 0U) {
      return;
    }
    while (next_bcast_us_ <= until) {
      const uint64_t start = (next_bcast_us_ > last_byte_us_) ? next_bcast_us_ : last_byte_us_;
      const uint64_t measured = start + 100'000U;
      std::string body = measureText(true, measured);
      const std::string text = withCrc(body, crc_on) + "\r";
      emit(text, measured, true);
      next_bcast_us_ = last_byte_us_ + (static_cast<uint64_t>(broadcast_ms) * 1000U);
    }
  }
};

// -------------------------------------------------------------------------------------------------
// Ports
// -------------------------------------------------------------------------------------------------

/// Failures the tests can inject into a port hook.
struct PortFaults {
  bool ensure_fails{false};
  bool write_fails{false};
  bool flush_fails{false};
  bool set_baud_fails{false};
  bool gpio_fails{false};
  bool read_oversize{false};
  uint32_t write_fail_after{0xFFFFFFFFU}; ///< Fail the Write numbered n (0 based).
};

/// The required hooks over the model.
template <typename Derived>
class PortBase : public fdo2::UartInterface<Derived> {
public:
  Module module;
  PortFaults port_faults{};
  uint32_t write_calls{0};
  uint32_t flush_fail_count{0};
  std::vector<std::vector<uint8_t>> written;
  std::vector<uint64_t> write_times_us;
  std::vector<std::string> logs;
  bool read_zero_forever{false};
  uint32_t read_returns_limit{0xFFFFFFFFU};
  bool tamper_read{false};
  std::vector<uint8_t> tamper_bytes;
  bool ensured{false};

  bool EnsureInitialized() noexcept {
    ensured = true;
    return !port_faults.ensure_fails;
  }

  bool Write(const uint8_t* data, std::size_t len) noexcept {
    const uint32_t n = write_calls++;
    written.emplace_back(data, data + len);
    write_times_us.push_back(module.now_us);
    if (port_faults.write_fails || (n == port_faults.write_fail_after)) {
      return false;
    }
    module.HostWrite(data, len);
    return true;
  }

  std::size_t Read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept {
    if (read_zero_forever) {
      if (timeout_ms != 0U) {
        module.Elapse(static_cast<uint64_t>(timeout_ms) * 1000U);
      }
      return 0;
    }
    if (port_faults.read_oversize) {
      return max + 1U;
    }
    return module.HostRead(data, max, timeout_ms);
  }

  bool FlushInput() noexcept {
    if (port_faults.flush_fails) {
      ++flush_fail_count;
      return false;
    }
    module.FlushRx();
    return true;
  }
};

/// Required hooks and `DelayUs()`.
class MinimalPort : public PortBase<MinimalPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    module.Elapse(us);
  }
};

/// Required hooks, `DelayUs()` and a clock.
class ClockPort : public PortBase<ClockPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    module.Elapse(us);
  }
  uint32_t NowUs() noexcept {
    return static_cast<uint32_t>(module.now_us & 0xFFFFFFFFULL) + clock_offset;
  }
  uint32_t clock_offset{0};
};

/// Every optional hook.
class FullPort : public PortBase<FullPort> {
public:
  std::vector<std::pair<fdo2::CtrlPin, fdo2::GpioSignal>> gpio_log;
  std::vector<uint32_t> baud_log;
  uint32_t current_baud{19200};

  void DelayUs(uint32_t us) noexcept {
    module.Elapse(us);
  }
  uint32_t NowUs() noexcept {
    return static_cast<uint32_t>(module.now_us & 0xFFFFFFFFULL);
  }
  bool SetBaudRate(uint32_t baud) noexcept {
    baud_log.push_back(baud);
    if (port_faults.set_baud_fails) {
      return false;
    }
    current_baud = baud;
    module.host_baud = baud;
    return true;
  }
  bool GpioSet(fdo2::CtrlPin pin, fdo2::GpioSignal level) noexcept {
    gpio_log.emplace_back(pin, level);
    if (port_faults.gpio_fails) {
      return false;
    }
    module.SetPower(level == fdo2::GpioSignal::ACTIVE);
    return true;
  }
  void Log(fdo2::LogLevel level, const char* message) noexcept {
    (void)level;
    logs.emplace_back(message);
  }
};

/// A port with neither `DelayUs()` nor anything optional (the blocking calls do not compile).
class BarePort : public PortBase<BarePort> {};

/// A port with a clock and no `DelayUs()`: only the non-blocking calls compile.
class PollOnlyPort : public PortBase<PollOnlyPort> {
public:
  uint32_t NowUs() noexcept {
    return static_cast<uint32_t>(module.now_us & 0xFFFFFFFFULL);
  }
};

/// A port without a clock and without `DelayUs()`: `Poll(elapsed_us)` only.
class NoClockPort : public PortBase<NoClockPort> {};

} // namespace fdo2_test

#endif // FDO2_MOCK_FDO2_HPP
