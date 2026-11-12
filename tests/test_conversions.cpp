/**
 * @file test_conversions.cpp
 * @brief Integer conversions: rounding, signs and the overflow edges.
 */
#include "fixtures.hpp"

TEST_CASE("conversions: volume fraction in milli percent") {
  auto r = fdo2::VolumePercentMilli(209'500, 1'013'000);
  CHECK(r.ok());
  CHECK_EQ(r.value, 20'681);
  CHECK_EQ(fdo2::VolumePercentMilli(0, 1'013'000).value, 0);
  CHECK_EQ(fdo2::VolumePercentMilli(-500, 1'013'000).value, -49);
  CHECK_EQ(fdo2::VolumePercentMilli(500, 1'013'000).value, 49);
  CHECK(fdo2::VolumePercentMilli(1, 0).error == fdo2::Error::InvalidArgument);
  CHECK(fdo2::VolumePercentMilli(1, -5).error == fdo2::Error::InvalidArgument);
  // pure oxygen at 1 ubar: 100000 * 2^31 overflows the result
  CHECK(fdo2::VolumePercentMilli(2'147'483'647, 1).error == fdo2::Error::OutOfRange);
  CHECK(fdo2::VolumePercentMilli(-2'147'483'647 - 1, 1).error == fdo2::Error::OutOfRange);
  CHECK(fdo2::VolumePercentMilli(21'474, 1).ok());
  CHECK(fdo2::VolumePercentMilli(21'475, 1).error == fdo2::Error::OutOfRange);
}

TEST_CASE("conversions: air calibration value") {
  CHECK_EQ(fdo2::AirO2Mhpa(1'013'000).value, 212'224);
  CHECK_EQ(fdo2::AirO2Mhpa(1'013'000, 1'000'000).value, 1'013'000);
  CHECK_EQ(fdo2::AirO2Mhpa(1, 500'000).value, 1);
  CHECK_EQ(fdo2::AirO2Mhpa(1, 499'999).value, 0);
  CHECK(fdo2::AirO2Mhpa(0).error == fdo2::Error::InvalidArgument);
  CHECK(fdo2::AirO2Mhpa(-1).error == fdo2::Error::InvalidArgument);
  CHECK(fdo2::AirO2Mhpa(1000, 0).error == fdo2::Error::InvalidArgument);
  CHECK(fdo2::AirO2Mhpa(1000, 1'000'001).error == fdo2::Error::InvalidArgument);
}

TEST_CASE("conversions: ambient load and airtime") {
  fdo2::Raw raw{};
  raw.ambient_uv = 12'000;
  raw.signal_uv = 500'000;
  CHECK_EQ(fdo2::AmbientLoadMv(raw), 512);
  raw.ambient_uv = 2'147'483'647;
  raw.signal_uv = 2'147'483'647;
  CHECK_EQ(fdo2::AmbientLoadMv(raw), 4'294'967);
  CHECK_EQ(fdo2::AirtimeUs(1, 19'200), 521U);
  CHECK_EQ(fdo2::AirtimeUs(800, 1200), 6'666'667U);
  CHECK_EQ(fdo2::AirtimeUs(1, 0), 0U);
  CHECK_EQ(fdo2::AirtimeUs(0xFFFFFFFFU, 1200), 0xFFFFFFFFU);
}

TEST_CASE("conversions: baud table") {
  for (const uint32_t b :
       {1200U, 2400U, 4800U, 9600U, 14400U, 19200U, 28800U, 38400U, 56000U, 57600U, 115200U}) {
    CHECK(fdo2::IsSupportedBaud(b));
  }
  for (const uint32_t b : {0U, 1U, 1201U, 19201U, 230400U, 115201U}) {
    CHECK(!fdo2::IsSupportedBaud(b));
  }
}
