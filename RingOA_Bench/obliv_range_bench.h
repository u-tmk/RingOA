#ifndef BENCH_OBLIV_RANGE_BENCH_H_
#define BENCH_OBLIV_RANGE_BENCH_H_

#include <cryptoTools/Common/CLP.h>

namespace bench_ringoa {

void OblivRange_VAF_Preprocess_Bench(const osuCrypto::CLP &cmd);
void OblivRange_VAF_Online_Bench(const osuCrypto::CLP &cmd);
void OblivRange_Fsc_VAF_Preprocess_Bench(const osuCrypto::CLP &cmd);
void OblivRange_Fsc_VAF_Online_Bench(const osuCrypto::CLP &cmd);

}    // namespace bench_ringoa

#endif    // BENCH_OBLIV_RANGE_BENCH_H_
