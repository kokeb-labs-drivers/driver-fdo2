/**
 * @file main.cpp
 * @brief The FDO2 driver on your PC: start a simulated module, measure, meet each kind of
 * failure and use the non-blocking and broadcast modes. Integer arithmetic only.
 *
 * No hardware and no SDK: the host port (host_fdo2_port.hpp) connects the real driver to the
 * module model the host tests use. The same calls run on an MCU with that MCU's port
 * (examples/esp32/ for ESP-IDF). The output is compared with expected_output.txt by CTest.
 * Exit code 0 when every step behaved as described.
 */
#include <array>
#include <cstdint>
#include <cstdio>

#include "fdo2.hpp"
#include "host_fdo2_port.hpp"

namespace {

using Sensor = fdo2::FDO2<HostFdo2Port>;

/// Print a value in thousandths with three decimals, by integer division: 20953 -> "20.953".
void PrintMilli(const char* label, int32_t milli, const char* unit) {
  const char* sign = (milli < 0) ? "-" : "";
  const int64_t magnitude =
      (milli < 0) ? -static_cast<int64_t>(milli) : static_cast<int64_t>(milli);
  std::printf("%s %s%lld.%03lld %s\n", label, sign, static_cast<long long>(magnitude / 1000),
              static_cast<long long>(magnitude % 1000), unit);
}

const char* QualityName(fdo2::Quality q) {
  switch (q) {
  case fdo2::Quality::Good:
    return "good";
  case fdo2::Quality::Warning:
    return "warning (detector amplification reduced)";
  case fdo2::Quality::Degraded:
    return "degraded";
  case fdo2::Quality::Invalid:
    return "invalid";
  default:
    break;
  }
  return "unknown";
}

bool Step(const char* name, fdo2::Error error, fdo2::Error expected) {
  const bool ok = error == expected;
  std::printf("%s: %s%s\n", name, fdo2::ToString(error), ok ? "" : "  <-- UNEXPECTED");
  return ok;
}

} // namespace

int main() {
  HostFdo2Port port;
  // 20.9 % oxygen at 1013 mbar and 21 degC.
  port.module.scenario.o2_hpa = 211.717;
  port.module.scenario.temperature_c = 21.0;
  port.module.scenario.pressure_mbar = 1013.0;
  bool all_ok = true;

  std::printf("FDO2 driver on the host\n");

  // 1. Start-up: power, UART, #VERS and #IDNR.
  Sensor sensor(port);
  if (const auto r = sensor.EnsureInitialized(); !r) {
    std::printf("EnsureInitialized failed: %s\n", fdo2::ToString(r.error));
    return 1;
  }
  const fdo2::Identity& id = sensor.GetIdentity();
  std::printf("module: device %d, firmware %d.%02d, sensors 0x%X\n",
              static_cast<int>(id.version.device_id), static_cast<int>(id.version.firmware / 100),
              static_cast<int>(id.version.firmware % 100),
              static_cast<unsigned>(id.version.sensors));
  std::printf("unique id: %llu\n", static_cast<unsigned long long>(id.unique_id));
  std::printf("family: %s, #SETM %s, broadcast %s\n",
              (id.family == fdo2::Family::Fdo2G2) ? "FDO2-G2" : "FDO2",
              id.caps.set_mode ? "yes" : "no", id.caps.broadcast ? "yes" : "no");

  // 2. A full measurement.
  const auto raw = sensor.MeasureRaw();
  if (raw) {
    PrintMilli("oxygen", raw.value.oxygen.o2_mhpa, "hPa");
    PrintMilli("temperature", raw.value.oxygen.temperature_mc, "degC");
    PrintMilli("pressure", raw.value.pressure_ubar, "mbar");
    PrintMilli("humidity", raw.value.humidity_mpct, "%RH");
    const auto volume = sensor.MeasureVolumePercentMilli();
    PrintMilli("oxygen fraction", volume.value, "%");
    std::printf("quality: %s\n", QualityName(raw.value.oxygen.quality));
  }
  all_ok = Step("MeasureRaw", raw.error, fdo2::Error::Ok) && all_ok;

  // 3. Status 1: the reading is valid, the detector amplification is reduced.
  port.module.scenario.status_override = 1;
  const auto warn = sensor.MeasureOxygen();
  all_ok = Step("MeasureOxygen with status 1", warn.error, fdo2::Error::Ok) && all_ok;
  std::printf("quality: %s\n", QualityName(warn.value.quality));

  // 4. Status 2 (signal too low): refused by the default policy.
  port.module.scenario.status_override = 2;
  all_ok =
      Step("MeasureOxygen with status 2", sensor.MeasureOxygen().error, fdo2::Error::SensorFault) &&
      all_ok;
  port.module.scenario.status_override = -1;

  // 5. The module answers #ERRO -26 (not supported).
  port.module.Arm(fdo2_test::FaultKind::ErrorCode, -26);
  std::array<int32_t, 2> memory{};
  all_ok = Step("ReadUserMemory with #ERRO -26", sensor.ReadUserMemory(0, memory).error,
                fdo2::Error::Unsupported) &&
           all_ok;
  std::printf("module error code: %d\n", static_cast<int>(sensor.DeviceErrorCode()));

  // 6. Calibration writes flash: refused until the application allows it.
  all_ok = Step("CalibrateAir without consent", sensor.CalibrateAir(300).error,
                fdo2::Error::NotPermitted) &&
           all_ok;
  std::printf("flash writes sent: %u\n", static_cast<unsigned>(sensor.FlashWritesUsed()));
  (void)sensor.Deinitialize();

  // 7. With consent and a budget of three flash writes.
  fdo2::Config config{};
  config.allow_persistent_writes = true;
  config.max_flash_writes = 3;
  Sensor writer(port, config);
  if (const auto r = writer.EnsureInitialized(); !r) {
    std::printf("EnsureInitialized failed: %s\n", fdo2::ToString(r.error));
    return 1;
  }
  // A calibration needs a warm sensor (3 minutes, data sheet 2): the driver refuses with `NotReady`
  // before. The application waits and, on a port without a clock, tells the driver how long.
  all_ok = Step("CalibrateAir while warming up", writer.CalibrateAir(300).error,
                fdo2::Error::NotReady) &&
           all_ok;
  port.DelayUs(180'000'000);
  (void)writer.Poll(180'000'000);
  all_ok =
      Step("CalibrateAir with consent", writer.CalibrateAir(300).error, fdo2::Error::Ok) && all_ok;
  std::printf("flash writes sent: %u of %u\n", static_cast<unsigned>(writer.FlashWritesUsed()),
              static_cast<unsigned>(writer.GetConfig().max_flash_writes));

  // 8. Non-blocking: the port has no clock, so the loop passes the elapsed time to Poll().
  all_ok = Step("StartMeasureOxygen", writer.StartMeasureOxygen().error, fdo2::Error::Ok) && all_ok;
  uint32_t other_task_ticks = 0;
  bool done = false;
  for (int step = 0; (step < 10) && !done; ++step) {
    port.DelayUs(50'000); // the super-loop's 50 ms tick
    ++other_task_ticks;   // another task keeps running meanwhile
    const auto p = writer.Poll(50'000);
    done = p.ok() && p.value;
  }
  std::printf("non-blocking measurement done after %u ticks: %s\n",
              static_cast<unsigned>(other_task_ticks), done ? "yes" : "no");
  all_ok = (done && writer.GetOxygen().ok()) && all_ok;

  // 9. Broadcast: the module streams #MRAW lines; decode five of them, then stop the stream.
  all_ok = Step("SetBroadcast 200 ms", writer.SetBroadcast(200).error, fdo2::Error::Ok) && all_ok;
  int frames = 0;
  for (int i = 0; (i < 400) && (frames < 5); ++i) {
    port.DelayUs(10'000);
    const auto p = writer.Poll(10'000);
    if (p.ok() && p.value) {
      const auto frame = writer.GetRaw();
      if (frame) {
        ++frames;
        PrintMilli("frame oxygen", frame.value.oxygen.o2_mhpa, "hPa");
      }
    }
  }
  all_ok = (frames == 5) && all_ok;
  all_ok = Step("DisableBroadcast", writer.DisableBroadcast().error, fdo2::Error::Ok) && all_ok;
  std::printf("flash writes sent: %u of %u\n", static_cast<unsigned>(writer.FlashWritesUsed()),
              static_cast<unsigned>(writer.GetConfig().max_flash_writes));

  std::printf("%s\n", all_ok ? "done" : "FAILED");
  return all_ok ? 0 : 1;
}
