#ifndef BENCH_SHARED_OT_BENCH_H_
#define BENCH_SHARED_OT_BENCH_H_

#include <cryptoTools/Common/CLP.h>

namespace bench_ringoa {

void SharedOT_Preprocess_Bench(const osuCrypto::CLP &cmd);
void SharedOT_Online_Bench(const osuCrypto::CLP &cmd);

}    // namespace bench_ringoa

#endif    // BENCH_SHARED_OT_BENCH_H_
