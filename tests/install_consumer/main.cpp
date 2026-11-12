/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version header and .ipp.
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

namespace {

// A port that never receives a byte: enough to instantiate the whole driver template.
class SilentPort : public fdo2::UartInterface<SilentPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Write(const uint8_t* data, std::size_t len) noexcept {
    (void)data;
    (void)len;
    return true;
  }
  std::size_t Read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept {
    (void)data;
    (void)max;
    (void)timeout_ms;
    return 0U;
  }
  bool FlushInput() noexcept {
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

} // namespace

int main() {
  SilentPort port;
  fdo2::FDO2<SilentPort> sensor(port);
  // No reply ever arrives, so EnsureInitialized() must time out: proof the template compiled and
  // ran.
  const auto r = sensor.EnsureInitialized();
  return (r.error == fdo2::Error::Timeout && fdo2::GetDriverVersion()[0] != '\0') ? 0 : 1;
}
