#ifndef TESTS_SOTFMI_TEST_H_
#define TESTS_SOTFMI_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void SotFMI_DataGen_Test();
void SotFMI_Preprocess_Test(const osuCrypto::CLP &cmd);
void SotFMI_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_SOTFMI_TEST_H_
