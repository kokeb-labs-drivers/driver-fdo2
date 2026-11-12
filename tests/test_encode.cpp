/**
 * @file test_encode.cpp
 * @brief Command encoders: exact bytes, argument ranges and the longest command.
 */
#include <array>

#include "fixtures.hpp"

using fdo2::Command;

namespace {

std::string Enc(Command c, std::initializer_list<int32_t> args) {
  std::array<char, fdo2::kMaxCommandLength> buf{};
  const std::vector<int32_t> a(args);
  const std::size_t n = fdo2::EncodeCommand(c, std::span<const int32_t>{a}, buf);
  return {buf.data(), n};
}

} // namespace

TEST_CASE("encode: every command's bytes") {
  CHECK(Enc(Command::Vers, {}) == "#VERS\r");
  CHECK(Enc(Command::Idnr, {}) == "#IDNR\r");
  CHECK(Enc(Command::Moxy, {}) == "#MOXY\r");
  CHECK(Enc(Command::Mraw, {}) == "#MRAW\r");
  CHECK(Enc(Command::Logo, {}) == "#LOGO\r");
  CHECK(Enc(Command::Calo, {}) == "#CALO\r");
  CHECK(Enc(Command::Crce, {1}) == "#CRCE 1\r");
  CHECK(Enc(Command::Crce, {0}) == "#CRCE 0\r");
  CHECK(Enc(Command::Rdum, {0, 64}) == "#RDUM 0 64\r");
  CHECK(Enc(Command::Rdum, {63, 1}) == "#RDUM 63 1\r");
  CHECK(Enc(Command::Wrum, {5, 3, -1, 0, 2147483647}) == "#WRUM 5 3 -1 0 2147483647\r");
  CHECK(Enc(Command::Wrum, {0, 1, -2147483647 - 1}) == "#WRUM 0 1 -2147483648\r");
  CHECK(Enc(Command::Baud, {115200}) == "#BAUD 115200\r");
  CHECK(Enc(Command::Baud, {1200}) == "#BAUD 1200\r");
  CHECK(Enc(Command::Cahi, {203456}) == "#CAHI 203456\r");
  CHECK(Enc(Command::Bcst, {0}) == "#BCST 0\r");
  CHECK(Enc(Command::Bcst, {100}) == "#BCST 100\r");
  CHECK(Enc(Command::Bcst, {10000}) == "#BCST 10000\r");
  CHECK(Enc(Command::Setm, {4}) == "#SETM 4\r");
}

TEST_CASE("encode: argument errors give zero length before anything is built") {
  CHECK(Enc(Command::Vers, {1}).empty());
  CHECK(Enc(Command::Crce, {2}).empty());
  CHECK(Enc(Command::Crce, {}).empty());
  CHECK(Enc(Command::Rdum, {0, 0}).empty());
  CHECK(Enc(Command::Rdum, {0, 65}).empty());
  CHECK(Enc(Command::Rdum, {64, 1}).empty());
  CHECK(Enc(Command::Rdum, {-1, 1}).empty());
  CHECK(Enc(Command::Rdum, {63, 2}).empty());
  CHECK(Enc(Command::Rdum, {1}).empty());
  CHECK(Enc(Command::Wrum, {0, 2, 1}).empty());
  CHECK(Enc(Command::Wrum, {0, 1, 1, 2}).empty());
  CHECK(Enc(Command::Wrum, {1}).empty());
  CHECK(Enc(Command::Baud, {9601}).empty());
  CHECK(Enc(Command::Baud, {0}).empty());
  CHECK(Enc(Command::Baud, {-19200}).empty());
  CHECK(Enc(Command::Cahi, {0}).empty());
  CHECK(Enc(Command::Bcst, {1}).empty());
  CHECK(Enc(Command::Bcst, {99}).empty());
  CHECK(Enc(Command::Bcst, {10001}).empty());
  CHECK(Enc(Command::Bcst, {-100}).empty());
  CHECK(Enc(Command::Setm, {5}).empty());
  CHECK(Enc(Command::Setm, {-1}).empty());
  CHECK(Enc(Command::Calo, {1}).empty());
}

TEST_CASE("encode: the longest command and a short buffer") {
  std::array<int32_t, 66> args{};
  args[0] = 0;
  args[1] = 64;
  for (std::size_t i = 2; i < args.size(); ++i) {
    args[i] = -2147483647 - 1;
  }
  std::array<char, fdo2::kMaxCommandLength> buf{};
  const std::size_t n = fdo2::EncodeCommand(Command::Wrum, std::span<const int32_t>{args}, buf);
  CHECK_EQ(n, 779U);
  CHECK(n < fdo2::kMaxCommandLength);
  CHECK_EQ(buf[n - 1U], '\r');
  std::array<char, 8> small{};
  CHECK_EQ(fdo2::EncodeCommand(Command::Setm, std::array<int32_t, 1>{4}, small), 8U);
  std::array<char, 7> tiny{};
  CHECK_EQ(fdo2::EncodeCommand(Command::Setm, std::array<int32_t, 1>{4}, tiny), 0U);
  std::array<char, 4> none{};
  CHECK_EQ(fdo2::EncodeCommand(Command::Vers, std::span<const int32_t>{}, none), 0U);
  std::array<char, 14> mid{};
  CHECK_EQ(fdo2::EncodeCommand(Command::Wrum, std::array<int32_t, 3>{0, 1, -2147483647 - 1}, mid),
           0U);
}
