#ifndef BENCH_SOTFMI_BENCH_H_
#define BENCH_SOTFMI_BENCH_H_

#include <cryptoTools/Common/CLP.h>

namespace bench_ringoa {

void SotFMI_Preprocess_Bench(const osuCrypto::CLP &cmd);
void SotFMI_Online_Bench(const osuCrypto::CLP &cmd);

}    // namespace bench_ringoa

#endif    // BENCH_SOTFMI_BENCH_H_
