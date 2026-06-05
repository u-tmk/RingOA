#ifndef TESTS_SOT_RANK_TEST_H_
#define TESTS_SOT_RANK_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void SotRank_DataGen_Test();
void SotRank_Preprocess_Test(const osuCrypto::CLP &cmd);
void SotRank_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_SOT_RANK_TEST_H_
