/**
 * @file FreeRTOS.h
 * @brief Host stand-in for the FreeRTOS tick type: a 100 Hz tick, as the ESP-IDF default.
 */
#ifndef FDO2_STUB_FREERTOS_H
#define FDO2_STUB_FREERTOS_H

#include <cstdint>

using TickType_t = uint32_t;

/// Milliseconds to ticks at 100 Hz (rounds down, as the real macro).
inline TickType_t pdMS_TO_TICKS(uint32_t ms) {
  return ms / 10U;
}

#endif // FDO2_STUB_FREERTOS_H
