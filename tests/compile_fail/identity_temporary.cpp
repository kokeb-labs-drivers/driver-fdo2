/**
 * @file identity_temporary.cpp
 * @brief Must NOT compile: `GetIdentity()` of a temporary driver would dangle, so the rvalue
 * overload is deleted.
 *
 * EXPECT: deleted|GetIdentity
 */
#include "fdo2.hpp"
#include "mock_fdo2.hpp"

int main() {
  fdo2_test::MinimalPort port;
  fdo2::FDO2<fdo2_test::MinimalPort>(port).GetIdentity();
  return 0;
}
