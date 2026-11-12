/**
 * @file calibration.cpp
 * @brief The Calibration page's examples, compiled by the host build (never run).
 */
#include "fdo2.hpp"
#include "my_port.hpp"

using Sensor = fdo2::FDO2<MyPort>;

// --8<-- [start:air]
// One high point in ambient air; the sensor has been in it for at least five minutes.
fdo2::Error CalibrateInAir(Sensor& sensor) {
  // Reads the module's pressure, takes 20.95 % oxygen (dry air) and sends #CAHI.
  return sensor.CalibrateAir(300).error; // NotPermitted unless allowed in the Config
}
// --8<-- [end:air]

// --8<-- [start:two-point]
// Zero in nitrogen, then a known partial pressure; each step needs its own steady time.
fdo2::Error CalibrateTwoPoint(Sensor& sensor, int32_t high_po2_mhpa) {
  if (const auto r = sensor.CalibrateZero(300); !r) {
    return r.error; // CalibrationRejected: #ERRO -60, the value is outside the expected range
  }
  return sensor.CalibrateHigh(high_po2_mhpa, 300).error;
}
// --8<-- [end:two-point]

// --8<-- [start:air-value]
// The value #CAHI will receive, before anything is written.
int32_t AirValue(int32_t pressure_ubar) {
  const auto v = fdo2::AirO2Mhpa(pressure_ubar); // 1013000 ubar -> 212224 (10^-3 hPa)
  return v ? v.value : 0;
}
// --8<-- [end:air-value]
