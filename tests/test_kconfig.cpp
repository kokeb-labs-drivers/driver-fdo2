/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): the defaults without Kconfig, and menuconfig values when they are set.
 *
 * Built twice: without CONFIG_ macros (KCONFIG_SET undefined) and with the values an sdkconfig.h
 * would define (KCONFIG_SET=1, see tests/CMakeLists.txt).
 */
#include "fdo2_kconfig.hpp"
#include "test_harness.hpp"

#ifdef KCONFIG_SET
TEST_CASE("Kconfig values reach the Config") {
  constexpr fdo2::Config kC = fdo2::KconfigDefaults();
  CHECK_EQ(kC.family, fdo2::Family::Fdo2);
  CHECK_EQ(kC.status_policy, fdo2::StatusPolicy::Documented);
  CHECK_EQ(kC.power_up_ms, 2000U);
  CHECK_EQ(kC.measurement_ms, 250U);
  CHECK_EQ(kC.retries, 2U);
  CHECK_EQ(kC.crc, fdo2::CrcMode::Required);
  CHECK_EQ(kC.broadcast, fdo2::BroadcastPolicy::Stream);
  CHECK(kC.allow_persistent_writes);
  CHECK_EQ(kC.max_flash_writes, 4U);
  CHECK_EQ(kC.baud, fdo2::kDefaultBaud); // not a Kconfig option: the module's power-up rate
  CHECK_EQ(fdo2::Validate(kC), fdo2::Error::Ok);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}") {
  constexpr fdo2::Config kC = fdo2::KconfigDefaults();
  constexpr fdo2::Config kD{};
  CHECK_EQ(kC.family, kD.family);
  CHECK_EQ(kC.status_policy, kD.status_policy);
  CHECK_EQ(kC.power_up_ms, kD.power_up_ms);
  CHECK_EQ(kC.measurement_ms, kD.measurement_ms);
  CHECK_EQ(kC.retries, kD.retries);
  CHECK_EQ(kC.crc, kD.crc);
  CHECK_EQ(kC.broadcast, kD.broadcast);
  CHECK_EQ(kC.allow_persistent_writes, kD.allow_persistent_writes);
  CHECK_EQ(kC.max_flash_writes, kD.max_flash_writes);
  CHECK_EQ(kC.baud, kD.baud);
}
#endif
