#ifndef TESTS_ADDITIVE_2P_TEST_H_
#define TESTS_ADDITIVE_2P_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void Additive2P_Evaluate_Offline_Test();
void Additive2P_BeaverTriples_Test();
void Additive2P_EvaluateAdd_Online_Test(const osuCrypto::CLP &cmd);
void Additive2P_EvaluatePubMult_Online_Test(const osuCrypto::CLP &cmd);
void Additive2P_EvaluateMult_Online_Test(const osuCrypto::CLP &cmd);
void Additive2P_EvaluateSelect_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_ADDITIVE_2P_TEST_H_
