/**
 * @file fdo2_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_FDO2_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-fdo2 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Include this header only where you want menuconfig to choose the defaults; the driver never
 * includes it and no driver signature depends on it. Under ESP-IDF the values come from
 * `sdkconfig.h`. Without Kconfig every option falls back to the default of `Config{}`, so
 * `KconfigDefaults()` equals `Config{}`.
 *
 * Kconfig writes a bool option set to "n" as an absent macro, so an absent option means off in
 * both cases.
 *
 * One configuration per program: the inline function below depends on the `CONFIG_` macros, so
 * every translation unit of one program must see the same values (ESP-IDF has one `sdkconfig.h`
 * per build; each host test is its own program). No driver or library header includes this one
 * (handbook KB-CPP20-018).
 */
#ifndef FDO2_KCONFIG_HPP
#define FDO2_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "fdo2_types.hpp"

#ifndef CONFIG_FDO2_POWER_UP_MS
/** Power-up wait in milliseconds when Kconfig does not set it: `Config{}.power_up_ms`. */
#define CONFIG_FDO2_POWER_UP_MS 1500
#endif
#ifndef CONFIG_FDO2_MEASUREMENT_MS
/** Measurement allowance in milliseconds when Kconfig does not set it. */
#define CONFIG_FDO2_MEASUREMENT_MS 300
#endif
#ifndef CONFIG_FDO2_RETRIES
/** Extra attempts of an idempotent read when Kconfig does not set it. */
#define CONFIG_FDO2_RETRIES 1
#endif
#ifndef CONFIG_FDO2_MAX_FLASH_WRITES
/** Flash-writing calls per driver lifetime when Kconfig does not set it. */
#define CONFIG_FDO2_MAX_FLASH_WRITES 8
#endif

namespace fdo2 {

/**
 * @brief The configuration menuconfig selected, on top of the defaults of `Config{}`.
 * @return `Config{}` with the family, status policy, power-up wait, measurement allowance,
 *         retries, CRC mode, broadcast policy and the flash-write guard taken from Kconfig.
 */
constexpr Config KconfigDefaults() noexcept {
  Config c{};
#ifdef CONFIG_FDO2_FAMILY_LEGACY
  c.family = Family::Fdo2;
#endif
#ifdef CONFIG_FDO2_STATUS_POLICY_DOCUMENTED
  c.status_policy = StatusPolicy::Documented;
#endif
  c.power_up_ms = CONFIG_FDO2_POWER_UP_MS;
  c.measurement_ms = CONFIG_FDO2_MEASUREMENT_MS;
  c.retries = CONFIG_FDO2_RETRIES;
#ifdef CONFIG_FDO2_CRC_OFF
  c.crc = CrcMode::Off;
#else
#ifdef CONFIG_FDO2_CRC_REQUIRED
  c.crc = CrcMode::Required;
#else
  c.crc = CrcMode::Auto;
#endif
#endif
#ifdef CONFIG_FDO2_BROADCAST_STREAM
  c.broadcast = BroadcastPolicy::Stream;
#else
#ifdef CONFIG_FDO2_BROADCAST_DISABLE
  c.broadcast = BroadcastPolicy::Disable;
#else
  c.broadcast = BroadcastPolicy::Refuse;
#endif
#endif
#if defined(CONFIG_FDO2_ALLOW_PERSISTENT_WRITES) && CONFIG_FDO2_ALLOW_PERSISTENT_WRITES
  c.allow_persistent_writes = true;
#endif
  c.max_flash_writes = CONFIG_FDO2_MAX_FLASH_WRITES;
  return c;
}

} // namespace fdo2

#endif // FDO2_KCONFIG_HPP
