/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include "fdo2.hpp"

// Returns 0 and the oxygen partial pressure in 0.001 hPa and the temperature in 0.001 degC.
int read_oxygen(MyPort& port, int32_t& o2_mhpa, int32_t& temperature_mc) {
  fdo2::FDO2<MyPort> sensor(port); // no I/O yet

  // 1. Waits for the module's power-up, reads #VERS and #IDNR and checks the device.
  if (auto r = sensor.EnsureInitialized(); !r) {
    return static_cast<int>(
        r.error); // Timeout or DeviceIdMismatch: check wiring and the 3 V levels
  }
  // 2. One #MOXY: oxygen, temperature and the status word.
  const auto o = sensor.MeasureOxygen();
  if (!o) {
    return static_cast<int>(o.error); // SensorFault: see the status word; OutOfRange; Timeout
  }
  o2_mhpa = o.value.o2_mhpa;               // 10^-3 hPa
  temperature_mc = o.value.temperature_mc; // 10^-3 degC
  return 0;
}
// --8<-- [end:quickstart]
