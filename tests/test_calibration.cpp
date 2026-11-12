/**
 * @file test_calibration.cpp
 * @brief Two-point calibration (data sheet 4.3, #CALO / #CAHI): steady-state guard, rejected
 * values, the air helper and its checks, and the guard that reads the sensor before the write.
 */
#include "fixtures.hpp"

using fdo2::Error;
using fdo2_test::FaultKind;

TEST_CASE("calibration: zero point") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 0.0);
  CHECK(r.drv.CalibrateZero(300).ok());
  CHECK(r.port.module.calo_done);
  CHECK_EQ(r.drv.FlashWritesUsed(), 1U);
  CHECK(r.port.module.now_us > 10'000'000U);
  // The sensor was read first (#MOXY), then the write was sent.
  const auto& c = r.port.module.commands;
  REQUIRE(c.size() >= 4U);
  CHECK(c[c.size() - 2U] == "#MOXY");
  CHECK(c.back() == "#CALO");
}

TEST_CASE("calibration: the steady-state time is checked first") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 0.0);
  CHECK(r.drv.CalibrateZero(299).error == Error::InvalidArgument);
  CHECK(r.drv.CalibrateHigh(209'500, 0).error == Error::InvalidArgument);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK_EQ(r.port.module.CountCommands("#CALO"), 0U);
  fdo2::Config c = fx::Permissive();
  c.min_steady_s = 0;
  fx::Rig q(c);
  CHECK(q.Start() == Error::Ok);
  fx::WarmUp(q, 0.0);
  CHECK(q.drv.CalibrateZero(0).ok());
}

TEST_CASE("calibration: high point value range is the data sheet's, 10 hPa to 2000 hPa") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 2000.0);
  CHECK(r.drv.CalibrateHigh(0, 300).error == Error::InvalidArgument);
  CHECK(r.drv.CalibrateHigh(-1, 300).error == Error::InvalidArgument);
  CHECK(r.drv.CalibrateHigh(9'999, 300).error == Error::InvalidArgument);
  CHECK(r.drv.CalibrateHigh(2'000'001, 300).error == Error::InvalidArgument);
  CHECK(r.drv.CalibrateHigh(2'100'000, 300).error == Error::InvalidArgument);
  CHECK_EQ(r.port.module.CountCommands("#CAHI"), 0U);
  CHECK_EQ(r.port.module.CountCommands("#MOXY"), 0U);
  CHECK(r.drv.CalibrateHigh(2'000'000, 300).ok());
  CHECK_EQ(r.port.module.last_cahi, 2'000'000LL);
  r.port.module.scenario.o2_hpa = 209.5;
  CHECK(r.drv.CalibrateHigh(209'500, 300).ok());
  CHECK_EQ(r.port.module.last_cahi, 209'500LL);
  r.port.module.scenario.o2_hpa = 10.0;
  CHECK(r.drv.CalibrateHigh(10'000, 300).ok());
}

TEST_CASE("calibration: a rejected value is reported and the state stays known") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 209.5);
  r.port.module.ArmWrite(FaultKind::ErrorCode, -60);
  CHECK(r.drv.CalibrateHigh(209'500, 300).error == Error::CalibrationRejected);
  CHECK(r.drv.GetState() == fdo2::State::Idle);
  CHECK_EQ(r.drv.FlashWritesUsed(), 1U);
}

TEST_CASE("calibration: air helper takes the pressure from the module") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 209.5);
  CHECK(r.drv.CalibrateAir(300).ok());
  CHECK_EQ(r.port.module.last_cahi, 212'224LL);
  r.port.module.scenario.o2_hpa = 1013.0;
  CHECK(r.drv.CalibrateAir(300, 1'000'000).ok());
  CHECK_EQ(r.port.module.last_cahi, 1'013'000LL);
}

TEST_CASE("calibration: air helper refuses a doubtful reading") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 209.5);
  r.port.module.scenario.status_override = 2;
  CHECK(r.drv.CalibrateAir(300).error == Error::SensorFault);
  r.port.module.scenario.status_override = -1;
  r.port.module.scenario.pressure_mbar = 1300.0;
  CHECK(r.drv.CalibrateAir(300).error == Error::OutOfRange);
  r.port.module.scenario.pressure_mbar = 1013.0;
  CHECK(r.drv.CalibrateAir(300, 0).error == Error::InvalidArgument);
  CHECK(r.drv.CalibrateAir(300, 1'000'001).error == Error::InvalidArgument);
  CHECK(r.drv.CalibrateAir(10).error == Error::InvalidArgument);
  CHECK_EQ(r.port.module.CountCommands("#CAHI"), 0U);
  fx::Rig q;
  CHECK(q.Start() == Error::Ok);
  CHECK(q.drv.CalibrateAir(300).error == Error::NotPermitted);
  CHECK_EQ(q.port.module.CountCommands("#MRAW"), 0U);
}

TEST_CASE("calibration: a warning status still calibrates, a degraded one does not") {
  fdo2::Config c = fx::Permissive();
  c.status_policy = fdo2::StatusPolicy::Documented;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 209.5);
  r.port.module.scenario.status_override = 1;
  CHECK(r.drv.CalibrateAir(300).ok());
  r.port.module.scenario.status_override = 128;
  CHECK(r.drv.CalibrateAir(300).error == Error::SensorFault);
}

TEST_CASE("calibration: a lost echo is an unknown state, the long wait is respected") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 0.0);
  r.port.module.ArmWrite(FaultKind::Silent);
  const uint64_t t0 = r.port.module.now_us;
  CHECK(r.drv.CalibrateZero(300).error == Error::Timeout);
  CHECK(r.port.module.now_us - t0 >= 10'000'000U);
  CHECK(r.drv.GetState() == fdo2::State::StateUnknown);
}

// M3: the gas must be the standard, the sensor must be warm, and nothing is written otherwise.

TEST_CASE("calibration guard: air calibration in nitrogen is refused and nothing is written") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 0.0); // the sensor sits in nitrogen
  CHECK(r.drv.CalibrateAir(300).error == Error::OutOfRange);
  CHECK_EQ(r.port.module.CountCommands("#CAHI"), 0U);
  CHECK_EQ(r.port.module.flash_cycles, 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK(r.drv.GetState() == fdo2::State::Idle);
  // The same for an explicit standard that does not match the gas.
  CHECK(r.drv.CalibrateHigh(209'500, 300).error == Error::OutOfRange);
  CHECK_EQ(r.port.module.CountCommands("#CAHI"), 0U);
}

TEST_CASE("calibration guard: zero calibration in air is refused") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 209.5);
  CHECK(r.drv.CalibrateZero(300).error == Error::OutOfRange);
  CHECK_EQ(r.port.module.CountCommands("#CALO"), 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
}

TEST_CASE("calibration guard: nothing is written while the sensor warms up") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok); // 1.5 s after power
  r.port.module.scenario.o2_hpa = 209.5;
  CHECK(r.drv.CalibrateHigh(209'500, 300).error == Error::NotReady);
  CHECK(r.drv.CalibrateAir(300).error == Error::NotReady);
  r.port.module.scenario.o2_hpa = 0.0;
  CHECK(r.drv.CalibrateZero(300).error == Error::NotReady);
  CHECK_EQ(r.port.module.flash_cycles, 0U);
  CHECK_EQ(r.drv.FlashWritesUsed(), 0U);
  CHECK(r.drv.GetState() == fdo2::State::Idle);
  r.port.module.Elapse(180'000'000U); // warm
  CHECK(r.drv.CalibrateZero(300).ok());
}

TEST_CASE("calibration guard: the window is configurable") {
  fdo2::Config c = fx::Permissive();
  c.cal_window_pct = 50;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 150.0); // 28 percent below air, inside 50 percent, outside the default 20 percent
  CHECK(r.drv.CalibrateHigh(209'500, 300).ok());
  fx::Rig q(fx::Permissive());
  CHECK(q.Start() == Error::Ok);
  fx::WarmUp(q, 150.0);
  CHECK(q.drv.CalibrateHigh(209'500, 300).error == Error::OutOfRange);
  fdo2::Config bad = fx::Permissive();
  bad.cal_window_pct = 0;
  CHECK(fdo2::Validate(bad) == Error::InvalidArgument);
  bad = fx::Permissive();
  bad.cal_window_pct = 101;
  CHECK(fdo2::Validate(bad) == Error::InvalidArgument);
  bad = fx::Permissive();
  bad.cal_window_min_mhpa = 999;
  CHECK(fdo2::Validate(bad) == Error::InvalidArgument);
  bad.cal_window_min_mhpa = 50'001;
  CHECK(fdo2::Validate(bad) == Error::InvalidArgument);
}

TEST_CASE("calibration guard: a fatal status in the guard read refuses the write") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 209.5);
  r.port.module.scenario.status_override = 2;
  CHECK(r.drv.CalibrateHigh(209'500, 300).error == Error::SensorFault);
  CHECK_EQ(r.port.module.CountCommands("#CAHI"), 0U);
  CHECK(r.drv.GetState() == fdo2::State::Idle);
}

TEST_CASE("calibration guard: the non-blocking call runs the same guard") {
  fx::Rig r(fx::Permissive());
  CHECK(r.Start() == Error::Ok);
  fx::WarmUp(r, 0.0);
  CHECK(r.drv.StartCalibrateHigh(209'500, 300).ok());
  fdo2::Result<bool> p = r.drv.Poll();
  for (int i = 0; (i < 30000) && p.ok() && !p.value; ++i) {
    r.port.module.Elapse(1000);
    p = r.drv.Poll();
  }
  CHECK(!p.ok());
  CHECK(p.error == Error::OutOfRange);
  CHECK_EQ(r.port.module.CountCommands("#CAHI"), 0U);
}
