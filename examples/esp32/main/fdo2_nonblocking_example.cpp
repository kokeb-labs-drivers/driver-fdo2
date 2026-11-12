/**
 * @file fdo2_nonblocking_example.cpp
 * @brief The same measurement without blocking: Start..() and Poll() from a task that does other
 * work.
 *
 * The port has a clock (NowUs), so Poll() takes no argument.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_fdo2_port.hpp"
#include "fdo2.hpp"
#include "fdo2_kconfig.hpp"

namespace {
constexpr const char* kTag = "fdo2_nb";
}

extern "C" void app_main(void) {
  static Esp32Fdo2Port port;
  static fdo2::FDO2<Esp32Fdo2Port> sensor(port, fdo2::KconfigDefaults());

  if (!sensor.BeginInitialize()) {
    ESP_LOGE(kTag, "BeginInitialize failed");
    return;
  }
  bool measuring = false;
  while (true) {
    const auto p = sensor.Poll(); // never blocks: it reads what has arrived
    if (!p) {
      ESP_LOGW(kTag, "operation failed: %s", fdo2::ToString(p.error));
      if (sensor.IsInitialized()) {
        (void)sensor.Resync(); // the state is unknown: find the module again
      } else {
        (void)sensor.BeginInitialize(); // start-up failed: begin it again from the start
      }
    } else if (p.value) { // an operation ended
      if (measuring) {
        const auto o = sensor.GetOxygen();
        if (o) {
          const Esp32Fdo2Milli v = Esp32Fdo2SplitMilli(o.value.o2_mhpa);
          ESP_LOGI(kTag, "pO2 %s%d.%03d hPa", v.sign, v.whole, v.frac);
        }
        measuring = false;
      }
    } else if (sensor.IsInitialized() && (sensor.Pending() == fdo2::OperationKind::None)) {
      measuring = sensor.StartMeasureOxygen().ok();
    } else {
      // an operation is still running: wait
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
