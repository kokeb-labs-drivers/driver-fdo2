/**
 * @file esp_log.h
 * @brief Host stand-in for the ESP log: the messages are kept for the test.
 */
#ifndef FDO2_STUB_ESP_LOG_H
#define FDO2_STUB_ESP_LOG_H

#include "esp32_host_hooks.hpp"

enum esp_log_level_t { ESP_LOG_ERROR, ESP_LOG_WARN, ESP_LOG_INFO, ESP_LOG_DEBUG };

inline void esp_log_write(esp_log_level_t level, const char* tag, const char* format,
                          const char* message) {
  (void)level;
  (void)tag;
  (void)format;
  esp_stub::Hooks().logs.emplace_back(message);
}

#endif // FDO2_STUB_ESP_LOG_H
