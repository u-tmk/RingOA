#ifndef TESTS_BINARY_3P_TEST_H_
#define TESTS_BINARY_3P_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void Rep3Binary_Offline_Test();
void Rep3Binary_Open_Online_Test(const osuCrypto::CLP &cm);
void Rep3Binary_EvaluateXor_Online_Test(const osuCrypto::CLP &cm);
void Rep3Binary_EvaluateAnd_Online_Test(const osuCrypto::CLP &cm);
void Rep3Binary_EvaluateSelect_Online_Test(const osuCrypto::CLP &cm);

}    // namespace test_ringoa

#endif    // TESTS_BINARY_3P_TEST_H_
