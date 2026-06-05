#ifndef TESTS_NETWORK_TEST_H_
#define TESTS_NETWORK_TEST_H_

#include <cryptoTools/Common/CLP.h>

namespace test_ringoa {

void Network_TwoPartyManager_Test(const osuCrypto::CLP &cmd);
void Network_ThreePartyManager_Test(const osuCrypto::CLP &cmd);

}    // namespace test_ringoa

#endif    // TESTS_NETWORK_TEST_H_
