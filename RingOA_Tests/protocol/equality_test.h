#ifndef TESTS_EQUALITY_TEST_H_
#define TESTS_EQUALITY_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void Equality_Offline_Test();
void Equality_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_EQUALITY_TEST_H_
