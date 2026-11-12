/**
 * @file esp32_host_hooks.hpp
 * @brief What the host stand-ins of the ESP-IDF headers share: the module model they talk to and
 * the calls they record.
 */
#ifndef FDO2_ESP32_HOST_HOOKS_HPP
#define FDO2_ESP32_HOST_HOOKS_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "mock_fdo2.hpp"

namespace esp_stub {

/// The shared state of the stand-ins.
struct State {
  fdo2_test::Module* module{nullptr};
  std::vector<std::pair<int, uint32_t>> gpio_levels; ///< `gpio_set_level(pin, level)` calls.
  uint64_t gpio_configured_mask{0};
  std::vector<std::string> logs;
  uint32_t spin_calls{0};  ///< `esp_rom_delay_us()` calls.
  uint32_t delay_calls{0}; ///< `vTaskDelay()` calls.
  int configured_baud{0};

  /// The model's time moves.
  void Elapse(uint64_t us) const {
    module->Elapse(us);
  }
};

/// The one instance.
inline State& Hooks() {
  static State state;
  return state;
}

} // namespace esp_stub

#endif // FDO2_ESP32_HOST_HOOKS_HPP
