/**
 * @file ports.cpp
 * @brief The port with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstddef>
#include <cstdint>

#include "fdo2.hpp"

bool platform_uart_init() noexcept;
bool platform_uart_write(const uint8_t* data, std::size_t len) noexcept;
std::size_t platform_uart_read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept;
bool platform_uart_flush_input() noexcept;
bool platform_uart_set_baud(uint32_t baud) noexcept;
void platform_delay_us(uint32_t us) noexcept;
uint32_t platform_now_us() noexcept;
bool platform_gpio_write(int pin, bool high) noexcept;
void platform_log(const char* line) noexcept;

// --8<-- [start:full-port]
class FullPort : public fdo2::UartInterface<FullPort> {
public:
  // Required hooks.
  bool EnsureInitialized() noexcept {
    return platform_uart_init();
  }
  bool Write(const uint8_t* data, std::size_t len) noexcept {
    return platform_uart_write(data, len);
  }
  std::size_t Read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept {
    return platform_uart_read(data, max, timeout_ms);
  }
  bool FlushInput() noexcept {
    return platform_uart_flush_input();
  }

  // Optional hooks: each enables part of the API.
  void DelayUs(uint32_t us) noexcept { // the blocking calls and PowerCycle()
    platform_delay_us(us);
  }
  uint32_t NowUs() noexcept { // Poll() without an argument
    return platform_now_us();
  }
  bool SetBaudRate(uint32_t baud) noexcept { // SetBaudRate()
    return platform_uart_set_baud(baud);
  }
  bool GpioSet(fdo2::CtrlPin pin, fdo2::GpioSignal level) noexcept { // the power pin
    (void)pin;
    return platform_gpio_write(7, level == fdo2::GpioSignal::ACTIVE);
  }
  void Log(fdo2::LogLevel level, const char* message) noexcept {
    (void)level;
    platform_log(message);
  }
};
// --8<-- [end:full-port]

// --8<-- [start:port-constants]
// The line the port must provide, checked at build time.
static_assert(fdo2::kPortDefaultBaud == 19'200U, "8N1 at 19200 baud");
static_assert(fdo2::kRxBufferBytes >= 800U, "a reply can be 800 characters");
// --8<-- [end:port-constants]

template class fdo2::FDO2<FullPort>;
