#ifndef TESTS_OBLIV_RANK_TEST_H_
#define TESTS_OBLIV_RANK_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void OblivRank_DataGen_Test();
void OblivRank_Preprocess_Test(const osuCrypto::CLP &cmd);
void OblivRank_Online_Test(const osuCrypto::CLP &cmd);
void OblivRank_Fsc_DataGen_Test();
void OblivRank_Fsc_Preprocess_Test(const osuCrypto::CLP &cmd);
void OblivRank_Fsc_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_OBLIV_RANK_TEST_H_
