#ifndef BENCH_RINGOA_BENCH_H_
#define BENCH_RINGOA_BENCH_H_

#include <cryptoTools/Common/CLP.h>

namespace bench_ringoa {

void RingOA_Preprocess_Bench(const osuCrypto::CLP &cmd);
void RingOA_Online_Bench(const osuCrypto::CLP &cmd);
void RingOA_Fsc_Preprocess_Bench(const osuCrypto::CLP &cmd);
void RingOA_Fsc_Online_Bench(const osuCrypto::CLP &cmd);
// void RingOA_FullDomainThenDotProduct_Bench(const osuCrypto::CLP &cmd);
// void RingOA_FullDomainThenDotProduct_UINT32_Bench(const osuCrypto::CLP &cmd);

}    // namespace bench_ringoa

#endif    // BENCH_RINGOA_BENCH_H_
