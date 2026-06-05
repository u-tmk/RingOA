#include "rep3_sharing_binary_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/sharing/rep3_share_io.h"
#include "RingOA/sharing/rep3_sharing_binary.h"
#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

const std::string kCurrentPath    = ringoa::GetCurrentDirectory();
const std::string kTestBinaryPath = kCurrentPath + "/data/test/sharing/rss3/binary/";

std::string MakeKeyPath(const std::string &base_path, std::uint64_t index) {
    return base_path + "prf_k" + std::to_string(index);
}

std::pair<ringoa::block, ringoa::block> LoadPrfKeyForParty(const std::string &base_path, std::uint64_t party_id) {
    const std::uint64_t self_idx      = party_id;
    const std::uint64_t from_next_idx = (party_id + 1) % 3;

    ringoa::FileIo io(".key");
    ringoa::block  prf_key_self      = ringoa::zero_block;
    ringoa::block  prf_key_from_next = ringoa::zero_block;
    io.ReadBinary(MakeKeyPath(base_path, self_idx), prf_key_self);
    io.ReadBinary(MakeKeyPath(base_path, from_next_idx), prf_key_from_next);

    ringoa::Logger::DebugLog(LOC, "self key (k_i): " + ringoa::Format(prf_key_self));
    ringoa::Logger::DebugLog(LOC, "from_next key (k_{i+1}): " + ringoa::Format(prf_key_from_next));

    return {prf_key_self, prf_key_from_next};
}

}    // namespace

namespace test_ringoa {

using ringoa::block;
using ringoa::Format, ringoa::FormatMatrix;
using ringoa::Logger;
using ringoa::ThreePartyNetworkManager, ringoa::Channels;
using ringoa::ToString, ringoa::ToStringMatrix;
using ringoa::sharing::BinaryReplicatedSharing3P;
using ringoa::sharing::Rep3Share64, ringoa::sharing::Rep3ShareVec64, ringoa::sharing::Rep3ShareMat64;
using ringoa::sharing::Rep3ShareBlock, ringoa::sharing::Rep3ShareVecBlock, ringoa::sharing::Rep3ShareMatBlock;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::ShareConfig;

const std::vector<ShareConfig> configs = {
    ShareConfig::Custom(5),
    ShareConfig::Arith16(),
    ShareConfig::Arith32(),
};

void Rep3Binary_Offline_Test() {
    Logger::DebugLog(LOC, "Rep3Binary_Open_Offline_Test...");

    for (const auto &cfg : configs) {
        BinaryReplicatedSharing3P rss(cfg);
        uint64_t                  bitsize = cfg.arith_bits;

        uint64_t              x     = 5;
        uint64_t              y     = 4;
        std::vector<uint64_t> c     = {0, 31};
        std::vector<uint64_t> x_vec = {1, 2, 3, 4, 5};
        std::vector<uint64_t> y_vec = {5, 4, 3, 2, 1};
        uint64_t              rows = 2, cols = 3;
        std::vector<uint64_t> x_flat     = {1, 2, 3, 4, 5, 6};    // 2 rows, 3 columns
        std::vector<uint64_t> y_flat     = {3, 4, 5, 6, 7, 8};    // 2 rows, 3 columns
        block                 x_blk      = ringoa::MakeBlock(0, 0b1010);
        block                 y_blk      = ringoa::MakeBlock(0, 0b0101);
        std::vector<block>    x_vec_blk  = {ringoa::MakeBlock(0, 0b0001), ringoa::MakeBlock(0, 0b0010), ringoa::MakeBlock(0, 0b0011)};
        std::vector<block>    y_vec_blk  = {ringoa::MakeBlock(0, 0b0100), ringoa::MakeBlock(0, 0b0101), ringoa::MakeBlock(0, 0b0110)};
        std::vector<block>    x_flat_blk = {ringoa::MakeBlock(0, 0b0001), ringoa::MakeBlock(0, 0b0010), ringoa::MakeBlock(0, 0b0011),
                                            ringoa::MakeBlock(0, 0b0100), ringoa::MakeBlock(0, 0b0101), ringoa::MakeBlock(0, 0b0110)};
        std::vector<block>    y_flat_blk = {ringoa::MakeBlock(0, 0b0111), ringoa::MakeBlock(0, 0b1000), ringoa::MakeBlock(0, 0b1001),
                                            ringoa::MakeBlock(0, 0b1010), ringoa::MakeBlock(0, 0b1011), ringoa::MakeBlock(0, 0b1100)};

        std::array<Rep3Share64, 3>       x_sh          = rss.ShareLocal(x);
        std::array<Rep3Share64, 3>       y_sh          = rss.ShareLocal(y);
        std::array<Rep3ShareVec64, 3>    c_sh          = rss.ShareLocal(c);
        std::array<Rep3ShareVec64, 3>    x_vec_sh      = rss.ShareLocal(x_vec);
        std::array<Rep3ShareVec64, 3>    y_vec_sh      = rss.ShareLocal(y_vec);
        std::array<Rep3ShareMat64, 3>    x_flat_sh     = rss.ShareLocal(x_flat, rows, cols);
        std::array<Rep3ShareMat64, 3>    y_flat_sh     = rss.ShareLocal(y_flat, rows, cols);
        std::array<Rep3ShareBlock, 3>    x_blk_sh      = rss.ShareLocal(x_blk);
        std::array<Rep3ShareBlock, 3>    y_blk_sh      = rss.ShareLocal(y_blk);
        std::array<Rep3ShareVecBlock, 3> x_vec_blk_sh  = rss.ShareLocal(x_vec_blk);
        std::array<Rep3ShareVecBlock, 3> y_vec_blk_sh  = rss.ShareLocal(y_vec_blk);
        std::array<Rep3ShareMatBlock, 3> x_flat_blk_sh = rss.ShareLocal(x_flat_blk, rows, cols);
        std::array<Rep3ShareMatBlock, 3> y_flat_blk_sh = rss.ShareLocal(y_flat_blk, rows, cols);

        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_sh: " + x_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_sh: " + y_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_vec_sh: " + x_vec_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_vec_sh: " + y_vec_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_flat_sh: " + x_flat_sh[p].ToStringMatrix());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_flat_sh: " + y_flat_sh[p].ToStringMatrix());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_blk_sh: " + x_blk_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_blk_sh: " + y_blk_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_vec_blk_sh: " + x_vec_blk_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_vec_blk_sh: " + y_vec_blk_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_flat_blk_sh: " + x_flat_blk_sh[p].ToStringMatrix());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_flat_blk_sh: " + y_flat_blk_sh[p].ToStringMatrix());
        }

        const std::string x_path = kTestBinaryPath + "x_n" + ToString(bitsize);
        const std::string y_path = kTestBinaryPath + "y_n" + ToString(bitsize);
        const std::string c_path = kTestBinaryPath + "c_n" + ToString(bitsize);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Rep3ShareIo::SaveShare(x_path + "_" + ToString(p), x_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_" + ToString(p), y_sh[p]);
            Rep3ShareIo::SaveShare(c_path + "_" + ToString(p), c_sh[p]);
            Rep3ShareIo::SaveShare(x_path + "_vec_" + ToString(p), x_vec_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_vec_" + ToString(p), y_vec_sh[p]);
            Rep3ShareIo::SaveShare(x_path + "_flat_" + ToString(p), x_flat_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_flat_" + ToString(p), y_flat_sh[p]);
            Rep3ShareIo::SaveShare(x_path + "_blk_" + ToString(p), x_blk_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_blk_" + ToString(p), y_blk_sh[p]);
            Rep3ShareIo::SaveShare(x_path + "_vec_blk_" + ToString(p), x_vec_blk_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_vec_blk_" + ToString(p), y_vec_blk_sh[p]);
            Rep3ShareIo::SaveShare(x_path + "_flat_blk_" + ToString(p), x_flat_blk_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_flat_blk_" + ToString(p), y_flat_blk_sh[p]);
        }

        // Offline setup
        const ringoa::block k0 = ringoa::GlobalRng::Rand<ringoa::block>();
        const ringoa::block k1 = ringoa::GlobalRng::Rand<ringoa::block>();
        const ringoa::block k2 = ringoa::GlobalRng::Rand<ringoa::block>();
        ringoa::Logger::DebugLog(LOC, "k0: " + ringoa::Format(k0));
        ringoa::Logger::DebugLog(LOC, "k1: " + ringoa::Format(k1));
        ringoa::Logger::DebugLog(LOC, "k2: " + ringoa::Format(k2));

        ringoa::FileIo io(".key");
        io.WriteBinary(kTestBinaryPath + "prf_k0", k0);
        io.WriteBinary(kTestBinaryPath + "prf_k1", k1);
        io.WriteBinary(kTestBinaryPath + "prf_k2", k2);
    }

    Logger::DebugLog(LOC, "Rep3Binary_Open_Offline_Test - Passed");
}

void Rep3Binary_Open_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Rep3Binary_Open_Online_Test...");

    for (const auto &cfg : configs) {
        uint64_t bitsize = cfg.arith_bits;

        // Variables for opened results (all parties will write into these)
        uint64_t              open_x = 0;
        std::vector<uint64_t> open_x_vec;
        std::vector<uint64_t> open_x_flat;
        block                 open_x_blk;
        std::vector<block>    open_x_vec_blk;
        std::vector<block>    open_x_flat_blk;

        const std::string x_path = kTestBinaryPath + "x_n" + ToString(bitsize);

        // Helper that returns a task lambda for a given party_id
        auto MakeTask = [&](int party_id) {
            // Capture bitsize, paths, Rep3ShareIo:: and references to opened-result variables
            return [=, &open_x,
                    &open_x_vec,
                    &open_x_flat,
                    &open_x_blk,
                    &open_x_vec_blk,
                    &open_x_flat_blk](osuCrypto::Channel &chl_next, osuCrypto::Channel &chl_prev) {
                // (1) Set up the binary replicated-sharing object and channels
                BinaryReplicatedSharing3P rss(cfg);
                Channels                  chls(party_id, chl_prev, chl_next);

                // (2) Prepare local share variables for all types
                Rep3Share64       x_sh;
                Rep3ShareVec64    x_vec_sh;
                Rep3ShareMat64    x_flat_sh;
                Rep3ShareBlock    x_blk_sh;
                Rep3ShareVecBlock x_vec_blk_sh;
                Rep3ShareMatBlock x_flat_blk_sh;

                // (3) Construct file names and load shares
                Rep3ShareIo::LoadShare(x_path + "_" + ToString(party_id), x_sh);
                Rep3ShareIo::LoadShare(x_path + "_vec_" + ToString(party_id), x_vec_sh);
                Rep3ShareIo::LoadShare(x_path + "_flat_" + ToString(party_id), x_flat_sh);
                Rep3ShareIo::LoadShare(x_path + "_blk_" + ToString(party_id), x_blk_sh);
                Rep3ShareIo::LoadShare(x_path + "_vec_blk_" + ToString(party_id), x_vec_blk_sh);
                Rep3ShareIo::LoadShare(x_path + "_flat_blk_" + ToString(party_id), x_flat_blk_sh);

                // (4) Open all shares and write the results into the common variables
                rss.Open(chls, x_sh, open_x);
                rss.Open(chls, x_vec_sh, open_x_vec);
                rss.Open(chls, x_flat_sh, open_x_flat);
                rss.Open(chls, x_blk_sh, open_x_blk);
                rss.Open(chls, x_vec_blk_sh, open_x_vec_blk);
                rss.Open(chls, x_flat_blk_sh, open_x_flat_blk);
            };
        };

        // Create tasks for parties 0, 1, and 2
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        // Configure network based on party ID (CLI/env) and wait for completion
        ThreePartyNetworkManager net_mgr;
        int                      party_id = cmd.getOr<int>("party", -1);
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        // At this point, all parties have the same opened values
        Logger::DebugLog(LOC, "open_x:         " + ToString(open_x));
        Logger::DebugLog(LOC, "open_x_vec:     " + ToString(open_x_vec));
        Logger::DebugLog(LOC, "open_x_flat:    " + ToStringMatrix(open_x_flat, 2, 3));
        Logger::DebugLog(LOC, "open_x_blk:     " + Format(open_x_blk));
        Logger::DebugLog(LOC, "open_x_vec_blk: " + Format(open_x_vec_blk));
        Logger::DebugLog(LOC, "open_x_flat_blk:" + FormatMatrix(open_x_flat_blk, 2, 3));

        // Validate the opened values
        if (open_x != 5)
            throw osuCrypto::UnitTestFail("Open protocol failed: open_x != 5");

        if (open_x_vec != std::vector<uint64_t>({1, 2, 3, 4, 5}))
            throw osuCrypto::UnitTestFail("Open protocol failed: open_x_vec mismatch");

        if (open_x_flat != std::vector<uint64_t>({1, 2, 3, 4, 5, 6}))
            throw osuCrypto::UnitTestFail("Open protocol failed: open_x_flat mismatch");

        if (open_x_blk != ringoa::MakeBlock(0, 0b1010))
            throw osuCrypto::UnitTestFail("Open protocol failed: open_x_blk mismatch");

        if (open_x_vec_blk != std::vector<block>({ringoa::MakeBlock(0, 0b0001),
                                                  ringoa::MakeBlock(0, 0b0010),
                                                  ringoa::MakeBlock(0, 0b0011)}))
            throw osuCrypto::UnitTestFail("Open protocol failed: open_x_vec_blk mismatch");

        if (open_x_flat_blk != std::vector<block>({ringoa::MakeBlock(0, 0b0001),
                                                   ringoa::MakeBlock(0, 0b0010),
                                                   ringoa::MakeBlock(0, 0b0011),
                                                   ringoa::MakeBlock(0, 0b0100),
                                                   ringoa::MakeBlock(0, 0b0101),
                                                   ringoa::MakeBlock(0, 0b0110)}))
            throw osuCrypto::UnitTestFail("Open protocol failed: open_x_flat_blk mismatch");
    }

    Logger::DebugLog(LOC, "Rep3Binary_Open_Online_Test - Passed");
}

void Rep3Binary_EvaluateXor_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Rep3Binary_EvaluateXor_Online_Test...");

    for (const auto &cfg : configs) {
        uint64_t bitsize = cfg.arith_bits;

        // Variables for opened results (all parties will write into these)
        uint64_t              open_z = 0;
        std::vector<uint64_t> open_z_vec;

        const std::string x_path = kTestBinaryPath + "x_n" + ToString(bitsize);
        const std::string y_path = kTestBinaryPath + "y_n" + ToString(bitsize);

        // Helper that returns a task lambda for a given party_id
        auto MakeTask = [&](int party_id) {
            // Capture bitsize, paths, Rep3ShareIo:: and references to opened-result variables
            return [=, &open_z, &open_z_vec](osuCrypto::Channel &chl_next,
                                             osuCrypto::Channel &chl_prev) {
                // (1) Set up the binary replicated-sharing object and channels
                BinaryReplicatedSharing3P rss(cfg);
                Channels                  chls(party_id, chl_prev, chl_next);

                // (2) Prepare local share variables for inputs and outputs
                Rep3Share64    x_sh, y_sh, z_sh;
                Rep3ShareVec64 x_vec_sh, y_vec_sh, z_vec_sh;

                // (3) Construct file names and load shares
                Rep3ShareIo::LoadShare(x_path + "_" + ToString(party_id), x_sh);
                Rep3ShareIo::LoadShare(y_path + "_" + ToString(party_id), y_sh);
                Rep3ShareIo::LoadShare(x_path + "_vec_" + ToString(party_id), x_vec_sh);
                Rep3ShareIo::LoadShare(y_path + "_vec_" + ToString(party_id), y_vec_sh);

                // (4) Perform secure XOR on both scalar and vector shares
                rss.EvaluateXor(x_sh, y_sh, z_sh);
                rss.EvaluateXor(x_vec_sh, y_vec_sh, z_vec_sh);

                // (5) Log each party’s local z shares for debugging
                Logger::DebugLog(LOC, "Party " + ToString(party_id) +
                                          " z: " + z_sh.ToString());
                Logger::DebugLog(LOC, "Party " + ToString(party_id) +
                                          " z_vec: " + z_vec_sh.ToString());

                // (6) Open the shares and write the results into the same variables for all parties
                rss.Open(chls, z_sh, open_z);
                rss.Open(chls, z_vec_sh, open_z_vec);
            };
        };

        // Create tasks for parties 0, 1, and 2
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        // Configure network based on party ID (CLI/env) and wait for completion
        ThreePartyNetworkManager net_mgr;
        int                      party_id = cmd.getOr<int>("party", -1);
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        // At this point, all parties have the same open_z and open_z_vec
        Logger::DebugLog(LOC, "open_z:     " + ToString(open_z));
        Logger::DebugLog(LOC, "open_z_vec: " + ToString(open_z_vec));

        // Validate the opened values
        if (open_z != (0x5 ^ 0x4))
            throw osuCrypto::UnitTestFail("Binary protocol failed: open_z != (5 ^ 4)");

        if (open_z_vec != std::vector<uint64_t>({(0x1 ^ 0x5),
                                                 (0x2 ^ 0x4),
                                                 (0x3 ^ 0x3),
                                                 (0x4 ^ 0x2),
                                                 (0x5 ^ 0x1)}))
            throw osuCrypto::UnitTestFail("Binary protocol failed: open_z_vec mismatch");
    }

    Logger::DebugLog(LOC, "Rep3Binary_EvaluateXor_Online_Test - Passed");
}

void Rep3Binary_EvaluateAnd_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Rep3Binary_EvaluateAnd_Online_Test...");

    for (const auto &cfg : configs) {
        uint64_t bitsize = cfg.arith_bits;

        // Variables for opened results (all parties will write into these)
        uint64_t              open_z = 0;
        std::vector<uint64_t> open_z_vec;

        const std::string x_path = kTestBinaryPath + "x_n" + ToString(bitsize);
        const std::string y_path = kTestBinaryPath + "y_n" + ToString(bitsize);

        // Helper that returns a task lambda for a given party_id
        auto MakeTask = [&](int party_id) {
            // Capture bitsize, paths, Rep3ShareIo:: and references to opened-result variables
            return [=, &open_z, &open_z_vec](osuCrypto::Channel &chl_next,
                                             osuCrypto::Channel &chl_prev) {
                // (1) Set up the binary replicated-sharing object and channels
                BinaryReplicatedSharing3P rss(cfg);
                Channels                  chls(party_id, chl_prev, chl_next);

                // (2) Prepare local share variables for inputs and outputs
                Rep3Share64    x_sh, y_sh, z_sh;
                Rep3ShareVec64 x_vec_sh, y_vec_sh, z_vec_sh;

                // (3) Construct file names and load shares
                Rep3ShareIo::LoadShare(x_path + "_" + ToString(party_id), x_sh);
                Rep3ShareIo::LoadShare(y_path + "_" + ToString(party_id), y_sh);
                Rep3ShareIo::LoadShare(x_path + "_vec_" + ToString(party_id), x_vec_sh);
                Rep3ShareIo::LoadShare(y_path + "_vec_" + ToString(party_id), y_vec_sh);

                // (4) Setup PRF keys for secure AND
                auto [k_self, k_from_next] = LoadPrfKeyForParty(kTestBinaryPath, party_id);
                rss.SetPrfKeys(k_self, k_from_next);

                // (5) Perform secure AND on both scalar and vector shares
                rss.EvaluateAnd(chls, x_sh, y_sh, z_sh);
                rss.EvaluateAnd(chls, x_vec_sh, y_vec_sh, z_vec_sh);

                // (6) Log each party’s local z shares for debugging
                Logger::DebugLog(LOC, "Party " + ToString(party_id) +
                                          " z: " + z_sh.ToString());
                Logger::DebugLog(LOC, "Party " + ToString(party_id) +
                                          " z_vec: " + z_vec_sh.ToString());

                // (7) Open the shares and write the results into the same variables for all parties
                rss.Open(chls, z_sh, open_z);
                rss.Open(chls, z_vec_sh, open_z_vec);
            };
        };

        // Create tasks for parties 0, 1, and 2
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        // Configure network based on party ID (CLI/env) and wait for completion
        ThreePartyNetworkManager net_mgr;
        int                      party_id = cmd.getOr<int>("party", -1);
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        // At this point, all parties have the same open_z and open_z_vec
        Logger::DebugLog(LOC, "open_z:     " + ToString(open_z));
        Logger::DebugLog(LOC, "open_z_vec: " + ToString(open_z_vec));

        // Validate the opened values
        if (open_z != (5 & 4))
            throw osuCrypto::UnitTestFail("Binary protocol failed: open_z != (5 & 4)");

        if (open_z_vec != std::vector<uint64_t>({(1 & 5),
                                                 (2 & 4),
                                                 (3 & 3),
                                                 (4 & 2),
                                                 (5 & 1)}))
            throw osuCrypto::UnitTestFail("Binary protocol failed: open_z_vec mismatch");
    }

    Logger::DebugLog(LOC, "Rep3Binary_EvaluateAnd_Online_Test - Passed");
}

void Rep3Binary_EvaluateSelect_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Rep3Binary_EvaluateSelect_Online_Test...");

    for (const auto &cfg : configs) {
        uint64_t bitsize = cfg.arith_bits;

        // Variables for opened results (all parties will write into these)
        uint64_t open_z0 = 0;
        uint64_t open_z1 = 0;

        const std::string x_path = kTestBinaryPath + "x_n" + ToString(bitsize);
        const std::string y_path = kTestBinaryPath + "y_n" + ToString(bitsize);
        const std::string c_path = kTestBinaryPath + "c_n" + ToString(bitsize);

        // Helper that returns a task lambda for a given party_id
        auto MakeTask = [&](int party_id) {
            // Capture bitsize, paths, Rep3ShareIo:: and references to opened-result variables
            return [=, &open_z0, &open_z1](osuCrypto::Channel &chl_next,
                                           osuCrypto::Channel &chl_prev) {
                // (1) Set up the binary replicated-sharing object and channels
                BinaryReplicatedSharing3P rss(cfg);
                Channels                  chls(party_id, chl_prev, chl_next);

                // (2) Prepare local share variables for inputs, conditions, and outputs
                Rep3Share64    x_sh, y_sh, z0_sh, z1_sh;
                Rep3ShareVec64 c_sh;

                // (3) Construct file names and load shares
                Rep3ShareIo::LoadShare(x_path + "_" + ToString(party_id), x_sh);
                Rep3ShareIo::LoadShare(y_path + "_" + ToString(party_id), y_sh);
                Rep3ShareIo::LoadShare(c_path + "_" + ToString(party_id), c_sh);

                // (4) Setup PRF keys for secure Select
                auto [k_self, k_from_next] = LoadPrfKeyForParty(kTestBinaryPath, party_id);
                rss.SetPrfKeys(k_self, k_from_next);

                // (5) Perform secure Select: two outputs based on two bits of c_sh
                rss.EvaluateSelect(chls, x_sh, y_sh, c_sh.At(0), z0_sh);
                rss.EvaluateSelect(chls, x_sh, y_sh, c_sh.At(1), z1_sh);

                // (6) Log each party’s local z0 and z1 shares for debugging
                Logger::DebugLog(LOC, "Party " + ToString(party_id) +
                                          " z0: " + z0_sh.ToString());
                Logger::DebugLog(LOC, "Party " + ToString(party_id) +
                                          " z1: " + z1_sh.ToString());

                // (7) Open the shares and write the results into the same variables for all parties
                rss.Open(chls, z0_sh, open_z0);
                rss.Open(chls, z1_sh, open_z1);
            };
        };

        // Create tasks for parties 0, 1, and 2
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        // Configure network based on party ID (CLI/env) and wait for completion
        ThreePartyNetworkManager net_mgr;
        int                      party_id = cmd.getOr<int>("party", -1);
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        // At this point, all parties have the same open_z0 and open_z1
        Logger::DebugLog(LOC, "open_z0: " + ToString(open_z0));
        Logger::DebugLog(LOC, "open_z1: " + ToString(open_z1));

        // Validate the opened values
        if (open_z0 != 5 || open_z1 != 4)
            throw osuCrypto::UnitTestFail("Binary protocol failed: open_z0/open_z1 mismatch");
    }

    Logger::DebugLog(LOC, "Rep3Binary_EvaluateSelect_Online_Test - Passed");
}

}    // namespace test_ringoa
