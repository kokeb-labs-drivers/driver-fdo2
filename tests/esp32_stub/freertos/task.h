/**
 * @file task.h
 * @brief Host stand-in for `vTaskDelay()`: the test's module clock moves.
 */
#ifndef FDO2_STUB_FREERTOS_TASK_H
#define FDO2_STUB_FREERTOS_TASK_H

#include "esp32_host_hooks.hpp"
#include "freertos/FreeRTOS.h"

inline void vTaskDelay(TickType_t ticks) {
  esp_stub::Hooks().delay_calls += 1U;
  esp_stub::Hooks().Elapse(static_cast<uint64_t>(ticks) * 10'000U);
}

#endif // FDO2_STUB_FREERTOS_TASK_H
