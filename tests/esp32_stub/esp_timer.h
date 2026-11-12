/**
 * @file esp_timer.h
 * @brief Host stand-in for the microsecond clock: the module model's time.
 */
#ifndef FDO2_STUB_ESP_TIMER_H
#define FDO2_STUB_ESP_TIMER_H

#include <cstdint>

#include "esp32_host_hooks.hpp"

inline int64_t esp_timer_get_time() {
  return static_cast<int64_t>(esp_stub::Hooks().module->now_us);
}

#endif // FDO2_STUB_ESP_TIMER_H
