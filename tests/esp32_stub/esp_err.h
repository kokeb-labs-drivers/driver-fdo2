/**
 * @file esp_err.h
 * @brief Host stand-in for the ESP-IDF error type, so that the example port compiles on the host.
 */
#ifndef FDO2_STUB_ESP_ERR_H
#define FDO2_STUB_ESP_ERR_H

using esp_err_t = int;
inline constexpr esp_err_t ESP_OK = 0;

#endif // FDO2_STUB_ESP_ERR_H
