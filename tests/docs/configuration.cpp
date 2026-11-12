/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "fdo2.hpp"
#include "fdo2_kconfig.hpp"
#include "my_port.hpp"

// --8<-- [start:custom]
fdo2::Config LegacyConfig() {
  fdo2::Config c{};                                 // the documented defaults
  c.family = fdo2::Family::Fdo2;                    // the older module: no #SETM
  c.status_policy = fdo2::StatusPolicy::Documented; // keep readings with bit 7, 9 or 10 set
  c.retries = 2;                                    // idempotent reads are sent again twice
  c.pressure_reference_ubar = 1'013'250;            // use the caller's pressure for the fraction
  return c; // fdo2::Validate(c) checks it without the module
}

fdo2::Error StartLegacy(MyPort& port) {
  static fdo2::FDO2<MyPort> sensor(port, LegacyConfig()); // stored, no I/O
  return sensor.EnsureInitialized().error; // InvalidArgument if the config is out of range
}
// --8<-- [end:custom]

// --8<-- [start:checked]
// A value that is wrong stops the build instead of the start-up.
[[maybe_unused]] constexpr fdo2::CheckedConfig kChecked{fdo2::Config{}};
// --8<-- [end:checked]

// --8<-- [start:consent]
fdo2::Config WritableConfig() {
  fdo2::Config c{};
  c.allow_persistent_writes = true; // every flash-writing call needs this
  c.max_flash_writes = 4; // and is counted against this budget (about 20000 cycles in all)
  return c;
}
// --8<-- [end:consent]

// --8<-- [start:kconfig]
// With the Kconfig layer, the values come from the project configuration.
fdo2::Config ProjectConfig() {
  return fdo2::KconfigDefaults();
}
// --8<-- [end:kconfig]
