/**
 * @file test_parse.cpp
 * @brief The strict reply grammar (data sheet 4.1, 4.3, 4.4) at its boundaries.
 */
#include "fixtures.hpp"

using fdo2::Command;
using fdo2::ParseStatus;
using fx::P;
using fx::PLoose;

TEST_CASE("parse: every reply form of the command table") {
  auto v = P("#VERS 8 1 357 15");
  CHECK_EQ(v.status, ParseStatus::Ok);
  CHECK(v.known);
  CHECK_EQ(v.command, Command::Vers);
  CHECK_EQ(v.count, 4U);
  CHECK_EQ(v.values[2], 357);
  auto m = P("#MOXY 203456 17892 0");
  CHECK_EQ(m.command, Command::Moxy);
  CHECK_EQ(m.count, 3U);
  auto r = P("#MRAW 203456 -1965 1 24385 124072 12792 999734 40365");
  CHECK_EQ(r.command, Command::Mraw);
  CHECK_EQ(r.count, 8U);
  CHECK_EQ(r.values[1], -1965);
  CHECK_EQ(P("#LOGO").count, 0U);
  CHECK_EQ(P("#CALO").status, ParseStatus::Ok);
  CHECK_EQ(P("#RDUM 0 2 5 -6").count, 4U);
  CHECK_EQ(P("#WRUM 0 1 7").command, Command::Wrum);
  CHECK_EQ(P("#BAUD 115200").values[0], 115200);
  CHECK_EQ(P("#CRCE 1").command, Command::Crce);
  CHECK_EQ(P("#CAHI 203456").command, Command::Cahi);
  CHECK_EQ(P("#BCST 100").command, Command::Bcst);
  CHECK_EQ(P("#SETM 4").command, Command::Setm);
}

TEST_CASE("parse: signed 32 bit boundaries") {
  CHECK_EQ(P("#MOXY -2147483648 2147483647 0").status, ParseStatus::Ok);
  CHECK_EQ(P("#MOXY -2147483648 2147483647 0").values[0], -2147483648LL);
  CHECK_EQ(P("#MOXY -2147483649 0 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 2147483648 0 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 0 -99999999999999999999999 0").status, ParseStatus::Malformed);
}

TEST_CASE("parse: the status word is unsigned 32 bit") {
  CHECK_EQ(P("#MOXY 1 2 4294967295").status, ParseStatus::Ok);
  CHECK_EQ(P("#MOXY 1 2 4294967295").values[2], 4294967295LL);
  CHECK_EQ(P("#MOXY 1 2 4294967296").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 1 2 -1").status, ParseStatus::Malformed);
}

TEST_CASE("parse: the unique id is unsigned 64 bit with up to 20 digits") {
  const auto max = P("#IDNR 18446744073709551615");
  CHECK_EQ(max.status, ParseStatus::Ok);
  CHECK(max.big == 18446744073709551615ULL);
  CHECK_EQ(P("#IDNR 18446744073709551616").status, ParseStatus::Malformed);
  CHECK_EQ(P("#IDNR 100000000000000000000").status, ParseStatus::Malformed);
  CHECK_EQ(P("#IDNR 123456789012345678901").status, ParseStatus::Malformed);
  CHECK_EQ(P("#IDNR -1").status, ParseStatus::Malformed);
  CHECK(P("#IDNR 0").big == 0ULL);
}

TEST_CASE("parse: number grammar") {
  CHECK_EQ(P("#MOXY 01 2 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY -0 2 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY +1 2 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 0x1 2 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 1.5 2 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY - 2 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 0 0 0").status, ParseStatus::Ok);
  CHECK_EQ(P("#MOXY 1\t2 0").status, ParseStatus::Malformed);
  const std::string nul("#MOXY 1 2 0\0junk", 16);
  CHECK_EQ(P(nul).status, ParseStatus::Malformed);
  const std::string high("#MOXY 1 2 \x80", 11);
  CHECK_EQ(P(high).status, ParseStatus::Malformed);
}

TEST_CASE("parse: spacing is strict by default and folded on request") {
  CHECK_EQ(P("#MOXY  1 2 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 1 2 0 ").status, ParseStatus::Malformed);
  CHECK_EQ(P("#VERS  8 1 357 15").status, ParseStatus::Malformed);
  CHECK_EQ(PLoose("#MOXY  1   2 0").status, ParseStatus::Ok);
  CHECK_EQ(PLoose("#MOXY  1   2 0").count, 3U);
  CHECK_EQ(PLoose("#MOXY 1 2 0 ").status, ParseStatus::Ok);
  CHECK_EQ(PLoose("#MOXY 1 2 0 ").count, 3U);
}

TEST_CASE("parse: header grammar") {
  CHECK_EQ(P("").status, ParseStatus::Malformed);
  CHECK_EQ(P("#VER").status, ParseStatus::Malformed);
  CHECK_EQ(P("#vers 8 1 357 15").status, ParseStatus::Malformed);
  CHECK_EQ(P("#VERSS 8 1 357 15").status, ParseStatus::Malformed);
  CHECK_EQ(P("VERS 8 1 357 15").status, ParseStatus::Malformed);
  CHECK_EQ(P("#VE1S").status, ParseStatus::Malformed);
  CHECK_EQ(P("#VERS").status, ParseStatus::Ok);
  CHECK_EQ(P("#VERS").count, 0U);
  const auto other = P("#ABCD 1 2");
  CHECK_EQ(other.status, ParseStatus::Ok);
  CHECK(!other.known);
}

TEST_CASE("parse: #ERRO is exactly a negative code") {
  const auto e = P("#ERRO -21");
  CHECK_EQ(e.status, ParseStatus::Ok);
  CHECK(e.erro);
  CHECK_EQ(e.error_code, -21);
  CHECK_EQ(P("#ERRO -2147483648").error_code, -2147483647 - 1);
  CHECK_EQ(P("#ERRO 0").status, ParseStatus::Malformed);
  CHECK_EQ(P("#ERRO 21").status, ParseStatus::Malformed);
  CHECK_EQ(P("#ERRO").status, ParseStatus::Malformed);
  CHECK_EQ(P("#ERRO -21 3").status, ParseStatus::Malformed);
  CHECK_EQ(P("#ERRO -99999999999").status, ParseStatus::Malformed);
  CHECK_EQ(P("#ERRO  -21").status, ParseStatus::Malformed);
  CHECK_EQ(PLoose("#ERRO  -21").status, ParseStatus::Malformed);
}

TEST_CASE("parse: the CRC suffix") {
  fdo2::ParseOptions need{};
  need.crc_required = true;
  const auto good = P(fx::WithCrc("#MOXY 203456 17892 0"), need);
  CHECK_EQ(good.status, ParseStatus::Ok);
  CHECK(good.crc_present);
  CHECK_EQ(P("#MOXY 203456 17892 0: 43291").status, ParseStatus::Ok);
  CHECK_EQ(P("#MOXY 203456 17892 0: 43292").status, ParseStatus::BadCrc);
  CHECK_EQ(P("#MOXY 203456 17892 0:43291").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0:  43291").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0: ").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0: 4329a").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0: 043291").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0: 065535").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0: 65536").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0: 123456").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0:").status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0", need).status, ParseStatus::BadCrc);
  fdo2::ParseOptions off{};
  off.crc_forbidden = true;
  CHECK_EQ(P("#MOXY 203456 17892 0: 43291", off).status, ParseStatus::Malformed);
  CHECK_EQ(P("#MOXY 203456 17892 0", off).status, ParseStatus::Ok);
}

TEST_CASE("parse: CRC with the value 0 and with the largest values") {
  // Find a body whose CRC is small by scanning: the grammar must accept every value 0..65535.
  bool saw_short = false;
  for (int i = 0; (i < 70000) && !saw_short; ++i) {
    const std::string body = "#IDNR " + std::to_string(i);
    if (fdo2_test::RefCrc16(body) < 1000U) {
      CHECK_EQ(P(fx::WithCrc(body)).status, ParseStatus::Ok);
      saw_short = true;
    }
  }
  CHECK(saw_short);
  bool saw_large = false;
  for (int i = 0; (i < 70000) && !saw_large; ++i) {
    const std::string body = "#IDNR " + std::to_string(i);
    if (fdo2_test::RefCrc16(body) >= 60000U) {
      CHECK_EQ(P(fx::WithCrc(body)).status, ParseStatus::Ok);
      saw_large = true;
    }
  }
  CHECK(saw_large);
}

TEST_CASE("parse: #ERRO without CRC while CRC is required") {
  fdo2::ParseOptions need{};
  need.crc_required = true;
  CHECK_EQ(P("#ERRO -26", need).status, ParseStatus::Ok);
  need.erro_without_crc = false;
  CHECK_EQ(P("#ERRO -26", need).status, ParseStatus::BadCrc);
  CHECK_EQ(P(fx::WithCrc("#ERRO -26"), need).status, ParseStatus::Ok);
}

TEST_CASE("parse: too many values") {
  std::string s = "#RDUM 0 64";
  for (int i = 0; i < 64; ++i) {
    s += " 1";
  }
  CHECK_EQ(P(s).status, ParseStatus::Ok);
  CHECK_EQ(P(s).count, 66U);
  s += " 1";
  CHECK_EQ(P(s).status, ParseStatus::Malformed);
}

TEST_CASE("parse: expected counts and echo verification") {
  const std::array<int32_t, 2> rd{3, 2};
  CHECK_EQ(fdo2::ExpectedCount(Command::Rdum, rd), 4U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Wrum, rd), 4U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Vers, {}), 4U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Idnr, {}), 1U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Moxy, {}), 3U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Mraw, {}), 8U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Logo, {}), 0U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Calo, {}), 0U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Bcst, {}), 1U);
  CHECK_EQ(fdo2::ExpectedCount(Command::Rdum, {}), 0U);
  CHECK(fdo2::VerifyEcho(Command::Rdum, rd, P("#RDUM 3 2 9 9")));
  CHECK(!fdo2::VerifyEcho(Command::Rdum, rd, P("#RDUM 3 3 9 9")));
  CHECK(!fdo2::VerifyEcho(Command::Rdum, rd, P("#RDUM 3")));
  const std::array<int32_t, 1> one{4};
  CHECK(fdo2::VerifyEcho(Command::Setm, one, P("#SETM 4")));
  CHECK(!fdo2::VerifyEcho(Command::Setm, one, P("#SETM 3")));
  CHECK(!fdo2::VerifyEcho(Command::Setm, one, P("#BCST 4")));
  CHECK(!fdo2::VerifyEcho(Command::Setm, one, P("#ERRO -1")));
  CHECK(!fdo2::VerifyEcho(Command::Setm, one, P("#ABCD 4")));
  CHECK(fdo2::VerifyEcho(Command::Vers, {}, P("#VERS 8 1 357 15")));
}
