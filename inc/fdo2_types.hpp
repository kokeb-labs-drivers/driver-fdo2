/**
 * @file fdo2_types.hpp
 * @brief Public types of the FDO2 driver: errors, `Result<T>`, the status word, measurements,
 * configuration, firmware capabilities and the pure conversions.
 * @copyright Copyright (c) 2026 The driver-fdo2 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is plain data and `constexpr` functions with no platform dependency, so
 * handlers, tests and tools can include it without the driver class. Values keep the units the
 * module reports (datasheet section 4.3): 10^-3 hPa, 10^-3 degC, millidegrees, microvolts,
 * microbar and 10^-3 %RH, as integers; the unit is part of each name.
 */
#ifndef FDO2_TYPES_HPP
#define FDO2_TYPES_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "fdo2_detail.hpp"
#include "fdo2_version.h"

/** @brief The FDO2 driver: everything it declares lives here; internals in `detail`. */
namespace fdo2 {

/**
 * @defgroup fdo2_types Driver types
 * @brief Errors, results, status, measurements, configuration and conversions.
 * @{
 */

/**
 * @brief Why a call failed. Every call of the driver returns one of these in its `Result`.
 *
 * Values 0 to 9 are the shared base set; chip-specific values start at 16. `[[nodiscard]]` on
 * the type: a function that returns a bare `Error` cannot have it ignored either (KB-CPP20-037).
 */
enum class [[nodiscard]] Error : uint8_t {
  // Base set: the same names and values in every driver of the org (standards/driver.md).
  Ok = 0,               ///< The call succeeded.
  NotInitialized = 1,   ///< `EnsureInitialized()` has not succeeded, or `Deinitialize()` ran since.
  InvalidArgument = 2,  ///< An argument or a `Config` field is out of range; nothing was sent.
  Bus = 3,              ///< A port hook returned `false`.
  Timeout = 4,          ///< No complete reply within the deadline (datasheet 4.1, 4.3).
  DeviceIdMismatch = 5, ///< `#VERS` is not an FDO2 with an oxygen sensor: wrong device or wiring.
  Crc = 6,              ///< The CRC-16 of a reply did not match (datasheet 4.2).
  Busy = 7,             ///< An operation is pending, draining or the module streams.
  DeviceFault = 8,      ///< `#ERRO` with a code the datasheet calls potentially fatal (4.4).
  Unsupported = 9,      ///< The hook, firmware revision or family does not offer this call.
  // Chip-specific values start at 16; each one is returned by some call (tests/test_errors.cpp).
  DeviceError = 16,  ///< `#ERRO -1`, `-21`, `-22` or `-23`: the module could not do it.
  InvalidReply = 17, ///< A line the protocol grammar refuses (bytes, header, tokens, number).
  EchoMismatch = 18, ///< The header or an echoed value differs from what was sent (4.2).
  SensorFault = 19,  ///< The status word was rejected by the `StatusPolicy` (4.3, `#MOXY`).
  OutOfRange = 20,   ///< A decoded value is outside the plausibility window of the `Config`.
  CalibrationRejected = 21, ///< `#ERRO -60`: the calibration is outside the expected range.
  NotPermitted = 22,        ///< A flash-writing call without consent, or over the write budget.
  StateUnknown = 23,        ///< A flash write or baud change ended without a confirmed echo.
  BroadcastActive = 24,     ///< The module streams `#MRAW` lines and the call needs it silent.
  NotReady = 25,            ///< Warming up: a calibration needs a settled sensor (datasheet 2).
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string such as `"Timeout"`; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::DeviceError:
    return "DeviceError";
  case Error::InvalidReply:
    return "InvalidReply";
  case Error::EchoMismatch:
    return "EchoMismatch";
  case Error::SensorFault:
    return "SensorFault";
  case Error::OutOfRange:
    return "OutOfRange";
  case Error::CalibrationRejected:
    return "CalibrationRejected";
  case Error::NotPermitted:
    return "NotPermitted";
  case Error::StateUnknown:
    return "StateUnknown";
  case Error::BroadcastActive:
    return "BroadcastActive";
  case Error::NotReady:
    return "NotReady";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 *
 * Check `ok()` (or the `bool` conversion) before reading `value`; on failure `value` is `T{}`.
 *
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected that the driver recovered from.
  Info,      ///< Lifecycle events: initialised, reset.
  Debug,     ///< Detail for bring-up.
};

/** @brief The module generation the driver talks to (datasheet 1.1; ambiguity A-3). */
enum class Family : uint8_t {
  Fdo2G2 = 0, ///< FDO2-G2, the current module (firmware 3.55 and later); `#SETM` is documented.
  Fdo2 = 1,   ///< FDO2, the legacy module (firmware 3.28 to 3.55); no `#SETM` in its data sheet.
};

/** @brief Whether replies carry the CRC-16 suffix (datasheet 4.2). */
enum class CrcMode : uint8_t {
  Auto = 0,     ///< Learn from the first `#VERS` reply: a suffix means CRC is on.
  Off = 1,      ///< Never expect a suffix: a reply with one is `InvalidReply`.
  Required = 2, ///< Every reply must carry a valid suffix (except `#ERRO` with `erro_without_crc`).
};

/** @brief What a status word other than 0 or 1 does (datasheet 4.3, ambiguity A-1). */
enum class StatusPolicy : uint8_t {
  Datasheet = 0, ///< "S should always be 0 or 1": any other word is `SensorFault`.
  Documented =
      1, ///< Only fatal and undefined bits are `SensorFault`; bits 7, 9, 10 are `Degraded`.
};

/** @brief What start-up does when the module streams `#MRAW` lines (datasheet 4.3, `#BCST`). */
enum class BroadcastPolicy : uint8_t {
  Refuse = 0,  ///< Fail with `BroadcastActive`; the driver stays uninitialised.
  Stream = 1,  ///< Go to the `Streaming` state and decode the frames.
  Disable = 2, ///< Send `#BCST 0` (a flash write: needs `allow_persistent_writes`).
};

/** @brief How far a reading can be trusted, from the status word. */
enum class Quality : uint8_t {
  Good = 0,     ///< S = 0.
  Warning = 1,  ///< S = 1: detector amplification reduced, the reading is valid.
  Degraded = 2, ///< No fatal or undefined bit; bit 7, 9 or 10 set.
  Invalid = 3,  ///< A fatal bit (1 to 5) or an undefined bit.
};

/** @brief The operation the driver has in flight. */
enum class OperationKind : uint8_t {
  None = 0,           ///< Nothing pending.
  Initialize,         ///< `BeginInitialize()` and its `#VERS` and `#IDNR` steps.
  Version,            ///< `#VERS`.
  UniqueId,           ///< `#IDNR`.
  MeasureOxygen,      ///< `#MOXY`.
  MeasureRaw,         ///< `#MRAW`.
  Identify,           ///< `#LOGO`.
  ReadUserMemory,     ///< `#RDUM`.
  WriteUserMemory,    ///< `#WRUM`.
  CalibrateZero,      ///< `#CALO`.
  CalibrateHigh,      ///< `#CAHI`.
  SetCrc,             ///< `#CRCE`.
  SetMeasurementMode, ///< `#SETM`.
  SetBroadcast,       ///< `#BCST`.
  SetBaudRate,        ///< `#BAUD`.
};

/** @brief The driver's state machine (docs/non-blocking.md). */
enum class State : uint8_t {
  Uninitialized = 0, ///< Nothing started, or `Deinitialize()` ran.
  PoweringUp,        ///< Waiting `power_up_ms` before the first command.
  Idle,              ///< Ready for a call.
  Waiting,           ///< An operation is waiting for its reply.
  Streaming,         ///< The module sends `#MRAW` lines by itself.
  Draining,          ///< Discarding input after a failed read, so a late reply is never taken.
  StateUnknown,      ///< A flash write or baud change ended without a confirmed echo.
};

namespace detail {
/// Last enumerator of `Family` (detail::IsEnumerator).
template <>
struct EnumLast<Family> {
  static constexpr Family kValue = Family::Fdo2; ///< The last enumerator.
};
/// Last enumerator of `CrcMode`.
template <>
struct EnumLast<CrcMode> {
  static constexpr CrcMode kValue = CrcMode::Required; ///< The last enumerator.
};
/// Last enumerator of `StatusPolicy`.
template <>
struct EnumLast<StatusPolicy> {
  static constexpr StatusPolicy kValue = StatusPolicy::Documented; ///< The last enumerator.
};
/// Last enumerator of `BroadcastPolicy`.
template <>
struct EnumLast<BroadcastPolicy> {
  static constexpr BroadcastPolicy kValue = BroadcastPolicy::Disable; ///< The last enumerator.
};
} // namespace detail

/**
 * @brief The status word S of `#MOXY` and `#MRAW`, decoded (datasheet 4.3, `#MOXY`). Pure
 * functions of the word; the word is kept whole.
 */
struct Status {
  uint32_t word{0}; ///< The unsigned 32 bit status word as the module sent it.

  /** @return Bit 0: WARNING, detector amplification reduced; the reading is valid. */
  [[nodiscard]] constexpr bool AmplificationReduced() const noexcept {
    return has(kBit0);
  }
  /** @return Bit 1: FATAL, oxygen signal intensity too low (below 20 mV). */
  [[nodiscard]] constexpr bool SignalTooLow() const noexcept {
    return has(kBit1);
  }
  /** @return Bit 2: FATAL, oxygen signal or ambient light too high. */
  [[nodiscard]] constexpr bool SignalTooHigh() const noexcept {
    return has(kBit2);
  }
  /** @return Bit 3: FATAL, reference signal intensity too low (below 20 mV). */
  [[nodiscard]] constexpr bool ReferenceTooLow() const noexcept {
    return has(kBit3);
  }
  /** @return Bit 4: FATAL, reference signal or ambient light too high (above 2400 mV). */
  [[nodiscard]] constexpr bool ReferenceTooHigh() const noexcept {
    return has(kBit4);
  }
  /** @return Bit 5: FATAL, temperature sensor failure. */
  [[nodiscard]] constexpr bool TemperatureSensorFailed() const noexcept {
    return has(kBit5);
  }
  /** @return Bit 7: WARNING, humidity inside the housing above 90 %RH. */
  [[nodiscard]] constexpr bool HumidityHigh() const noexcept {
    return has(kBit7);
  }
  /** @return Bit 9: ERROR, pressure sensor failure; the pO2 is not affected. */
  [[nodiscard]] constexpr bool PressureSensorFailed() const noexcept {
    return has(kBit9);
  }
  /** @return Bit 10: ERROR, humidity sensor failure; the pO2 is not affected. */
  [[nodiscard]] constexpr bool HumiditySensorFailed() const noexcept {
    return has(kBit10);
  }
  /** @return Any of the FATAL bits 1 to 5. */
  [[nodiscard]] constexpr bool AnyFatal() const noexcept {
    return (word & 0x3EU) != 0U;
  }
  /** @return Any bit the datasheet does not define: 6, 8 and 11 to 31. */
  [[nodiscard]] constexpr bool UndefinedBits() const noexcept {
    return (word & ~0x6BFU) != 0U;
  }
  /** @return The quality class of the word (see `Quality`). */
  [[nodiscard]] constexpr Quality Classify() const noexcept {
    if (AnyFatal() || UndefinedBits()) {
      return Quality::Invalid;
    }
    if ((word & 0x680U) != 0U) {
      return Quality::Degraded;
    }
    return (word == 0U) ? Quality::Good : Quality::Warning;
  }

private:
  static constexpr uint32_t kBit0{0x1U};
  static constexpr uint32_t kBit1{0x2U};
  static constexpr uint32_t kBit2{0x4U};
  static constexpr uint32_t kBit3{0x8U};
  static constexpr uint32_t kBit4{0x10U};
  static constexpr uint32_t kBit5{0x20U};
  static constexpr uint32_t kBit7{0x80U};
  static constexpr uint32_t kBit9{0x200U};
  static constexpr uint32_t kBit10{0x400U};
  [[nodiscard]] constexpr bool has(uint32_t mask) const noexcept {
    return (word & mask) != 0U;
  }
};

/** @brief `#MOXY` result: oxygen partial pressure, temperature and status. */
struct Oxygen {
  int32_t o2_mhpa{0};             ///< Oxygen partial pressure, 10^-3 hPa (typ. 0 to 210 hPa).
  int32_t temperature_mc{0};      ///< Module temperature, 10^-3 degC (typ. 0 to 50 degC).
  Status status{};                ///< The status word of this measurement.
  Quality quality{Quality::Good}; ///< `status.Classify()`, kept for the caller.
  bool warming_up{false}; ///< Less than 3 minutes since power-up (datasheet 2): reduced accuracy.
};

/** @brief `#MRAW` result: the `Oxygen` fields plus the raw values. */
struct Raw {
  Oxygen oxygen{};          ///< O, T and S.
  int32_t dphi_mdeg{0};     ///< Phase shift dphi, 10^-3 degree (typ. 5 to 60 degrees).
  int32_t signal_uv{0};     ///< Signal intensity of the oxygen sensor, microvolt.
  int32_t ambient_uv{0};    ///< Ambient light, microvolt.
  int32_t pressure_ubar{0}; ///< Pressure at the back side of the module, microbar.
  int32_t humidity_mpct{0}; ///< Relative humidity inside the housing, 10^-3 %RH.
};

/** @brief `#VERS` result (datasheet 4.3, `#VERS`). */
struct Version {
  int32_t device_id{0}; ///< D: 8 for every FDO2 and FDO2-G2.
  int32_t channels{0};  ///< N: 1.
  int32_t firmware{0};  ///< R: 100 x major + minor, 357 is firmware 3.57.
  int32_t sensors{0};   ///< S: bit 0 oxygen, 1 temperature, 2 pressure, 3 humidity (15 = all).
};

/** @brief What the firmware revision offers (derived from datasheet 5; `CapabilitiesFor`). */
struct Capabilities {
  bool user_memory{false};        ///< `#RDUM`, `#WRUM`.
  bool broadcast{false};          ///< `#BCST`.
  bool crc_persistent{false};     ///< `#CRCE` is stored in flash (before: lost at power cycle).
  bool calibration_error{false};  ///< `#ERRO -60` for an out-of-range calibration.
  bool set_mode{false};           ///< `#SETM` (FDO2-G2 only).
  bool status_bit0{false};        ///< Status bit 0 exists.
  bool temp_flag_reliable{false}; ///< Status bit 5 is set on a temperature sensor failure.
};

/** @brief Who the module is: `#VERS`, `#IDNR`, the family and what follows from the firmware. */
struct Identity {
  Version version{};             ///< The `#VERS` reply.
  uint64_t unique_id{0};         ///< `#IDNR`, unsigned 64 bit, not the serial number.
  Family family{Family::Fdo2G2}; ///< From `Config::family`.
  bool noise_erratum{
      false};          ///< Firmware affected by the 8 to 15 degC noise erratum (informational).
  Capabilities caps{}; ///< `CapabilitiesFor(family, firmware)`, with `-26` learning applied.
};

/** @brief Counters of what happened on the line; they only grow (docs/non-blocking.md). */
struct Counters {
  uint32_t transactions{0};    ///< Command lines written.
  uint32_t retries{0};         ///< Extra attempts of an idempotent read.
  uint32_t timeouts{0};        ///< Replies that did not complete in time.
  uint32_t crc_errors{0};      ///< Replies with a wrong CRC.
  uint32_t invalid_replies{0}; ///< Lines the grammar refused.
  uint32_t device_errors{0};   ///< `#ERRO` replies.
  uint32_t stale_lines{0};     ///< Complete lines of another command, dropped.
  uint32_t broadcast_lines{0}; ///< `#MRAW` lines received without asking.
  uint32_t flash_writes{0};    ///< Flash-writing commands sent.
  uint32_t bytes_discarded{0}; ///< Bytes dropped while resynchronising or draining.
};

/** @brief Lowest firmware revision any document covers: 3.28 (datasheet G1, 5). */
inline constexpr int32_t kMinFirmware = 328;
/// First revision with the `#BCST` command, the persistent `#CRCE` and the broadcast bug fix 3.53.
inline constexpr int32_t kFirmwareBroadcast = 341;
/// First revision with status bit 0.
inline constexpr int32_t kFirmwareStatusBit0 = 329;
/// First revision with `#ERRO -60`.
inline constexpr int32_t kFirmwareCalibrationError = 351;
/// First revision whose temperature-sensor failure sets status bit 5.
inline constexpr int32_t kFirmwareTempFlag = 354;
/// First revision of the FDO2-G2 (the first with `#SETM`).
inline constexpr int32_t kFirmwareG2 = 355;
/// First revision of the FDO2 (the device id of both families).
inline constexpr int32_t kDeviceId = 8;
/// Number of oxygen channels.
inline constexpr int32_t kChannels = 1;
/// `#VERS` sensor mask bit: oxygen.
inline constexpr int32_t kSensorOxygen = 1;
/// Words of user memory (`#RDUM`, `#WRUM`).
inline constexpr uint32_t kUserMemoryWords = 64;
/// Shortest and longest broadcast interval, milliseconds (`#BCST`).
inline constexpr uint32_t kBroadcastMinMs = 100;
/// Longest broadcast interval, milliseconds.
inline constexpr uint32_t kBroadcastMaxMs = 10'000;
/// Highest `#SETM` mode.
inline constexpr uint32_t kMaxMeasurementMode = 4;
/// Warm-up after power-up, seconds (datasheet 2, Specifications).
inline constexpr uint32_t kWarmUpSeconds = 180;

/**
 * @brief What a firmware revision offers.
 * @param family The module generation.
 * @param firmware R of `#VERS`, 100 x major + minor.
 * @return The capabilities; all false below `kMinFirmware`.
 */
constexpr Capabilities CapabilitiesFor(Family family, int32_t firmware) noexcept {
  Capabilities c{};
  if (firmware < kMinFirmware) {
    return c;
  }
  c.user_memory = true;
  c.status_bit0 = firmware >= kFirmwareStatusBit0;
  c.broadcast = firmware >= kFirmwareBroadcast;
  c.crc_persistent = firmware >= kFirmwareBroadcast;
  c.calibration_error = firmware >= kFirmwareCalibrationError;
  c.temp_flag_reliable = firmware >= kFirmwareTempFlag;
  c.set_mode = (family == Family::Fdo2G2) && (firmware >= kFirmwareG2);
  return c;
}

/**
 * @brief Whether a firmware revision shows the noise erratum (datasheet G2 6.1, G1 6.4).
 * @param firmware R of `#VERS`.
 * @return true for 3.54 and 3.55 (before the 3.57 hardware change); informational only.
 */
constexpr bool HasNoiseErratum(int32_t firmware) noexcept {
  return (firmware >= kFirmwareTempFlag) && (firmware <= 356);
}

/** @brief The baud rates `#BAUD` accepts, ascending (datasheet 4.3, `#BAUD`). */
inline constexpr std::array<uint32_t, 11> kBaudRates = {1200,  2400,  4800,  9600,  14400, 19200,
                                                        28800, 38400, 56000, 57600, 115200};

/** @brief The rate after every power-up. */
inline constexpr uint32_t kDefaultBaud = 19'200;

/**
 * @brief Whether `baud` is one of the eleven rates.
 * @param baud The rate.
 * @return true when `#BAUD` accepts it.
 */
constexpr bool IsSupportedBaud(uint32_t baud) noexcept {
  return std::ranges::find(kBaudRates, baud) != kBaudRates.end();
}

/**
 * @brief Time to send `chars` characters, 10 bits each at 8N1.
 * @param chars The number of characters.
 * @param baud The rate; 0 gives 0.
 * @return Microseconds, rounded up.
 */
constexpr uint32_t AirtimeUs(uint32_t chars, uint32_t baud) noexcept {
  if (baud == 0U) {
    return 0U;
  }
  const uint64_t bits = static_cast<uint64_t>(chars) * 10U;
  const uint64_t us = ((bits * 1'000'000U) + baud - 1U) / baud;
  return static_cast<uint32_t>((us > 0xFFFF'FFFFU) ? 0xFFFF'FFFFU : us);
}

/**
 * @brief Everything the driver needs from the application, with the datasheet defaults.
 *
 * Defaults follow the datasheet where it states a value and are documented assumptions (with a
 * knob) where it does not: see the ambiguity table in docs/configuration.md. It has no
 * `operator==`; compare fields.
 */
struct Config {
  Family family{Family::Fdo2G2}; ///< Decides whether `#SETM` is attempted (A-3).
  uint32_t baud{19'200};         ///< The rate the module runs at now; used for airtime.
  uint32_t power_up_ms{1500};    ///< Wait after power before `#VERS`, 1000 to 30000 (A-4).
  /** Processing allowance of `#VERS #IDNR #RDUM #BAUD`, 10 to 5000 (A-5). */
  uint32_t command_ms{100};
  /** Allowance of `#MOXY` and `#MRAW`, 50 to 5000 (150 or 300 ms, 4.3). */
  uint32_t measurement_ms{300};
  uint32_t calibration_ms{10'000}; ///< Allowance of `#CALO` and `#CAHI`, 10000 to 60000 (4.3).
  uint32_t flash_ms{2000};         ///< Allowance of `#CRCE #SETM #BCST #WRUM`, 100 to 30000 (A-7).
  uint32_t logo_ms{1100};          ///< Allowance of `#LOGO`, 100 to 10000 (A-6).
  uint32_t margin_ms{50};          ///< Added to every deadline, 0 to 1000.
  uint32_t power_off_ms{500};      ///< Off time of `PowerCycle()`, 10 to 10000.
  uint8_t retries{1};              ///< Extra attempts of an idempotent read, 0 to 3.
  CrcMode crc{CrcMode::Auto};      ///< Expectation for the CRC suffix.
  StatusPolicy status_policy{StatusPolicy::Datasheet}; ///< Status acceptance (A-1).
  bool strict_spacing{true};                           ///< Exactly one space between tokens (A-11).
  bool ignore_lf{true};                                ///< A LF at the start of a line is dropped.
  bool erro_without_crc{true}; ///< Accept `#ERRO` without CRC while CRC is on (A-12).
  BroadcastPolicy broadcast{BroadcastPolicy::Refuse}; ///< Start-up on a streaming module.
  uint32_t broadcast_listen_ms{0}; ///< Passive listen at start-up, 0 (off) to 12000.
  /** Every flash-writing call needs this (docs/calibration.md). */
  bool allow_persistent_writes{false};
  uint8_t max_flash_writes{8}; ///< Flash-writing calls per driver lifetime, 1 to 64.
  /** Steady-state seconds a calibration needs (datasheet: 5 min), 0 to 3600. */
  uint16_t min_steady_s{300};
  int32_t o2_min_mhpa{-5000};           ///< Plausible O, lower bound (10^-3 hPa).
  int32_t o2_max_mhpa{2'100'000};       ///< Plausible O, upper bound (spec 0 to 2000 hPa).
  int32_t temp_min_mc{-20'000};         ///< Plausible T, lower bound (10^-3 degC; spec -10 degC).
  int32_t temp_max_mc{70'000};          ///< Plausible T, upper bound (spec 60 degC).
  int32_t pressure_min_ubar{250'000};   ///< Plausible P, lower bound (sensor 300 mbar).
  int32_t pressure_max_ubar{1'200'000}; ///< Plausible P, upper bound (sensor 1100 mbar).
  int32_t humidity_max_mpct{100'000};   ///< Plausible H, upper bound (100 %RH).
  int32_t pressure_reference_ubar{0};   ///< 0: use the module's pressure; else the caller's.
  /** Calibration guard: the reading must be within this percent of the standard, 1 to 100. */
  uint8_t cal_window_pct{20};
  /** Calibration guard: smallest window, and the whole window of `#CALO`, 10^-3 hPa. */
  int32_t cal_window_min_mhpa{5000};
};

/// Bounds of the `Config` fields, shared by `Validate()` and the docs.
inline constexpr uint32_t kPowerUpMsMin = 1000;       ///< Smallest `Config::power_up_ms`, ms.
inline constexpr uint32_t kPowerUpMsMax = 30'000;     ///< Largest `Config::power_up_ms`, ms.
inline constexpr uint32_t kCommandMsMin = 10;         ///< Smallest `Config::command_ms`, ms.
inline constexpr uint32_t kCommandMsMax = 5000;       ///< Largest `Config::command_ms`, ms.
inline constexpr uint32_t kMeasurementMsMin = 50;     ///< Smallest `Config::measurement_ms`, ms.
inline constexpr uint32_t kMeasurementMsMax = 5000;   ///< Largest `Config::measurement_ms`, ms.
inline constexpr uint32_t kCalibrationMsMin = 10'000; ///< Smallest `Config::calibration_ms`, ms.
inline constexpr uint32_t kCalibrationMsMax = 60'000; ///< Largest `Config::calibration_ms`, ms.
inline constexpr uint32_t kFlashMsMin = 100;          ///< Smallest `Config::flash_ms`, ms.
inline constexpr uint32_t kFlashMsMax = 30'000;       ///< Largest `Config::flash_ms`, ms.
inline constexpr uint32_t kLogoMsMin = 100;           ///< Smallest `Config::logo_ms`, ms.
inline constexpr uint32_t kLogoMsMax = 10'000;        ///< Largest `Config::logo_ms`, ms.
inline constexpr uint32_t kMarginMsMax = 1000;        ///< Largest `Config::margin_ms`, ms.
inline constexpr uint32_t kPowerOffMsMin = 10;        ///< Smallest `Config::power_off_ms`, ms.
inline constexpr uint32_t kPowerOffMsMax = 10'000;    ///< Largest `Config::power_off_ms`, ms.
inline constexpr uint8_t kRetriesMax = 3;             ///< Largest `Config::retries`.
inline constexpr uint32_t kBroadcastListenMsMax =
    12'000; ///< Largest `Config::broadcast_listen_ms`, ms (covers the longest interval).
inline constexpr uint8_t kFlashBudgetMax = 64;          ///< Largest `Config::max_flash_writes`.
inline constexpr uint16_t kMinSteadyMax = 3600;         ///< Largest `Config::min_steady_s`, s.
inline constexpr uint8_t kCalWindowPctMax = 100;        ///< Largest `Config::cal_window_pct`.
inline constexpr int32_t kCalWindowMinMhpaMin = 1000;   ///< Smallest `Config::cal_window_min_mhpa`.
inline constexpr int32_t kCalWindowMinMhpaMax = 50'000; ///< Largest `Config::cal_window_min_mhpa`.
/// Lowest standard `#CAHI` accepts: the datasheet gives accuracy from 10 hPa up (2); below that
/// the zero point `#CALO` is the right call.
inline constexpr int32_t kCalHighMinMhpa = 10'000;
/// Highest standard `#CAHI` accepts: the maximum measuring range, 2000 hPa (2, not specified).
inline constexpr int32_t kCalHighMaxMhpa = 2'000'000;

namespace detail {
/// True when the signed `value` is in `[low, high]`.
constexpr bool InWindowI32(int32_t value, int32_t low, int32_t high) noexcept {
  return (value >= low) && (value <= high);
}

/// True when `value` is in `[low, high]`.
constexpr bool InRange(uint32_t value, uint32_t low, uint32_t high) noexcept {
  return (value >= low) && (value <= high);
}

/// Every enumeration field of a `Config` holds an enumerator.
constexpr bool ConfigEnumsValid(const Config& config) noexcept {
  return IsEnumerator(config.family) && IsEnumerator(config.crc) &&
         IsEnumerator(config.status_policy) && IsEnumerator(config.broadcast);
}

/// The baud rate and every timing field of a `Config` are inside their bounds.
constexpr bool ConfigTimesValid(const Config& config) noexcept {
  return IsSupportedBaud(config.baud) &&
         InRange(config.power_up_ms, kPowerUpMsMin, kPowerUpMsMax) &&
         InRange(config.command_ms, kCommandMsMin, kCommandMsMax) &&
         InRange(config.measurement_ms, kMeasurementMsMin, kMeasurementMsMax) &&
         InRange(config.calibration_ms, kCalibrationMsMin, kCalibrationMsMax) &&
         InRange(config.flash_ms, kFlashMsMin, kFlashMsMax) &&
         InRange(config.logo_ms, kLogoMsMin, kLogoMsMax) && (config.margin_ms <= kMarginMsMax) &&
         InRange(config.power_off_ms, kPowerOffMsMin, kPowerOffMsMax) &&
         (config.broadcast_listen_ms <= kBroadcastListenMsMax);
}

/// The counts of a `Config` (retries, flash budget, steady time) are inside their bounds.
constexpr bool ConfigLimitsValid(const Config& config) noexcept {
  return (config.retries <= kRetriesMax) && (config.max_flash_writes >= 1U) &&
         (config.max_flash_writes <= kFlashBudgetMax) && (config.min_steady_s <= kMinSteadyMax) &&
         (config.cal_window_pct >= 1U) && (config.cal_window_pct <= kCalWindowPctMax) &&
         InWindowI32(config.cal_window_min_mhpa, kCalWindowMinMhpaMin, kCalWindowMinMhpaMax);
}

/// The plausibility windows of a `Config` are ordered and the reference pressure is inside its
/// window.
constexpr bool ConfigWindowsValid(const Config& config) noexcept {
  const bool reference_ok = (config.pressure_reference_ubar == 0) ||
                            ((config.pressure_reference_ubar >= config.pressure_min_ubar) &&
                             (config.pressure_reference_ubar <= config.pressure_max_ubar));
  return (config.o2_min_mhpa < config.o2_max_mhpa) && (config.o2_min_mhpa <= 0) &&
         (config.temp_min_mc < config.temp_max_mc) &&
         (config.pressure_min_ubar < config.pressure_max_ubar) && (config.pressure_min_ubar > 0) &&
         (config.humidity_max_mpct > 0) && reference_ok;
}
} // namespace detail

/**
 * @brief Check a configuration without touching the module (zero port traffic).
 *
 * Fails when a baud rate is not one of the eleven, a timeout is out of its bound, `retries` is
 * above 3, an enumeration holds a value that is not an enumerator, the plausibility windows are
 * not ordered, `pressure_reference_ubar` is neither 0 nor inside the pressure window, or
 * `max_flash_writes` is outside 1 to 64, `cal_window_pct` is outside 1 to 100 or
 * `cal_window_min_mhpa` is outside 1000 to 50000.
 *
 * @param config The configuration.
 * @return `Error::Ok` or `Error::InvalidArgument`.
 */
constexpr Error Validate(const Config& config) noexcept {
  const bool valid = detail::ConfigEnumsValid(config) && detail::ConfigTimesValid(config) &&
                     detail::ConfigLimitsValid(config) && detail::ConfigWindowsValid(config);
  return valid ? Error::Ok : Error::InvalidArgument;
}

// The defaults are valid, proved at build time (KB-CPP20-012).
static_assert(Validate(Config{}) == Error::Ok);
static_assert(CapabilitiesFor(Family::Fdo2G2, 357).set_mode);
static_assert(!CapabilitiesFor(Family::Fdo2, 357).set_mode);
namespace detail {
static_assert(AirtimeUs(1_u32, 19'200_u32) == 521_u32);
} // namespace detail

/**
 * @brief Reached only when a `CheckedConfig` is built from an invalid configuration: it is never
 * defined, so the build fails and names it (KB-CPP20-013).
 */
void ConfigIsInvalid() noexcept;

/**
 * @brief A configuration proven valid at build time.
 *
 * `constexpr CheckedConfig kBoard{Config{.baud = 9600}};` fails to compile, naming
 * `ConfigIsInvalid`, when `Validate()` rejects the configuration. Pass `kBoard.Get()` to the
 * driver; the driver still validates at start-up, for configurations built at run time.
 */
class CheckedConfig {
public:
  /**
   * @brief Check `config` at build time.
   * @param config The configuration; it must pass `Validate()`.
   */
  consteval explicit CheckedConfig(const Config& config) noexcept : config_(config) {
    if (Validate(config) != Error::Ok) {
      ConfigIsInvalid();
    }
  }
  /** @return The checked configuration. */
  [[nodiscard]] constexpr const Config& Get() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary: the reference would dangle (KB-CPP20-044). */
  void Get() const&& = delete;

private:
  Config config_;
};

/**
 * @brief The oxygen content in volume percent of the gas at the membrane (datasheet 2, p. 6):
 * `100 x pO2 / p`, in 10^-3 percent, rounded half up.
 *
 * The factor 1000 of the two units cancels (`o2_mhpa` and `pressure_ubar` are both thousandths).
 *
 * @param o2_mhpa The partial pressure, 10^-3 hPa.
 * @param pressure_ubar The total pressure at the membrane, microbar (equal to 10^-3 hPa).
 * @return 10^-3 percent (20351 is 20.351 percent); `InvalidArgument` for a pressure of 0 or
 *         below, `OutOfRange` when the result does not fit 32 bits. Never a silent 0.
 */
constexpr Result<int32_t> VolumePercentMilli(int32_t o2_mhpa, int32_t pressure_ubar) noexcept {
  if (pressure_ubar <= 0) {
    return Result<int32_t>::Failure(Error::InvalidArgument);
  }
  const auto num = static_cast<int64_t>(o2_mhpa) * 100'000;
  const auto den = static_cast<int64_t>(pressure_ubar);
  const int64_t half = den / 2;
  const int64_t q = (num >= 0) ? ((num + half) / den) : -(((-num) + half) / den);
  if ((q > INT32_MAX) || (q < INT32_MIN)) {
    return Result<int32_t>::Failure(Error::OutOfRange);
  }
  return Result<int32_t>::Success(static_cast<int32_t>(q));
}

/// Fraction of oxygen in dry air, parts per million (standard dry air, assumption A-10).
inline constexpr uint32_t kDryAirO2Ppm = 209'500;

/**
 * @brief The pO2 of dry air at a pressure, for a one-point calibration (A-10).
 * @param pressure_ubar The pressure, microbar.
 * @param fraction_ppm The oxygen fraction, parts per million (`kDryAirO2Ppm` for dry air).
 * @return 10^-3 hPa, rounded; `InvalidArgument` for a pressure of 0 or below, or a fraction of 0
 *         or above 1000000. Humid air has a lower pO2 than this and is not corrected.
 */
constexpr Result<int32_t> AirO2Mhpa(int32_t pressure_ubar, uint32_t fraction_ppm) noexcept {
  if ((pressure_ubar <= 0) || (fraction_ppm == 0U) || (fraction_ppm > 1'000'000U)) {
    return Result<int32_t>::Failure(Error::InvalidArgument);
  }
  const int64_t num = static_cast<int64_t>(pressure_ubar) * static_cast<int64_t>(fraction_ppm);
  return Result<int32_t>::Success(static_cast<int32_t>((num + 500'000) / 1'000'000));
}

/**
 * @brief `AirO2Mhpa()` for standard dry air.
 * @param pressure_ubar The pressure, microbar.
 * @return See above.
 */
constexpr Result<int32_t> AirO2Mhpa(int32_t pressure_ubar) noexcept {
  return AirO2Mhpa(pressure_ubar, kDryAirO2Ppm);
}

/**
 * @brief Sum of ambient light and signal intensity in millivolts, for the 2000 mV advice
 * (datasheet 4.3, `#MRAW`, ambient light).
 * @param raw A `#MRAW` result.
 * @return `(A + I) / 1000` using 64-bit arithmetic.
 */
constexpr int32_t AmbientLoadMv(const Raw& raw) noexcept {
  return static_cast<int32_t>((static_cast<int64_t>(raw.ambient_uv) + raw.signal_uv) / 1000);
}

/** @return The driver version, e.g. `"0.1.0"`, from the generated `fdo2_version.h`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return FDO2_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return FDO2_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return FDO2_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return FDO2_VERSION_PATCH;
}

/** @} */

} // namespace fdo2

#endif // FDO2_TYPES_HPP
