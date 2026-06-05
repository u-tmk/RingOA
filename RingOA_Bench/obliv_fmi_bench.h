#ifndef BENCH_OblivFMI_BENCH_H_
#define BENCH_OblivFMI_BENCH_H_

#include <cryptoTools/Common/CLP.h>

namespace bench_ringoa {

void OblivFMI_Preprocess_Bench(const osuCrypto::CLP &cmd);
void OblivFMI_Online_Bench(const osuCrypto::CLP &cmd);
void OblivFMI_Fsc_Preprocess_Bench(const osuCrypto::CLP &cmd);
void OblivFMI_Fsc_Online_Bench(const osuCrypto::CLP &cmd);

}    // namespace bench_ringoa

#endif    // BENCH_OblivFMI_BENCH_H_
