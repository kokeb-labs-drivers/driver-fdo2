/**
 * @file esp32_port_test.cpp
 * @brief The ESP-IDF example port on the host, over stand-ins of the ESP-IDF headers: the default
 * configuration starts, the supply pin is optional, waits yield, and the example's number
 * formatting handles negative values.
 */
#include <cstring>

#include "esp32_fdo2_port.hpp"
#include "test_harness.hpp"

namespace {

/// The model attached to fresh stand-in state; the state is cleared again when the test ends, so it
/// never refers to a model that is gone.
struct Rig {
  fdo2_test::Module module;
  Rig() {
    esp_stub::Hooks() = esp_stub::State{};
    esp_stub::Hooks().module = &module;
  }
  Rig(const Rig&) = delete;
  Rig& operator=(const Rig&) = delete;
  ~Rig() {
    esp_stub::Hooks() = esp_stub::State{};
  }
};

} // namespace

TEST_CASE("esp32 port: the default pins (no supply switch) start the driver and measure") {
  Rig rig;
  Esp32Fdo2Port port; // power = -1
  fdo2::FDO2<Esp32Fdo2Port> sensor(port);
  CHECK(sensor.EnsureInitialized().ok());
  CHECK(sensor.MeasureOxygen().ok());
  CHECK(esp_stub::Hooks().gpio_levels.empty()); // no pin was driven
  CHECK_EQ(esp_stub::Hooks().configured_baud, 19'200);
  CHECK(sensor.Deinitialize().ok()); // nothing to switch off: the module stays powered
}

TEST_CASE("esp32 port: GpioSet with no supply pin succeeds and does nothing") {
  Rig rig;
  Esp32Fdo2Port port;
  CHECK(port.GpioSet(fdo2::CtrlPin::Power, fdo2::GpioSignal::ACTIVE));
  CHECK(port.GpioSet(fdo2::CtrlPin::Power, fdo2::GpioSignal::INACTIVE));
  CHECK(esp_stub::Hooks().gpio_levels.empty());
}

TEST_CASE("esp32 port: a configured supply pin is driven off first and then on and off") {
  Rig rig;
  Esp32Fdo2Pins pins;
  pins.power = 7;
  Esp32Fdo2Port port(pins);
  fdo2::FDO2<Esp32Fdo2Port> sensor(port);
  CHECK(sensor.EnsureInitialized().ok());
  const auto& levels = esp_stub::Hooks().gpio_levels;
  REQUIRE(levels.size() >= 2U);
  CHECK_EQ(levels[0].second, 0U); // inactive before the output is enabled (KB-HW-018)
  CHECK_EQ(levels[1].second, 1U);
  CHECK(sensor.Deinitialize().ok());
  CHECK_EQ(levels.back().second, 0U);
  CHECK_EQ(levels.back().first, 7);
  CHECK_EQ(esp_stub::Hooks().gpio_configured_mask, 1ULL << 7U);
}

TEST_CASE("esp32 port: waits of a millisecond or more yield, shorter ones spin") {
  Rig rig;
  fdo2_test::Module& module = rig.module;
  Esp32Fdo2Port port;
  port.DelayUs(500);
  CHECK_EQ(esp_stub::Hooks().spin_calls, 1U);
  CHECK_EQ(esp_stub::Hooks().delay_calls, 0U);
  port.DelayUs(1000);
  port.DelayUs(25'000);
  CHECK_EQ(esp_stub::Hooks().spin_calls, 1U);
  CHECK_EQ(esp_stub::Hooks().delay_calls, 2U);
  CHECK(module.now_us >= 25'500U);
}

TEST_CASE("esp32 example: negative values keep their sign and a positive fraction") {
  const Esp32Fdo2Milli a = Esp32Fdo2SplitMilli(-1500);
  CHECK(std::strcmp(a.sign, "-") == 0);
  CHECK_EQ(a.whole, 1);
  CHECK_EQ(a.frac, 500);
  const Esp32Fdo2Milli b = Esp32Fdo2SplitMilli(-5);
  CHECK(std::strcmp(b.sign, "-") == 0);
  CHECK_EQ(b.whole, 0);
  CHECK_EQ(b.frac, 5);
  const Esp32Fdo2Milli c = Esp32Fdo2SplitMilli(209'500);
  CHECK(std::strcmp(c.sign, "") == 0);
  CHECK_EQ(c.whole, 209);
  CHECK_EQ(c.frac, 500);
  const Esp32Fdo2Milli d = Esp32Fdo2SplitMilli(INT32_MIN);
  CHECK(std::strcmp(d.sign, "-") == 0);
  CHECK_EQ(d.whole, 2'147'483);
  CHECK_EQ(d.frac, 648);
}
