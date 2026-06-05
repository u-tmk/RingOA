#ifndef TESTS_RINGOA_TEST_H_
#define TESTS_RINGOA_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void RingOa_DataGen_Test();
void RingOa_Preprocess_Test(const osuCrypto::CLP &cmd);
void RingOa_Online_Test(const osuCrypto::CLP &cmd);
void RingOa_Fsc_DataGen_Test();
void RingOa_Fsc_Preprocess_Test(const osuCrypto::CLP &cmd);
void RingOa_Fsc_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_RINGOA_TEST_H_
