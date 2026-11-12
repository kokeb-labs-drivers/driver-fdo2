/**
 * @file fdo2.hpp
 * @brief FDO2 driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-fdo2 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `fdo2::FDO2<Port>` drives one PyroScience FDO2-G2 or FDO2 optical oxygen module through a
 * platform port you write (see `fdo2_uart_interface.hpp`). The driver has no platform code,
 * allocates nothing, throws nothing, takes no lock and is not thread-safe: give each object one
 * owner, or guard it with one mutex.
 *
 * Every operation exists in two forms over one state machine: `Start*()` sends the command and
 * returns at once, `Poll()` advances it and reads what arrived (a super-loop or an RTOS task
 * calls it every few milliseconds), and the blocking wrappers (`MeasureOxygen()` and the rest)
 * call both with `DelayUs()`. Exactly one operation is in flight.
 */
#ifndef FDO2_HPP
#define FDO2_HPP

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "fdo2_detail.hpp"
#include "fdo2_protocol.hpp"
#include "fdo2_types.hpp"
#include "fdo2_uart_interface.hpp"
#include "fdo2_version.h"

namespace fdo2 {

namespace detail {

// Optional hooks: a `Declares*` concept only tests the name, the `Has*` concept the exact
// signature, `noexcept` and the result type; the driver rejects a hook that is declared but does
// not match (KB-CPP20-007).

/// The port declares something called `GpioSet` (used to reject a misdeclared hook).
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::Power, GpioSignal::ACTIVE); };

/// The port drives control pins: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };

/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1_u32); };

/// The port waits: `void DelayUs(uint32_t us) noexcept`. A narrower parameter would truncate long
/// waits.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };

/// The port declares something called `NowUs`.
template <typename Port>
concept DeclaresNowUs = requires(Port& port) { port.NowUs(); };

/// The port has a clock: `uint32_t NowUs() noexcept`.
template <typename Port>
concept HasNowUs = requires { ExactHook<Port, uint32_t() noexcept>::Take(&Port::NowUs); };

/// The port declares something called `SetBaudRate`.
template <typename Port>
concept DeclaresSetBaudRate = requires(Port& port) { port.SetBaudRate(1_u32); };

/// The port reconfigures its UART: `bool SetBaudRate(uint32_t baud) noexcept`.
template <typename Port>
concept HasSetBaudRate =
    requires { ExactHook<Port, bool(uint32_t) noexcept>::Take(&Port::SetBaudRate); };

/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };

/// The port takes log lines: `void Log(LogLevel, const char* message) noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// Wait through the port. A port without `DelayUs()` fails to compile at the first call that needs
/// a wait: the blocking calls are the ones that wait.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (HasDelayUs<Port>) {
    port.DelayUs(us);
  } else {
    static_assert(kAlwaysFalse<Port>,
                  "fdo2: the port must implement `void DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`. The driver never formats text.
template <typename Port>
void Log(Port& port, [[maybe_unused]] LogLevel level,
         [[maybe_unused]] const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  } else {
    (void)port;
  }
}

/// Where an operation is in its sequence (docs/non-blocking.md).
enum class Phase : uint8_t {
  PowerWait, ///< Initialize: waiting `power_up_ms`.
  Listen,    ///< Initialize: listening for a broadcast stream.
  Main,      ///< The command itself.
  Settle,    ///< `#BAUD`: two character times at the new rate.
  Verify,    ///< `#CRCE` and `#BAUD`: the `#VERS` that proves the new setting.
  Disable,   ///< Initialize: `#BCST 0` against a streaming module.
  Idnr,      ///< Initialize: `#IDNR`.
  Guard,     ///< A flash-writing call: `#VERS` (or `#MOXY` for a calibration) before the write.
};

/// What one step of the state machine did.
struct Step {
  bool ended{false};      ///< The operation ended (with `error`).
  Error error{Error::Ok}; ///< `Ok` unless the operation failed.
};

/// The operation being waited for, its arguments and its deadline.
struct Op {
  OperationKind kind{OperationKind::None};
  Command command{Command::Vers};
  detail::Phase phase{detail::Phase::Main};
  std::array<int32_t, kMaxValues> args{};
  std::size_t arg_count{0};
  uint64_t deadline_us{0};
  uint64_t elapsed_us{0};
  uint32_t cmd_len{0};
  uint32_t stale{0};
  uint32_t new_baud{0};
  int32_t crc_target{0};
  int32_t expect_mhpa{0};              ///< Guard of a calibration: the pO2 the gas should have.
  Command main_command{Command::Vers}; ///< Guard: the flash-writing command that follows.
  std::size_t main_arg_count{0};       ///< Guard: its argument count.
  uint8_t attempts_left{0};
  bool tolerant{false};
  bool sent{false};
};

/// What a start adds to the common checks.
struct StartOptions {
  bool supported{true};        ///< The firmware and family offer the command.
  bool allow_streaming{false}; ///< The call may start while the module streams.
  bool tolerant{false};        ///< Broadcast lines are ignored while waiting.
  int32_t crc_target{0};       ///< `#CRCE`: the mode to verify.
  uint32_t new_baud{0};        ///< `#BAUD`: the rate to switch to.
};

} // namespace detail

/**
 * @defgroup fdo2_driver FDO2 driver
 * @brief The driver class.
 * @{
 */

// clang-format off: the table below must keep one row per line.
/**
 * @brief Driver for one FDO2-G2 or FDO2 on a platform port.
 *
 * Lifecycle: construct with the configuration (no I/O), then `BeginInitialize()` and `Poll()`
 * or `EnsureInitialized()`, use, optionally `Deinitialize()`. Every call that touches the
 * module returns `Error::NotInitialized` until start-up has succeeded.
 *
 * Optional port hooks, found at compile time:
 *
 * - `void DelayUs(uint32_t us) noexcept`: the blocking calls and `PowerCycle()`; without it the
 *   first blocking call is a compile error.
 * - `uint32_t NowUs() noexcept`: `Poll()` measures its own time; without it only
 *   `Poll(elapsed_us)` exists.
 * - `bool SetBaudRate(uint32_t) noexcept`: `#BAUD` and `Resync()`; without it `SetBaudRate()`
 *   returns `Unsupported`.
 * - `bool GpioSet(CtrlPin, GpioSignal) noexcept`: `CtrlPin::Power` at start-up, in
 *   `Deinitialize()` and in `PowerCycle()`; without it the application powers the module and
 *   `PowerCycle()` returns `Unsupported`.
 * - `void Log(LogLevel, const char*) noexcept`: one line per lifecycle event or failure; without
 *   it nothing is logged.
 *
 * A hook declared with the wrong signature is a compile error.
 *
 * @tparam Port Your class derived from `UartInterface<Port>`.
 */
// clang-format on
template <UartPort Port>
class FDO2 {
  static_assert(!detail::DeclaresGpioSet<Port> || detail::HasGpioSet<Port>,
                "fdo2: the optional GpioSet hook must be `bool GpioSet(CtrlPin, GpioSignal) "
                "noexcept`, so that a failed pin write is reported");
  static_assert(!detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
                "fdo2: the DelayUs hook must be `void DelayUs(uint32_t us) noexcept`, so that no "
                "wait is truncated");
  static_assert(!detail::DeclaresNowUs<Port> || detail::HasNowUs<Port>,
                "fdo2: the optional NowUs hook must be `uint32_t NowUs() noexcept`");
  static_assert(!detail::DeclaresSetBaudRate<Port> || detail::HasSetBaudRate<Port>,
                "fdo2: the optional SetBaudRate hook must be `bool SetBaudRate(uint32_t baud) "
                "noexcept`");
  static_assert(!detail::DeclaresLog<Port> || detail::HasLog<Port>,
                "fdo2: the optional Log hook must be `void Log(LogLevel, const char* message) "
                "noexcept`");

public:
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O, cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config Checked by `BeginInitialize()`, before any I/O.
   */
  explicit FDO2(Port& port, const Config& config = {}) noexcept
      : port_(port), config_(config), baud_(config.baud) {}

  /** @brief One driver object per module. */
  FDO2(const FDO2&) = delete;
  /** @brief One driver object per module. */
  FDO2& operator=(const FDO2&) = delete;

  /// @name Lifecycle
  /// @{

  /**
   * @brief Start the module without waiting: validate the `Config`, bring the UART up, power the
   * module through `GpioSet(CtrlPin::Power)` when the port has it, and enter `PoweringUp`.
   *
   * `Poll()` then waits `power_up_ms`, optionally listens for a stream, reads `#VERS` (device id
   * 8, one channel, firmware 3.28 or later, an oxygen sensor), then `#IDNR`. On an initialised
   * driver it returns `Ok` with no traffic.
   *
   * @return `Ok`, `InvalidArgument` (`Validate()`, no I/O), `Bus`, `Busy` (an operation is in
   *         flight).
   */
  Result<void> BeginInitialize() noexcept;

  /**
   * @brief `BeginInitialize()` and `Poll()` until it ends; needs `DelayUs()`. Bounded by the sum
   * of the allowances.
   * @return `Ok`, or the error of the failed step: `InvalidArgument`, `Bus`, `Timeout`,
   *         `DeviceIdMismatch`, `Unsupported` (firmware below 3.28), `BroadcastActive`,
   *         `NotPermitted`, `InvalidReply`, `Crc`, `DeviceError`.
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after start-up succeeded and before `Deinitialize()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return (state_ == State::Idle) || (state_ == State::Waiting) || (state_ == State::Draining) ||
           (state_ == State::StateUnknown);
  }

  /** @return The state machine's state (`Streaming` while the module streams). No I/O. */
  [[nodiscard]] State GetState() const noexcept {
    return ((state_ == State::Idle) && streaming_) ? State::Streaming : state_;
  }

  /**
   * @brief Drop the operation in flight, leave the broadcast alone, set the power pin INACTIVE
   * (when the port has it) and mark the driver uninitialised. A failed pin write is reported as
   * `Bus`.
   *
   * The supply is never cut while a flash write may be in progress (datasheet 4.3, WARNING: a
   * power loss during a flash cycle can corrupt the memory): the call returns `Busy`, changes
   * nothing, and succeeds once the echo has arrived or the write's allowance has elapsed
   * (`FlashWritePending()` says which; the time passes with `Poll()`). On a port without `NowUs()`
   * the time moves only through `Poll(elapsed_us)`: a caller that never polls stays `Busy`.
   * @return `Ok`, `Busy` (a flash write is pending) or `Bus`.
   */
  Result<void> Deinitialize() noexcept;

  /**
   * @brief Find the module again after `StateUnknown`: `#VERS` at the candidate baud rates (the
   * rate being set, the declared rate, 19200), each with the expected CRC mode and, with
   * `CrcMode::Auto`, the other one (a lost `#CRCE` echo leaves the two sides in different modes),
   * then the broadcast check. A stream seen in `StateUnknown` does not make the call `Busy`.
   * Needs `DelayUs()`.
   * @return `Ok`, `Busy`, `NotInitialized`, `Timeout` (nothing answered), `DeviceIdMismatch`
   * (another module answered), `BroadcastActive`.
   */
  Result<void> Resync() noexcept;

  /**
   * @brief Switch the module off through `CtrlPin::Power`, wait `power_off_ms`, switch it on and
   * start again at 19200 baud. Needs the `GpioSet()` hook and `DelayUs()`. When a flash write may
   * still be in progress the call first waits out its allowance (at most `calibration_ms` plus the
   * margin): the supply is not cut during a flash cycle (datasheet 4.3, WARNING).
   * @return As `EnsureInitialized()`, or `Unsupported` without the hook.
   */
  Result<void> PowerCycle() noexcept;

  /// @}
  /// @name Non-blocking core
  /// @{

  /**
   * @brief Advance the state machine and read what arrived, with the port's clock. One `Read()` of
   * at most 64 bytes with a zero timeout per call (bytes left over from the last read are
   * processed first).
   * @return `Ok(false)` still pending; `Ok(true)` the operation ended, or nothing is pending, or
   *         one broadcast frame was decoded; a failed `Result` the operation ended with that error.
   */
  Result<bool> Poll() noexcept
    requires detail::HasNowUs<Port>;

  /**
   * @brief `Poll()` for a port without a clock: the caller says how much time passed since the
   * previous call.
   * @param elapsed_us Microseconds since the previous `Poll()` (0 is allowed).
   * @return See `Poll()`.
   */
  Result<bool> Poll(uint32_t elapsed_us) noexcept
    requires(!detail::HasNowUs<Port>);

  /**
   * @brief Drop the operation in flight. A read is dropped into `Draining`; a flash write or
   * `#BAUD` becomes `StateUnknown` (the bytes were sent). Never talks to the port.
   * @return `Ok`.
   */
  Result<void> AbortOperation() noexcept;

  /** @return The operation in flight. No I/O. */
  [[nodiscard]] OperationKind Pending() const noexcept {
    return op_.kind;
  }

  /**
   * @brief Send `#MOXY`; `Poll()` ends it; `GetOxygen()` returns it.
   * @return `Ok`, or `Busy`, `NotInitialized`, `StateUnknown`, `Bus` (nothing was sent when it is
   * not `Ok`).
   */
  Result<void> StartMeasureOxygen() noexcept;
  /**
   * @brief Send `#MRAW`; `GetRaw()` returns it.
   * @return `Ok`, or `Busy`, `NotInitialized`, `StateUnknown`, `Bus` (nothing was sent when it is
   * not `Ok`).
   */
  Result<void> StartMeasureRaw() noexcept;
  /**
   * @brief Send `#VERS`; `GetVersion()` returns it.
   * @return `Ok`, or `Busy`, `NotInitialized`, `StateUnknown`, `Bus` (nothing was sent when it is
   * not `Ok`).
   */
  Result<void> StartReadVersion() noexcept;
  /**
   * @brief Send `#IDNR`; `GetUniqueId()` returns it.
   * @return `Ok`, or `Busy`, `NotInitialized`, `StateUnknown`, `Bus` (nothing was sent when it is
   * not `Ok`).
   */
  Result<void> StartReadUniqueId() noexcept;
  /**
   * @brief Send `#LOGO`: the status LED flashes four times.
   * @return `Ok`, or `Busy`, `NotInitialized`, `StateUnknown`, `Bus` (nothing was sent when it is
   * not `Ok`).
   */
  Result<void> StartIdentify() noexcept;
  /**
   * @brief Send `#RDUM first count`.
   * @param first The first word, 0 to 63.
   * @param count The number of words, 1 to 64, with `first + count <= 64`.
   * @return `InvalidArgument` for a range error (nothing sent), or the common start errors.
   */
  Result<void> StartReadUserMemory(uint8_t first, uint8_t count) noexcept;
  /**
   * @brief Send `#WRUM first count Y...` (a flash write, guarded).
   * @param first The first word.
   * @param words The values; 1 to 64 with `first + size <= 64`.
   * @return `NotPermitted` without consent or over the budget, `InvalidArgument`, `Unsupported`.
   */
  Result<void> StartWriteUserMemory(uint8_t first, std::span<const int32_t> words) noexcept;
  /**
   * @brief Send `#CALO` (a flash write; loses the factory calibration).
   * @param steady_s Seconds the module has measured steadily at 0 % oxygen; at least
   *        `Config::min_steady_s`.
   * @return `NotPermitted`, `InvalidArgument` or the common start errors.
   */
  Result<void> StartCalibrateZero(uint32_t steady_s) noexcept;
  /**
   * @brief Send `#CAHI P` (a flash write; loses the factory calibration).
   * @param po2_mhpa The pO2 of the standard, 10^-3 hPa, above 0 and at most `o2_max_mhpa`.
   * @param steady_s Seconds of steady measuring, at least `Config::min_steady_s`.
   * @return As `StartCalibrateZero()`.
   */
  Result<void> StartCalibrateHigh(int32_t po2_mhpa, uint32_t steady_s) noexcept;
  /**
   * @brief Send `#CRCE K` (a flash write), then `#VERS` to verify the mode.
   * @param enabled Whether replies carry the CRC.
   * @return `NotPermitted`, `InvalidArgument` (against `Config::crc`), or the common start errors.
   */
  Result<void> StartSetCrcEnabled(bool enabled) noexcept;
  /**
   * @brief Send `#SETM M` (a flash write; the FDO2-G2 only).
   * @param mode The measurement mode, 0 to 4.
   * @return `NotPermitted`, `InvalidArgument`, `Unsupported`, or the common start errors.
   */
  Result<void> StartSetMeasurementMode(uint8_t mode) noexcept;
  /**
   * @brief Send `#BCST T` (a flash write).
   * @param interval_ms 0 (off) or 100 to 10000 ms.
   * @return `NotPermitted`, `InvalidArgument`, `Unsupported`, or the common start errors.
   */
  Result<void> StartSetBroadcast(uint32_t interval_ms) noexcept;
  /**
   * @brief Send `#BAUD R`, switch the port, then `#VERS` at the new rate. Needs the port's
   * `SetBaudRate()`.
   * @param baud A rate of `kBaudRates`.
   * @return `InvalidArgument`, `Unsupported` (no hook), or the common start errors.
   */
  Result<void> StartSetBaudRate(uint32_t baud) noexcept;

  /** @return The last `#MOXY`; `Busy` while pending; `InvalidArgument` when there is none. */
  [[nodiscard]] Result<Oxygen> GetOxygen() const noexcept;
  /** @return The last `#MRAW` (or the next broadcast frame while streaming, once per frame). */
  [[nodiscard]] Result<Raw> GetRaw() noexcept;
  /** @return The last `#VERS` reply. */
  [[nodiscard]] Result<Version> GetVersion() const noexcept;
  /** @return The last `#IDNR` reply. */
  [[nodiscard]] Result<uint64_t> GetUniqueId() const noexcept;
  /** @return The words of the last `#RDUM`, valid until the next operation. */
  [[nodiscard]] Result<std::span<const int32_t>> GetUserMemory() const noexcept;
  /**
   * @brief The last decoded measurement frame even when the status policy rejected it, for
   * diagnostics. Never present it as a measurement.
   * @return The frame, or `InvalidArgument` when there is none.
   */
  [[nodiscard]] Result<Raw> GetLastFrame() const noexcept;

  /// @}
  /// @name Blocking calls (need `DelayUs()`)
  /// @{

  /**
   * @brief `#MOXY`, with the status and plausibility checks.
   * @return The oxygen and temperature, or one of `SensorFault`, `OutOfRange`, `Timeout`, `Crc`,
   * `InvalidReply`, `EchoMismatch`, `DeviceError`, `Bus`, `Busy`, `NotInitialized`.
   */
  Result<Oxygen> MeasureOxygen() noexcept;
  /**
   * @brief `#MRAW`, with the same checks.
   * @return The frame, or an error as `MeasureOxygen()`.
   */
  Result<Raw> MeasureRaw() noexcept;
  /**
   * @brief `#MRAW` once: the same checks as `MeasureRaw()` but a failed read is reported, never
   * repeated.
   * @return The frame, or an error as `MeasureRaw()`.
   */
  Result<Raw> ReadRawFrame() noexcept;
  /**
   * @brief Oxygen in 10^-3 volume percent: `#MRAW`, then `VolumePercentMilli()` with the module's
   * back-side pressure or `Config::pressure_reference_ubar`.
   * @return The value, or `SensorFault` (bit 9 with the module's pressure), `OutOfRange`.
   */
  Result<int32_t> MeasureVolumePercentMilli() noexcept;
  /**
   * @brief `#VERS`.
   * @return The version, or `Timeout`, `Crc`, `InvalidReply`, `EchoMismatch`, `DeviceError`, `Bus`,
   * `Busy`.
   */
  Result<Version> ReadVersion() noexcept;
  /**
   * @brief `#IDNR`.
   * @return The unique id, or an error as `ReadVersion()`.
   */
  Result<uint64_t> ReadUniqueId() noexcept;
  /**
   * @brief `#LOGO`: waits for the echo (about 1 s).
   * @return `Ok` or an error as `ReadVersion()`.
   */
  Result<void> Identify() noexcept;
  /**
   * @brief `#RDUM first out.size()`; fills `out`.
   * @param first The first word, 0 to 63.
   * @param out Receives 1 to 64 words, with `first + out.size() <= 64`.
   * @return `Ok`, `InvalidArgument`, `Unsupported`, or an error as `ReadVersion()`.
   */
  Result<void> ReadUserMemory(uint8_t first, std::span<int32_t> out) noexcept;
  /**
   * @brief `#WRUM`; see `StartWriteUserMemory()`.
   * @param first The first word.
   * @param in The values to store.
   * @return `NotPermitted`, `InvalidArgument`, `Unsupported`, `StateUnknown` after a lost echo, or
   * a bus error.
   */
  Result<void> WriteUserMemory(uint8_t first, std::span<const int32_t> in) noexcept;
  /**
   * @brief `#CALO`; up to 10 s.
   * @param steady_s Seconds of steady measuring at 0 % oxygen.
   * @return As `StartCalibrateZero()`, or the error of the wait.
   */
  Result<void> CalibrateZero(uint32_t steady_s) noexcept;
  /**
   * @brief `#CAHI P`; up to 10 s.
   * @param po2_mhpa The pO2 of the standard, 10^-3 hPa.
   * @param steady_s Seconds of steady measuring.
   * @return As `StartCalibrateHigh()`, or the error of the wait.
   */
  Result<void> CalibrateHigh(int32_t po2_mhpa, uint32_t steady_s) noexcept;
  /**
   * @brief One-point calibration in dry ambient air: `#MRAW` for the pressure, then `#CAHI` with
   * `AirO2Mhpa(P, fraction_ppm)`. Only correct when the membrane sees dry air at the measured
   * back-side pressure; humid air has a lower pO2 (A-10).
   * @param steady_s Seconds of steady measuring, at least `Config::min_steady_s`.
   * @param fraction_ppm Oxygen fraction in ppm (`kDryAirO2Ppm`).
   * @return As `CalibrateHigh()`, and `SensorFault` unless the status is good.
   */
  Result<void> CalibrateAir(uint32_t steady_s, uint32_t fraction_ppm = kDryAirO2Ppm) noexcept;
  /**
   * @brief `#CRCE`, then `#VERS`; see `StartSetCrcEnabled()`.
   * @param enabled Whether replies carry the CRC.
   * @return `Ok` or the error of the call.
   */
  Result<void> SetCrcEnabled(bool enabled) noexcept;
  /**
   * @brief `#SETM`; see `StartSetMeasurementMode()`.
   * @param mode The measurement mode, 0 to 4.
   * @return `Ok` or the error of the call.
   */
  Result<void> SetMeasurementMode(uint8_t mode) noexcept;
  /**
   * @brief `#BCST T`: with T above 0 the module streams and the driver enters `Streaming`.
   * @param interval_ms 0 (off) or 100 to 10000 ms.
   * @return `Ok` or the error of the call.
   */
  Result<void> SetBroadcast(uint32_t interval_ms) noexcept;
  /**
   * @brief `#BCST 0`: the vendor's workaround, sent right after a frame, among frames.
   * @return `Ok`, `NotPermitted`, `Timeout`, or the error of the call.
   */
  Result<void> DisableBroadcast() noexcept;
  /**
   * @brief `#BAUD`; see `StartSetBaudRate()`.
   * @param baud A rate of `kBaudRates`.
   * @return `Ok`, `InvalidArgument`, `Unsupported`, or the error of the call.
   */
  Result<void> SetBaudRate(uint32_t baud) noexcept;
  /**
   * @brief Listen without sending for a window and say whether `#MRAW` lines arrived.
   * @param window_ms The window, 1 to 12000 ms (10500 covers every interval).
   * @return `true` when a broadcast frame was seen.
   */
  Result<bool> ProbeBroadcast(uint32_t window_ms) noexcept;

  /// @}
  /// @name Observers (no I/O)
  /// @{

  /** @return Who the module is; valid after start-up. */
  [[nodiscard]] const Identity& GetIdentity() const& noexcept {
    return identity_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (KB-CPP20-044). */
  void GetIdentity() const&& = delete;
  /** @return The counters of line events. */
  [[nodiscard]] const Counters& GetCounters() const& noexcept {
    return counters_;
  }
  /** @brief Not on a temporary driver. */
  void GetCounters() const&& = delete;
  /** @return E of the last `#ERRO`, 0 when none. */
  [[nodiscard]] int32_t DeviceErrorCode() const noexcept {
    return device_error_code_;
  }
  /** @return The configuration; `baud` follows a confirmed `#BAUD`. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary driver. */
  void GetConfig() const&& = delete;
  /** @return Whether replies carry a CRC (learned from `#VERS`, verified after `#CRCE`). */
  [[nodiscard]] bool CrcActive() const noexcept {
    return crc_active_;
  }
  /** @return Flash-writing commands sent since construction. */
  [[nodiscard]] uint8_t FlashWritesUsed() const noexcept {
    return flash_used_;
  }
  /**
   * @return true while a flash write may still be in progress in the module: from its command to
   * the echo (or an error that says nothing was written), or to the end of its allowance when no
   * echo came. `Deinitialize()` and `PowerCycle()` do not cut the supply meanwhile. No I/O.
   */
  [[nodiscard]] bool FlashWritePending() const noexcept {
    return flash_quiet_us_ > 0U;
  }
  /**
   * @return While the module streams: milliseconds since the last accepted frame, or since the
   * stream started; 0 when it does not stream. A value of several intervals means the stream
   * died. No I/O.
   */
  [[nodiscard]] uint32_t StreamSilentMs() const noexcept;

  /// @}

private:
  using Op = detail::Op;
  using StartOptions = detail::StartOptions;

  // Entry checks and the common start.
  [[nodiscard]] Error gate(bool allow_streaming) const noexcept;
  Result<void> begin(OperationKind kind, Command command, std::span<const int32_t> args,
                     const StartOptions& options) noexcept;
  Result<void> startPlain(OperationKind kind, Command command) noexcept;
  Result<void> startTwoPoint(OperationKind kind, Command command, int32_t value,
                             uint32_t steady_s) noexcept;
  Error transmit() noexcept;
  void armDeadline() noexcept;
  [[nodiscard]] static bool isFlash(Command command) noexcept;
  [[nodiscard]] Error flashGate() const noexcept;
  [[nodiscard]] Error steadyGate(uint32_t steady_s) const noexcept;
  void resetSession() noexcept;

  // Time.
  void advanceTime(uint32_t dt_us) noexcept;
  uint32_t clockDelta() noexcept;
  uint32_t stepDelta() noexcept;

  // Polling.
  Result<bool> pollCore(uint32_t dt_us) noexcept;
  Result<bool> pollPowerUp() noexcept;
  Result<bool> pollWaiting() noexcept;
  Result<bool> pollSettle() noexcept;
  Result<bool> pollDraining() noexcept;
  Result<bool> pollStreaming() noexcept;
  [[nodiscard]] static Result<bool> toResult(const detail::Step& step) noexcept;
  Error fillRx() noexcept;
  void scanInput(bool count_discard) noexcept;
  void noteIdleLine(std::span<const char> line) noexcept;
  [[nodiscard]] ParseOptions parseOptions(bool relaxed_crc) const noexcept;

  // Line handling.
  detail::Step onReplyLine(std::span<const char> line) noexcept;
  detail::Step onStreamLine(std::span<const char> line) noexcept;
  detail::Step onForeignLine(const ParsedLine& parsed) noexcept;
  detail::Step onErro(const ParsedLine& parsed) noexcept;
  detail::Step complete(const ParsedLine& parsed) noexcept;
  detail::Step completeMain(const ParsedLine& parsed) noexcept;
  detail::Step completeBaud() noexcept;
  void storeUserMemory(const ParsedLine& parsed) noexcept;
  detail::Step completeVerify(const ParsedLine& parsed) noexcept;
  detail::Step completeInit(const ParsedLine& parsed) noexcept;
  detail::Step completeInitVersion(const ParsedLine& parsed) noexcept;
  detail::Step sendInit(Command command, std::span<const int32_t> args,
                        detail::Phase phase) noexcept;
  detail::Step startVerify() noexcept;
  Error startGuard() noexcept;
  detail::Step completeGuard(const ParsedLine& parsed) noexcept;
  [[nodiscard]] Error checkCalibrationReading(const ParsedLine& parsed) const noexcept;
  [[nodiscard]] bool versionMatches(const Version& version) const noexcept;
  detail::Step failUnknown(Error error) noexcept;
  void invalidateResult(OperationKind kind) noexcept;
  Result<void> resyncAt(uint32_t rate) noexcept;
  detail::Step finishOk() noexcept;
  detail::Step fail(Error error, bool refused) noexcept;
  detail::Step failInit(Error error) noexcept;
  detail::Step retryOrFailInit(Error error) noexcept;
  detail::Step scheduleRetry(Error error) noexcept;
  detail::Step restartAfterDrain() noexcept;
  void enterDraining(bool retry) noexcept;
  void learnUnsupported() noexcept;

  // Decoding and acceptance.
  [[nodiscard]] static Version decodeVersion(const ParsedLine& parsed) noexcept;
  [[nodiscard]] Oxygen decodeOxygen(const ParsedLine& parsed) const noexcept;
  [[nodiscard]] Error checkStatus(const Status& status) const noexcept;
  [[nodiscard]] Error checkOxygen(const Oxygen& oxygen) const noexcept;
  [[nodiscard]] Error plausibleRaw(const Raw& raw) const noexcept;
  Error acceptOxygen(const ParsedLine& parsed, Oxygen& out) noexcept;
  Error acceptRaw(const ParsedLine& parsed, Raw& out) noexcept;
  [[nodiscard]] Error resultGate(bool have) const noexcept;

  // Port and log.
  bool setPower(GpioSignal level) noexcept;
  void say(LogLevel level, const char* message) noexcept;

  // Blocking helpers.
  Result<bool> stepOnce() noexcept;
  Result<void> waitIdle() noexcept;
  Result<void> await() noexcept;
  Result<void> runBlocking(const Result<void>& started) noexcept;
  [[nodiscard]] uint64_t budgetUs() const noexcept;
  Result<void> applyBroadcastAfterResync() noexcept;

  Port& port_;
  Config config_;
  uint32_t baud_;
  State state_{State::Uninitialized};
  bool streaming_{false};
  bool power_set_{false};
  bool crc_active_{false};
  bool broadcast_seen_{false};
  bool retry_pending_{false};
  Op op_{};
  uint64_t drain_deadline_us_{0};
  uint64_t drain_elapsed_us_{0};
  uint64_t uptime_us_{0};
  bool uptime_valid_{false};
  uint32_t last_now_{0};
  std::array<uint8_t, 64> rx_{};
  std::size_t rx_len_{0};
  std::size_t rx_pos_{0};
  LineAssembler assembler_;
  Identity identity_{};
  Counters counters_{};
  int32_t device_error_code_{0};
  uint8_t flash_used_{0};
  uint32_t baud_candidate_{0};
  uint64_t flash_quiet_us_{0};
  uint64_t stream_silent_us_{0};
  Oxygen oxygen_{};
  Raw raw_{};
  Raw last_frame_{};
  Version version_{};
  uint64_t unique_id_{0};
  std::array<int32_t, kUserMemoryWords> user_memory_{};
  std::size_t user_memory_count_{0};
  bool have_oxygen_{false};
  bool have_raw_{false};
  bool have_version_{false};
  bool have_id_{false};
  bool have_user_memory_{false};
  bool have_last_frame_{false};
  bool frame_fresh_{false};
};

/** @} */

} // namespace fdo2

// The implementation is part of the header so that the driver is a header-only library.
// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/fdo2.ipp"

#endif // FDO2_HPP
