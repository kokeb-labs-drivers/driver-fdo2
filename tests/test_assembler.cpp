/**
 * @file test_assembler.cpp
 * @brief The line assembler: framing, resynchronisation and the 800-character boundary.
 */
#include "fixtures.hpp"

TEST_CASE("assembler: byte at a time and bursts give the same lines") {
  const std::string stream = "#VERS 8 1 357 15\r#IDNR 5\r#LOGO\r";
  fdo2::LineAssembler a;
  fdo2::LineAssembler b;
  const std::string all = fx::Feed(a, stream);
  CHECK(all == "#VERS 8 1 357 15|#IDNR 5|#LOGO|");
  std::string split;
  for (const char c : stream) {
    split += fx::Feed(b, std::string(1, c));
  }
  CHECK(split == all);
}

TEST_CASE("assembler: two replies in one burst are both kept") {
  fdo2::LineAssembler a;
  CHECK(fx::Feed(a, "#MOXY 1 2 0\r#MOXY 3 4 0\r") == "#MOXY 1 2 0|#MOXY 3 4 0|");
}

TEST_CASE("assembler: a bad byte discards the line up to the next CR") {
  fdo2::LineAssembler a;
  CHECK(fx::Feed(a, std::string("#MO\x01XY 1 2 0\r#LOGO\r")) == "!#LOGO|");
  CHECK(a.TakeDiscarded() > 0U);
  CHECK_EQ(a.TakeDiscarded(), 0U);
  CHECK(fx::Feed(a, std::string("ab\0cd\r#LOGO\r", 12)) == "!#LOGO|");
  CHECK(fx::Feed(a, std::string("\xFF\r#LOGO\r")) == "!#LOGO|");
  CHECK(!a.Discarding());
}

TEST_CASE("assembler: CR LF and empty lines") {
  fdo2::LineAssembler a;
  CHECK(fx::Feed(a, "#LOGO\r\n#LOGO\r") == "#LOGO|#LOGO|");
  CHECK(fx::Feed(a, "\r\r#LOGO\r") == "#LOGO|");
  fdo2::LineAssembler strict;
  strict.Reset(false);
  CHECK(fx::Feed(strict, "#LOGO\r\n#LOGO\r") == "#LOGO|!");
  fdo2::LineAssembler mid;
  CHECK(fx::Feed(mid, "#LO\nGO\r#LOGO\r") == "!#LOGO|");
}

TEST_CASE("assembler: the 800 character boundary") {
  auto run = [](std::size_t chars) {
    fdo2::LineAssembler a;
    const std::string line(chars, 'A');
    return fx::Feed(a, line + "\r#LOGO\r");
  };
  CHECK(run(799) == std::string(799, 'A') + "|#LOGO|");
  CHECK(run(800) == "!#LOGO|");
  CHECK(run(801) == "!#LOGO|");
  CHECK(run(5000) == "!#LOGO|");
}

TEST_CASE("assembler: reset drops the partial line and the discard state") {
  fdo2::LineAssembler a;
  (void)fx::Feed(a, "#MOX");
  CHECK_EQ(a.Pending(), 4U);
  a.Reset(true);
  CHECK_EQ(a.Pending(), 0U);
  CHECK(fx::Feed(a, "#LOGO\r") == "#LOGO|");
  (void)fx::Feed(a, "\x01");
  CHECK(a.Discarding());
  a.Reset(true);
  CHECK(!a.Discarding());
}
