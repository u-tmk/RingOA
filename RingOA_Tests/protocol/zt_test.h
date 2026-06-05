#ifndef TESTS_ZT_TEST_H_
#define TESTS_ZT_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void ZeroTest_Offline_Test();
void ZeroTest_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_ZT_TEST_H_
