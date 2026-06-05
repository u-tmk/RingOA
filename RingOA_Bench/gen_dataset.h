#ifndef BENCH_GEN_DATASET_H_
#define BENCH_GEN_DATASET_H_

#include <cryptoTools/Common/CLP.h>

namespace bench_ringoa {

void Gen_ObliviousAccess_Dataset(const osuCrypto::CLP &cmd);
void Gen_FullTextSearch_Dataset(const osuCrypto::CLP &cmd);
void Gen_RangeSearch_Dataset(const osuCrypto::CLP &cmd);

}    // namespace bench_ringoa

#endif    // BENCH_GEN_DATASET_H_
