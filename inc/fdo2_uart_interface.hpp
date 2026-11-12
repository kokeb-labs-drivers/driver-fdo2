// clang-format off: the table below must keep one row per line.
/**
 * @file fdo2_uart_interface.hpp
 * @brief The UART port of the FDO2 driver: the CRTP base, its pin enums, the `UartPort` concept
 * and the line requirements.
 * @copyright Copyright (c) 2026 The driver-fdo2 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The driver never calls an SDK. It calls hooks on a class you derive from
 * `fdo2::UartInterface<Self>`:
 *
 * - `bool EnsureInitialized() noexcept` (required): UART up at `Config::baud` (19200 after every
 *   power-up), 8N1, no flow control; `true` at once afterwards.
 * - `bool Write(const uint8_t* data, std::size_t len) noexcept` (required): send all `len` bytes
 *   (may block until queued); `false` on an error. One command line per call.
 * - `std::size_t Read(uint8_t* data, std::size_t max, uint32_t timeout_ms) noexcept` (required):
 *   return the bytes already received, 0 to `max`, waiting at most `timeout_ms` for the first
 *   byte; 0 must not block; never more than `max`.
 * - `bool FlushInput() noexcept` (required): drop every received byte not yet returned by `Read()`.
 * - `void DelayUs(uint32_t us) noexcept` (for the blocking calls): wait at least `us`; a compile
 *   error at the first blocking call otherwise.
 * - `uint32_t NowUs() noexcept` (optional): free-running monotonic microsecond counter, wrap-safe;
 *   with it `Poll()` measures its own time, without it `Poll(elapsed_us)` exists.
 * - `bool SetBaudRate(uint32_t baud) noexcept` (optional): reconfigure the UART; without it
 *   `SetBaudRate()` returns `Unsupported`.
 * - `bool GpioSet(CtrlPin, GpioSignal) noexcept` (optional): `CtrlPin::Power`: ACTIVE powers the
 *   module (an external load switch).
 * - `void Log(LogLevel, const char*) noexcept` (optional): plain text, never a format.
 *
 * 19200 baud, 8 data bits, no parity, 1 stop bit, no handshake after every power-up (datasheet
 * section 4). The port moves raw bytes; the driver builds, frames, checks and times everything.
 * `Read()` is not an all-or-nothing read: the reply length is unknown, so it returns the count
 * received. A port whose SDK returns only a status polls its FIFO level first.
 */
// clang-format on
#ifndef FDO2_UART_INTERFACE_HPP
#define FDO2_UART_INTERFACE_HPP

#include <concepts>
#include <cstddef>
#include <cstdint>

#include "fdo2_detail.hpp"
#include "fdo2_protocol.hpp"
#include "fdo2_types.hpp"

namespace fdo2 {

/** @brief Control pins the port may drive through the optional `GpioSet()` hook. */
enum class CtrlPin : uint8_t {
  Power = 0, ///< Supply switch of the module: ACTIVE powers it. It idles INACTIVE (off).
};

/** @brief Logical level of a control pin; the port maps it to the physical polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted (module unpowered).
  ACTIVE = 1,   ///< The pin function is asserted (module powered).
};

/**
 * @name Bus requirements
 * What the FDO2 needs from the UART and its supply (datasheet sections 2 and 4). A port and a
 * board configuration check their settings against these with `static_assert` (handbook
 * KB-CPP-123).
 * @{
 */
/// Rate after every power-up (datasheet 4).
inline constexpr uint32_t kPortDefaultBaud = 19'200;
/// Logic level of the module's UART, millivolts (datasheet 2, Interface).
inline constexpr uint32_t kLogicLevelMv = 3000;
/// Absolute maximum on the module's UART pins, millivolts: a 5 V host needs a level shifter.
inline constexpr uint32_t kMaxLogicLevelMv = 3300;
/// Lowest supply the datasheet allows, millivolts (below it the readings can be wrong).
inline constexpr uint32_t kSupplyMinMv = 3300;
/// Highest supply, millivolts (absolute maximum 5.5 V).
inline constexpr uint32_t kSupplyMaxMv = 5000;
/// Time from power to the first answer, milliseconds ("ca. 1 sec", datasheet 4).
inline constexpr uint32_t kPowerUpMs = 1000;
/// Receive buffer a port needs to never lose a `#RDUM` reply, bytes (or it polls every 300 ms).
inline constexpr uint32_t kRxBufferBytes = 800;
/// Highest baud rate the module accepts (datasheet 4.3, `#BAUD`).
inline constexpr uint32_t kMaxBaudRate = 115'200;
/// Design value: the longest wait from the last request byte to the first reply byte that the
/// driver treats as normal, microseconds. The datasheet gives no turnaround; the allowances cover
/// it.
inline constexpr uint32_t kTurnaroundUs = 2000;
/** @} */

/**
 * @brief CRTP base of the platform UART port.
 *
 * Derive `class MyPort : public fdo2::UartInterface<MyPort>` and implement the hooks in this
 * file's description. The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion) and no data; the driver checks your class with the `UartPort`
 * concept, so a missing or misdeclared hook is a compile error that names it.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class UartInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  UartInterface(const UartInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  UartInterface& operator=(const UartInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  UartInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~UartInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `UartInterface<Port>` and implements the required
 * hooks with exactly the types above and `noexcept`.
 *
 * Each hook's address is matched with `detail::ExactHook`, which takes a member function of exactly
 * the signature and converts no value: a hook declared with a narrower length or timeout, or
 * without `noexcept`, does not satisfy the concept (handbook KB-CPP20-006).
 *
 * @tparam Port The port class.
 */
template <typename Port>
concept UartPort = std::derived_from<Port, UartInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, bool(const uint8_t*, std::size_t) noexcept>::Take(&Port::Write);
  detail::ExactHook<Port, std::size_t(uint8_t*, std::size_t, uint32_t) noexcept>::Take(&Port::Read);
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::FlushInput);
};

} // namespace fdo2

#endif // FDO2_UART_INTERFACE_HPP
