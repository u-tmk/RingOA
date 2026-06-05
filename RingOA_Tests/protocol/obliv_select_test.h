#ifndef TESTS_OBLIV_SELECT_TEST_H_
#define TESTS_OBLIV_SELECT_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void OblivSelect_DataGen_Test();
void OblivSelect_Preprocess_Test(const osuCrypto::CLP &cmd);
void OblivSelect_SingleBitMask_Online_Test(const osuCrypto::CLP &cmd);
void OblivSelect_ShiftedAdditive_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_OBLIV_SELECT_TEST_H_
