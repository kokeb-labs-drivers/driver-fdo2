/**
 * @file fdo2_basic_example.cpp
 * @brief Start the FDO2 from the menuconfig values and log the oxygen partial pressure and the
 * temperature once a second, with the blocking calls.
 *
 * Wiring: examples/esp32/README.md. Every Result is checked and logged by name.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_fdo2_port.hpp"
#include "fdo2.hpp"
#include "fdo2_kconfig.hpp"

namespace {
constexpr const char* kTag = "fdo2_example";
}

extern "C" void app_main(void) {
  static Esp32Fdo2Port port; // static: the driver keeps a reference
  static fdo2::FDO2<Esp32Fdo2Port> sensor(port, fdo2::KconfigDefaults()); // no I/O

  ESP_LOGI(kTag, "FDO2 driver %s", fdo2::GetDriverVersion());
  const auto init = sensor.EnsureInitialized(); // about 1.5 s: the module's power-up
  if (!init) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", fdo2::ToString(init.error));
    return;
  }
  const fdo2::Identity& id = sensor.GetIdentity();
  ESP_LOGI(kTag, "initialised, firmware %d", static_cast<int>(id.version.firmware));

  while (true) {
    const auto o = sensor.MeasureOxygen();
    if (o) {
      const Esp32Fdo2Milli p = Esp32Fdo2SplitMilli(o.value.o2_mhpa);
      const Esp32Fdo2Milli t = Esp32Fdo2SplitMilli(o.value.temperature_mc);
      ESP_LOGI(kTag, "pO2 %s%d.%03d hPa, %s%d.%03d degC", p.sign, p.whole, p.frac, t.sign, t.whole,
               t.frac);
    } else {
      ESP_LOGW(kTag, "measurement failed: %s", fdo2::ToString(o.error));
      if (o.error == fdo2::Error::StateUnknown) {
        (void)sensor.Resync();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
