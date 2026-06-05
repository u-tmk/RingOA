#ifndef TESTS_ADDITIVE_3P_TEST_H_
#define TESTS_ADDITIVE_3P_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void Rep3_Offline_Test();
void Rep3_Open_Online_Test(const osuCrypto::CLP &cmd);
void Rep3_EvaluateAdd_Online_Test(const osuCrypto::CLP &cmd);
void Rep3_EvaluateMult_Online_Test(const osuCrypto::CLP &cmd);
void Rep3_EvaluateInnerProduct_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_ADDITIVE_3P_TEST_H_
