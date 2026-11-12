/**
 * @file test_validate.cpp
 * @brief Config validation: each bound at and one step beyond its limit.
 */
#include "fixtures.hpp"

using fdo2::Config;
using fdo2::Error;

namespace {

template <typename F>
void Edge(F set, uint32_t lo, uint32_t hi) {
  Config c{};
  set(c, lo);
  CHECK(fdo2::Validate(c) == Error::Ok);
  set(c, hi);
  CHECK(fdo2::Validate(c) == Error::Ok);
  if (lo > 0U) {
    set(c, lo - 1U);
    CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  }
  set(c, hi + 1U);
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
}

} // namespace

TEST_CASE("validate: time bounds") {
  Edge([](Config& c, uint32_t v) { c.power_up_ms = v; }, 1000, 30000);
  Edge([](Config& c, uint32_t v) { c.command_ms = v; }, 10, 5000);
  Edge([](Config& c, uint32_t v) { c.measurement_ms = v; }, 50, 5000);
  Edge([](Config& c, uint32_t v) { c.calibration_ms = v; }, 10000, 60000);
  Edge([](Config& c, uint32_t v) { c.flash_ms = v; }, 100, 30000);
  Edge([](Config& c, uint32_t v) { c.logo_ms = v; }, 100, 10000);
  Edge([](Config& c, uint32_t v) { c.margin_ms = v; }, 0, 1000);
  Edge([](Config& c, uint32_t v) { c.power_off_ms = v; }, 10, 10000);
  Edge([](Config& c, uint32_t v) { c.broadcast_listen_ms = v; }, 0, 12000);
}

TEST_CASE("validate: counts") {
  Config c{};
  c.retries = 3;
  CHECK(fdo2::Validate(c) == Error::Ok);
  c.retries = 4;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.max_flash_writes = 0;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c.max_flash_writes = 64;
  CHECK(fdo2::Validate(c) == Error::Ok);
  c.max_flash_writes = 65;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.min_steady_s = 3600;
  CHECK(fdo2::Validate(c) == Error::Ok);
  c.min_steady_s = 3601;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
}

TEST_CASE("validate: baud and enumerations") {
  Config c{};
  c.baud = 115200;
  CHECK(fdo2::Validate(c) == Error::Ok);
  c.baud = 115201;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): the point of the test
  c.family = static_cast<fdo2::Family>(2);
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): the point of the test
  c.crc = static_cast<fdo2::CrcMode>(3);
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): the point of the test
  c.status_policy = static_cast<fdo2::StatusPolicy>(2);
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): the point of the test
  c.broadcast = static_cast<fdo2::BroadcastPolicy>(3);
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
}

TEST_CASE("validate: windows") {
  Config c{};
  c.o2_min_mhpa = c.o2_max_mhpa;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.o2_min_mhpa = 1;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.temp_min_mc = c.temp_max_mc;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.pressure_min_ubar = 0;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.pressure_min_ubar = c.pressure_max_ubar;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.humidity_max_mpct = 0;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c = Config{};
  c.pressure_reference_ubar = c.pressure_min_ubar - 1;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c.pressure_reference_ubar = c.pressure_max_ubar + 1;
  CHECK(fdo2::Validate(c) == Error::InvalidArgument);
  c.pressure_reference_ubar = c.pressure_min_ubar;
  CHECK(fdo2::Validate(c) == Error::Ok);
}

TEST_CASE("validate: checked config at compile time") {
  constexpr fdo2::CheckedConfig kOk{Config{}};
  CHECK_EQ(kOk.Get().baud, 19200U);
  CHECK(std::string(fdo2::ToString(Error::Ok)) == "Ok");
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): the point of the test
  CHECK(std::string(fdo2::ToString(static_cast<Error>(99))) == "Unknown");
  CHECK(!std::string(fdo2::GetDriverVersion()).empty());
}
