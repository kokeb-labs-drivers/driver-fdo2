/**
 * @file settings.cpp
 * @brief The Settings page's examples (CRC, baud rate, broadcast, user memory), compiled by the
 * host build (never run).
 */
#include <array>

#include "fdo2.hpp"
#include "my_port.hpp"

using Sensor = fdo2::FDO2<MyPort>;

// --8<-- [start:crc]
// Checksummed replies; the driver verifies the change with the next #VERS.
fdo2::Error TurnOnCrc(Sensor& sensor) {
  return sensor.SetCrcEnabled(true).error; // a flash write: needs consent
}
// --8<-- [end:crc]

// --8<-- [start:baud]
// A faster line; the module returns to 19200 baud at every power-up.
fdo2::Error Faster(Sensor& sensor) {
  const auto r = sensor.SetBaudRate(115200); // needs the SetBaudRate hook in the port
  if (r.error == fdo2::Error::StateUnknown) {
    return sensor.Resync().error; // tries the new rate, the old one and 19200
  }
  return r.error;
}
// --8<-- [end:baud]

// --8<-- [start:broadcast]
// The module sends #MRAW lines by itself; read them as they come.
fdo2::Error Stream(Sensor& sensor) {
  if (const auto r = sensor.SetBroadcast(500); !r) { // every 500 ms, stored in flash
    return r.error;
  }
  for (int i = 0; i < 100; ++i) {
    platform_delay_us(10'000);
    const auto p = sensor.Poll(10'000); // this port has no clock: pass the elapsed time
    if (p.ok() && p.value && sensor.GetRaw().ok()) {
      break; // one frame decoded
    }
  }
  return sensor.DisableBroadcast().error; // waits for a frame boundary first
}
// --8<-- [end:broadcast]

// --8<-- [start:user-memory]
// 64 words that survive power-off, for your own use.
fdo2::Error KeepSerial(Sensor& sensor, int32_t serial) {
  const std::array<int32_t, 1> word{serial};
  if (const auto r = sensor.WriteUserMemory(0, word); !r) {
    return r.error;
  }
  std::array<int32_t, 1> back{};
  return sensor.ReadUserMemory(0, back).error;
}
// --8<-- [end:user-memory]
