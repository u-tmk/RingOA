#include "rep3_sharing_ring_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/sharing/rep3_share_io.h"
#include "RingOA/sharing/rep3_sharing_ring.h"
#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/rng.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

const std::string kCurrentPath      = ringoa::GetCurrentDirectory();
const std::string kTestAdditivePath = kCurrentPath + "/data/test/sharing/rss3/ring/";

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

using ringoa::Logger, ringoa::ToString, ringoa::ToStringMatrix;
using ringoa::ThreePartyNetworkManager, ringoa::Channels;
using ringoa::sharing::Rep3Share64, ringoa::sharing::Rep3ShareVec64, ringoa::sharing::Rep3ShareMat64;
using ringoa::sharing::Rep3ShareIo;
using ringoa::sharing::ReplicatedSharing3P;
using ringoa::sharing::ShareConfig;

const std::vector<ShareConfig> configs = {
    ShareConfig::Custom(5),
    ShareConfig::Arith16(),
    ShareConfig::Arith32(),
};

void Rep3_Offline_Test() {
    Logger::DebugLog(LOC, "Rep3_Open_Offline_Test...");

    for (const auto &cfg : configs) {
        ReplicatedSharing3P rss(cfg);
        uint64_t            bitsize = cfg.arith_bits;

        uint64_t              x     = 5;
        uint64_t              y     = 4;
        std::vector<uint64_t> x_vec = {1, 2, 3, 4, 5};
        std::vector<uint64_t> y_vec = {5, 4, 3, 2, 1};
        uint64_t              rows = 2, cols = 3;
        std::vector<uint64_t> x_flat = {1, 2, 3, 4, 5, 6};    // 2 rows, 3 columns
        std::vector<uint64_t> y_flat = {3, 4, 5, 6, 7, 8};    // 2 rows, 3 columns

        std::array<Rep3Share64, 3>    x_sh      = rss.ShareLocal(x);
        std::array<Rep3Share64, 3>    y_sh      = rss.ShareLocal(y);
        std::array<Rep3ShareVec64, 3> x_vec_sh  = rss.ShareLocal(x_vec);
        std::array<Rep3ShareVec64, 3> y_vec_sh  = rss.ShareLocal(y_vec);
        std::array<Rep3ShareMat64, 3> x_flat_sh = rss.ShareLocal(x_flat, rows, cols);
        std::array<Rep3ShareMat64, 3> y_flat_sh = rss.ShareLocal(y_flat, rows, cols);

        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_sh: " + x_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_sh: " + y_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_vec_sh: " + x_vec_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_vec_sh: " + y_vec_sh[p].ToString());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " x_flat_sh: " + x_flat_sh[p].ToStringMatrix());
            Logger::DebugLog(LOC, "Party " + ToString(p) + " y_flat_sh: " + y_flat_sh[p].ToStringMatrix());
        }

        const std::string x_path = kTestAdditivePath + "x_n" + ToString(bitsize);
        const std::string y_path = kTestAdditivePath + "y_n" + ToString(bitsize);
        for (size_t p = 0; p < ringoa::sharing::kThreeParties; ++p) {
            Rep3ShareIo::SaveShare(x_path + "_" + ToString(p), x_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_" + ToString(p), y_sh[p]);
            Rep3ShareIo::SaveShare(x_path + "_vec_" + ToString(p), x_vec_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_vec_" + ToString(p), y_vec_sh[p]);
            Rep3ShareIo::SaveShare(x_path + "_mat_" + ToString(p), x_flat_sh[p]);
            Rep3ShareIo::SaveShare(y_path + "_mat_" + ToString(p), y_flat_sh[p]);
        }

        // Offline setup
        const ringoa::block k0 = ringoa::GlobalRng::Rand<ringoa::block>();
        const ringoa::block k1 = ringoa::GlobalRng::Rand<ringoa::block>();
        const ringoa::block k2 = ringoa::GlobalRng::Rand<ringoa::block>();
        ringoa::Logger::DebugLog(LOC, "k0: " + ringoa::Format(k0));
        ringoa::Logger::DebugLog(LOC, "k1: " + ringoa::Format(k1));
        ringoa::Logger::DebugLog(LOC, "k2: " + ringoa::Format(k2));

        ringoa::FileIo io(".key");
        io.WriteBinary(kTestAdditivePath + "prf_k0", k0);
        io.WriteBinary(kTestAdditivePath + "prf_k1", k1);
        io.WriteBinary(kTestAdditivePath + "prf_k2", k2);
    }

    Logger::DebugLog(LOC, "Rep3_Open_Offline_Test - Passed");
}

void Rep3_Open_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Rep3_Open_Online_Test...");

    for (const auto &cfg : configs) {
        uint64_t bitsize = cfg.arith_bits;

        uint64_t              open_x;
        std::vector<uint64_t> open_x_vec;
        std::vector<uint64_t> open_x_flat;
        const std::string     x_path = kTestAdditivePath + "x_n" + ToString(bitsize);

        // Define the task for each party
        auto MakeTask = [&](int party_id) {
            return [=, &open_x, &open_x_vec, &open_x_flat](osuCrypto::Channel &chl_next,
                                                           osuCrypto::Channel &chl_prev) {
                // (1) Set up the sharing object and channels
                ReplicatedSharing3P rss(cfg);
                Channels            chls(party_id, chl_prev, chl_next);

                // (2) Prepare local variables for each party (different variables for each type)
                Rep3Share64    x_sh;
                Rep3ShareVec64 x_vec_sh;
                Rep3ShareMat64 x_flat_sh;

                // (3) Construct file names and load shares
                Rep3ShareIo::LoadShare(x_path + "_" + ToString(party_id), x_sh);
                Rep3ShareIo::LoadShare(x_path + "_vec_" + ToString(party_id), x_vec_sh);
                Rep3ShareIo::LoadShare(x_path + "_mat_" + ToString(party_id), x_flat_sh);

                // (4) Call Open and write the disclosure results to the same variables for all parties
                rss.Open(chls, x_sh, open_x);
                rss.Open(chls, x_vec_sh, open_x_vec);
                rss.Open(chls, x_flat_sh, open_x_flat);
            };
        };

        // Create tasks for each party
        auto task_p0 = MakeTask(0);
        auto task_p1 = MakeTask(1);
        auto task_p2 = MakeTask(2);

        // Configure network based on party ID and wait for completion
        ThreePartyNetworkManager net_mgr;
        int                      party_id = cmd.getOr<int>("party", -1);
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        Logger::DebugLog(LOC, "open_x: " + ToString(open_x));
        Logger::DebugLog(LOC, "open_x_vec: " + ToString(open_x_vec));
        Logger::DebugLog(LOC, "open_x_flat: " + ToStringMatrix(open_x_flat, 2, 3));

        // Validate the opened value
        if (open_x != 5)
            throw osuCrypto::UnitTestFail("Open protocol failed.");
        if (open_x_vec != std::vector<uint64_t>({1, 2, 3, 4, 5}))
            throw osuCrypto::UnitTestFail("Open protocol failed.");
        if (open_x_flat != std::vector<uint64_t>({1, 2, 3, 4, 5, 6}))
            throw osuCrypto::UnitTestFail("Open protocol failed.");
    }

    Logger::DebugLog(LOC, "Rep3_Open_Online_Test - Passed");
}

void Rep3_EvaluateAdd_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Rep3_EvaluateAdd_Online_Test...");

    for (const auto &cfg : configs) {
        uint64_t bitsize = cfg.arith_bits;

        // Variables to hold the opened results for all parties
        uint64_t              open_z = 0;
        std::vector<uint64_t> open_z_vec;

        const std::string x_path = kTestAdditivePath + "x_n" + ToString(bitsize);
        const std::string y_path = kTestAdditivePath + "y_n" + ToString(bitsize);

        // Helper that returns a task lambda for a given party_id
        auto MakeTask = [&](int party_id) {
            // Capture bitsize, paths, Rep3ShareIo:: and opened-result references
            return [=, &open_z, &open_z_vec](osuCrypto::Channel &chl_next,
                                             osuCrypto::Channel &chl_prev) {
                // (1) Set up the sharing object and channels
                ReplicatedSharing3P rss(cfg);
                Channels            chls(party_id, chl_prev, chl_next);

                // (2) Prepare local share variables for inputs and outputs
                Rep3Share64    x_sh, y_sh, z_sh;
                Rep3ShareVec64 x_vec_sh, y_vec_sh, z_vec_sh;

                // (3) Construct file names and load shares
                Rep3ShareIo::LoadShare(x_path + "_" + ToString(party_id), x_sh);
                Rep3ShareIo::LoadShare(y_path + "_" + ToString(party_id), y_sh);
                Rep3ShareIo::LoadShare(x_path + "_vec_" + ToString(party_id), x_vec_sh);
                Rep3ShareIo::LoadShare(y_path + "_vec_" + ToString(party_id), y_vec_sh);

                // (4) Perform the additive evaluation on both scalar and vector
                rss.EvaluateAdd(x_sh, y_sh, z_sh);
                rss.EvaluateAdd(x_vec_sh, y_vec_sh, z_vec_sh);

                // (5) Log each party's z and z_vec for debugging
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

        // Configure network based on party ID and wait for completion
        ThreePartyNetworkManager net_mgr;
        int                      party_id = cmd.getOr<int>("party", -1);
        net_mgr.AutoConfigure(party_id, task_p0, task_p1, task_p2);
        net_mgr.WaitForCompletion();

        // At this point, all parties have the same open_z and open_z_vec
        Logger::DebugLog(LOC, "open_z:     " + ToString(open_z));
        Logger::DebugLog(LOC, "open_z_vec: " + ToString(open_z_vec));

        // Validate the opened values
        if (open_z != 9)
            throw osuCrypto::UnitTestFail("Additive protocol failed: open_z != 9");
        if (open_z_vec != std::vector<uint64_t>({6, 6, 6, 6, 6}))
            throw osuCrypto::UnitTestFail("Additive protocol failed: open_z_vec mismatch");
    }

    Logger::DebugLog(LOC, "Rep3_EvaluateAdd_Online_Test - Passed");
}

void Rep3_EvaluateMult_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Rep3_EvaluateMult_Online_Test...");

    for (const auto &cfg : configs) {
        uint64_t bitsize = cfg.arith_bits;

        // Variables for opened results (all parties will write into these)
        uint64_t              open_z = 0;
        std::vector<uint64_t> open_z_vec;

        const std::string x_path = kTestAdditivePath + "x_n" + ToString(bitsize);
        const std::string y_path = kTestAdditivePath + "y_n" + ToString(bitsize);

        // Helper that returns a task lambda for a given party_id
        auto MakeTask = [&](int party_id) {
            // Capture bitsize, paths, Rep3ShareIo:: and references to opened-result variables
            return [=, &open_z, &open_z_vec](osuCrypto::Channel &chl_next,
                                             osuCrypto::Channel &chl_prev) {
                // (1) Set up the replicated-sharing object and channels
                ReplicatedSharing3P rss(cfg);
                Channels            chls(party_id, chl_prev, chl_next);

                // (2) Prepare local share variables for inputs and outputs
                Rep3Share64    x_sh, y_sh, z_sh;
                Rep3ShareVec64 x_vec_sh, y_vec_sh, z_vec_sh;

                // (3) Construct file names and load shares
                Rep3ShareIo::LoadShare(x_path + "_" + ToString(party_id), x_sh);
                Rep3ShareIo::LoadShare(y_path + "_" + ToString(party_id), y_sh);
                Rep3ShareIo::LoadShare(x_path + "_vec_" + ToString(party_id), x_vec_sh);
                Rep3ShareIo::LoadShare(y_path + "_vec_" + ToString(party_id), y_vec_sh);

                // (4) Setup PRF keys for secure multiplication
                auto [k_self, k_from_next] = LoadPrfKeyForParty(kTestAdditivePath, party_id);
                rss.SetPrfKeys(k_self, k_from_next);

                // (5) Perform secure multiplication on both scalar and vector shares
                rss.EvaluateMult(chls, x_sh, y_sh, z_sh);
                rss.EvaluateMult(chls, x_vec_sh, y_vec_sh, z_vec_sh);

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
        if (open_z != 20)
            throw osuCrypto::UnitTestFail("Additive protocol failed: open_z != 20");
        if (open_z_vec != std::vector<uint64_t>({5, 8, 9, 8, 5}))
            throw osuCrypto::UnitTestFail("Additive protocol failed: open_z_vec mismatch");
    }

    Logger::DebugLog(LOC, "Rep3_EvaluateMult_Online_Test - Passed");
}

void Rep3_EvaluateInnerProduct_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Rep3_EvaluateInnerProduct_Online_Test...");

    for (const auto &cfg : configs) {
        uint64_t bitsize = cfg.arith_bits;

        // Variable for opened result (all parties will write into this)
        uint64_t open_z = 0;

        const std::string x_path = kTestAdditivePath + "x_n" + ToString(bitsize);
        const std::string y_path = kTestAdditivePath + "y_n" + ToString(bitsize);

        // Helper that returns a task lambda for a given party_id
        auto MakeTask = [&](int party_id) {
            // Capture bitsize, paths, Rep3ShareIo:: and reference to opened-result variable
            return [=, &open_z](osuCrypto::Channel &chl_next,
                                osuCrypto::Channel &chl_prev) {
                // (1) Set up the replicated-sharing object and channels
                ReplicatedSharing3P rss(cfg);
                Channels            chls(party_id, chl_prev, chl_next);

                // (2) Prepare local share variables for vector inputs and output
                Rep3Share64    z_sh;
                Rep3ShareVec64 x_vec_sh, y_vec_sh;

                // (3) Construct file names and load vector shares
                Rep3ShareIo::LoadShare(x_path + "_vec_" + ToString(party_id), x_vec_sh);
                Rep3ShareIo::LoadShare(y_path + "_vec_" + ToString(party_id), y_vec_sh);

                // (4) Setup PRF keys for secure inner product
                auto [k_self, k_from_next] = LoadPrfKeyForParty(kTestAdditivePath, party_id);
                rss.SetPrfKeys(k_self, k_from_next);

                // (5) Perform secure inner product
                rss.EvaluateInnerProduct(chls, x_vec_sh, y_vec_sh, z_sh);

                // (6) Log each party’s local z share for debugging
                Logger::DebugLog(LOC, "Party " + ToString(party_id) +
                                          " z: " + z_sh.ToString());

                // (7) Open the share and write the result into the same variable for all parties
                rss.Open(chls, z_sh, open_z);
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

        // At this point, all parties have the same open_z
        Logger::DebugLog(LOC, "open_z: " + ToString(open_z));

        // Validate the opened value
        if (open_z != ringoa::Mod2N(35, bitsize))
            throw osuCrypto::UnitTestFail(
                "Additive protocol failed: open_z != Mod2N(35, " + ToString(bitsize) + ")");
    }

    Logger::DebugLog(LOC, "Rep3_EvaluateInnerProduct_Online_Test - Passed");
}

}    // namespace test_ringoa
