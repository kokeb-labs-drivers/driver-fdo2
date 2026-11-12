/**
 * @file test_crc.cpp
 * @brief CRC-16/MODBUS (data sheet 4.2): the check value, the worked examples, equality with the
 * model's independent implementation.
 */
#include <array>

#include "fixtures.hpp"

namespace {

constexpr std::array<uint8_t, 9> kCheck{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
static_assert(fdo2::Crc16Modbus(std::span<const uint8_t>{kCheck}) == 0x4B37U);

uint16_t CrcOf(const std::string& s) {
  return fdo2::Crc16Modbus(std::span<const char>{s.data(), s.size()});
}

} // namespace

TEST_CASE("crc: the standard check value") {
  CHECK_EQ(fdo2::Crc16Modbus(std::span<const uint8_t>{kCheck}), 0x4B37U);
  CHECK_EQ(fdo2_test::RefCrc16("123456789"), 0x4B37U);
  CHECK_EQ(fdo2::Crc16Modbus(std::span<const uint8_t>{}), 0xFFFFU);
}

TEST_CASE("crc: worked examples of the protocol page") {
  CHECK_EQ(CrcOf("#MOXY 203456 17892 0"), 43291U);
  CHECK_EQ(CrcOf("#VERS 8 1 357 15"), 17525U);
  CHECK_EQ(CrcOf("#LOGO"), 33972U);
  CHECK_EQ(CrcOf("#IDNR 18446744073709551615"), 18155U);
  CHECK_EQ(CrcOf("#MRAW 203456 -1965 1 24385 124072 12792 999734 40365"), 49266U);
}

TEST_CASE("crc: spaces are part of the checksum") {
  CHECK(CrcOf("#VERS 8 1 357 15") != CrcOf("#VERS  8 1 357 15"));
  CHECK(CrcOf("a b") != CrcOf("ab"));
}

TEST_CASE("crc: equals the independent implementation over random strings") {
  uint32_t state = 12345U;
  for (int n = 0; n < 4096; ++n) {
    std::string s;
    state = (state * 1664525U) + 1013904223U;
    const uint32_t len = (state >> 24U) % 40U;
    for (uint32_t i = 0; i < len; ++i) {
      state = (state * 1664525U) + 1013904223U;
      s.push_back(static_cast<char>((state >> 16U) & 0xFFU));
    }
    CHECK_EQ(CrcOf(s), fdo2_test::RefCrc16(s));
    std::vector<uint8_t> bytes(s.begin(), s.end());
    CHECK_EQ(fdo2::Crc16Modbus(std::span<const uint8_t>{bytes}), fdo2_test::RefCrc16(s));
  }
}
