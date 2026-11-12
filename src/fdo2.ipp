/**
 * @file fdo2.ipp
 * @brief FDO2 driver implementation, included by `fdo2.hpp`.
 * @copyright Copyright (c) 2026 The driver-fdo2 authors. SPDX-License-Identifier: MIT
 *
 * Section numbers refer to the FDO2-G2 data sheet (printed pages equal PDF pages).
 */
#ifndef FDO2_IPP
#define FDO2_IPP

namespace fdo2 {

namespace detail {
/// Microseconds in one millisecond.
inline constexpr uint64_t kUsPerMs = 1000;
/// One blocking poll step, microseconds.
inline constexpr uint32_t kPollStepUs = 1000;
/// Microseconds in one millisecond, as the 32-bit type the port's `DelayUs()` takes.
inline constexpr uint32_t kUsPerMs32 = 1000_u32;
/// The longest reply that can still arrive after an abandoned `#MOXY` or `#MRAW`, characters.
inline constexpr uint32_t kLateReplyChars = 120_u32;
/// Time a stream interval is never trusted for: `#BCST 0` waits this long for its echo (4.3,
/// `#BCST`: the longest interval is 10 s).
inline constexpr uint64_t kBroadcastWaitUs = 10'000'000;
/// Warm-up after power-up, microseconds (2, Specifications).
inline constexpr uint64_t kWarmUpUs = 180'000'000;
/// ERRO codes of the datasheet table (4.4).
inline constexpr int32_t kErrGeneral = -1;
inline constexpr int32_t kErrParse = -21;
inline constexpr int32_t kErrRx = -22;
inline constexpr int32_t kErrHeader = -23;
inline constexpr int32_t kErrRequest = -26;
inline constexpr int32_t kErrCalibration = -60;

/// The commands that trigger a flash cycle (4.3, WARNING).
[[nodiscard]] constexpr bool IsFlashCommand(Command command) noexcept {
  return (command == Command::Crce) || (command == Command::Wrum) || (command == Command::Calo) ||
         (command == Command::Cahi) || (command == Command::Bcst) || (command == Command::Setm);
}

/// Operations that leave the module's settings in doubt when no echo is accepted.
[[nodiscard]] constexpr bool IsUncertainKind(OperationKind kind) noexcept {
  return (kind == OperationKind::SetCrc) || (kind == OperationKind::WriteUserMemory) ||
         (kind == OperationKind::CalibrateZero) || (kind == OperationKind::CalibrateHigh) ||
         (kind == OperationKind::SetBroadcast) || (kind == OperationKind::SetMeasurementMode) ||
         (kind == OperationKind::SetBaudRate);
}

/// The reads that may be sent again (they change nothing on the module).
[[nodiscard]] constexpr bool IsIdempotentKind(OperationKind kind) noexcept {
  return (kind == OperationKind::Initialize) || (kind == OperationKind::Version) ||
         (kind == OperationKind::UniqueId) || (kind == OperationKind::MeasureOxygen) ||
         (kind == OperationKind::MeasureRaw) || (kind == OperationKind::Identify) ||
         (kind == OperationKind::ReadUserMemory);
}

/// Processing allowance of a command, milliseconds (4.3 durations; A-5, A-6, A-7).
[[nodiscard]] constexpr uint32_t ProcessingMs(Command command, const Config& config) noexcept {
  switch (command) {
  case Command::Moxy:
  case Command::Mraw:
    return config.measurement_ms;
  case Command::Calo:
  case Command::Cahi:
    return config.calibration_ms;
  case Command::Crce:
  case Command::Wrum:
  case Command::Bcst:
  case Command::Setm:
    return config.flash_ms;
  case Command::Logo:
    return config.logo_ms;
  case Command::Vers:
  case Command::Idnr:
  case Command::Rdum:
  case Command::Baud:
  default:
    break;
  }
  return config.command_ms;
}

/// Whether an error of a read is worth sending the same read again.
[[nodiscard]] constexpr bool IsRetryable(Error error, int32_t device_code) noexcept {
  switch (error) {
  case Error::Timeout:
  case Error::Crc:
  case Error::InvalidReply:
  case Error::EchoMismatch:
    return true;
  case Error::DeviceError:
    return (device_code == kErrParse) || (device_code == kErrRx) || (device_code == kErrHeader);
  case Error::Ok:
  case Error::NotInitialized:
  case Error::InvalidArgument:
  case Error::Bus:
  case Error::DeviceIdMismatch:
  case Error::Busy:
  case Error::DeviceFault:
  case Error::Unsupported:
  case Error::SensorFault:
  case Error::OutOfRange:
  case Error::CalibrationRejected:
  case Error::NotPermitted:
  case Error::StateUnknown:
  case Error::BroadcastActive:
  case Error::NotReady:
  default:
    break;
  }
  return false;
}

/// The read errors after which a late reply may still be on its way: the input is drained.
[[nodiscard]] constexpr bool NeedsDrain(Error error) noexcept {
  return (error == Error::Timeout) || (error == Error::Crc) || (error == Error::InvalidReply);
}

/// True when candidate `i` of the resync list repeats an earlier one.
inline bool RepeatedCandidate(const std::array<uint32_t, 3>& candidates, std::size_t i) noexcept {
  if (i == 1U) {
    return std::get<1>(candidates) == std::get<0>(candidates);
  }
  return (i == 2U) && ((std::get<2>(candidates) == std::get<0>(candidates)) ||
                       (std::get<2>(candidates) == std::get<1>(candidates)));
}

} // namespace detail

// -------------------------------------------------------------------------------------------------
// Small helpers
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
void FDO2<Port>::say(LogLevel level, const char* message) noexcept {
  detail::Log(port_, level, message);
}

template <UartPort Port>
bool FDO2<Port>::setPower(GpioSignal level) noexcept {
  if constexpr (detail::HasGpioSet<Port>) {
    return port_.GpioSet(CtrlPin::Power, level);
  } else {
    (void)level;
    return true;
  }
}

template <UartPort Port>
uint32_t FDO2<Port>::clockDelta() noexcept {
  if constexpr (detail::HasNowUs<Port>) {
    const uint32_t now = port_.NowUs();
    const uint32_t delta = now - last_now_;
    last_now_ = now;
    return delta;
  } else {
    return 0U;
  }
}

template <UartPort Port>
uint32_t FDO2<Port>::stepDelta() noexcept {
  if constexpr (detail::HasNowUs<Port>) {
    return clockDelta();
  } else {
    return detail::kPollStepUs;
  }
}

template <UartPort Port>
void FDO2<Port>::advanceTime(uint32_t dt_us) noexcept {
  if (uptime_valid_) {
    uptime_us_ += dt_us;
  }
  op_.elapsed_us += dt_us;
  drain_elapsed_us_ += dt_us;
  flash_quiet_us_ = (dt_us >= flash_quiet_us_) ? 0U : (flash_quiet_us_ - dt_us);
  if (streaming_) {
    stream_silent_us_ += dt_us;
  }
}

template <UartPort Port>
uint32_t FDO2<Port>::StreamSilentMs() const noexcept {
  using detail::operator""_u32;
  const uint64_t ms = stream_silent_us_ / detail::kUsPerMs;
  const uint64_t cap = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
  return streaming_ ? static_cast<uint32_t>((ms > cap) ? cap : ms) : 0_u32;
}

template <UartPort Port>
ParseOptions FDO2<Port>::parseOptions(bool relaxed_crc) const noexcept {
  ParseOptions options{};
  options.strict_spacing = config_.strict_spacing;
  options.erro_without_crc = config_.erro_without_crc;
  if (!relaxed_crc) {
    options.crc_forbidden = config_.crc == CrcMode::Off;
    options.crc_required =
        (config_.crc == CrcMode::Required) || ((config_.crc == CrcMode::Auto) && crc_active_);
  }
  return options;
}

template <UartPort Port>
bool FDO2<Port>::isFlash(Command command) noexcept {
  return detail::IsFlashCommand(command);
}

// -------------------------------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
void FDO2<Port>::resetSession() noexcept {
  op_ = Op{};
  streaming_ = false;
  crc_active_ = false;
  broadcast_seen_ = false;
  retry_pending_ = false;
  uptime_us_ = 0U;
  uptime_valid_ = false;
  rx_len_ = 0U;
  rx_pos_ = 0U;
  identity_ = Identity{};
  identity_.family = config_.family;
  have_oxygen_ = false;
  have_raw_ = false;
  have_version_ = false;
  have_id_ = false;
  have_user_memory_ = false;
  have_last_frame_ = false;
  frame_fresh_ = false;
  device_error_code_ = 0;
  assembler_.Reset(config_.ignore_lf);
}

template <UartPort Port>
Result<void> FDO2<Port>::BeginInitialize() noexcept {
  if (state_ == State::Idle) {
    return Result<void>::Success();
  }
  if ((state_ == State::PoweringUp) || (state_ == State::Waiting) || (state_ == State::Draining)) {
    return Result<void>::Failure(Error::Busy);
  }
  if (Validate(config_) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (!port_.EnsureInitialized()) {
    return Result<void>::Failure(Error::Bus);
  }
  baud_ = config_.baud;
  if constexpr (detail::HasSetBaudRate<Port>) {
    if (!port_.SetBaudRate(baud_)) {
      return Result<void>::Failure(Error::Bus);
    }
  }
  if (!setPower(GpioSignal::ACTIVE)) {
    (void)setPower(GpioSignal::INACTIVE);
    return Result<void>::Failure(Error::Bus);
  }
  power_set_ = detail::HasGpioSet<Port>;
  if (!port_.FlushInput()) {
    failInit(Error::Bus);
    return Result<void>::Failure(Error::Bus);
  }
  resetSession();
  if constexpr (detail::HasNowUs<Port>) {
    last_now_ = port_.NowUs();
  }
  op_.kind = OperationKind::Initialize;
  op_.phase = detail::Phase::PowerWait;
  op_.deadline_us = static_cast<uint64_t>(config_.power_up_ms) * detail::kUsPerMs;
  state_ = State::PoweringUp;
  return Result<void>::Success();
}

template <UartPort Port>
Result<void> FDO2<Port>::EnsureInitialized() noexcept {
  if (state_ == State::Idle) {
    return Result<void>::Success();
  }
  if (const auto started = BeginInitialize(); !started) {
    return started;
  }
  return await();
}

template <UartPort Port>
Result<void> FDO2<Port>::Deinitialize() noexcept {
  advanceTime(clockDelta());
  if (flash_quiet_us_ > 0U) {
    // A flash cycle may be running: cutting the supply can corrupt the memory (4.3, WARNING).
    return Result<void>::Failure(Error::Busy);
  }
  op_ = Op{};
  streaming_ = false;
  retry_pending_ = false;
  state_ = State::Uninitialized;
  const bool powered_off = setPower(GpioSignal::INACTIVE);
  power_set_ = false;
  return powered_off ? Result<void>::Success() : Result<void>::Failure(Error::Bus);
}

template <UartPort Port>
Result<void> FDO2<Port>::AbortOperation() noexcept {
  retry_pending_ = false;
  const bool starting = op_.kind == OperationKind::Initialize;
  if ((state_ == State::PoweringUp) ||
      (starting && ((state_ == State::Waiting) || (state_ == State::Draining)))) {
    // Start-up that did not reach its identity check never ends in Idle: the next
    // EnsureInitialized() runs the whole sequence again.
    (void)failInit(Error::Timeout);
    return Result<void>::Success();
  }
  if (state_ == State::Waiting) {
    const bool written = op_.sent && (op_.phase != detail::Phase::Guard);
    if (written && (detail::IsUncertainKind(op_.kind) || (op_.phase == detail::Phase::Verify))) {
      state_ = State::StateUnknown;
      say(LogLevel::Warning, "fdo2: state unknown after an aborted write");
    } else {
      enterDraining(false);
    }
    op_.kind = OperationKind::None;
  }
  return Result<void>::Success();
}

template <UartPort Port>
Result<void> FDO2<Port>::PowerCycle() noexcept {
  if constexpr (detail::HasGpioSet<Port>) {
    advanceTime(clockDelta());
    const uint64_t cap = (flash_quiet_us_ / detail::kPollStepUs) + 8U;
    for (uint64_t i = 0U; (i < cap) && (flash_quiet_us_ > 0U); ++i) {
      detail::DelayUs(port_, detail::kPollStepUs); // not during a flash cycle (4.3, WARNING)
      advanceTime(stepDelta());
    }
    (void)AbortOperation();
    streaming_ = false;
    if (!setPower(GpioSignal::INACTIVE)) {
      state_ = State::Uninitialized;
      return Result<void>::Failure(Error::Bus);
    }
    detail::DelayUs(port_, config_.power_off_ms * detail::kUsPerMs32);
    config_.baud = kDefaultBaud;
    state_ = State::Uninitialized;
    return EnsureInitialized();
  } else {
    return Result<void>::Failure(Error::Unsupported);
  }
}

template <UartPort Port>
Result<void> FDO2<Port>::Resync() noexcept {
  if (state_ == State::Uninitialized) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if ((state_ != State::Idle) && (state_ != State::StateUnknown)) {
    return Result<void>::Failure(Error::Busy);
  }
  if (streaming_ && (state_ != State::StateUnknown)) {
    return Result<void>::Failure(Error::Busy);
  }
  streaming_ = false; // in `StateUnknown` the stream is found again by the broadcast check below
  const std::array<uint32_t, 3> candidates{(baud_candidate_ != 0U) ? baud_candidate_ : config_.baud,
                                           config_.baud, kDefaultBaud};
  Error last = Error::Timeout;
  for (std::size_t i = 0U; i < candidates.size(); ++i) {
    const uint32_t rate = candidates[i];
    if (detail::RepeatedCandidate(candidates, i)) {
      continue;
    }
    if constexpr (detail::HasSetBaudRate<Port>) {
      if (!port_.SetBaudRate(rate)) {
        last = Error::Bus;
        continue;
      }
    } else {
      if (rate != baud_) {
        continue;
      }
    }
    const Result<void> found = resyncAt(rate);
    if (found.ok() || (found.error == Error::BroadcastActive)) {
      return found;
    }
    last = found.error;
  }
  state_ = State::StateUnknown;
  return Result<void>::Failure(last);
}

template <UartPort Port>
Result<void> FDO2<Port>::resyncAt(uint32_t rate) noexcept {
  baud_ = rate;
  broadcast_seen_ = false;
  const bool declared = crc_active_;
  Error last = Error::Timeout;
  // The module's CRC mode is persistent and a lost `#CRCE` echo leaves the driver on the other
  // one: with `CrcMode::Auto` the mode that was not expected is tried as well, and the one that
  // parses is adopted.
  const uint32_t passes = (config_.crc == CrcMode::Auto) ? 2U : 1U;
  for (uint32_t pass = 0U; pass < passes; ++pass) {
    crc_active_ = (pass == 0U) ? declared : !declared;
    state_ = State::Idle;
    const Result<Version> version = ReadVersion();
    if (version.ok()) {
      config_.baud = rate;
      baud_candidate_ = 0U;
      identity_.caps = CapabilitiesFor(config_.family, version.value.firmware);
      return applyBroadcastAfterResync();
    }
    last = version.error;
  }
  crc_active_ = declared;
  return Result<void>::Failure(last);
}

template <UartPort Port>
Result<void> FDO2<Port>::applyBroadcastAfterResync() noexcept {
  if (!broadcast_seen_) {
    return Result<void>::Success();
  }
  switch (config_.broadcast) {
  case BroadcastPolicy::Stream:
    streaming_ = true;
    stream_silent_us_ = 0U;
    return Result<void>::Success();
  case BroadcastPolicy::Disable:
    streaming_ = true;
    return DisableBroadcast();
  case BroadcastPolicy::Refuse:
  default:
    break;
  }
  state_ = State::StateUnknown;
  return Result<void>::Failure(Error::BroadcastActive);
}

// -------------------------------------------------------------------------------------------------
// Start of an operation
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
Error FDO2<Port>::gate(bool allow_streaming) const noexcept {
  switch (state_) {
  case State::Uninitialized:
    return Error::NotInitialized;
  case State::PoweringUp:
  case State::Waiting:
  case State::Draining:
    return Error::Busy;
  case State::StateUnknown:
    return Error::StateUnknown;
  case State::Idle:
    return (streaming_ && !allow_streaming) ? Error::Busy : Error::Ok;
  case State::Streaming:
  default:
    break;
  }
  return Error::Busy;
}

template <UartPort Port>
Error FDO2<Port>::flashGate() const noexcept {
  if (!config_.allow_persistent_writes || (flash_used_ >= config_.max_flash_writes)) {
    return Error::NotPermitted;
  }
  return Error::Ok;
}

template <UartPort Port>
Error FDO2<Port>::steadyGate(uint32_t steady_s) const noexcept {
  return (steady_s >= static_cast<uint32_t>(config_.min_steady_s)) ? Error::Ok
                                                                   : Error::InvalidArgument;
}

template <UartPort Port>
Error FDO2<Port>::transmit() noexcept {
  std::array<char, kMaxCommandLength> text{};
  const std::size_t n =
      EncodeCommand(op_.command, std::span<const int32_t>{op_.args}.first(op_.arg_count), text);
  if (n == 0U) {
    return Error::InvalidArgument;
  }
  std::array<uint8_t, kMaxCommandLength> bytes{};
  for (std::size_t i = 0U; i < n; ++i) {
    bytes[i] = static_cast<uint8_t>(text[i]);
  }
  assembler_.Reset(config_.ignore_lf);
  rx_len_ = 0U;
  rx_pos_ = 0U;
  if (!port_.FlushInput()) {
    return Error::Bus;
  }
  if (isFlash(op_.command)) {
    // Counted when sent: the flash cycle may happen even if the echo is lost (4.3, WARNING).
    ++flash_used_;
    ++counters_.flash_writes;
  }
  ++counters_.transactions;
  op_.sent = true;
  op_.cmd_len = static_cast<uint32_t>(n);
  return port_.Write(bytes.data(), n) ? Error::Ok : Error::Bus;
}

template <UartPort Port>
void FDO2<Port>::armDeadline() noexcept {
  const uint32_t words = ((op_.command == Command::Rdum) || (op_.command == Command::Wrum))
                             ? static_cast<uint32_t>(op_.args[1])
                             : 0U;
  const uint32_t reply = MaxReplyChars(op_.command, words);
  const uint64_t processing =
      op_.tolerant
          ? (detail::kBroadcastWaitUs +
             (static_cast<uint64_t>(config_.flash_ms) * detail::kUsPerMs))
          : (static_cast<uint64_t>(detail::ProcessingMs(op_.command, config_)) * detail::kUsPerMs);
  op_.deadline_us = static_cast<uint64_t>(AirtimeUs(op_.cmd_len, baud_)) + processing +
                    static_cast<uint64_t>(AirtimeUs(reply, baud_)) +
                    (static_cast<uint64_t>(config_.margin_ms) * detail::kUsPerMs);
  op_.elapsed_us = 0U;
  if (isFlash(op_.command)) {
    flash_quiet_us_ = op_.deadline_us; // until the echo, the module may be writing its flash
  }
}

template <UartPort Port>
Result<void> FDO2<Port>::begin(OperationKind kind, Command command, std::span<const int32_t> args,
                               const StartOptions& options) noexcept {
  advanceTime(clockDelta());
  if (const Error g = gate(options.allow_streaming); g != Error::Ok) {
    return Result<void>::Failure(g);
  }
  if (!detail::ArgumentsValid(command, args)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (!options.supported) {
    return Result<void>::Failure(Error::Unsupported);
  }
  if (isFlash(command)) {
    if (const Error f = flashGate(); f != Error::Ok) {
      return Result<void>::Failure(f);
    }
  }
  op_ = Op{};
  op_.kind = kind;
  op_.command = command;
  op_.phase = detail::Phase::Main;
  op_.arg_count = args.size();
  for (std::size_t i = 0U; i < args.size(); ++i) {
    op_.args[i] = args[i];
  }
  op_.tolerant = options.tolerant;
  op_.crc_target = options.crc_target;
  op_.new_baud = options.new_baud;
  op_.attempts_left = detail::IsIdempotentKind(kind) ? config_.retries : 0U;
  if (options.new_baud != 0U) {
    baud_candidate_ = options.new_baud;
  }
  frame_fresh_ = false;
  invalidateResult(kind);
  // Every flash-writing command is preceded by a read that proves the module is the one the
  // driver knows and that it did not reset unseen (`startGuard()`). Switching the stream off while
  // it runs is the one exception: the frames themselves prove the module is alive.
  const bool guarded = isFlash(command) && !options.tolerant;
  if (const Error e = guarded ? startGuard() : transmit(); e != Error::Ok) {
    const detail::Step s = fail(e, false);
    return Result<void>::Failure(s.error);
  }
  armDeadline();
  state_ = State::Waiting;
  return Result<void>::Success();
}

template <UartPort Port>
void FDO2<Port>::invalidateResult(OperationKind kind) noexcept {
  switch (kind) {
  case OperationKind::MeasureOxygen:
    have_oxygen_ = false;
    break;
  case OperationKind::MeasureRaw:
    have_raw_ = false;
    break;
  case OperationKind::Version:
    have_version_ = false;
    break;
  case OperationKind::UniqueId:
    have_id_ = false;
    break;
  case OperationKind::ReadUserMemory:
    have_user_memory_ = false;
    break;
  case OperationKind::None:
  case OperationKind::Initialize:
  case OperationKind::Identify:
  case OperationKind::WriteUserMemory:
  case OperationKind::CalibrateZero:
  case OperationKind::CalibrateHigh:
  case OperationKind::SetCrc:
  case OperationKind::SetMeasurementMode:
  case OperationKind::SetBroadcast:
  case OperationKind::SetBaudRate:
  default:
    break;
  }
}

template <UartPort Port>
Error FDO2<Port>::startGuard() noexcept {
  op_.main_command = op_.command;
  op_.main_arg_count = op_.arg_count;
  op_.expect_mhpa = (op_.command == Command::Cahi) ? op_.args[0] : 0;
  op_.phase = detail::Phase::Guard;
  op_.command = ((op_.command == Command::Calo) || (op_.command == Command::Cahi)) ? Command::Moxy
                                                                                   : Command::Vers;
  op_.arg_count = 0U;
  return transmit();
}

template <UartPort Port>
Result<void> FDO2<Port>::startPlain(OperationKind kind, Command command) noexcept {
  return begin(kind, command, std::span<const int32_t>{}, StartOptions{});
}

template <UartPort Port>
Result<void> FDO2<Port>::StartMeasureOxygen() noexcept {
  return startPlain(OperationKind::MeasureOxygen, Command::Moxy);
}
template <UartPort Port>
Result<void> FDO2<Port>::StartMeasureRaw() noexcept {
  return startPlain(OperationKind::MeasureRaw, Command::Mraw);
}
template <UartPort Port>
Result<void> FDO2<Port>::StartReadVersion() noexcept {
  return startPlain(OperationKind::Version, Command::Vers);
}
template <UartPort Port>
Result<void> FDO2<Port>::StartReadUniqueId() noexcept {
  return startPlain(OperationKind::UniqueId, Command::Idnr);
}
template <UartPort Port>
Result<void> FDO2<Port>::StartIdentify() noexcept {
  return startPlain(OperationKind::Identify, Command::Logo);
}

template <UartPort Port>
Result<void> FDO2<Port>::StartReadUserMemory(uint8_t first, uint8_t count) noexcept {
  const std::array<int32_t, 2> args{static_cast<int32_t>(first), static_cast<int32_t>(count)};
  StartOptions options{};
  options.supported = identity_.caps.user_memory;
  return begin(OperationKind::ReadUserMemory, Command::Rdum, args, options);
}

template <UartPort Port>
Result<void> FDO2<Port>::StartWriteUserMemory(uint8_t first,
                                              std::span<const int32_t> words) noexcept {
  if ((words.empty()) || (words.size() > kUserMemoryWords)) {
    if (const Error g = gate(false); g != Error::Ok) {
      return Result<void>::Failure(g);
    }
    return Result<void>::Failure(Error::InvalidArgument);
  }
  std::array<int32_t, kMaxValues> args{};
  args[0] = static_cast<int32_t>(first);
  args[1] = static_cast<int32_t>(words.size());
  for (std::size_t i = 0U; i < words.size(); ++i) {
    args[2U + i] = words[i];
  }
  StartOptions options{};
  options.supported = identity_.caps.user_memory;
  return begin(OperationKind::WriteUserMemory, Command::Wrum,
               std::span<const int32_t>{args}.first(2U + words.size()), options);
}

template <UartPort Port>
Result<void> FDO2<Port>::startTwoPoint(OperationKind kind, Command command, int32_t value,
                                       uint32_t steady_s) noexcept {
  if (const Error g = gate(false); g != Error::Ok) {
    return Result<void>::Failure(g);
  }
  if (steadyGate(steady_s) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (command == Command::Cahi) {
    if (!detail::InWindowI32(value, kCalHighMinMhpa, kCalHighMaxMhpa)) {
      return Result<void>::Failure(Error::InvalidArgument);
    }
    const std::array<int32_t, 1> args{value};
    return begin(kind, command, args, StartOptions{});
  }
  return begin(kind, command, std::span<const int32_t>{}, StartOptions{});
}

template <UartPort Port>
Result<void> FDO2<Port>::StartCalibrateZero(uint32_t steady_s) noexcept {
  return startTwoPoint(OperationKind::CalibrateZero, Command::Calo, static_cast<int32_t>(0),
                       steady_s);
}

template <UartPort Port>
Result<void> FDO2<Port>::StartCalibrateHigh(int32_t po2_mhpa, uint32_t steady_s) noexcept {
  return startTwoPoint(OperationKind::CalibrateHigh, Command::Cahi, po2_mhpa, steady_s);
}

template <UartPort Port>
Result<void> FDO2<Port>::StartSetCrcEnabled(bool enabled) noexcept {
  if ((enabled && (config_.crc == CrcMode::Off)) ||
      (!enabled && (config_.crc == CrcMode::Required))) {
    if (const Error g = gate(false); g != Error::Ok) {
      return Result<void>::Failure(g);
    }
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const std::array<int32_t, 1> args{enabled ? 1 : 0};
  StartOptions options{};
  options.crc_target = enabled ? 1 : 0;
  return begin(OperationKind::SetCrc, Command::Crce, args, options);
}

template <UartPort Port>
Result<void> FDO2<Port>::StartSetMeasurementMode(uint8_t mode) noexcept {
  const std::array<int32_t, 1> args{static_cast<int32_t>(mode)};
  StartOptions options{};
  options.supported = identity_.caps.set_mode;
  return begin(OperationKind::SetMeasurementMode, Command::Setm, args, options);
}

template <UartPort Port>
Result<void> FDO2<Port>::StartSetBroadcast(uint32_t interval_ms) noexcept {
  if (interval_ms > 0xFFFFU) {
    if (const Error g = gate(true); g != Error::Ok) {
      return Result<void>::Failure(g);
    }
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const std::array<int32_t, 1> args{static_cast<int32_t>(interval_ms)};
  StartOptions options{};
  options.supported = identity_.caps.broadcast;
  options.allow_streaming = interval_ms == 0U;
  options.tolerant = (interval_ms == 0U) && streaming_;
  return begin(OperationKind::SetBroadcast, Command::Bcst, args, options);
}

template <UartPort Port>
Result<void> FDO2<Port>::StartSetBaudRate(uint32_t baud) noexcept {
  if constexpr (detail::HasSetBaudRate<Port>) {
    if (!IsSupportedBaud(baud)) {
      if (const Error g = gate(false); g != Error::Ok) {
        return Result<void>::Failure(g);
      }
      return Result<void>::Failure(Error::InvalidArgument);
    }
    const std::array<int32_t, 1> args{static_cast<int32_t>(baud)};
    StartOptions options{};
    options.new_baud = baud;
    return begin(OperationKind::SetBaudRate, Command::Baud, args, options);
  } else {
    (void)baud;
    return Result<void>::Failure(Error::Unsupported);
  }
}

// -------------------------------------------------------------------------------------------------
// Result getters
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
Error FDO2<Port>::resultGate(bool have) const noexcept {
  if (state_ == State::Uninitialized) {
    return Error::NotInitialized;
  }
  if ((state_ == State::Waiting) || (state_ == State::PoweringUp)) {
    return Error::Busy;
  }
  return have ? Error::Ok : Error::InvalidArgument;
}

template <UartPort Port>
Result<Oxygen> FDO2<Port>::GetOxygen() const noexcept {
  const Error e = resultGate(have_oxygen_);
  return (e == Error::Ok) ? Result<Oxygen>::Success(oxygen_) : Result<Oxygen>::Failure(e);
}

template <UartPort Port>
Result<Raw> FDO2<Port>::GetRaw() noexcept {
  const Error e = resultGate(have_raw_);
  if (e != Error::Ok) {
    return Result<Raw>::Failure(e);
  }
  if (streaming_) {
    if (!frame_fresh_) {
      return Result<Raw>::Failure(Error::InvalidArgument);
    }
    frame_fresh_ = false;
  }
  return Result<Raw>::Success(raw_);
}

template <UartPort Port>
Result<Version> FDO2<Port>::GetVersion() const noexcept {
  const Error e = resultGate(have_version_);
  return (e == Error::Ok) ? Result<Version>::Success(version_) : Result<Version>::Failure(e);
}

template <UartPort Port>
Result<uint64_t> FDO2<Port>::GetUniqueId() const noexcept {
  const Error e = resultGate(have_id_);
  return (e == Error::Ok) ? Result<uint64_t>::Success(unique_id_) : Result<uint64_t>::Failure(e);
}

template <UartPort Port>
Result<std::span<const int32_t>> FDO2<Port>::GetUserMemory() const noexcept {
  const Error e = resultGate(have_user_memory_);
  if (e != Error::Ok) {
    return Result<std::span<const int32_t>>::Failure(e);
  }
  return Result<std::span<const int32_t>>::Success(
      std::span<const int32_t>{user_memory_}.first(user_memory_count_));
}

template <UartPort Port>
Result<Raw> FDO2<Port>::GetLastFrame() const noexcept {
  return have_last_frame_ ? Result<Raw>::Success(last_frame_)
                          : Result<Raw>::Failure(Error::InvalidArgument);
}

// -------------------------------------------------------------------------------------------------
// Polling
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
Result<bool> FDO2<Port>::Poll() noexcept
  requires detail::HasNowUs<Port>
{
  return pollCore(clockDelta());
}

template <UartPort Port>
Result<bool> FDO2<Port>::Poll(uint32_t elapsed_us) noexcept
  requires(!detail::HasNowUs<Port>)
{
  return pollCore(elapsed_us);
}

template <UartPort Port>
Result<bool> FDO2<Port>::toResult(const detail::Step& step) noexcept {
  if (!step.ended) {
    return Result<bool>::Success(false);
  }
  return (step.error == Error::Ok) ? Result<bool>::Success(true)
                                   : Result<bool>::Failure(step.error);
}

template <UartPort Port>
Result<bool> FDO2<Port>::pollCore(uint32_t dt_us) noexcept {
  advanceTime(dt_us);
  switch (state_) {
  case State::PoweringUp:
    return pollPowerUp();
  case State::Waiting:
    return pollWaiting();
  case State::Draining:
    return pollDraining();
  case State::Idle:
    return streaming_ ? pollStreaming() : Result<bool>::Success(true);
  case State::Uninitialized:
  case State::Streaming:
  case State::StateUnknown:
  default:
    break;
  }
  return Result<bool>::Success(true);
}

template <UartPort Port>
Error FDO2<Port>::fillRx() noexcept {
  using detail::operator""_u32;
  if (rx_pos_ < rx_len_) {
    return Error::Ok;
  }
  rx_pos_ = 0U;
  rx_len_ = 0U;
  const std::size_t got = port_.Read(rx_.data(), rx_.size(), 0_u32);
  if (got > rx_.size()) {
    return Error::Bus; // a port that returns more than it was asked for breaks its contract
  }
  rx_len_ = got;
  return Error::Ok;
}

template <UartPort Port>
void FDO2<Port>::noteIdleLine(std::span<const char> line) noexcept {
  const ParsedLine parsed = ParseLine(line, parseOptions(true));
  if ((parsed.status == ParseStatus::Ok) && parsed.known && !parsed.erro &&
      (parsed.command == Command::Mraw)) {
    broadcast_seen_ = true;
    ++counters_.broadcast_lines;
  }
}

template <UartPort Port>
void FDO2<Port>::scanInput(bool count_discard) noexcept {
  while (rx_pos_ < rx_len_) {
    const PushResult pushed = assembler_.PushByte(rx_[rx_pos_]);
    ++rx_pos_;
    if (pushed == PushResult::Line) {
      noteIdleLine(assembler_.Line());
    }
    if (count_discard) {
      ++counters_.bytes_discarded;
    }
  }
  counters_.bytes_discarded += assembler_.TakeDiscarded();
}

template <UartPort Port>
Result<bool> FDO2<Port>::pollPowerUp() noexcept {
  if (fillRx() != Error::Ok) {
    return toResult(failInit(Error::Bus));
  }
  scanInput(false);
  if (op_.elapsed_us < op_.deadline_us) {
    return Result<bool>::Success(false);
  }
  if (op_.phase == detail::Phase::PowerWait) {
    uptime_us_ = op_.elapsed_us - op_.deadline_us;
    uptime_valid_ = true;
    if (config_.broadcast_listen_ms > 0U) {
      op_.phase = detail::Phase::Listen;
      op_.elapsed_us = 0U;
      op_.deadline_us = static_cast<uint64_t>(config_.broadcast_listen_ms) * detail::kUsPerMs;
      return Result<bool>::Success(false);
    }
  }
  return toResult(sendInit(Command::Vers, std::span<const int32_t>{}, detail::Phase::Main));
}

template <UartPort Port>
Result<bool> FDO2<Port>::pollSettle() noexcept {
  if (fillRx() != Error::Ok) {
    return toResult(fail(Error::Bus, false));
  }
  scanInput(true);
  if (op_.elapsed_us < op_.deadline_us) {
    return Result<bool>::Success(false);
  }
  return toResult(startVerify());
}

template <UartPort Port>
Result<bool> FDO2<Port>::pollWaiting() noexcept {
  if (op_.phase == detail::Phase::Settle) {
    return pollSettle();
  }
  if (fillRx() != Error::Ok) {
    return toResult(fail(Error::Bus, false));
  }
  while (rx_pos_ < rx_len_) {
    const PushResult pushed = assembler_.PushByte(rx_[rx_pos_]);
    ++rx_pos_;
    if (pushed == PushResult::Pending) {
      continue;
    }
    const detail::Step step = (pushed == PushResult::Line) ? onReplyLine(assembler_.Line())
                                                           : fail(Error::InvalidReply, false);
    if (step.ended || (state_ != State::Waiting)) {
      counters_.bytes_discarded += assembler_.TakeDiscarded();
      return toResult(step);
    }
  }
  counters_.bytes_discarded += assembler_.TakeDiscarded();
  if (op_.elapsed_us >= op_.deadline_us) {
    return toResult(fail(Error::Timeout, false));
  }
  return Result<bool>::Success(false);
}

template <UartPort Port>
Result<bool> FDO2<Port>::pollDraining() noexcept {
  if (fillRx() != Error::Ok) {
    return Result<bool>::Failure(Error::Bus);
  }
  scanInput(true);
  if (drain_elapsed_us_ < drain_deadline_us_) {
    return Result<bool>::Success(false);
  }
  if (retry_pending_) {
    retry_pending_ = false;
    return toResult(restartAfterDrain());
  }
  state_ = State::Idle;
  return Result<bool>::Success(true);
}

template <UartPort Port>
Result<bool> FDO2<Port>::pollStreaming() noexcept {
  if (fillRx() != Error::Ok) {
    return Result<bool>::Failure(Error::Bus);
  }
  while (rx_pos_ < rx_len_) {
    const PushResult pushed = assembler_.PushByte(rx_[rx_pos_]);
    ++rx_pos_;
    if (pushed == PushResult::Bad) {
      ++counters_.invalid_replies;
    }
    if (pushed != PushResult::Line) {
      continue;
    }
    const detail::Step step = onStreamLine(assembler_.Line());
    if (step.ended) {
      counters_.bytes_discarded += assembler_.TakeDiscarded();
      return toResult(step);
    }
  }
  counters_.bytes_discarded += assembler_.TakeDiscarded();
  return Result<bool>::Success(false);
}

template <UartPort Port>
detail::Step FDO2<Port>::onStreamLine(std::span<const char> line) noexcept {
  const ParsedLine parsed = ParseLine(line, parseOptions(false));
  if (parsed.status == ParseStatus::BadCrc) {
    ++counters_.crc_errors;
    return detail::Step{.ended = true, .error = Error::Crc};
  }
  if (parsed.status != ParseStatus::Ok) {
    ++counters_.invalid_replies;
    return detail::Step{};
  }
  if (!parsed.known || parsed.erro || (parsed.command != Command::Mraw) || (parsed.count != 8U)) {
    ++counters_.stale_lines;
    return detail::Step{};
  }
  ++counters_.broadcast_lines;
  Raw frame{};
  const Error e = acceptRaw(parsed, frame);
  if (e != Error::Ok) {
    return detail::Step{.ended = true, .error = e};
  }
  raw_ = frame;
  have_raw_ = true;
  frame_fresh_ = true;
  stream_silent_us_ = 0U;
  return detail::Step{.ended = true, .error = Error::Ok};
}

// -------------------------------------------------------------------------------------------------
// Reply handling
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
detail::Step FDO2<Port>::onReplyLine(std::span<const char> line) noexcept {
  const bool relaxed = (op_.kind == OperationKind::SetCrc) &&
                       ((op_.phase == detail::Phase::Main) || (op_.phase == detail::Phase::Verify));
  const bool relaxed_baud =
      (op_.kind == OperationKind::SetBaudRate) && (op_.phase == detail::Phase::Verify);
  const ParsedLine parsed = ParseLine(line, parseOptions(relaxed || relaxed_baud));
  if (parsed.status == ParseStatus::BadCrc) {
    return fail(Error::Crc, false);
  }
  if (parsed.status == ParseStatus::Malformed) {
    return fail(Error::InvalidReply, false);
  }
  if (parsed.erro) {
    return onErro(parsed);
  }
  if (!parsed.known || (parsed.command != op_.command)) {
    return onForeignLine(parsed);
  }
  const std::span<const int32_t> sent{op_.args.data(), op_.arg_count};
  if (parsed.count != ExpectedCount(op_.command, sent)) {
    return fail(Error::InvalidReply, false);
  }
  if (!VerifyEcho(op_.command, sent, parsed)) {
    return fail(Error::EchoMismatch, false);
  }
  return complete(parsed);
}

template <UartPort Port>
detail::Step FDO2<Port>::onForeignLine(const ParsedLine& parsed) noexcept {
  if (parsed.known && (parsed.command == Command::Mraw)) {
    broadcast_seen_ = true;
    ++counters_.broadcast_lines;
    if (op_.tolerant) {
      return detail::Step{};
    }
  }
  ++counters_.stale_lines;
  ++op_.stale;
  return (op_.stale > kMaxStale) ? fail(Error::InvalidReply, false) : detail::Step{};
}

template <UartPort Port>
detail::Step FDO2<Port>::onErro(const ParsedLine& parsed) noexcept {
  device_error_code_ = parsed.error_code;
  ++counters_.device_errors;
  say(LogLevel::Warning, "fdo2: the module reported an error");
  // Only the codes that say "not executed" are a clean refusal. `-1` ("a non-specific error") and
  // a code the table does not list may come after a flash cycle started: the write is then
  // indeterminate (4.4).
  switch (parsed.error_code) {
  case detail::kErrParse:
  case detail::kErrRx:
  case detail::kErrHeader:
    return fail(Error::DeviceError, true);
  case detail::kErrGeneral:
    return fail(Error::DeviceError, false);
  case detail::kErrRequest:
    learnUnsupported();
    return fail(Error::Unsupported, true);
  case detail::kErrCalibration:
    return fail(Error::CalibrationRejected, true);
  default:
    break;
  }
  return fail(Error::DeviceFault, false);
}

template <UartPort Port>
void FDO2<Port>::learnUnsupported() noexcept {
  switch (op_.command) {
  case Command::Setm:
    identity_.caps.set_mode = false;
    break;
  case Command::Bcst:
    identity_.caps.broadcast = false;
    break;
  case Command::Rdum:
  case Command::Wrum:
    identity_.caps.user_memory = false;
    break;
  case Command::Vers:
  case Command::Idnr:
  case Command::Moxy:
  case Command::Mraw:
  case Command::Logo:
  case Command::Crce:
  case Command::Baud:
  case Command::Calo:
  case Command::Cahi:
  default:
    break;
  }
}

template <UartPort Port>
detail::Step FDO2<Port>::fail(Error error, bool refused) noexcept {
  switch (error) {
  case Error::Timeout:
    ++counters_.timeouts;
    break;
  case Error::Crc:
    ++counters_.crc_errors;
    break;
  case Error::InvalidReply:
    ++counters_.invalid_replies;
    break;
  case Error::Ok:
  case Error::NotInitialized:
  case Error::InvalidArgument:
  case Error::Bus:
  case Error::DeviceIdMismatch:
  case Error::Busy:
  case Error::DeviceFault:
  case Error::Unsupported:
  case Error::DeviceError:
  case Error::EchoMismatch:
  case Error::SensorFault:
  case Error::OutOfRange:
  case Error::CalibrationRejected:
  case Error::NotPermitted:
  case Error::StateUnknown:
  case Error::BroadcastActive:
  case Error::NotReady:
  default:
    break;
  }
  if (refused && isFlash(op_.command)) {
    flash_quiet_us_ = 0U; // the module answered that it did not execute the command
  }
  if (op_.kind == OperationKind::Initialize) {
    return retryOrFailInit(error);
  }
  if ((op_.phase == detail::Phase::Guard) && !refused) {
    return failUnknown(error); // nothing was written, but the module is not what the driver knew
  }
  const bool uncertain =
      !refused && op_.sent && (detail::IsUncertainKind(op_.kind) || (error == Error::StateUnknown));
  if (uncertain) {
    state_ = State::StateUnknown;
    op_.kind = OperationKind::None;
    say(LogLevel::Warning, "fdo2: state unknown, resync needed");
    return detail::Step{.ended = true, .error = error};
  }
  if (detail::IsRetryable(error, device_error_code_) && (op_.attempts_left > 0U)) {
    return scheduleRetry(error);
  }
  op_.kind = OperationKind::None;
  if (detail::NeedsDrain(error)) {
    enterDraining(false);
  } else {
    state_ = State::Idle;
  }
  return detail::Step{.ended = true, .error = error};
}

template <UartPort Port>
detail::Step FDO2<Port>::scheduleRetry(Error error) noexcept {
  --op_.attempts_left;
  ++counters_.retries;
  if (detail::NeedsDrain(error)) {
    enterDraining(true);
    return detail::Step{};
  }
  return restartAfterDrain();
}

template <UartPort Port>
detail::Step FDO2<Port>::retryOrFailInit(Error error) noexcept {
  const bool idempotent = op_.phase != detail::Phase::Disable;
  if (idempotent && detail::IsRetryable(error, device_error_code_) && (op_.attempts_left > 0U)) {
    return scheduleRetry(error);
  }
  return failInit(error);
}

template <UartPort Port>
detail::Step FDO2<Port>::restartAfterDrain() noexcept {
  const Error e = transmit();
  if (e != Error::Ok) {
    // A failed re-send ends the operation here; it never schedules another retry (no recursion).
    if (op_.kind == OperationKind::Initialize) {
      return failInit(e);
    }
    op_.kind = OperationKind::None;
    state_ = State::Idle;
    return detail::Step{.ended = true, .error = e};
  }
  armDeadline();
  state_ = State::Waiting;
  return detail::Step{};
}

template <UartPort Port>
void FDO2<Port>::enterDraining(bool retry) noexcept {
  using detail::operator""_u32;
  state_ = State::Draining;
  retry_pending_ = retry;
  drain_elapsed_us_ = 0U;
  drain_deadline_us_ = (static_cast<uint64_t>(config_.measurement_ms) * detail::kUsPerMs) +
                       static_cast<uint64_t>(AirtimeUs(MaxReplyChars(Command::Mraw, 0_u32), baud_));
}

template <UartPort Port>
detail::Step FDO2<Port>::failInit(Error error) noexcept {
  op_ = Op{};
  retry_pending_ = false;
  streaming_ = false;
  state_ = State::Uninitialized;
  if (power_set_ && (flash_quiet_us_ == 0U)) {
    (void)setPower(GpioSignal::INACTIVE);
    power_set_ = false;
  }
  // With a flash write pending (`#BCST 0` at start-up) the supply stays on: see `Deinitialize()`.
  say(LogLevel::Error, "fdo2: start-up failed");
  return detail::Step{.ended = true, .error = error};
}

template <UartPort Port>
detail::Step FDO2<Port>::finishOk() noexcept {
  op_.kind = OperationKind::None;
  state_ = State::Idle;
  return detail::Step{.ended = true, .error = Error::Ok};
}

template <UartPort Port>
detail::Step FDO2<Port>::complete(const ParsedLine& parsed) noexcept {
  if (op_.kind == OperationKind::Initialize) {
    return completeInit(parsed);
  }
  if (op_.phase == detail::Phase::Guard) {
    return completeGuard(parsed);
  }
  if (isFlash(op_.command)) {
    flash_quiet_us_ = 0U; // the echo is the module's acknowledgement (4.3, SAFETY MEASURES)
  }
  if (op_.phase == detail::Phase::Verify) {
    return completeVerify(parsed);
  }
  return completeMain(parsed);
}

template <UartPort Port>
Version FDO2<Port>::decodeVersion(const ParsedLine& parsed) noexcept {
  Version v{};
  v.device_id = static_cast<int32_t>(parsed.values[0]);
  v.channels = static_cast<int32_t>(parsed.values[1]);
  v.firmware = static_cast<int32_t>(parsed.values[2]);
  v.sensors = static_cast<int32_t>(parsed.values[3]);
  return v;
}

template <UartPort Port>
detail::Step FDO2<Port>::completeMain(const ParsedLine& parsed) noexcept {
  switch (op_.kind) {
  case OperationKind::Version:
    if (!versionMatches(decodeVersion(parsed))) {
      return failUnknown(Error::DeviceIdMismatch);
    }
    version_ = decodeVersion(parsed);
    have_version_ = true;
    crc_active_ = crc_active_ || (parsed.crc_present && (config_.crc != CrcMode::Off));
    break;
  case OperationKind::UniqueId:
    if (parsed.big != identity_.unique_id) {
      return failUnknown(Error::DeviceIdMismatch); // another module is on the line
    }
    unique_id_ = parsed.big;
    have_id_ = true;
    break;
  case OperationKind::MeasureOxygen: {
    Oxygen o{};
    if (const Error e = acceptOxygen(parsed, o); e != Error::Ok) {
      return fail(e, false);
    }
    oxygen_ = o;
    have_oxygen_ = true;
    break;
  }
  case OperationKind::MeasureRaw: {
    Raw r{};
    if (const Error e = acceptRaw(parsed, r); e != Error::Ok) {
      return fail(e, false);
    }
    raw_ = r;
    have_raw_ = true;
    break;
  }
  case OperationKind::ReadUserMemory:
    storeUserMemory(parsed);
    break;
  case OperationKind::SetCrc:
    return startVerify();
  case OperationKind::SetBaudRate:
    return completeBaud();
  case OperationKind::SetBroadcast:
    streaming_ = op_.args[0] > 0;
    stream_silent_us_ = 0U;
    break;
  case OperationKind::None:
  case OperationKind::Initialize:
  case OperationKind::Identify:
  case OperationKind::WriteUserMemory:
  case OperationKind::CalibrateZero:
  case OperationKind::CalibrateHigh:
  case OperationKind::SetMeasurementMode:
  default:
    break;
  }
  return finishOk();
}

template <UartPort Port>
void FDO2<Port>::storeUserMemory(const ParsedLine& parsed) noexcept {
  user_memory_count_ = parsed.count - 2U;
  for (std::size_t i = 0U; i < user_memory_count_; ++i) {
    user_memory_[i] = static_cast<int32_t>(parsed.values[2U + i]);
  }
  have_user_memory_ = true;
}

template <UartPort Port>
detail::Step FDO2<Port>::completeBaud() noexcept {
  using detail::operator""_u32;
  if constexpr (detail::HasSetBaudRate<Port>) {
    if (!port_.SetBaudRate(op_.new_baud)) {
      return fail(Error::Bus, false);
    }
    baud_ = op_.new_baud;
    op_.phase = detail::Phase::Settle;
    op_.elapsed_us = 0U;
    op_.deadline_us = 2U * static_cast<uint64_t>(AirtimeUs(1_u32, baud_));
    return detail::Step{};
  } else {
    return fail(Error::Unsupported, false);
  }
}

template <UartPort Port>
detail::Step FDO2<Port>::startVerify() noexcept {
  op_.phase = detail::Phase::Verify;
  op_.command = Command::Vers;
  op_.arg_count = 0U;
  op_.stale = 0U;
  if (const Error e = transmit(); e != Error::Ok) {
    return fail(e, false);
  }
  armDeadline();
  state_ = State::Waiting;
  return detail::Step{};
}

template <UartPort Port>
detail::Step FDO2<Port>::completeVerify(const ParsedLine& parsed) noexcept {
  if (!versionMatches(decodeVersion(parsed))) {
    return failUnknown(Error::DeviceIdMismatch);
  }
  version_ = decodeVersion(parsed);
  have_version_ = true;
  if (op_.kind == OperationKind::SetCrc) {
    crc_active_ = parsed.crc_present;
    if (crc_active_ != (op_.crc_target != 0)) {
      return fail(Error::StateUnknown, false);
    }
  } else {
    config_.baud = baud_;
    baud_candidate_ = 0U;
  }
  return finishOk();
}

// -------------------------------------------------------------------------------------------------
// Guard of the flash-writing commands
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
bool FDO2<Port>::versionMatches(const Version& version) const noexcept {
  const Version& known = identity_.version;
  return (version.device_id == known.device_id) && (version.channels == known.channels) &&
         (version.firmware == known.firmware) && (version.sensors == known.sensors);
}

template <UartPort Port>
detail::Step FDO2<Port>::failUnknown(Error error) noexcept {
  op_.kind = OperationKind::None;
  state_ = State::StateUnknown;
  say(LogLevel::Warning, "fdo2: state unknown, resync needed");
  return detail::Step{.ended = true, .error = error};
}

template <UartPort Port>
Error FDO2<Port>::checkCalibrationReading(const ParsedLine& parsed) const noexcept {
  const Oxygen o = decodeOxygen(parsed);
  if (const Error e = checkOxygen(o); e != Error::Ok) {
    return e;
  }
  if (o.warming_up) {
    return Error::NotReady; // 3 minutes of warm-up with reduced accuracy (2)
  }
  // The gas must be the standard: a reading far from it would calibrate the wrong point.
  const int64_t expected = static_cast<int64_t>(op_.expect_mhpa);
  const int64_t relative = (expected * static_cast<int64_t>(config_.cal_window_pct)) / 100;
  const int64_t window = (relative > static_cast<int64_t>(config_.cal_window_min_mhpa))
                             ? relative
                             : static_cast<int64_t>(config_.cal_window_min_mhpa);
  const int64_t distance = static_cast<int64_t>(o.o2_mhpa) - expected;
  return (((distance < 0) ? -distance : distance) <= window) ? Error::Ok : Error::OutOfRange;
}

template <UartPort Port>
detail::Step FDO2<Port>::completeGuard(const ParsedLine& parsed) noexcept {
  const bool calibration =
      (op_.main_command == Command::Calo) || (op_.main_command == Command::Cahi);
  if (calibration) {
    if (const Error e = checkCalibrationReading(parsed); e != Error::Ok) {
      op_.kind = OperationKind::None; // refused: nothing was written, the module is as it was
      state_ = State::Idle;
      return detail::Step{.ended = true, .error = e};
    }
  } else if (!versionMatches(decodeVersion(parsed))) {
    return failUnknown(Error::DeviceIdMismatch);
  }
  op_.command = op_.main_command;
  op_.arg_count = op_.main_arg_count;
  op_.phase = detail::Phase::Main;
  op_.stale = 0U;
  if (const Error e = transmit(); e != Error::Ok) {
    return fail(e, false);
  }
  armDeadline();
  return detail::Step{};
}

// -------------------------------------------------------------------------------------------------
// Start-up sequence
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
detail::Step FDO2<Port>::sendInit(Command command, std::span<const int32_t> args,
                                  detail::Phase phase) noexcept {
  op_.command = command;
  op_.arg_count = args.size();
  for (std::size_t i = 0U; i < args.size(); ++i) {
    op_.args[i] = args[i];
  }
  op_.phase = phase;
  op_.stale = 0U;
  op_.tolerant =
      ((phase == detail::Phase::Disable) || (phase == detail::Phase::Idnr)) && broadcast_seen_;
  op_.attempts_left = (phase == detail::Phase::Disable) ? 0U : config_.retries;
  if (const Error e = transmit(); e != Error::Ok) {
    return failInit(e);
  }
  armDeadline();
  state_ = State::Waiting;
  return detail::Step{};
}

template <UartPort Port>
detail::Step FDO2<Port>::completeInit(const ParsedLine& parsed) noexcept {
  switch (op_.phase) {
  case detail::Phase::Main:
    return completeInitVersion(parsed);
  case detail::Phase::Disable:
    flash_quiet_us_ = 0U; // `#BCST 0` acknowledged
    broadcast_seen_ = false;
    streaming_ = false;
    return sendInit(Command::Idnr, std::span<const int32_t>{}, detail::Phase::Idnr);
  case detail::Phase::Idnr:
    unique_id_ = parsed.big;
    have_id_ = true;
    identity_.unique_id = parsed.big;
    streaming_ = broadcast_seen_ && (config_.broadcast == BroadcastPolicy::Stream);
    stream_silent_us_ = 0U;
    op_ = Op{};
    state_ = State::Idle;
    say(LogLevel::Info, "fdo2: initialised");
    return detail::Step{.ended = true, .error = Error::Ok};
  case detail::Phase::PowerWait:
  case detail::Phase::Listen:
  case detail::Phase::Settle:
  case detail::Phase::Verify:
  case detail::Phase::Guard:
  default:
    break;
  }
  return failInit(Error::InvalidReply);
}

template <UartPort Port>
detail::Step FDO2<Port>::completeInitVersion(const ParsedLine& parsed) noexcept {
  const Version v = decodeVersion(parsed);
  if ((v.device_id != kDeviceId) || (v.channels != kChannels) ||
      ((static_cast<uint32_t>(v.sensors) & static_cast<uint32_t>(kSensorOxygen)) == 0U)) {
    return failInit(Error::DeviceIdMismatch);
  }
  if (v.firmware < kMinFirmware) {
    return failInit(Error::Unsupported);
  }
  version_ = v;
  have_version_ = true;
  identity_.version = v;
  identity_.family = config_.family;
  identity_.caps = CapabilitiesFor(config_.family, v.firmware);
  identity_.noise_erratum = HasNoiseErratum(v.firmware);
  crc_active_ = parsed.crc_present && (config_.crc != CrcMode::Off);
  if (!broadcast_seen_) {
    return sendInit(Command::Idnr, std::span<const int32_t>{}, detail::Phase::Idnr);
  }
  switch (config_.broadcast) {
  case BroadcastPolicy::Stream:
    return sendInit(Command::Idnr, std::span<const int32_t>{}, detail::Phase::Idnr);
  case BroadcastPolicy::Disable: {
    if (flashGate() != Error::Ok) {
      return failInit(Error::NotPermitted);
    }
    const std::array<int32_t, 1> off{0};
    return sendInit(Command::Bcst, off, detail::Phase::Disable);
  }
  case BroadcastPolicy::Refuse:
  default:
    break;
  }
  return failInit(Error::BroadcastActive);
}

// -------------------------------------------------------------------------------------------------
// Measurement acceptance
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
Oxygen FDO2<Port>::decodeOxygen(const ParsedLine& parsed) const noexcept {
  Oxygen o{};
  o.o2_mhpa = static_cast<int32_t>(parsed.values[0]);
  o.temperature_mc = static_cast<int32_t>(parsed.values[1]);
  o.status.word = static_cast<uint32_t>(parsed.values[2]);
  o.quality = o.status.Classify();
  o.warming_up = !uptime_valid_ || (uptime_us_ < detail::kWarmUpUs);
  return o;
}

template <UartPort Port>
Error FDO2<Port>::checkStatus(const Status& status) const noexcept {
  switch (status.Classify()) {
  case Quality::Good:
  case Quality::Warning:
    return Error::Ok;
  case Quality::Degraded:
    return (config_.status_policy == StatusPolicy::Documented) ? Error::Ok : Error::SensorFault;
  case Quality::Invalid:
  default:
    break;
  }
  return Error::SensorFault;
}

template <UartPort Port>
Error FDO2<Port>::checkOxygen(const Oxygen& o) const noexcept {
  if (const Error s = checkStatus(o.status); s != Error::Ok) {
    return s;
  }
  const bool plausible =
      detail::InWindowI32(o.o2_mhpa, config_.o2_min_mhpa, config_.o2_max_mhpa) &&
      detail::InWindowI32(o.temperature_mc, config_.temp_min_mc, config_.temp_max_mc);
  return plausible ? Error::Ok : Error::OutOfRange;
}

template <UartPort Port>
Error FDO2<Port>::acceptOxygen(const ParsedLine& parsed, Oxygen& out) noexcept {
  const Oxygen o = decodeOxygen(parsed);
  last_frame_ = Raw{};
  last_frame_.oxygen = o;
  have_last_frame_ = true;
  if (const Error e = checkOxygen(o); e != Error::Ok) {
    return e;
  }
  out = o;
  return Error::Ok;
}

template <UartPort Port>
Error FDO2<Port>::plausibleRaw(const Raw& raw) const noexcept {
  const bool plausible = (raw.signal_uv >= 0) && (raw.ambient_uv >= 0) &&
                         (raw.pressure_ubar >= 0) &&
                         detail::InWindowI32(raw.dphi_mdeg, 0, 90'000) &&
                         detail::InWindowI32(raw.humidity_mpct, 0, config_.humidity_max_mpct);
  return plausible ? Error::Ok : Error::OutOfRange;
}

template <UartPort Port>
Error FDO2<Port>::acceptRaw(const ParsedLine& parsed, Raw& out) noexcept {
  Raw r{};
  r.oxygen = decodeOxygen(parsed);
  r.dphi_mdeg = static_cast<int32_t>(parsed.values[3]);
  r.signal_uv = static_cast<int32_t>(parsed.values[4]);
  r.ambient_uv = static_cast<int32_t>(parsed.values[5]);
  r.pressure_ubar = static_cast<int32_t>(parsed.values[6]);
  r.humidity_mpct = static_cast<int32_t>(parsed.values[7]);
  last_frame_ = r;
  have_last_frame_ = true;
  if (const Error e = checkOxygen(r.oxygen); e != Error::Ok) {
    return e;
  }
  if (const Error e = plausibleRaw(r); e != Error::Ok) {
    return e;
  }
  out = r;
  return Error::Ok;
}

// -------------------------------------------------------------------------------------------------
// Blocking calls
// -------------------------------------------------------------------------------------------------

template <UartPort Port>
uint64_t FDO2<Port>::budgetUs() const noexcept {
  const uint64_t drain = (static_cast<uint64_t>(config_.measurement_ms) * detail::kUsPerMs) +
                         static_cast<uint64_t>(AirtimeUs(detail::kLateReplyChars, baud_));
  uint64_t per_attempt = op_.deadline_us + drain;
  if (op_.kind == OperationKind::Initialize) {
    per_attempt += (static_cast<uint64_t>(config_.power_up_ms) + config_.broadcast_listen_ms) *
                   detail::kUsPerMs;
    per_attempt += (3U * (static_cast<uint64_t>(config_.command_ms) * detail::kUsPerMs)) +
                   (2U * detail::kBroadcastWaitUs);
    per_attempt += 4U * static_cast<uint64_t>(AirtimeUs(kMaxLineLength, baud_));
  }
  if (op_.kind == OperationKind::SetBaudRate) {
    per_attempt += static_cast<uint64_t>(config_.command_ms) * detail::kUsPerMs;
  }
  if (op_.phase == detail::Phase::Guard) {
    // The guard is followed by the write itself: its allowance and airtime come on top.
    per_attempt += (static_cast<uint64_t>(detail::ProcessingMs(op_.main_command, config_)) +
                    config_.margin_ms) *
                   detail::kUsPerMs;
    per_attempt += 2U * static_cast<uint64_t>(AirtimeUs(kMaxLineLength, baud_));
  }
  return (per_attempt * (1U + static_cast<uint64_t>(config_.retries))) + 1'000'000U;
}

template <UartPort Port>
Result<bool> FDO2<Port>::stepOnce() noexcept {
  detail::DelayUs(port_, detail::kPollStepUs);
  if constexpr (detail::HasNowUs<Port>) {
    return Poll();
  } else {
    return Poll(detail::kPollStepUs);
  }
}

template <UartPort Port>
Result<void> FDO2<Port>::waitIdle() noexcept {
  const uint64_t cap = (drain_deadline_us_ / detail::kPollStepUs) + 8U;
  for (uint64_t i = 0U; (i < cap) && (state_ == State::Draining); ++i) {
    (void)stepOnce();
  }
  return Result<void>::Success();
}

template <UartPort Port>
Result<void> FDO2<Port>::await() noexcept {
  const uint64_t cap = (budgetUs() / detail::kPollStepUs) + 16U;
  for (uint64_t i = 0U; i < cap; ++i) {
    const Result<bool> r = stepOnce();
    if (!r.ok()) {
      (void)waitIdle();
      return Result<void>::Failure(r.error);
    }
    if (r.value) {
      return Result<void>::Success();
    }
  }
  (void)AbortOperation();
  (void)waitIdle();
  return Result<void>::Failure(Error::Timeout);
}

template <UartPort Port>
Result<void> FDO2<Port>::runBlocking(const Result<void>& started) noexcept {
  if (!started.ok()) {
    return started;
  }
  return await();
}

template <UartPort Port>
Result<Oxygen> FDO2<Port>::MeasureOxygen() noexcept {
  if (const auto r = runBlocking(StartMeasureOxygen()); !r) {
    return Result<Oxygen>::Failure(r.error);
  }
  return GetOxygen();
}

template <UartPort Port>
Result<Raw> FDO2<Port>::MeasureRaw() noexcept {
  if (const auto r = runBlocking(StartMeasureRaw()); !r) {
    return Result<Raw>::Failure(r.error);
  }
  return GetRaw();
}

template <UartPort Port>
Result<Raw> FDO2<Port>::ReadRawFrame() noexcept {
  const uint8_t saved = config_.retries;
  config_.retries = 0U; // single shot: a failed read is reported, not repeated
  const Result<Raw> raw = MeasureRaw();
  config_.retries = saved;
  return raw;
}

template <UartPort Port>
Result<int32_t> FDO2<Port>::MeasureVolumePercentMilli() noexcept {
  const Result<Raw> raw = MeasureRaw();
  if (!raw.ok()) {
    return Result<int32_t>::Failure(raw.error);
  }
  int32_t pressure = config_.pressure_reference_ubar;
  if (pressure == 0) {
    if (raw.value.oxygen.status.PressureSensorFailed()) {
      return Result<int32_t>::Failure(Error::SensorFault);
    }
    pressure = raw.value.pressure_ubar;
    if (!detail::InWindowI32(pressure, config_.pressure_min_ubar, config_.pressure_max_ubar)) {
      return Result<int32_t>::Failure(Error::OutOfRange);
    }
  }
  return VolumePercentMilli(raw.value.oxygen.o2_mhpa, pressure);
}

template <UartPort Port>
Result<Version> FDO2<Port>::ReadVersion() noexcept {
  if (const auto r = runBlocking(StartReadVersion()); !r) {
    return Result<Version>::Failure(r.error);
  }
  return GetVersion();
}

template <UartPort Port>
Result<uint64_t> FDO2<Port>::ReadUniqueId() noexcept {
  if (const auto r = runBlocking(StartReadUniqueId()); !r) {
    return Result<uint64_t>::Failure(r.error);
  }
  return GetUniqueId();
}

template <UartPort Port>
Result<void> FDO2<Port>::Identify() noexcept {
  return runBlocking(StartIdentify());
}

template <UartPort Port>
Result<void> FDO2<Port>::ReadUserMemory(uint8_t first, std::span<int32_t> out) noexcept {
  if (out.empty() || (out.size() > kUserMemoryWords)) {
    return Result<void>::Failure((gate(false) != Error::Ok) ? gate(false) : Error::InvalidArgument);
  }
  if (const auto r = runBlocking(StartReadUserMemory(first, static_cast<uint8_t>(out.size())));
      !r) {
    return r;
  }
  const auto words = GetUserMemory();
  if (!words.ok()) {
    return Result<void>::Failure(words.error);
  }
  for (std::size_t i = 0U; i < out.size(); ++i) {
    out[i] = words.value[i];
  }
  return Result<void>::Success();
}

template <UartPort Port>
Result<void> FDO2<Port>::WriteUserMemory(uint8_t first, std::span<const int32_t> in) noexcept {
  return runBlocking(StartWriteUserMemory(first, in));
}

template <UartPort Port>
Result<void> FDO2<Port>::CalibrateZero(uint32_t steady_s) noexcept {
  return runBlocking(StartCalibrateZero(steady_s));
}

template <UartPort Port>
Result<void> FDO2<Port>::CalibrateHigh(int32_t po2_mhpa, uint32_t steady_s) noexcept {
  return runBlocking(StartCalibrateHigh(po2_mhpa, steady_s));
}

template <UartPort Port>
Result<void> FDO2<Port>::CalibrateAir(uint32_t steady_s, uint32_t fraction_ppm) noexcept {
  if (const Error g = gate(false); g != Error::Ok) {
    return Result<void>::Failure(g);
  }
  if ((steadyGate(steady_s) != Error::Ok) || (fraction_ppm == 0U) || (fraction_ppm > 1'000'000U)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (const Error f = flashGate(); f != Error::Ok) {
    return Result<void>::Failure(f);
  }
  const Result<Raw> raw = MeasureRaw();
  if (!raw.ok()) {
    return Result<void>::Failure(raw.error);
  }
  const Quality quality = raw.value.oxygen.quality;
  if ((quality != Quality::Good) && (quality != Quality::Warning)) {
    return Result<void>::Failure(Error::SensorFault);
  }
  if (!detail::InWindowI32(raw.value.pressure_ubar, config_.pressure_min_ubar,
                           config_.pressure_max_ubar)) {
    return Result<void>::Failure(Error::OutOfRange);
  }
  const Result<int32_t> po2 = AirO2Mhpa(raw.value.pressure_ubar, fraction_ppm);
  if (!po2.ok()) {
    return Result<void>::Failure(po2.error);
  }
  return CalibrateHigh(po2.value, steady_s);
}

template <UartPort Port>
Result<void> FDO2<Port>::SetCrcEnabled(bool enabled) noexcept {
  return runBlocking(StartSetCrcEnabled(enabled));
}

template <UartPort Port>
Result<void> FDO2<Port>::SetMeasurementMode(uint8_t mode) noexcept {
  return runBlocking(StartSetMeasurementMode(mode));
}

template <UartPort Port>
Result<void> FDO2<Port>::SetBroadcast(uint32_t interval_ms) noexcept {
  return runBlocking(StartSetBroadcast(interval_ms));
}

template <UartPort Port>
Result<void> FDO2<Port>::DisableBroadcast() noexcept {
  using detail::operator""_u32;
  if ((state_ == State::Idle) && streaming_) {
    // The vendor's workaround for the firmware 3.51 collision bug (G1 data sheet 6.1): send the
    // command right after a frame has ended. The interval is unknown, so wait at most 10 s.
    const uint64_t cap = ((detail::kBroadcastWaitUs +
                           (static_cast<uint64_t>(config_.margin_ms) * detail::kUsPerMs)) /
                          detail::kPollStepUs) +
                         8U;
    for (uint64_t i = 0U; i < cap; ++i) {
      const Result<bool> r = stepOnce();
      if (!r.ok() || r.value) {
        break;
      }
    }
  }
  return runBlocking(StartSetBroadcast(0_u32));
}

template <UartPort Port>
Result<void> FDO2<Port>::SetBaudRate(uint32_t baud) noexcept {
  return runBlocking(StartSetBaudRate(baud));
}

template <UartPort Port>
Result<bool> FDO2<Port>::ProbeBroadcast(uint32_t window_ms) noexcept {
  if (const Error g = gate(false); g != Error::Ok) {
    return Result<bool>::Failure(g);
  }
  if ((window_ms == 0U) || (window_ms > kBroadcastListenMsMax)) {
    return Result<bool>::Failure(Error::InvalidArgument);
  }
  broadcast_seen_ = false;
  assembler_.Reset(config_.ignore_lf);
  rx_len_ = 0U;
  rx_pos_ = 0U;
  if (!port_.FlushInput()) {
    return Result<bool>::Failure(Error::Bus);
  }
  for (uint32_t ms = 0U; ms < window_ms; ++ms) {
    detail::DelayUs(port_, detail::kPollStepUs);
    advanceTime(stepDelta());
    if (fillRx() != Error::Ok) {
      return Result<bool>::Failure(Error::Bus);
    }
    scanInput(false);
  }
  return Result<bool>::Success(broadcast_seen_);
}

} // namespace fdo2

#endif // FDO2_IPP
