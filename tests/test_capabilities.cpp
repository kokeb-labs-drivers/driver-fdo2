/**
 * @file test_capabilities.cpp
 * @brief Firmware gates (data sheets, revision tables) at every boundary.
 */
#include "fixtures.hpp"

using fdo2::Family;

TEST_CASE("capabilities: below the minimum firmware nothing is offered") {
  const auto c = fdo2::CapabilitiesFor(Family::Fdo2G2, 327);
  CHECK(!c.user_memory);
  CHECK(!c.broadcast);
  CHECK(!c.set_mode);
}

TEST_CASE("capabilities: boundaries") {
  CHECK(fdo2::CapabilitiesFor(Family::Fdo2, 328).user_memory);
  CHECK(!fdo2::CapabilitiesFor(Family::Fdo2, 328).status_bit0);
  CHECK(fdo2::CapabilitiesFor(Family::Fdo2, 329).status_bit0);
  CHECK(!fdo2::CapabilitiesFor(Family::Fdo2, 340).broadcast);
  CHECK(fdo2::CapabilitiesFor(Family::Fdo2, 341).broadcast);
  CHECK(!fdo2::CapabilitiesFor(Family::Fdo2, 340).crc_persistent);
  CHECK(fdo2::CapabilitiesFor(Family::Fdo2, 341).crc_persistent);
  CHECK(!fdo2::CapabilitiesFor(Family::Fdo2, 350).calibration_error);
  CHECK(fdo2::CapabilitiesFor(Family::Fdo2, 351).calibration_error);
  CHECK(!fdo2::CapabilitiesFor(Family::Fdo2, 353).temp_flag_reliable);
  CHECK(fdo2::CapabilitiesFor(Family::Fdo2, 354).temp_flag_reliable);
  CHECK(!fdo2::CapabilitiesFor(Family::Fdo2G2, 354).set_mode);
  CHECK(fdo2::CapabilitiesFor(Family::Fdo2G2, 355).set_mode);
  CHECK(!fdo2::CapabilitiesFor(Family::Fdo2, 400).set_mode);
}

TEST_CASE("capabilities: the noise erratum window") {
  CHECK(!fdo2::HasNoiseErratum(353));
  CHECK(fdo2::HasNoiseErratum(354));
  CHECK(fdo2::HasNoiseErratum(356));
  CHECK(!fdo2::HasNoiseErratum(357));
}
