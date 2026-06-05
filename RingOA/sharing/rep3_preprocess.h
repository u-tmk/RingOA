#ifndef SHARING_REP3_PREPROCESS_H_
#define SHARING_REP3_PREPROCESS_H_

#include <vector>

#include "RingOA/utils/block.h"
#include "RingOA/utils/network.h"

namespace ringoa {
namespace sharing {

struct Rep3PreprocessMsg {
    block prf_key{};

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer);
    void   Deserialize(const std::vector<uint8_t> &buffer, size_t &offset);
};

struct Rep3PreprocessData {
    block prf_key_self{};
    block prf_key_next{};

    Rep3PreprocessData() = default;
    Rep3PreprocessData(const block &prf_self, const block &prf_next)
        : prf_key_self(prf_self),
          prf_key_next(prf_next) {
    }

    static Rep3PreprocessData FromMessage(
        Rep3PreprocessMsg &&from_next,
        const block        &prf_key_self_in);

    size_t CalculateSerializedSize() const;
    void   Serialize(std::vector<uint8_t> &buffer) const;
    void   Deserialize(const std::vector<uint8_t> &buffer);
    void   Deserialize(const std::vector<uint8_t> &buffer, size_t &offset);
};

struct Rep3PrfOutgoing {
    block             prf_key_self{};
    Rep3PreprocessMsg to_prev{};
};

Rep3PrfOutgoing    MakeRep3PrfOutgoing();
Rep3PreprocessMsg  ExchangeRep3PrfMsg(Channels &chls, const Rep3PreprocessMsg &to_prev);
Rep3PreprocessData BuildRep3PrfData(block prf_key_self, Rep3PreprocessMsg &&from_next);
Rep3PreprocessData PreprocessRep3PrfKeys(Channels &chls);

void SaveRep3PreprocessDataToFile(const std::string &path, const Rep3PreprocessData &data);
void LoadRep3PreprocessDataFromFile(const std::string &path, Rep3PreprocessData &data);

}    // namespace sharing
}    // namespace ringoa

#endif    // SHARING_REP3_PREPROCESS_H_
