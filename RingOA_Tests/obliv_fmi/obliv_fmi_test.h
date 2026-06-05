#ifndef TESTS_OBLIV_FMI_TEST_H_
#define TESTS_OBLIV_FMI_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void OblivFMI_DataGen_Test();
void OblivFMI_Preprocess_Test(const osuCrypto::CLP &cmd);
void OblivFMI_Online_Test(const osuCrypto::CLP &cmd);
void OblivFMI_Fsc_DataGen_Test();
void OblivFMI_Fsc_Preprocess_Test(const osuCrypto::CLP &cmd);
void OblivFMI_Fsc_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_OBLIV_FMI_TEST_H_
