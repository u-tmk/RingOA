#ifndef TESTS_BINARY_2P_TEST_H_
#define TESTS_BINARY_2P_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void Binary2P_EvaluateXor_Offline_Test();
void Binary2P_EvaluateXor_Online_Test(const osuCrypto::CLP &cmd);
void Binary2P_BeaverTriples_Test();
void Binary2P_EvaluateAnd_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_BINARY_2P_TEST_H_
