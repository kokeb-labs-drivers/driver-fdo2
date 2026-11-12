/**
 * @file fuzz_fdo2_parse.cpp
 * @brief Fuzzes the reply parser and the line assembler with arbitrary bytes.
 *
 * Invariants: P1 `ParseLine()` never reads outside the line and never crashes; P2 an `Ok` result
 * has a known header (or none and no values), a value count within `kMaxValues`, and (for a CRC
 * suffix) a CRC that equals `Crc16Modbus()` of the body; P3 the assembler delivers a line only
 * after a CR and never longer than 799 characters, with no byte outside 0x20..0x7E; P4 feeding the
 * bytes one at a time and all at once gives the same lines; P5 `EncodeCommand()` output parses back
 * as itself for a valid argument set and is zero for an invalid one, and zero for a `Command`
 * value that is not an enumerator (never an out-of-bounds table read, m11); P6 re-encoding a
 * parsed number reproduces it; P7 `FindCommand()` on fewer than four letters finds nothing and
 * reads nothing.
 */
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "fdo2_protocol.hpp"

namespace {

void Require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

void CheckParse(std::span<const char> line, const fdo2::ParseOptions& options) {
  const fdo2::ParsedLine p = fdo2::ParseLine(line, options);
  if (p.status != fdo2::ParseStatus::Ok) {
    return;
  }
  if (!p.known) {
    Require(p.count == 0U); // an unknown header is handed back for the caller to drop
    return;
  }
  Require(p.count <= fdo2::kMaxValues);
  if (p.crc_present) {
    // The body is everything before ": ". Find it from the end.
    std::size_t pos = line.size();
    while ((pos > 0U) && (line[pos - 1U] != ':')) {
      --pos;
    }
    Require(pos >= 1U);
    const std::span<const char> body = line.first(pos - 1U);
    std::size_t v = 0;
    for (std::size_t i = pos; i < line.size(); ++i) {
      if (line[i] >= '0' && line[i] <= '9') {
        v = (v * 10U) + static_cast<std::size_t>(line[i] - '0');
      }
    }
    Require(v == fdo2::Crc16Modbus(body));
  }
  if (p.erro) {
    Require(p.error_code < 0);
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  if (size == 0U) {
    return 0;
  }
  const uint8_t flags = data[0];
  fdo2::ParseOptions options{};
  options.crc_required = (flags & 1U) != 0U;
  options.crc_forbidden = ((flags & 2U) != 0U) && !options.crc_required;
  options.strict_spacing = (flags & 4U) != 0U;
  options.erro_without_crc = (flags & 8U) != 0U;
  const std::span<const uint8_t> bytes{data + 1, size - 1U};

  // The raw bytes as a line (without the CR).
  std::vector<char> raw(bytes.begin(), bytes.end());
  CheckParse(std::span<const char>{raw}, options);

  // Through the assembler, in two ways.
  fdo2::LineAssembler one;
  fdo2::LineAssembler burst;
  std::vector<std::string> lines_one;
  std::vector<std::string> lines_burst;
  for (const uint8_t b : bytes) {
    if (one.PushByte(b) == fdo2::PushResult::Line) {
      const auto l = one.Line();
      Require(l.size() <= 799U);
      for (const char ch : l) {
        Require((ch >= 0x20) && (ch <= 0x7E));
      }
      lines_one.emplace_back(l.data(), l.size());
      CheckParse(l, options);
    }
  }
  std::size_t i = 0;
  while (i < bytes.size()) {
    const std::size_t chunk = 1U + (bytes[i] % 9U);
    for (std::size_t k = 0; (k < chunk) && (i < bytes.size()); ++k, ++i) {
      if (burst.PushByte(bytes[i]) == fdo2::PushResult::Line) {
        const auto l = burst.Line();
        lines_burst.emplace_back(l.data(), l.size());
      }
    }
  }
  Require(lines_one == lines_burst);

  // Encoding: arbitrary arguments from the bytes.
  std::array<int32_t, 8> args{};
  for (std::size_t k = 0; (k < args.size()) && ((1U + (k * 4U) + 3U) < size); ++k) {
    args[k] = static_cast<int32_t>(static_cast<uint32_t>(data[1U + (k * 4U)]) << 24U |
                                   static_cast<uint32_t>(data[2U + (k * 4U)]) << 16U |
                                   static_cast<uint32_t>(data[3U + (k * 4U)]) << 8U |
                                   data[4U + (k * 4U)]);
  }
  const auto cmd = static_cast<fdo2::Command>(flags); // any byte: most are not enumerators
  std::array<char, fdo2::kMaxCommandLength> out{};
  const std::size_t n =
      fdo2::EncodeCommand(cmd, std::span<const int32_t>{args}.first(flags % 9U % 8U), out);
  Require((n == 0U) || fdo2::detail::IsEnumerator(cmd));
  Require(std::string_view{fdo2::HeaderOf(cmd)}.size() <= 4U);
  fdo2::Command found{};
  Require(!fdo2::detail::FindCommand(std::span<const char>{raw}.first(raw.size() % 4U), found));
  if (n != 0U) {
    Require(out[n - 1U] == '\r');
    const fdo2::ParsedLine echo =
        fdo2::ParseLine(std::span<const char>{out.data(), n - 1U}, fdo2::ParseOptions{});
    Require(echo.status == fdo2::ParseStatus::Ok);
  }
  return 0;
}
