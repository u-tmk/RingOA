#ifndef SHARING_BINARY_3P_H_
#define SHARING_BINARY_3P_H_

#include <cryptoTools/Crypto/AES.h>

#include "RingOA/utils/network.h"
#include "share_aliases.h"
#include "share_config.h"

namespace ringoa {
namespace sharing {

class BinaryReplicatedSharing3P {
public:
    BinaryReplicatedSharing3P() = delete;
    BinaryReplicatedSharing3P(const ShareConfig &config);

    // Share data
    std::array<Rep3Share32, kThreeParties>       ShareLocal(const uint32_t &x) const;
    std::array<Rep3ShareVec32, kThreeParties>    ShareLocal(const std::vector<uint32_t> &x_vec) const;
    std::array<Rep3ShareMat32, kThreeParties>    ShareLocal(const std::vector<uint32_t> &x_mat, size_t rows, size_t cols) const;
    std::array<Rep3Share64, kThreeParties>       ShareLocal(const uint64_t &x) const;
    std::array<Rep3ShareVec64, kThreeParties>    ShareLocal(const std::vector<uint64_t> &x_vec) const;
    std::array<Rep3ShareMat64, kThreeParties>    ShareLocal(const std::vector<uint64_t> &x_mat, size_t rows, size_t cols) const;
    std::array<Rep3ShareBlock, kThreeParties>    ShareLocal(const block &x) const;
    std::array<Rep3ShareVecBlock, kThreeParties> ShareLocal(const std::vector<block> &x_vec_block) const;
    std::array<Rep3ShareMatBlock, kThreeParties> ShareLocal(const std::vector<block> &x_mat_block, size_t rows, size_t cols) const;

    // Open data
    void Open(Channels &chls, const Rep3Share64 &x_sh, uint64_t &open_x) const;
    void Open(Channels &chls, const Rep3ShareVec64 &x_vec_sh, std::vector<uint64_t> &open_x_vec) const;
    void Open(Channels &chls, const Rep3ShareMat64 &x_mat_sh, std::vector<uint64_t> &open_x_mat) const;
    void Open(Channels &chls, const Rep3ShareBlock &x_sh, block &open_x) const;
    void Open(Channels &chls, const Rep3ShareVecBlock &x_vec_sh, std::vector<block> &open_x_vec) const;
    void Open(Channels &chls, const Rep3ShareMatBlock &x_mat_sh, std::vector<block> &open_x_mat) const;

    // Randomness generation
    void Rand(Rep3Share64 &x);
    void Rand(Rep3ShareBlock &x);

    // Evaluation operations (addition, subtraction, multiplication, inner product)
    void EvaluateXor(const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, Rep3Share64 &z_sh) const;
    void EvaluateXor(const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, Rep3ShareVec64 &z_vec_sh) const;

    void EvaluateAnd(Channels &chls, const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, Rep3Share64 &z_sh);
    void EvaluateAnd(Channels &chls, const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, Rep3ShareVec64 &z_vec_sh);

    void EvaluateSelect(Channels &chls, const Rep3Share64 &x_sh, const Rep3Share64 &y_sh, const Rep3Share64 &c_sh, Rep3Share64 &z_sh);
    void EvaluateSelect(Channels &chls, const Rep3ShareVec64 &x_vec_sh, const Rep3ShareVec64 &y_vec_sh, const Rep3Share64 &c_sh, Rep3ShareVec64 &z_vec_sh);

    uint64_t GetBitSize() const;

    void SetPrfKeys(const block &prf_key_self, const block &prf_key_from_next, uint64_t buffer_size = 256);
    void ClearPrf();
    bool HasPrf() const;
    void RequirePrf(const std::string &func_name) const;

private:
    const uint64_t                    bitsize_;
    bool                              prf_ready_;
    std::array<osuCrypto::AES, 2>     prf_;
    std::array<block, 2>              prf_key_;
    uint64_t                          prf_idx_;
    uint64_t                          prf_counter_;
    uint64_t                          prf_buff_size_;
    std::array<std::vector<block>, 2> prf_buff_;

    // Internal functions
    void RefillBuffer();
};

}    // namespace sharing
}    // namespace ringoa

#endif    // SHARING_BINARY_3P_H_
