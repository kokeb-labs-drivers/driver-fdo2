/**
 * @file fixtures.hpp
 * @brief Shared helpers of the host tests: a driver over the model, parsing and feeding helpers.
 */
#ifndef FDO2_FIXTURES_HPP
#define FDO2_FIXTURES_HPP

#include <cstdint>
#include <string>

#include "fdo2.hpp"
#include "mock_fdo2.hpp"
#include "test_harness.hpp"

namespace fx {

using Port = fdo2_test::FullPort;
using Driver = fdo2::FDO2<Port>;

/// A configuration that permits flash writes (tests that need them say so).
inline fdo2::Config Permissive() {
  fdo2::Config c{};
  c.allow_persistent_writes = true;
  return c;
}

/// A driver over a fresh model, optionally started.
struct Rig {
  Port port;
  Driver drv;
  explicit Rig(const fdo2::Config& config = fdo2::Config{}) : drv(port, config) {}
  /// Start-up; the model's knobs are applied first.
  fdo2::Error Start() {
    port.module.Reset();
    return drv.EnsureInitialized().error;
  }
};

/// Let the sensor warm up (3 minutes, data sheet 2) and set the gas the module measures.
inline void WarmUp(Rig& r, double o2_hpa) {
  r.port.module.Elapse(200'000'000U);
  r.port.module.scenario.o2_hpa = o2_hpa;
}

/// Parse a literal line with the default options.
inline fdo2::ParsedLine P(const std::string& s,
                          const fdo2::ParseOptions& o = fdo2::ParseOptions{}) {
  return fdo2::ParseLine(std::span<const char>{s.data(), s.size()}, o);
}

/// Parse a literal line without strict spacing.
inline fdo2::ParsedLine PLoose(const std::string& s) {
  fdo2::ParseOptions o{};
  o.strict_spacing = false;
  return P(s, o);
}

/// `body` plus the CRC suffix computed by the model's own CRC.
inline std::string WithCrc(const std::string& body) {
  return body + ": " + std::to_string(fdo2_test::RefCrc16(body));
}

/// Feed a string to an assembler; returns the lines completed, `\x01` marking a Bad result.
inline std::string Feed(fdo2::LineAssembler& a, const std::string& bytes) {
  std::string out;
  for (const char c : bytes) {
    const auto r = a.PushByte(static_cast<uint8_t>(c));
    if (r == fdo2::PushResult::Line) {
      const auto line = a.Line();
      out += std::string(line.data(), line.size()) + "|";
    } else if (r == fdo2::PushResult::Bad) {
      out += "!";
    } else {
      // a partial line: more bytes are needed
    }
  }
  return out;
}

} // namespace fx

#endif // FDO2_FIXTURES_HPP
