/**
 * @file api_guide.cpp
 * @brief The API guide's examples, compiled by the host build (never run).
 */
#include "fdo2.hpp"
#include "my_port.hpp"

using Sensor = fdo2::FDO2<MyPort>;

// --8<-- [start:volume]
// The oxygen fraction in 0.001 percent, from the module's own pressure reading.
fdo2::Result<int32_t> OxygenFraction(Sensor& sensor) {
  return sensor.MeasureVolumePercentMilli(); // needs a good status and a plausible pressure
}
// --8<-- [end:volume]

// --8<-- [start:raw]
// A full reading: oxygen, temperature, phase shift, signals, pressure and humidity.
bool ReadAll(Sensor& sensor, fdo2::Raw& out) {
  const auto raw = sensor.MeasureRaw();
  if (!raw) {
    return false; // raw.error says why
  }
  out = raw.value;
  return out.oxygen.quality == fdo2::Quality::Good;
}
// --8<-- [end:raw]

// --8<-- [start:errors]
const char* Explain(fdo2::Error error) {
  switch (error) {
  case fdo2::Error::Timeout:
    return "no reply: wiring, 3 V levels, power";
  case fdo2::Error::Crc:
    return "a corrupted reply: noise on the line";
  case fdo2::Error::SensorFault:
    return "the status word says the reading is not valid";
  case fdo2::Error::NotPermitted:
    return "a flash write needs Config::allow_persistent_writes";
  case fdo2::Error::StateUnknown:
    return "call Resync()";
  default:
    break;
  }
  return fdo2::ToString(error);
}
// --8<-- [end:errors]

// --8<-- [start:identity]
// The module's identity is read once at start-up and kept.
bool IsG2(Sensor& sensor) {
  const fdo2::Identity& id = sensor.GetIdentity();
  return id.caps.set_mode && (id.version.firmware >= 355);
}
// --8<-- [end:identity]
