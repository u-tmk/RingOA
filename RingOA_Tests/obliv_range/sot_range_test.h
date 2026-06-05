#ifndef TESTS_SOT_RANGE_TEST_H_
#define TESTS_SOT_RANGE_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void SotRange_DataGen_Test();
void SotRange_Preprocess_Test(const osuCrypto::CLP &cmd);
void SotRange_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_SOT_RANGE_TEST_H_
