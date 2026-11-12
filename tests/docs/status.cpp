/**
 * @file status.cpp
 * @brief The Status and errors page's examples, compiled by the host build (never run).
 */
#include "fdo2.hpp"

// --8<-- [start:status]
const char* Describe(const fdo2::Status& status) {
  if (status.SignalTooLow()) {
    return "signal too low: the sensor is not covered by the medium or is damaged";
  }
  if (status.AmplificationReduced()) {
    return "detector amplification reduced: the reading is valid";
  }
  switch (status.Classify()) {
  case fdo2::Quality::Good:
    return "good";
  case fdo2::Quality::Degraded:
    return "a warning bit is set";
  default:
    break;
  }
  return "do not use the reading";
}
// --8<-- [end:status]
