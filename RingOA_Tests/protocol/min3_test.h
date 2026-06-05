#ifndef TESTS_MIN3_TEST_H_
#define TESTS_MIN3_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void Min3_Offline_Test();
void Min3_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_MIN3_TEST_H_
