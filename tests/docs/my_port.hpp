/**
 * @file my_port.hpp
 * @brief The UART port shown on the Quickstart and Porting pages, compiled by the host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces
 * them with its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux termios).
 */
#ifndef FDO2_MY_PORT_HPP
#define FDO2_MY_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "fdo2_uart_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the port compiles.
bool platform_uart_init() noexcept; // 19200 8N1, receive buffer of at least 800 bytes
bool platform_uart_write(const uint8_t* data, std::size_t len) noexcept;
/// Up to `max` bytes, waiting up to `timeout_ms` for the first one; returns the count.
std::size_t platform_uart_read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept;
bool platform_uart_flush_input() noexcept;
void platform_delay_us(uint32_t us) noexcept;

// --8<-- [start:port]
class MyPort : public fdo2::UartInterface<MyPort> {
public:
  // Bring the UART up once; later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_uart_init();
    }
    return ready_;
  }

  bool Write(const uint8_t* data, std::size_t len) noexcept {
    return platform_uart_write(data, len);
  }

  // The number of bytes read (0 to `max`), waiting up to `timeout_ms` for the first one.
  // The reply length is not known in advance, so this is not an exact-length read.
  std::size_t Read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept {
    return platform_uart_read(data, max, timeout_ms);
  }

  // Drop stale bytes before each request.
  bool FlushInput() noexcept {
    return platform_uart_flush_input();
  }

  // Wait at least `us` microseconds: the module's processing times depend on it.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:port]

#endif // FDO2_MY_PORT_HPP
