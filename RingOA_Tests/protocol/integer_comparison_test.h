#ifndef TESTS_INTEGER_COMPARISON_H_
#define TESTS_INTEGER_COMPARISON_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void IntegerComparison_Offline_Test();
void IntegerComparison_Online_Test(const osuCrypto::CLP &cmd);

// TODO: fix these tests
void IntegerComparison_FullDomain_Offline_Test();
void IntegerComparison_FullDomain_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_INTEGER_COMPARISON_H_
