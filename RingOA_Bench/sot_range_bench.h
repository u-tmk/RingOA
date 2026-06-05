#ifndef BENCH_SOT_RANGE_BENCH_H_
#define BENCH_SOT_RANGE_BENCH_H_

#include <cryptoTools/Common/CLP.h>

namespace bench_ringoa {

void SotRange_VAF_Preprocess_Bench(const osuCrypto::CLP &cmd);
void SotRange_VAF_Online_Bench(const osuCrypto::CLP &cmd);

}    // namespace bench_ringoa

#endif    // BENCH_SOT_RANGE_BENCH_H_
