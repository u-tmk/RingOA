#ifndef TESTS_OBLIV_RANGE_TEST_H_
#define TESTS_OBLIV_RANGE_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void OblivRange_DataGen_Test();
void OblivRange_Preprocess_Test(const osuCrypto::CLP &cmd);
void OblivRange_Online_Test(const osuCrypto::CLP &cmd);
void OblivRange_Fsc_DataGen_Test();
void OblivRange_Fsc_Preprocess_Test(const osuCrypto::CLP &cmd);
void OblivRange_Fsc_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_OBLIV_RANGE_TEST_H_
