/**
 * @file gpio.h
 * @brief Host stand-in for the ESP-IDF GPIO driver: every call is recorded.
 */
#ifndef FDO2_STUB_DRIVER_GPIO_H
#define FDO2_STUB_DRIVER_GPIO_H

#include <cstdint>

#include "esp32_host_hooks.hpp"
#include "esp_err.h"

enum gpio_num_t : int { GPIO_NUM_NC = -1 };
enum gpio_mode_t : int { GPIO_MODE_OUTPUT = 2 };

struct gpio_config_t {
  uint64_t pin_bit_mask;
  gpio_mode_t mode;
};

inline esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
  esp_stub::Hooks().gpio_levels.emplace_back(static_cast<int>(pin), level);
  return ESP_OK;
}

inline esp_err_t gpio_config(const gpio_config_t* config) {
  esp_stub::Hooks().gpio_configured_mask = config->pin_bit_mask;
  return ESP_OK;
}

#endif // FDO2_STUB_DRIVER_GPIO_H
