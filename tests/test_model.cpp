/**
 * @file test_model.cpp
 * @brief Measurements through the model: values, units, the status policies, plausibility windows
 * and the warm-up flag.
 */
#include "fixtures.hpp"

using fdo2::Error;
using fdo2::Quality;
using fdo2::StatusPolicy;

TEST_CASE("model: #MOXY decodes oxygen, temperature and status") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto o = r.drv.MeasureOxygen();
  CHECK(o.ok());
  CHECK_EQ(o.value.o2_mhpa, 209'500);
  CHECK_EQ(o.value.temperature_mc, 21'000);
  CHECK_EQ(o.value.status.word, 0U);
  CHECK(o.value.quality == Quality::Good);
  CHECK(o.value.warming_up);
}

TEST_CASE("model: #MRAW decodes every field") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.scenario.pressure_mbar = 998.5;
  r.port.module.scenario.humidity_pct = 37.25;
  const auto m = r.drv.MeasureRaw();
  CHECK(m.ok());
  CHECK_EQ(m.value.oxygen.o2_mhpa, 209'500);
  CHECK_EQ(m.value.pressure_ubar, 998'500);
  CHECK_EQ(m.value.humidity_mpct, 37'250);
  CHECK(m.value.dphi_mdeg != 0);
  CHECK(r.drv.GetLastFrame().ok());
  CHECK_EQ(r.drv.GetLastFrame().value.pressure_ubar, 998'500);
}

TEST_CASE("model: the volume fraction uses the module's pressure") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  const auto v = r.drv.MeasureVolumePercentMilli();
  CHECK(v.ok());
  CHECK_EQ(v.value, 20'681);
}

TEST_CASE("model: a caller's pressure reference replaces the module's") {
  fdo2::Config c{};
  c.pressure_reference_ubar = 900'000;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  const auto v = r.drv.MeasureVolumePercentMilli();
  CHECK(v.ok());
  CHECK_EQ(v.value, 23'278);
}

TEST_CASE("model: the warm-up flag clears after three minutes of uptime") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  CHECK(r.drv.MeasureOxygen().value.warming_up);
  r.port.module.Elapse(200ULL * 1'000'000ULL);
  CHECK(!r.drv.MeasureOxygen().value.warming_up);
}

TEST_CASE("model: status policy Datasheet accepts only 0 and 1") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.scenario.status_override = 1;
  const auto w = r.drv.MeasureOxygen();
  CHECK(w.ok());
  CHECK(w.value.quality == Quality::Warning);
  for (const int s : {2, 4, 8, 16, 32, 128, 512, 1024, 64, 3}) {
    r.port.module.scenario.status_override = s;
    CHECK(r.drv.MeasureOxygen().error == Error::SensorFault);
  }
  r.port.module.scenario.status_override = 0;
  CHECK(r.drv.MeasureOxygen().ok());
}

TEST_CASE("model: status policy Documented keeps degraded readings") {
  fdo2::Config c{};
  c.status_policy = StatusPolicy::Documented;
  fx::Rig r(c);
  CHECK(r.Start() == Error::Ok);
  for (const int s : {128, 512, 1024, 129, 640}) {
    r.port.module.scenario.status_override = s;
    const auto o = r.drv.MeasureOxygen();
    CHECK(o.ok());
    CHECK(o.value.quality == Quality::Degraded);
  }
  for (const int s : {2, 4, 8, 16, 32, 64, 256, 2048}) {
    r.port.module.scenario.status_override = s;
    CHECK(r.drv.MeasureOxygen().error == Error::SensorFault);
  }
}

TEST_CASE("model: the status word is unsigned 32 bit") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.scenario.status_override = 0x7FFFFFFF;
  CHECK(r.drv.MeasureOxygen().error == Error::SensorFault);
}

TEST_CASE("model: values outside the plausibility windows are OutOfRange") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.scenario.temperature_c = 71.0;
  CHECK(r.drv.MeasureOxygen().error == Error::OutOfRange);
  r.port.module.scenario.temperature_c = 70.0;
  CHECK(r.drv.MeasureOxygen().ok());
  r.port.module.scenario.temperature_c = -20.001;
  CHECK(r.drv.MeasureOxygen().error == Error::OutOfRange);
  r.port.module.scenario.temperature_c = -20.0;
  CHECK(r.drv.MeasureOxygen().ok());
  r.port.module.scenario.temperature_c = 21.0;
  r.port.module.scenario.o2_hpa = 2200.0;
  CHECK(r.drv.MeasureOxygen().error == Error::OutOfRange);
  r.port.module.scenario.o2_hpa = -5.1;
  CHECK(r.drv.MeasureOxygen().error == Error::OutOfRange);
  r.port.module.scenario.o2_hpa = 209.5;
  // The pressure is a reading, not a plausibility gate of #MRAW; the conversions that use it
  // check it.
  r.port.module.scenario.pressure_mbar = 1201.0;
  CHECK(r.drv.MeasureRaw().ok());
  CHECK(r.drv.MeasureVolumePercentMilli().error == Error::OutOfRange);
  r.port.module.scenario.pressure_mbar = 249.0;
  CHECK(r.drv.MeasureVolumePercentMilli().error == Error::OutOfRange);
  r.port.module.scenario.pressure_mbar = 1013.0;
  r.port.module.scenario.humidity_pct = 100.5;
  CHECK(r.drv.MeasureRaw().error == Error::OutOfRange);
}

TEST_CASE("model: a rejected reading is not kept as the latest value") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.scenario.status_override = 2;
  CHECK(r.drv.MeasureOxygen().error == Error::SensorFault);
  CHECK(r.drv.GetOxygen().error != Error::Ok);
}

TEST_CASE("model: the measurement time follows the mode") {
  fx::Rig r;
  CHECK(r.Start() == Error::Ok);
  r.port.module.knobs.measure_us = 280'000;
  const uint64_t t0 = r.port.module.now_us;
  CHECK(r.drv.MeasureOxygen().ok());
  CHECK(r.port.module.now_us - t0 >= 280'000U);
  r.port.module.knobs.measure_us = 400'000;
  CHECK(r.drv.MeasureOxygen().error == Error::Timeout);
}
