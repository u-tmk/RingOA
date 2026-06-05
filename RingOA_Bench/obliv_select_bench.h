#ifndef BENCH_OBLIV_SELECT_BENCH_H_
#define BENCH_OBLIV_SELECT_BENCH_H_

#include <cryptoTools/Common/CLP.h>

namespace bench_ringoa {

void OblivSelect_ShiftedAdditive_Preprocess_Bench(const osuCrypto::CLP &cmd);
void OblivSelect_ShiftedAdditive_Online_Bench(const osuCrypto::CLP &cmd);
void OblivSelect_SingleBitMask_Preprocess_Bench(const osuCrypto::CLP &cmd);
void OblivSelect_SingleBitMask_Online_Bench(const osuCrypto::CLP &cmd);
// void OblivSelect_EvaluateFullDomainThenDotProduct_Bench(const osuCrypto::CLP &cmd);
// void OblivSelect_ComputeDotProductBlockSIMD_Bench(const osuCrypto::CLP &cmd);

}    // namespace bench_ringoa

#endif    // BENCH_OBLIV_SELECT_BENCH_H_
