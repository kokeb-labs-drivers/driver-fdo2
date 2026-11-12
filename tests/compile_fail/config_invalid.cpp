/**
 * @file config_invalid.cpp
 * @brief Must NOT compile: a `CheckedConfig` of an invalid `Config` is rejected at build time
 * (handbook KB-CPP20-012).
 *
 * EXPECT: ConfigIsInvalid|non-constant|constexpr
 */
#include "fdo2_types.hpp"

int main() {
  fdo2::Config config{};
  config.power_up_ms = 1;
  constexpr fdo2::CheckedConfig checked{config};
  return static_cast<int>(checked.Get().baud);
}
