/**
 * @file fdo2_protocol.hpp
 * @brief The FDO2 line protocol as pure functions: CRC-16, command encoders, the line assembler
 * and the strict reply parser. No port, no state outside the objects, no allocation.
 * @copyright Copyright (c) 2026 The driver-fdo2 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Datasheet section 4 (G2 data sheet, printed pages 10 to 17). A command is `#HEAD`, values
 * separated by single spaces, and a carriage return; the module answers with the command echoed
 * and the requested values, ended by one carriage return, with `: C` (the CRC-16/MODBUS in
 * decimal) before it when `#CRCE 1` is active. An error is `#ERRO E` with a negative E.
 *
 * Because the layer is pure (bytes in, a struct out), an interrupt handler, a DMA completion or
 * an RTOS task can decode what it received with the same checks the driver applies, and the fuzz
 * target drives the parser directly.
 */
#ifndef FDO2_PROTOCOL_HPP
#define FDO2_PROTOCOL_HPP

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "fdo2_types.hpp"

namespace fdo2 {

/**
 * @defgroup fdo2_protocol Protocol layer
 * @brief CRC, encoders, line assembler and parser.
 * @{
 */

/// Longest line, including its carriage return: a line of up to 799 characters is accepted.
/// The longest legitimate reply (`#RDUM` of 64 words with CRC) has about 790 characters.
inline constexpr std::size_t kMaxLineLength = 800;
/// Longest command line the driver sends (`#WRUM 0 64` and 64 values), including the CR.
inline constexpr std::size_t kMaxCommandLength = 800;
/// Most values a reply holds: `#RDUM R N` and 64 words.
inline constexpr std::size_t kMaxValues = 66;
/// Complete lines of another command dropped per transaction before the reply is `InvalidReply`.
inline constexpr uint32_t kMaxStale = 4;

/** @brief The thirteen commands of the module (datasheet 4.3). */
enum class Command : uint8_t {
  Vers = 0, ///< `#VERS`
  Idnr,     ///< `#IDNR`
  Moxy,     ///< `#MOXY`
  Mraw,     ///< `#MRAW`
  Logo,     ///< `#LOGO`
  Crce,     ///< `#CRCE K`
  Rdum,     ///< `#RDUM R N`
  Wrum,     ///< `#WRUM R N Y1 ... YN`
  Baud,     ///< `#BAUD R`
  Calo,     ///< `#CALO`
  Cahi,     ///< `#CAHI P`
  Bcst,     ///< `#BCST T`
  Setm,     ///< `#SETM M`
};

namespace detail {
/// Last enumerator of `Command`.
template <>
struct EnumLast<Command> {
  static constexpr Command kValue = Command::Setm; ///< The last enumerator.
};
} // namespace detail

/**
 * @brief The four header letters of a command, without the `#`.
 * @param command The command.
 * @return A string such as `"VERS"`; `""` for a value that is not an enumerator.
 */
constexpr const char* HeaderOf(Command command) noexcept {
  switch (command) {
  case Command::Vers:
    return "VERS";
  case Command::Idnr:
    return "IDNR";
  case Command::Moxy:
    return "MOXY";
  case Command::Mraw:
    return "MRAW";
  case Command::Logo:
    return "LOGO";
  case Command::Crce:
    return "CRCE";
  case Command::Rdum:
    return "RDUM";
  case Command::Wrum:
    return "WRUM";
  case Command::Baud:
    return "BAUD";
  case Command::Calo:
    return "CALO";
  case Command::Cahi:
    return "CAHI";
  case Command::Bcst:
    return "BCST";
  case Command::Setm:
    return "SETM";
  default:
    break;
  }
  return "";
}

/**
 * @brief The CRC-16/MODBUS of a byte or character string (polynomial 0xA001 reflected, start
 * 0xFFFF), as the module computes it over the reply before the colon (datasheet 4.2).
 * `"123456789"` gives 0x4B37.
 * @tparam T `uint8_t` or `char`.
 * @param data The bytes or characters (spaces included).
 * @return The CRC.
 */
template <typename T>
  requires(std::is_same_v<T, uint8_t> || std::is_same_v<T, char>)
constexpr uint16_t Crc16Modbus(std::span<const T> data) noexcept {
  uint32_t crc = 0xFFFFU;
  for (const T item : data) {
    crc ^=
        static_cast<uint32_t>(std::bit_cast<uint8_t>(item)); // the byte, not the character's value
    for (uint32_t bit = 0U; bit < 8U; ++bit) {
      const bool lsb = (crc & 1U) != 0U;
      crc >>= 1U;
      if (lsb) {
        crc ^= 0xA001U;
      }
    }
  }
  return static_cast<uint16_t>(crc);
}

namespace detail {

/// The decimal digits in order: a digit's value is its position (no arithmetic on characters).
inline constexpr std::string_view kDecimalDigits = "0123456789";

/// Writes `value` in decimal at `out[pos...]`; false when it does not fit.
constexpr bool PutInt(std::span<char> out, std::size_t& pos, int32_t value) noexcept {
  std::array<char, 12> digits{};
  std::size_t n = 0U;
  const bool neg = value < 0;
  uint32_t mag = neg ? (0U - static_cast<uint32_t>(value)) : static_cast<uint32_t>(value);
  do {
    digits[n] = kDecimalDigits[mag % 10U];
    ++n;
    mag /= 10U;
  } while (mag != 0U);
  const std::size_t total = n + (neg ? 1U : 0U);
  if ((pos + total) > out.size()) {
    return false;
  }
  if (neg) {
    out[pos] = '-';
    ++pos;
  }
  while (n > 0U) {
    --n;
    out[pos] = digits[n];
    ++pos;
  }
  return true;
}

/// The number of arguments a fixed-argument command takes, or 0 for `Wrum` (variable).
constexpr std::size_t FixedArgCount(Command command) noexcept {
  switch (command) {
  case Command::Crce:
  case Command::Baud:
  case Command::Cahi:
  case Command::Bcst:
  case Command::Setm:
    return 1U;
  case Command::Rdum:
    return 2U;
  case Command::Vers:
  case Command::Idnr:
  case Command::Moxy:
  case Command::Mraw:
  case Command::Logo:
  case Command::Calo:
  case Command::Wrum:
  default:
    break;
  }
  return 0U;
}

/// The range rule of datasheet 4.3 for the one argument of `#CRCE`, `#BAUD`, `#CAHI`, `#BCST` and
/// `#SETM`.
constexpr bool OneArgumentValid(Command command, int32_t value) noexcept {
  switch (command) {
  case Command::Crce:
    return (value == 0) || (value == 1);
  case Command::Baud:
    return (value > 0) && IsSupportedBaud(static_cast<uint32_t>(value));
  case Command::Cahi:
    return value != 0;
  case Command::Bcst:
    return (value == 0) || (std::cmp_greater_equal(value, kBroadcastMinMs) &&
                            std::cmp_less_equal(value, kBroadcastMaxMs));
  case Command::Setm:
    return (value >= 0) && std::cmp_less_equal(value, kMaxMeasurementMode);
  case Command::Vers:
  case Command::Idnr:
  case Command::Moxy:
  case Command::Mraw:
  case Command::Logo:
  case Command::Rdum:
  case Command::Wrum:
  case Command::Calo:
  default:
    break;
  }
  return false;
}

/// The range rules of datasheet 4.3 for `#RDUM R N` and `#WRUM R N Y1 ... YN`.
constexpr bool UserMemoryArgumentsValid(bool is_write, std::span<const int32_t> args) noexcept {
  if (args.size() < 2U) {
    return false;
  }
  const int64_t first = args[0];
  const int64_t count = args[1];
  const bool span_ok =
      (first >= 0) && (first <= 63) && (count >= 1) && (count <= 64) && ((first + count) <= 64);
  const bool length_ok =
      is_write ? (static_cast<int64_t>(args.size()) == (count + 2)) : (args.size() == 2U);
  return span_ok && length_ok;
}

/// The range rules of datasheet 4.3 for the arguments of a command; false when violated.
constexpr bool ArgumentsValid(Command command, std::span<const int32_t> args) noexcept {
  switch (command) {
  case Command::Crce:
  case Command::Baud:
  case Command::Cahi:
  case Command::Bcst:
  case Command::Setm:
    return (args.size() == 1U) && OneArgumentValid(command, args[0]);
  case Command::Rdum:
  case Command::Wrum:
    return UserMemoryArgumentsValid(command == Command::Wrum, args);
  case Command::Vers:
  case Command::Idnr:
  case Command::Moxy:
  case Command::Mraw:
  case Command::Logo:
  case Command::Calo:
  default:
    break;
  }
  return args.empty();
}

} // namespace detail

/**
 * @brief Encode a command line: `#HEAD`, each argument as plain decimal with one space before it,
 * and one carriage return (never a LF: datasheet 4.1 allows CR or CR LF, the driver sends CR).
 *
 * The arguments are range-checked first (`R` 0 to 63, `N` 1 to 64, `R + N` at most 64, a
 * baud rate from the table, `#BCST` 0 or 100 to 10000, `#SETM` 0 to 4, `#CRCE` 0 or 1, `#CAHI`
 * not 0, and the count of arguments).
 *
 * @param command The command.
 * @param args The arguments; for `Wrum` `R, N, Y1 ... YN`.
 * @param out The buffer; at least `kMaxCommandLength` bytes fits every command.
 * @return The length written, including the CR; 0 when an argument is invalid or `out` is short.
 */
constexpr std::size_t EncodeCommand(Command command, std::span<const int32_t> args,
                                    std::span<char> out) noexcept {
  if (!detail::IsEnumerator(command) || !detail::ArgumentsValid(command, args)) {
    return 0U;
  }
  const char* header = HeaderOf(command);
  std::size_t pos = 0U;
  if (out.size() < 6U) {
    return 0U;
  }
  out[pos] = '#';
  ++pos;
  for (std::size_t i = 0U; i < 4U; ++i) {
    out[pos] = header[i];
    ++pos;
  }
  for (const int32_t value : args) {
    if ((pos >= out.size()) || (pos >= (kMaxCommandLength - 1U))) {
      return 0U;
    }
    out[pos] = ' ';
    ++pos;
    if (!detail::PutInt(out, pos, value)) {
      return 0U;
    }
  }
  if (pos >= out.size()) {
    return 0U;
  }
  out[pos] = '\r';
  ++pos;
  return pos;
}

/** @brief What `LineAssembler::PushByte()` did with a byte. */
enum class PushResult : uint8_t {
  Pending = 0, ///< The line is not complete.
  Line,        ///< A carriage return completed a line: `Line()` holds it (without the CR).
  Bad, ///< The byte, or the length, broke the grammar; the line is discarded to the next CR.
};

/**
 * @brief Collects the bytes of a reply into lines, one byte at a time (datasheet 4.1: one CR ends
 * a reply).
 *
 * A byte outside 0x20 to 0x7E other than CR (a LF at the start of a line is dropped when
 * `ignore_lf`), or 800 characters without a CR, makes `PushByte()` return `Bad` once; the rest of
 * that line is discarded up to the next CR (resynchronisation) and counted. A line is never
 * delivered without its CR; an empty line (CR alone) is ignored.
 */
class LineAssembler {
public:
  /** @brief An empty assembler that ignores a leading LF. */
  constexpr LineAssembler() noexcept = default;

  /**
   * @brief Drop the partial line and the discard state.
   * @param ignore_lf Whether a LF at the start of a line is dropped.
   */
  constexpr void Reset(bool ignore_lf) noexcept {
    len_ = 0U;
    discarding_ = false;
    ready_ = false;
    ignore_lf_ = ignore_lf;
  }

  /**
   * @brief Feed one byte.
   * @param byte The byte.
   * @return `Pending`, `Line` or `Bad`.
   */
  constexpr PushResult PushByte(uint8_t byte) noexcept {
    if (ready_) {
      ready_ = false;
      len_ = 0U;
    }
    if (byte == 0x0DU) {
      return endOfLine();
    }
    if (discarding_) {
      ++discarded_;
      return PushResult::Pending;
    }
    if ((byte == 0x0AU) && ignore_lf_ && (len_ == 0U)) {
      return PushResult::Pending;
    }
    if ((byte < 0x20U) || (byte > 0x7EU) || (len_ >= (kMaxLineLength - 1U))) {
      discarded_ += static_cast<uint32_t>(len_) + 1U;
      len_ = 0U;
      discarding_ = true;
      return PushResult::Bad;
    }
    buf_[len_] = std::bit_cast<char>(byte);
    ++len_;
    return PushResult::Pending;
  }

  /** @return The line completed by the last `Line` result; valid until the next `PushByte()`. */
  [[nodiscard]] constexpr std::span<const char> Line() const noexcept {
    return std::span<const char>{buf_}.first(len_);
  }
  /** @return The number of bytes discarded while resynchronising since the last `TakeDiscarded()`.
   */
  [[nodiscard]] constexpr uint32_t TakeDiscarded() noexcept {
    const uint32_t n = discarded_;
    discarded_ = 0U;
    return n;
  }
  /** @return true while the rest of a bad line is being discarded. */
  [[nodiscard]] constexpr bool Discarding() const noexcept {
    return discarding_;
  }
  /** @return The characters of the partial line. */
  [[nodiscard]] constexpr std::size_t Pending() const noexcept {
    return len_;
  }

private:
  constexpr PushResult endOfLine() noexcept {
    if (discarding_) {
      discarding_ = false;
      return PushResult::Pending;
    }
    if (len_ == 0U) {
      return PushResult::Pending;
    }
    ready_ = true;
    return PushResult::Line;
  }

  std::array<char, kMaxLineLength> buf_{};
  std::size_t len_{0};
  uint32_t discarded_{0};
  bool discarding_{false};
  bool ready_{false};
  bool ignore_lf_{true};
};

/** @brief The outcome class of `ParseLine()`. */
enum class ParseStatus : uint8_t {
  Ok = 0,    ///< The line follows the grammar (and its CRC matches when present).
  BadCrc,    ///< A CRC suffix is wrong, or required and missing.
  Malformed, ///< The line breaks the grammar.
};

/** @brief How `ParseLine()` treats the CRC and the spacing (from the `Config`). */
struct ParseOptions {
  bool crc_required{false};  ///< A suffix must be present (except `#ERRO` with `erro_without_crc`).
  bool crc_forbidden{false}; ///< A suffix is `Malformed` (`CrcMode::Off`).
  bool strict_spacing{true}; ///< Exactly one space between tokens, none leading or trailing.
  bool erro_without_crc{
      true}; ///< An `#ERRO` line without a suffix is accepted while CRC is required.
};

/** @brief A parsed reply line. */
struct ParsedLine {
  ParseStatus status{ParseStatus::Malformed}; ///< Whether the line is valid.
  bool known{false};                          ///< The header is one of the thirteen (or `#ERRO`).
  bool erro{false};                           ///< The line is `#ERRO E`.
  bool crc_present{false};                    ///< A CRC suffix was present and valid.
  Command command{Command::Vers};             ///< The header, when `known` and not `erro`.
  std::size_t count{0};                       ///< Values parsed (for `Idnr`: 1, in `big`).
  int32_t error_code{0};                      ///< E of `#ERRO`.
  uint64_t big{0};                            ///< The `#IDNR` value.
  std::array<int64_t, kMaxValues> values{};   ///< The values, signed 32 bit or unsigned 32 bit.
};

namespace detail {

/// A number token: `0`, or an optional `-` and a digit 1 to 9, then digits; no `+`, no leading
/// zero, no `-0`, at most 20 digits and no overflow of 64 bits. `mag` is the magnitude.
constexpr bool ParseNumber(std::span<const char> token, bool& negative, uint64_t& mag) noexcept {
  std::size_t i = 0U;
  negative = false;
  mag = 0U;
  if (token.empty()) {
    return false;
  }
  if (std::char_traits<char>::eq(token[0], '-')) {
    negative = true;
    i = 1U;
  }
  const std::size_t digits = token.size() - i;
  if ((digits == 0U) || (digits > 20U)) {
    return false;
  }
  if (std::char_traits<char>::eq(token[i], '0') && ((digits > 1U) || negative)) {
    return false;
  }
  for (; i < token.size(); ++i) {
    const std::size_t position = kDecimalDigits.find(token[i]); // the digit's value is its place
    if (position == std::string_view::npos) {
      return false;
    }
    const auto d = static_cast<uint64_t>(position);
    if (mag > ((0xFFFF'FFFF'FFFF'FFFFULL - d) / 10U)) {
      return false;
    }
    mag = (mag * 10U) + d;
  }
  return true;
}

/// The value kinds of the reply fields.
enum class ValueKind : uint8_t { Int32, UInt32, UInt64 };

/// The kind of value `index` of the reply to `command` (datasheet 4.3): S is unsigned 32 bit,
/// the `#IDNR` value unsigned 64 bit, everything else signed 32 bit.
constexpr ValueKind FieldOf(Command command, std::size_t index) noexcept {
  if (command == Command::Idnr) {
    return ValueKind::UInt64;
  }
  if (((command == Command::Moxy) || (command == Command::Mraw)) && (index == 2U)) {
    return ValueKind::UInt32;
  }
  return ValueKind::Int32;
}

/// Converts a token to its value by kind; false when it is out of range.
constexpr bool ConvertValue(ValueKind kind, std::span<const char> token, int64_t& out,
                            uint64_t& big) noexcept {
  bool neg = false;
  uint64_t mag = 0U;
  if (!ParseNumber(token, neg, mag)) {
    return false;
  }
  switch (kind) {
  case ValueKind::UInt64:
    big = mag;
    out = 0;
    return !neg;
  case ValueKind::UInt32:
    out = static_cast<int64_t>(mag);
    return !neg && (mag <= 0xFFFF'FFFFULL);
  case ValueKind::Int32:
    if (neg) {
      out = (mag <= 2'147'483'648ULL) ? -static_cast<int64_t>(mag) : 0;
      return mag <= 2'147'483'648ULL;
    }
    out = static_cast<int64_t>(mag);
    return mag <= 2'147'483'647ULL;
  default:
    break;
  }
  return false;
}

/// The command whose header letters are `letters`, or false.
constexpr bool FindCommand(std::span<const char> letters, Command& out) noexcept {
  if (letters.size() < 4U) {
    return false;
  }
  for (uint8_t i = 0U; i <= static_cast<uint8_t>(Command::Setm); ++i) {
    const auto candidate = static_cast<Command>(i);
    const char* header = HeaderOf(candidate);
    if ((letters[0] == header[0]) && (letters[1] == header[1]) && (letters[2] == header[2]) &&
        (letters[3] == header[3])) {
      out = candidate;
      return true;
    }
  }
  return false;
}

/// Splits the part after the header into tokens and converts them. False on a grammar break.
constexpr bool ParseValues(std::span<const char> rest, bool strict, Command command, bool erro,
                           ParsedLine& out) noexcept {
  std::size_t pos = 0U;
  while (pos < rest.size()) {
    if (rest[pos] != ' ') {
      return false;
    }
    ++pos;
    if (!strict) {
      while ((pos < rest.size()) && (rest[pos] == ' ')) {
        ++pos;
      }
      if (pos >= rest.size()) {
        break;
      }
    }
    std::size_t end = pos;
    while ((end < rest.size()) && (rest[end] != ' ')) {
      ++end;
    }
    if ((end == pos) || (out.count >= kMaxValues)) {
      return false;
    }
    const std::span<const char> token = rest.subspan(pos, end - pos);
    int64_t value = 0;
    const ValueKind kind = erro ? ValueKind::Int32 : FieldOf(command, out.count);
    if (!ConvertValue(kind, token, value, out.big)) {
      return false;
    }
    out.values[out.count] = value;
    ++out.count;
    pos = end;
  }
  return true;
}

/// Splits the CRC suffix `: C` off `line`. `Ok` with `suffix` false when there is none.
constexpr ParseStatus SplitCrc(std::span<const char> line, std::span<const char>& body,
                               bool& suffix, uint32_t& crc) noexcept {
  body = line;
  suffix = false;
  std::size_t colon = line.size();
  for (std::size_t i = 0U; i < line.size(); ++i) {
    if (line[i] == ':') {
      colon = i;
    }
  }
  if (colon == line.size()) {
    return ParseStatus::Ok;
  }
  // `colon + 2` must lie inside the line before the digit count is taken: the count is unsigned.
  if (((colon + 2U) >= line.size()) || (line[colon + 1U] != ' ')) {
    return ParseStatus::Malformed;
  }
  const std::size_t digits = line.size() - colon - 2U;
  if (digits > 5U) {
    return ParseStatus::Malformed;
  }
  uint64_t mag = 0U;
  bool neg = false;
  if (!ParseNumber(line.subspan(colon + 2U), neg, mag) || neg || (mag > 0xFFFFU)) {
    return ParseStatus::Malformed;
  }
  body = line.first(colon);
  suffix = true;
  crc = static_cast<uint32_t>(mag);
  return ParseStatus::Ok;
}

/// Checks the CRC suffix rules of `ParseOptions` on a line whose suffix `SplitCrc()` has split off.
constexpr ParseStatus CheckCrc(std::span<const char> body, bool suffix, uint32_t crc, bool is_erro,
                               const ParseOptions& options) noexcept {
  if (suffix && options.crc_forbidden) {
    return ParseStatus::Malformed;
  }
  if (suffix && (Crc16Modbus(body) != static_cast<uint16_t>(crc))) {
    return ParseStatus::BadCrc;
  }
  if (!suffix && options.crc_required && !(is_erro && options.erro_without_crc)) {
    return ParseStatus::BadCrc;
  }
  return ParseStatus::Ok;
}

/// Stores the one negative value of an `#ERRO` line as its error code; false for any other shape.
constexpr bool TakeErrorCode(ParsedLine& line) noexcept {
  if ((line.count != 1U) || (line.values[0] >= 0)) {
    return false;
  }
  line.error_code = static_cast<int32_t>(line.values[0]);
  return true;
}

/// True when the body starts with `#ERRO`.
constexpr bool IsErroHeader(std::span<const char> body) noexcept {
  return (body.size() >= 5U) && (body[0] == '#') && (body[1] == 'E') && (body[2] == 'R') &&
         (body[3] == 'R') && (body[4] == 'O');
}

/// True when the body starts with `#`, four capital letters and then ends or has a space.
constexpr bool HasHeader(std::span<const char> body) noexcept {
  if ((body.size() < 5U) || (body[0] != '#')) {
    return false;
  }
  for (std::size_t i = 1U; i < 5U; ++i) {
    if (std::char_traits<char>::lt(body[i], 'A') || std::char_traits<char>::lt('Z', body[i])) {
      return false;
    }
  }
  return (body.size() == 5U) || (body[5] == ' ');
}
} // namespace detail

/**
 * @brief Parse one reply line (without its CR) under the grammar of datasheet 4.
 *
 * (1) The CRC suffix `: C` (when present or required) is checked over every character before the
 * colon. (2) The header is `#` and four letters A to Z, then the end or a space. (3) Values are
 * separated by exactly one space (`strict_spacing`), each is `0` or an optional minus and a digit
 * 1 to 9 and digits, in range for its field (S unsigned 32 bit, `#IDNR` unsigned 64 bit, the rest
 * signed 32 bit). `#ERRO` is exactly `#ERRO E` with E negative. The token count is checked by the
 * caller against the command's reply (`ExpectedCount()`). No allocation, no `strtol`, no locale.
 *
 * @param line The line, without CR.
 * @param options The CRC and spacing rules.
 * @return The parsed line; check `status`, then `known`, `erro`, `command`.
 */
constexpr ParsedLine ParseLine(std::span<const char> line, const ParseOptions& options) noexcept {
  ParsedLine out{};
  std::span<const char> body{};
  bool suffix = false;
  uint32_t crc = 0U;
  if (detail::SplitCrc(line, body, suffix, crc) != ParseStatus::Ok) {
    return out;
  }
  const bool is_erro = detail::IsErroHeader(body);
  const ParseStatus crc_status = detail::CheckCrc(body, suffix, crc, is_erro, options);
  if (crc_status != ParseStatus::Ok) {
    out.status = crc_status;
    return out;
  }
  if (!detail::HasHeader(body)) {
    return out;
  }
  out.crc_present = suffix;
  out.erro = is_erro;
  out.known = is_erro || detail::FindCommand(body.subspan(1U, 4U), out.command);
  if (!out.known) {
    out.status = ParseStatus::Ok;
    return out;
  }
  const bool strict = options.strict_spacing || is_erro;
  if (!detail::ParseValues(body.subspan(5U), strict, out.command, is_erro, out)) {
    out.count = 0U;
    return out;
  }
  if (is_erro && !detail::TakeErrorCode(out)) {
    out.count = 0U;
    return out;
  }
  out.status = ParseStatus::Ok;
  return out;
}

/**
 * @brief How many values the reply to a command holds (datasheet 4.3).
 * @param command The command.
 * @param args The arguments that were sent (`#RDUM` and `#WRUM` need `N`).
 * @return The count; for `Rdum` and `Wrum` it is `2 + N`.
 */
constexpr std::size_t ExpectedCount(Command command, std::span<const int32_t> args) noexcept {
  switch (command) {
  case Command::Vers:
    return 4U;
  case Command::Idnr:
    return 1U;
  case Command::Moxy:
    return 3U;
  case Command::Mraw:
    return 8U;
  case Command::Crce:
  case Command::Baud:
  case Command::Cahi:
  case Command::Bcst:
  case Command::Setm:
    return 1U;
  case Command::Rdum:
  case Command::Wrum:
    return (args.size() >= 2U) ? (2U + static_cast<std::size_t>(args[1])) : 0U;
  case Command::Logo:
  case Command::Calo:
  default:
    break;
  }
  return 0U;
}

/**
 * @brief The echo check of datasheet 4.2: the header and the echoed arguments equal what was sent.
 * @param command The command that was sent.
 * @param args The arguments that were sent.
 * @param reply The parsed reply (its header must be `command`).
 * @return true when the echo matches. `#RDUM` echoes `R N` and then returns data.
 */
constexpr bool VerifyEcho(Command command, std::span<const int32_t> args,
                          const ParsedLine& reply) noexcept {
  if (!reply.known || reply.erro || (reply.command != command)) {
    return false;
  }
  const std::size_t echoed = args.size();
  if ((command == Command::Vers) || (command == Command::Idnr) || (command == Command::Moxy) ||
      (command == Command::Mraw)) {
    return true;
  }
  if (reply.count < echoed) {
    return false;
  }
  for (std::size_t i = 0U; i < echoed; ++i) {
    if (reply.values[i] != static_cast<int64_t>(args[i])) {
      return false;
    }
  }
  return true;
}

/**
 * @brief The longest reply of a command, in characters including the CR and a CRC suffix, for the
 * deadline (derived from datasheet 4.1 and 4.3: `#MRAW` with eight 11-character values).
 * @param command The command.
 * @param words `N` of `#RDUM`/`#WRUM`, 0 otherwise.
 * @return The bound.
 */
constexpr uint32_t MaxReplyChars(Command command, uint32_t words) noexcept {
  switch (command) {
  case Command::Vers:
    return 48U;
  case Command::Idnr:
    return 40U;
  case Command::Moxy:
    return 56U;
  case Command::Mraw:
    return 120U;
  case Command::Rdum:
  case Command::Wrum:
    return 32U + (12U * words);
  case Command::Logo:
  case Command::Crce:
  case Command::Baud:
  case Command::Calo:
  case Command::Cahi:
  case Command::Bcst:
  case Command::Setm:
  default:
    break;
  }
  return 40U;
}

/** @} */

} // namespace fdo2

#endif // FDO2_PROTOCOL_HPP
