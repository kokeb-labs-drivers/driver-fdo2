/**
 * @file esp_rom_sys.h
 * @brief Host stand-in for the ROM busy-wait.
 */
#ifndef FDO2_STUB_ESP_ROM_SYS_H
#define FDO2_STUB_ESP_ROM_SYS_H

#include <cstdint>

#include "esp32_host_hooks.hpp"

inline void esp_rom_delay_us(uint32_t us) {
  esp_stub::Hooks().spin_calls += 1U;
  esp_stub::Hooks().Elapse(us);
}

#endif // FDO2_STUB_ESP_ROM_SYS_H
