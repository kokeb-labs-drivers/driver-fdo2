/**
 * @file esp32_fdo2_port.hpp
 * @brief ESP-IDF port for the FDO2 driver: the UART driver for the line, esp_timer for the clock,
 * esp_rom for delays, an optional GPIO for the supply switch and ESP_LOG for the log hook.
 * @copyright Copyright (c) 2026 The driver-fdo2 authors. SPDX-License-Identifier: MIT
 *
 * Start from this file for any ESP-IDF target. The pins are in Esp32Fdo2Pins; the defaults match
 * the wiring table in examples/esp32/README.md.
 */
#ifndef FDO2_ESP32_FDO2_PORT_HPP
#define FDO2_ESP32_FDO2_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fdo2.hpp"

/// The receive buffer: a reply can be 800 characters (datasheet 4.3), so the driver's own constant
/// is the floor.
inline constexpr int kEsp32Fdo2RxBytes = 1024;
static_assert(kEsp32Fdo2RxBytes >= static_cast<int>(fdo2::kRxBufferBytes),
              "the receive buffer must hold the longest reply");
static_assert(fdo2::kPortDefaultBaud == 19'200U, "the module starts at 19200 baud, 8N1");
static_assert(fdo2::kMaxLogicLevelMv == 3300U, "RXD tolerates 3.3 V at most: check the wiring");

/// A value in thousandths split for a `"%s%d.%03d"` format: a sign, a whole part and a fraction
/// that are never negative, so that -1.500 does not print as "-1.-500".
struct Esp32Fdo2Milli {
  const char* sign; ///< "-" or "".
  int whole;        ///< The part before the point.
  int frac;         ///< The thousandths, 0 to 999.
};

/// Split `milli` (10^-3 units) into sign, whole part and fraction.
inline Esp32Fdo2Milli Esp32Fdo2SplitMilli(int32_t milli) noexcept {
  const int64_t magnitude =
      (milli < 0) ? -static_cast<int64_t>(milli) : static_cast<int64_t>(milli);
  return Esp32Fdo2Milli{(milli < 0) ? "-" : "", static_cast<int>(magnitude / 1000),
                        static_cast<int>(magnitude % 1000)};
}

/// Pins and bus settings of the ESP-IDF port.
struct Esp32Fdo2Pins {
  uart_port_t port = UART_NUM_1;
  int tx = 4;     ///< To the module's RXD.
  int rx = 5;     ///< From the module's TXD.
  int power = -1; ///< A load switch for the module's supply; -1 when the supply is always on.
  int baud = static_cast<int>(fdo2::kPortDefaultBaud);
};

/// The FDO2 UART port on ESP-IDF, with every optional hook.
class Esp32Fdo2Port : public fdo2::UartInterface<Esp32Fdo2Port> {
public:
  explicit Esp32Fdo2Port(const Esp32Fdo2Pins& pins = {}) noexcept : pins_(pins) {}

  bool EnsureInitialized() noexcept {
    if (ready_) {
      return true;
    }
    uart_config_t cfg{};
    cfg.baud_rate = pins_.baud;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;
    if (uart_driver_install(pins_.port, kEsp32Fdo2RxBytes, 0, 0, nullptr, 0) != ESP_OK ||
        uart_param_config(pins_.port, &cfg) != ESP_OK ||
        uart_set_pin(pins_.port, pins_.tx, pins_.rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) !=
            ESP_OK) {
      return false;
    }
    if (pins_.power >= 0) {
      gpio_config_t io{};
      io.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.power);
      io.mode = GPIO_MODE_OUTPUT;
      // The supply switch is driven to its inactive level (off) before its output is enabled, so
      // configuring the pin never powers the module by accident (handbook KB-HW-018).
      if (gpio_set_level(static_cast<gpio_num_t>(pins_.power), 0) != ESP_OK ||
          gpio_config(&io) != ESP_OK) {
        return false;
      }
    }
    ready_ = true;
    return true;
  }

  bool Write(const uint8_t* data, std::size_t len) noexcept {
    return uart_write_bytes(pins_.port, data, len) == static_cast<int>(len);
  }

  // The count of bytes read, waiting up to timeout_ms for the first one.
  std::size_t Read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept {
    const int n =
        uart_read_bytes(pins_.port, data, static_cast<uint32_t>(max), pdMS_TO_TICKS(timeout_ms));
    return (n > 0) ? static_cast<std::size_t>(n) : 0U;
  }

  bool FlushInput() noexcept {
    return uart_flush_input(pins_.port) == ESP_OK;
  }

  // Waits of a millisecond or more yield to the other tasks (at least one tick); only shorter ones
  // spin. The driver measures its deadlines with NowUs(), so a coarser wait costs latency, not
  // correctness.
  void DelayUs(uint32_t us) noexcept {
    if (us < 1000U) {
      esp_rom_delay_us(us);
      return;
    }
    const TickType_t ticks = pdMS_TO_TICKS(us / 1000U);
    vTaskDelay((ticks > 0U) ? ticks : 1U);
  }

  // A free-running microsecond counter; the driver's Poll() takes no argument with it.
  uint32_t NowUs() noexcept {
    return static_cast<uint32_t>(esp_timer_get_time());
  }

  bool SetBaudRate(uint32_t baud) noexcept {
    return uart_set_baudrate(pins_.port, baud) == ESP_OK;
  }

  // With no supply switch (power = -1, the default) there is nothing to drive: the call succeeds
  // and does nothing, so that start-up works on a module that is always powered.
  bool GpioSet(fdo2::CtrlPin pin, fdo2::GpioSignal signal) noexcept {
    if (pin != fdo2::CtrlPin::Power) {
      return false;
    }
    if (pins_.power < 0) {
      return true;
    }
    const uint32_t level = (signal == fdo2::GpioSignal::ACTIVE) ? 1U : 0U;
    return gpio_set_level(static_cast<gpio_num_t>(pins_.power), level) == ESP_OK;
  }

  void Log(fdo2::LogLevel level, const char* message) noexcept {
    esp_log_level_t esp_level = ESP_LOG_DEBUG;
    switch (level) {
    case fdo2::LogLevel::Error:
      esp_level = ESP_LOG_ERROR;
      break;
    case fdo2::LogLevel::Warning:
      esp_level = ESP_LOG_WARN;
      break;
    case fdo2::LogLevel::Info:
      esp_level = ESP_LOG_INFO;
      break;
    case fdo2::LogLevel::Debug:
    default:
      break;
    }
    esp_log_write(esp_level, kTag, "%s\n", message); // plain text, never a format
  }

private:
  static constexpr const char* kTag = "fdo2";
  Esp32Fdo2Pins pins_;
  bool ready_ = false;
};

#endif // FDO2_ESP32_FDO2_PORT_HPP
