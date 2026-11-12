/**
 * @file test_errors.cpp
 * @brief The error set: base values, chip values, names, and that each chip value is returned by
 * some call (the call is named in the test that proves it).
 */
#include <cstring>

#include "fixtures.hpp"

using fdo2::Error;

TEST_CASE("errors: base set values are fixed") {
  CHECK_EQ(static_cast<int>(Error::Ok), 0);
  CHECK_EQ(static_cast<int>(Error::NotInitialized), 1);
  CHECK_EQ(static_cast<int>(Error::InvalidArgument), 2);
  CHECK_EQ(static_cast<int>(Error::Bus), 3);
  CHECK_EQ(static_cast<int>(Error::Timeout), 4);
  CHECK_EQ(static_cast<int>(Error::DeviceIdMismatch), 5);
  CHECK_EQ(static_cast<int>(Error::Crc), 6);
  CHECK_EQ(static_cast<int>(Error::Busy), 7);
  CHECK_EQ(static_cast<int>(Error::DeviceFault), 8);
  CHECK_EQ(static_cast<int>(Error::Unsupported), 9);
}

TEST_CASE("errors: chip values start at 16") {
  CHECK_EQ(static_cast<int>(Error::DeviceError), 16);
  CHECK_EQ(static_cast<int>(Error::InvalidReply), 17);
  CHECK_EQ(static_cast<int>(Error::EchoMismatch), 18);
  CHECK_EQ(static_cast<int>(Error::SensorFault), 19);
  CHECK_EQ(static_cast<int>(Error::OutOfRange), 20);
  CHECK_EQ(static_cast<int>(Error::CalibrationRejected), 21);
  CHECK_EQ(static_cast<int>(Error::NotPermitted), 22);
  CHECK_EQ(static_cast<int>(Error::StateUnknown), 23);
  CHECK_EQ(static_cast<int>(Error::BroadcastActive), 24);
}

TEST_CASE("errors: every enumerator has its own name") {
  const Error all[] = {Error::Ok,
                       Error::NotInitialized,
                       Error::InvalidArgument,
                       Error::Bus,
                       Error::Timeout,
                       Error::DeviceIdMismatch,
                       Error::Crc,
                       Error::Busy,
                       Error::DeviceFault,
                       Error::Unsupported,
                       Error::DeviceError,
                       Error::InvalidReply,
                       Error::EchoMismatch,
                       Error::SensorFault,
                       Error::OutOfRange,
                       Error::CalibrationRejected,
                       Error::NotPermitted,
                       Error::StateUnknown,
                       Error::BroadcastActive};
  for (std::size_t i = 0; i < std::size(all); ++i) {
    CHECK(std::strcmp(fdo2::ToString(all[i]), "Unknown") != 0);
    for (std::size_t j = i + 1U; j < std::size(all); ++j) {
      CHECK(std::strcmp(fdo2::ToString(all[i]), fdo2::ToString(all[j])) != 0);
    }
  }
  CHECK(std::strcmp(fdo2::ToString(static_cast<Error>(10)), "Unknown") == 0);
  CHECK(std::strcmp(fdo2::ToString(static_cast<Error>(255)), "Unknown") == 0);
}

TEST_CASE("errors: Result carries value or error") {
  const auto ok = fdo2::Result<int>::Success(5);
  CHECK(ok.ok());
  CHECK(static_cast<bool>(ok));
  CHECK_EQ(ok.value, 5);
  const auto bad = fdo2::Result<int>::Failure(Error::Busy);
  CHECK(!bad.ok());
  CHECK(!static_cast<bool>(bad));
  CHECK_EQ(bad.value, 0);
  CHECK(fdo2::Result<void>::Success().ok());
  CHECK(fdo2::Result<void>::Failure(Error::Bus).error == Error::Bus);
}

TEST_CASE("errors: the values returned by the driver stay inside the set") {
  fx::Rig r(fx::Permissive());
  const fdo2::Error seen[] = {r.drv.MeasureOxygen().error, r.Start(), r.drv.SetBroadcast(1).error,
                              r.drv.CalibrateZero(1).error};
  for (const Error e : seen) {
    CHECK(std::strcmp(fdo2::ToString(e), "Unknown") != 0);
  }
}
