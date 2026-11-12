/**
 * @file uart.h
 * @brief Host stand-in for the ESP-IDF UART driver: the line is the module model.
 */
#ifndef FDO2_STUB_DRIVER_UART_H
#define FDO2_STUB_DRIVER_UART_H

#include <cstddef>
#include <cstdint>

#include "esp32_host_hooks.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

enum uart_port_t : int { UART_NUM_1 = 1 };
enum uart_word_length_t : int { UART_DATA_8_BITS = 3 };
enum uart_parity_t : int { UART_PARITY_DISABLE = 0 };
enum uart_stop_bits_t : int { UART_STOP_BITS_1 = 1 };
enum uart_hw_flowcontrol_t : int { UART_HW_FLOWCTRL_DISABLE = 0 };
enum uart_sclk_t : int { UART_SCLK_DEFAULT = 0 };
inline constexpr int UART_PIN_NO_CHANGE = -1;

struct uart_config_t {
  int baud_rate;
  uart_word_length_t data_bits;
  uart_parity_t parity;
  uart_stop_bits_t stop_bits;
  uart_hw_flowcontrol_t flow_ctrl;
  uart_sclk_t source_clk;
};

inline esp_err_t uart_driver_install(uart_port_t, int, int, int, void*, int) {
  return ESP_OK;
}
inline esp_err_t uart_param_config(uart_port_t, const uart_config_t* config) {
  esp_stub::Hooks().configured_baud = config->baud_rate;
  return ESP_OK;
}
inline esp_err_t uart_set_pin(uart_port_t, int, int, int, int) {
  return ESP_OK;
}
inline int uart_write_bytes(uart_port_t, const uint8_t* data, std::size_t len) {
  esp_stub::Hooks().module->HostWrite(data, len);
  return static_cast<int>(len);
}
inline int uart_read_bytes(uart_port_t, uint8_t* data, uint32_t len, TickType_t ticks) {
  const std::size_t n =
      esp_stub::Hooks().module->HostRead(data, len, static_cast<uint32_t>(ticks) * 10U);
  return static_cast<int>(n);
}
inline esp_err_t uart_flush_input(uart_port_t) {
  esp_stub::Hooks().module->FlushRx();
  return ESP_OK;
}
inline esp_err_t uart_set_baudrate(uart_port_t, uint32_t baud) {
  esp_stub::Hooks().module->host_baud = baud;
  return ESP_OK;
}

#endif // FDO2_STUB_DRIVER_UART_H
