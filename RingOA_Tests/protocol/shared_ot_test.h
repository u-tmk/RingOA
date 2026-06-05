#ifndef TESTS_SHARED_OT_TEST_H_
#define TESTS_SHARED_OT_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void SharedOT_DataGen_Test();
void SharedOT_Preprocess_Test(const osuCrypto::CLP &cmd);
void SharedOT_Online_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_SHARED_OT_TEST_H_
