#include "sharing_2p_ring_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/sharing/beaver_triples_gen.h"
#include "RingOA/sharing/beaver_triples_io.h"
#include "RingOA/sharing/sharing_2p_ring.h"
#include "RingOA/utils/file_io.h"
#include "RingOA/utils/logger.h"
#include "RingOA/utils/network.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

const std::string kCurrentPath      = ringoa::GetCurrentDirectory();
const std::string kTestAdditivePath = kCurrentPath + "/data/test/sharing/ass2/";

}    // namespace

namespace test_ringoa {

using ringoa::FileIo;
using ringoa::Logger;
using ringoa::ToString;
using ringoa::TwoPartyNetworkManager;
using ringoa::sharing::AdditiveSharing2P;
using ringoa::sharing::ShareConfig;

const std::vector<ShareConfig> configs = {
    ShareConfig::Custom(5),
    ShareConfig::Arith16(),
    ShareConfig::Arith32(),
};

void Additive2P_Evaluate_Offline_Test() {
    Logger::DebugLog(LOC, "Additive2P_Evaluate_Offline_Test...");

    for (const auto &cfg : configs) {
        AdditiveSharing2P ss(cfg);
        uint64_t          bitsize = cfg.arith_bits;
        FileIo            file_io;

        // Generate input
        uint64_t                x     = 5;
        uint64_t                y     = 4;
        std::array<uint64_t, 2> x_arr = {1, 2};
        std::array<uint64_t, 2> y_arr = {5, 4};
        std::vector<uint64_t>   x_vec = {1, 2, 3, 4, 5};
        std::vector<uint64_t>   y_vec = {5, 4, 3, 2, 1};

        uint64_t                cond     = 1;
        std::array<uint64_t, 2> cond_arr = {1, 0};
        std::vector<uint64_t>   cond_vec = {1, 0, 1, 0, 1};

        // Generate shares
        std::pair<uint64_t, uint64_t>                               x_sh        = ss.Share(x);
        std::pair<uint64_t, uint64_t>                               y_sh        = ss.Share(y);
        std::pair<uint64_t, uint64_t>                               cond_sh     = ss.Share(cond);
        std::pair<std::array<uint64_t, 2>, std::array<uint64_t, 2>> x_arr_sh    = ss.Share(x_arr);
        std::pair<std::array<uint64_t, 2>, std::array<uint64_t, 2>> y_arr_sh    = ss.Share(y_arr);
        std::pair<std::array<uint64_t, 2>, std::array<uint64_t, 2>> cond_arr_sh = ss.Share(cond_arr);
        std::pair<std::vector<uint64_t>, std::vector<uint64_t>>     x_vec_sh    = ss.Share(x_vec);
        std::pair<std::vector<uint64_t>, std::vector<uint64_t>>     y_vec_sh    = ss.Share(y_vec);
        std::pair<std::vector<uint64_t>, std::vector<uint64_t>>     cond_vec_sh = ss.Share(cond_vec);

        Logger::DebugLog(LOC, "x: " + ToString(x) + ", y: " + ToString(y));
        Logger::DebugLog(LOC, "x_0: " + ToString(x_sh.first) + ", x_1: " + ToString(x_sh.second));
        Logger::DebugLog(LOC, "y_0: " + ToString(y_sh.first) + ", y_1: " + ToString(y_sh.second));
        Logger::DebugLog(LOC, "x_arr: " + ToString(x_arr) + ", y_arr: " + ToString(y_arr));
        Logger::DebugLog(LOC, "x_arr_0: " + ToString(x_arr_sh.first) + ", x_arr_1: " + ToString(x_arr_sh.second));
        Logger::DebugLog(LOC, "y_arr_0: " + ToString(y_arr_sh.first) + ", y_arr_1: " + ToString(y_arr_sh.second));
        Logger::DebugLog(LOC, "x_vec: " + ToString(x_vec) + ", y_vec: " + ToString(y_vec));
        Logger::DebugLog(LOC, "x_vec_0: " + ToString(x_vec_sh.first) + ", x_vec_1: " + ToString(x_vec_sh.second));
        Logger::DebugLog(LOC, "y_vec_0: " + ToString(y_vec_sh.first) + ", y_vec_1: " + ToString(y_vec_sh.second));
        Logger::DebugLog(LOC, "cond: " + ToString(cond));
        Logger::DebugLog(LOC, "cond_0: " + ToString(cond_sh.first) + ", cond_1: " + ToString(cond_sh.second));
        Logger::DebugLog(LOC, "cond_arr: " + ToString(cond_arr));
        Logger::DebugLog(LOC, "cond_arr_0: " + ToString(cond_arr_sh.first) + ", cond_arr_1: " + ToString(cond_arr_sh.second));
        Logger::DebugLog(LOC, "cond_vec: " + ToString(cond_vec));
        Logger::DebugLog(LOC, "cond_vec_0: " + ToString(cond_vec_sh.first) + ", cond_vec_1: " + ToString(cond_vec_sh.second));

        // Save input
        std::string x_path    = kTestAdditivePath + "x_n" + ToString(bitsize);
        std::string y_path    = kTestAdditivePath + "y_n" + ToString(bitsize);
        std::string cond_path = kTestAdditivePath + "cond_n" + ToString(bitsize);
        file_io.WriteBinary(x_path + "_0", x_sh.first);
        file_io.WriteBinary(x_path + "_1", x_sh.second);
        file_io.WriteBinary(y_path + "_0", y_sh.first);
        file_io.WriteBinary(y_path + "_1", y_sh.second);
        file_io.WriteBinary(x_path + "_arr_0", x_arr_sh.first);
        file_io.WriteBinary(x_path + "_arr_1", x_arr_sh.second);
        file_io.WriteBinary(y_path + "_arr_0", y_arr_sh.first);
        file_io.WriteBinary(y_path + "_arr_1", y_arr_sh.second);
        file_io.WriteBinary(x_path + "_vec_0", x_vec_sh.first);
        file_io.WriteBinary(x_path + "_vec_1", x_vec_sh.second);
        file_io.WriteBinary(y_path + "_vec_0", y_vec_sh.first);
        file_io.WriteBinary(y_path + "_vec_1", y_vec_sh.second);
        file_io.WriteBinary(cond_path + "_0", cond_sh.first);
        file_io.WriteBinary(cond_path + "_1", cond_sh.second);
        file_io.WriteBinary(cond_path + "_arr_0", cond_arr_sh.first);
        file_io.WriteBinary(cond_path + "_arr_1", cond_arr_sh.second);
        file_io.WriteBinary(cond_path + "_vec_0", cond_vec_sh.first);
        file_io.WriteBinary(cond_path + "_vec_1", cond_vec_sh.second);
        Logger::DebugLog(LOC, "Saved shared inputs to files: " + x_path + "[_0|_1]");
        Logger::DebugLog(LOC, "Saved shared inputs to files: " + y_path + "[_0|_1]");

        // Load input
        uint64_t                x_0, x_1, y_0, y_1;
        std::array<uint64_t, 2> x_arr_0, x_arr_1, y_arr_0, y_arr_1;
        std::vector<uint64_t>   x_vec_0, x_vec_1, y_vec_0, y_vec_1;
        uint64_t                cond_0, cond_1;
        std::array<uint64_t, 2> cond_arr_0, cond_arr_1;
        std::vector<uint64_t>   cond_vec_0, cond_vec_1;
        file_io.ReadBinary(x_path + "_0", x_0);
        file_io.ReadBinary(x_path + "_1", x_1);
        file_io.ReadBinary(y_path + "_0", y_0);
        file_io.ReadBinary(y_path + "_1", y_1);
        file_io.ReadBinary(x_path + "_arr_0", x_arr_0);
        file_io.ReadBinary(x_path + "_arr_1", x_arr_1);
        file_io.ReadBinary(y_path + "_arr_0", y_arr_0);
        file_io.ReadBinary(y_path + "_arr_1", y_arr_1);
        file_io.ReadBinary(x_path + "_vec_0", x_vec_0);
        file_io.ReadBinary(x_path + "_vec_1", x_vec_1);
        file_io.ReadBinary(y_path + "_vec_0", y_vec_0);
        file_io.ReadBinary(y_path + "_vec_1", y_vec_1);
        file_io.ReadBinary(cond_path + "_0", cond_0);
        file_io.ReadBinary(cond_path + "_1", cond_1);
        file_io.ReadBinary(cond_path + "_arr_0", cond_arr_0);
        file_io.ReadBinary(cond_path + "_arr_1", cond_arr_1);
        file_io.ReadBinary(cond_path + "_vec_0", cond_vec_0);
        file_io.ReadBinary(cond_path + "_vec_1", cond_vec_1);

        // Reconstruct
        uint64_t                x_rec, y_rec;
        std::array<uint64_t, 2> x_arr_rec, y_arr_rec;
        std::vector<uint64_t>   x_vec_rec, y_vec_rec;
        uint64_t                cond_rec;
        std::array<uint64_t, 2> cond_arr_rec;
        std::vector<uint64_t>   cond_vec_rec;

        ss.ReconstLocal(x_0, x_1, x_rec);
        ss.ReconstLocal(y_0, y_1, y_rec);
        ss.ReconstLocal(x_arr_0, x_arr_1, x_arr_rec);
        ss.ReconstLocal(y_arr_0, y_arr_1, y_arr_rec);
        ss.ReconstLocal(x_vec_0, x_vec_1, x_vec_rec);
        ss.ReconstLocal(y_vec_0, y_vec_1, y_vec_rec);
        ss.ReconstLocal(cond_0, cond_1, cond_rec);
        ss.ReconstLocal(cond_arr_0, cond_arr_1, cond_arr_rec);
        ss.ReconstLocal(cond_vec_0, cond_vec_1, cond_vec_rec);

        if (x_rec != x || y_rec != y)
            throw osuCrypto::UnitTestFail("Reconstruction failed for scalar.");
        if (x_arr_rec != x_arr || y_arr_rec != y_arr)
            throw osuCrypto::UnitTestFail("Reconstruction failed for array.");
        if (x_vec_rec != x_vec || y_vec_rec != y_vec)
            throw osuCrypto::UnitTestFail("Reconstruction failed for vector.");
        if (cond_rec != cond || cond_arr_rec != cond_arr || cond_vec_rec != cond_vec)
            throw osuCrypto::UnitTestFail("Reconstruction failed for condition.");
    }

    Logger::DebugLog(LOC, "Additive2P_Evaluate_Offline_Test - Passed");
}

void Additive2P_BeaverTriples_Test() {
    Logger::DebugLog(LOC, "Additive2P_BeaverTriples_Test...");

    for (const auto &cfg : configs) {
        const uint64_t bitsize = cfg.arith_bits;

        AdditiveSharing2P ss0(cfg);
        AdditiveSharing2P ss1(cfg);

        const size_t      num_triples = 10;
        const std::string base_path   = kTestAdditivePath + "triple_n" + ToString(bitsize);

        // Offline: generate and save shares.
        auto shares = ringoa::sharing::GenerateAndShareBeaverTriples(num_triples, cfg);
        ringoa::sharing::Save2PTriplesShares(base_path, shares.first, shares.second);

        // Online: load party shares and set them.
        ss0.SetTriples(ringoa::sharing::Load2PTriplesShare(base_path, /*party_id=*/0));
        ss1.SetTriples(ringoa::sharing::Load2PTriplesShare(base_path, /*party_id=*/1));

        Logger::DebugLog(LOC, "bitsize=" + ToString(bitsize) + " party0 triples:");
        ss0.PrintTriples();

        Logger::DebugLog(LOC, "bitsize=" + ToString(bitsize) + " party1 triples:");
        ss1.PrintTriples();
    }

    Logger::DebugLog(LOC, "Additive2P_BeaverTriples_Test - Passed");
}

void Additive2P_EvaluateAdd_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Additive2P_EvaluateAdd_Online_Test...");
    int party_id = cmd.getOr<int>("party", -1);

    for (const auto &cfg : configs) {
        AdditiveSharing2P ss(cfg);
        uint64_t          bitsize = cfg.arith_bits;
        FileIo            file_io;

        // Start network communication
        TwoPartyNetworkManager net_mgr("Additive2P_EvaluateAdd_Online_Test");

        uint64_t                z_0, z_1, z;
        std::array<uint64_t, 2> z_arr_0, z_arr_1, z_arr;
        std::vector<uint64_t>   z_vec_0, z_vec_1, z_vec;
        std::string             x_path = kTestAdditivePath + "x_n" + ToString(bitsize);
        std::string             y_path = kTestAdditivePath + "y_n" + ToString(bitsize);

        // Server task
        auto server_task = [&](osuCrypto::Channel &chl) {
            uint64_t                x_0, y_0;
            std::array<uint64_t, 2> x_arr_0, y_arr_0;
            std::vector<uint64_t>   x_vec_0, y_vec_0;

            // Load input
            file_io.ReadBinary(x_path + "_0", x_0);
            file_io.ReadBinary(y_path + "_0", y_0);
            file_io.ReadBinary(x_path + "_arr_0", x_arr_0);
            file_io.ReadBinary(y_path + "_arr_0", y_arr_0);
            file_io.ReadBinary(x_path + "_vec_0", x_vec_0);
            file_io.ReadBinary(y_path + "_vec_0", y_vec_0);
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + x_path + "_0");
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + y_path + "_0");

            // Evaluate Add
            ss.EvaluateAdd(x_0, y_0, z_0);
            ss.EvaluateAdd(x_arr_0, y_arr_0, z_arr_0);
            ss.EvaluateAdd(x_vec_0, y_vec_0, z_vec_0);

            Logger::DebugLog(LOC, "z_0: " + ToString(z_0));
            Logger::DebugLog(LOC, "z_arr_0: " + ToString(z_arr_0));
            Logger::DebugLog(LOC, "z_vec_0: " + ToString(z_vec_0));

            // Reconstruct
            ss.Reconst(0, chl, z_0, z_1, z);
            ss.Reconst(0, chl, z_arr_0, z_arr_1, z_arr);
            ss.Reconst(0, chl, z_vec_0, z_vec_1, z_vec);
        };

        // Client task
        auto client_task = [&](osuCrypto::Channel &chl) {
            uint64_t                x_1, y_1;
            std::array<uint64_t, 2> x_arr_1, y_arr_1;
            std::vector<uint64_t>   x_vec_1, y_vec_1;

            // Load input
            file_io.ReadBinary(x_path + "_1", x_1);
            file_io.ReadBinary(y_path + "_1", y_1);
            file_io.ReadBinary(x_path + "_arr_1", x_arr_1);
            file_io.ReadBinary(y_path + "_arr_1", y_arr_1);
            file_io.ReadBinary(x_path + "_vec_1", x_vec_1);
            file_io.ReadBinary(y_path + "_vec_1", y_vec_1);
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + x_path + "_1");
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + y_path + "_1");

            // Evaluate Add
            ss.EvaluateAdd(x_1, y_1, z_1);
            ss.EvaluateAdd(x_arr_1, y_arr_1, z_arr_1);
            ss.EvaluateAdd(x_vec_1, y_vec_1, z_vec_1);

            Logger::DebugLog(LOC, "z_1: " + ToString(z_1));
            Logger::DebugLog(LOC, "z_arr_1: " + ToString(z_arr_1));
            Logger::DebugLog(LOC, "z_vec_1: " + ToString(z_vec_1));

            // Reconstruct
            ss.Reconst(1, chl, z_0, z_1, z);
            ss.Reconst(1, chl, z_arr_0, z_arr_1, z_arr);
            ss.Reconst(1, chl, z_vec_0, z_vec_1, z_vec);
        };

        // Configure network based on party ID and wait for completion
        net_mgr.AutoConfigure(party_id, server_task, client_task);
        net_mgr.WaitForCompletion();

        Logger::DebugLog(LOC, "z: " + ToString(z));
        Logger::DebugLog(LOC, "z_arr: " + ToString(z_arr));
        Logger::DebugLog(LOC, "z_vec: " + ToString(z_vec));

        // Validate the result
        if (z != 9 || z_arr != std::array<uint64_t, 2>{6, 6} || z_vec != std::vector<uint64_t>{6, 6, 6, 6, 6})
            throw osuCrypto::UnitTestFail("EvaluateAdd failed.");

        Logger::DebugLog(LOC, "Additive2P_EvaluateAdd_Online_Test - Passed");
    }
}

void Additive2P_EvaluatePubMult_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Additive2P_EvaluatePubMult_Online_Test...");
    int party_id = cmd.getOr<int>("party", -1);

    for (const auto &cfg : configs) {
        AdditiveSharing2P ss(cfg);
        uint64_t          bitsize = cfg.arith_bits;
        FileIo            file_io;

        // Start network communication
        TwoPartyNetworkManager net_mgr("Additive2P_EvaluatePubMult_Test");

        uint64_t                z_0, z_1, z;
        std::array<uint64_t, 2> z_arr_0, z_arr_1, z_arr;
        std::vector<uint64_t>   z_vec_0, z_vec_1, z_vec;
        std::string             x_path = kTestAdditivePath + "x_n" + ToString(bitsize);
        std::string             y_path = kTestAdditivePath + "y_n" + ToString(bitsize);

        // Server task
        auto server_task = [&](osuCrypto::Channel &chl) {
            uint64_t                x_0, y;
            std::array<uint64_t, 2> x_arr_0;
            std::vector<uint64_t>   x_vec_0;

            // Load input
            file_io.ReadBinary(x_path + "_0", x_0);
            file_io.ReadBinary(y_path, y);
            file_io.ReadBinary(x_path + "_arr_0", x_arr_0);
            file_io.ReadBinary(x_path + "_vec_0", x_vec_0);

            // Evaluate PubMult
            ss.EvaluatePubMult(x_0, y, z_0);
            // TODO: Implement EvaluatePubMult for array and vector inputs

            Logger::DebugLog(LOC, "z_0: " + ToString(z_0));
            Logger::DebugLog(LOC, "z_arr_0: " + ToString(z_arr_0));
            Logger::DebugLog(LOC, "z_vec_0: " + ToString(z_vec_0));

            // Reconstruct
            ss.Reconst(0, chl, z_0, z_1, z);
            ss.Reconst(0, chl, z_arr_0, z_arr_1, z_arr);
            ss.Reconst(0, chl, z_vec_0, z_vec_1, z_vec);
        };

        // Client task
        auto client_task = [&](osuCrypto::Channel &chl) {
            uint64_t                x_1, y;
            std::array<uint64_t, 2> x_arr_1;
            std::vector<uint64_t>   x_vec_1;

            // Load input
            file_io.ReadBinary(x_path + "_1", x_1);
            file_io.ReadBinary(y_path, y);
            file_io.ReadBinary(x_path + "_arr_1", x_arr_1);
            file_io.ReadBinary(x_path + "_vec_1", x_vec_1);

            // Evaluate PubMult
            ss.EvaluatePubMult(x_1, y, z_1);
            // TODO: Implement EvaluatePubMult for array and vector inputs

            Logger::DebugLog(LOC, "z_1: " + ToString(z_1));
            Logger::DebugLog(LOC, "z_arr_1: " + ToString(z_arr_1));
            Logger::DebugLog(LOC, "z_vec_1: " + ToString(z_vec_1));

            // Reconstruct
            ss.Reconst(1, chl, z_0, z_1, z);
            ss.Reconst(1, chl, z_arr_0, z_arr_1, z_arr);
            ss.Reconst(1, chl, z_vec_0, z_vec_1, z_vec);
        };

        // Configure network based on party ID and wait for completion
        net_mgr.AutoConfigure(party_id, server_task, client_task);
        net_mgr.WaitForCompletion();

        Logger::DebugLog(LOC, "z: " + ToString(z));
        Logger::DebugLog(LOC, "z_arr: " + ToString(z_arr));
        Logger::DebugLog(LOC, "z_vec: " + ToString(z_vec));

        // Validate the result
        if (z != 20 || z_arr != std::array<uint64_t, 2>{15, 8} || z_vec != std::vector<uint64_t>{5, 8, 11, 14, 17})
            throw osuCrypto::UnitTestFail("EvaluatePubMult failed.");
        Logger::DebugLog(LOC, "Additive2P_EvaluatePubMult_Online_Test - Passed");
    }
}

void Additive2P_EvaluateMult_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Additive2P_EvaluateMult_Online_Test...");
    int party_id = cmd.getOr<int>("party", -1);

    for (const auto &cfg : configs) {
        AdditiveSharing2P ss(cfg);
        uint64_t          bitsize = cfg.arith_bits;
        FileIo            file_io;

        // Start network communication
        TwoPartyNetworkManager net_mgr("Additive2P_EvaluateMult_Test");

        uint64_t                z_0, z_1, z;
        std::array<uint64_t, 2> z_arr_0, z_arr_1, z_arr;
        std::vector<uint64_t>   z_vec_0, z_vec_1, z_vec;
        std::string             x_path      = kTestAdditivePath + "x_n" + ToString(bitsize);
        std::string             y_path      = kTestAdditivePath + "y_n" + ToString(bitsize);
        std::string             triple_path = kTestAdditivePath + "triple_n" + ToString(bitsize);

        // Server task
        auto server_task = [&](osuCrypto::Channel &chl) {
            AdditiveSharing2P       ss(cfg);
            uint64_t                x_0, y_0;
            std::array<uint64_t, 2> x_arr_0, y_arr_0;
            std::vector<uint64_t>   x_vec_0, y_vec_0;

            // Load input
            file_io.ReadBinary(x_path + "_0", x_0);
            file_io.ReadBinary(y_path + "_0", y_0);
            file_io.ReadBinary(x_path + "_arr_0", x_arr_0);
            file_io.ReadBinary(y_path + "_arr_0", y_arr_0);
            file_io.ReadBinary(x_path + "_vec_0", x_vec_0);
            file_io.ReadBinary(y_path + "_vec_0", y_vec_0);
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + x_path + "_0");
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + y_path + "_0");

            // Setup additive sharing
            ss.SetTriples(ringoa::sharing::Load2PTriplesShare(triple_path, /*party_id=*/0));

            Logger::DebugLog(LOC, "Before: Remaining triples: " + ToString(ss.GetRemainingTripleCount()));

            // Evaluate Mult
            ss.EvaluateMult(0, chl, x_0, y_0, z_0);
            ss.EvaluateMult(0, chl, x_arr_0, y_arr_0, z_arr_0);
            ss.EvaluateMult(0, chl, x_vec_0, y_vec_0, z_vec_0);

            Logger::DebugLog(LOC, "After: Remaining triples: " + ToString(ss.GetRemainingTripleCount()));

            // Reconstruct
            ss.Reconst(0, chl, z_0, z_1, z);
            ss.Reconst(0, chl, z_arr_0, z_arr_1, z_arr);
            ss.Reconst(0, chl, z_vec_0, z_vec_1, z_vec);

            Logger::DebugLog(LOC, "z_0: " + ToString(z_0));
            Logger::DebugLog(LOC, "z_arr_0: " + ToString(z_arr_0));
            Logger::DebugLog(LOC, "z_vec_0: " + ToString(z_vec_0));
        };

        // Client task

        auto client_task = [&](osuCrypto::Channel &chl) {
            AdditiveSharing2P       ss(cfg);
            uint64_t                x_1, y_1;
            std::array<uint64_t, 2> x_arr_1, y_arr_1;
            std::vector<uint64_t>   x_vec_1, y_vec_1;

            // Load input
            file_io.ReadBinary(x_path + "_1", x_1);
            file_io.ReadBinary(y_path + "_1", y_1);
            file_io.ReadBinary(x_path + "_arr_1", x_arr_1);
            file_io.ReadBinary(y_path + "_arr_1", y_arr_1);
            file_io.ReadBinary(x_path + "_vec_1", x_vec_1);
            file_io.ReadBinary(y_path + "_vec_1", y_vec_1);
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + x_path + "_1");
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + y_path + "_1");

            // Setup additive sharing
            ss.SetTriples(ringoa::sharing::Load2PTriplesShare(triple_path, /*party_id=*/1));

            // Evaluate Mult
            ss.EvaluateMult(1, chl, x_1, y_1, z_1);
            ss.EvaluateMult(1, chl, x_arr_1, y_arr_1, z_arr_1);
            ss.EvaluateMult(1, chl, x_vec_1, y_vec_1, z_vec_1);

            // Reconstruct
            ss.Reconst(1, chl, z_0, z_1, z);
            ss.Reconst(1, chl, z_arr_0, z_arr_1, z_arr);
            ss.Reconst(1, chl, z_vec_0, z_vec_1, z_vec);

            Logger::DebugLog(LOC, "z_1: " + ToString(z_1));
            Logger::DebugLog(LOC, "z_arr_1: " + ToString(z_arr_1));
            Logger::DebugLog(LOC, "z_vec_1: " + ToString(z_vec_1));
        };

        // Configure network based on party ID and wait for completion
        net_mgr.AutoConfigure(party_id, server_task, client_task);
        net_mgr.WaitForCompletion();

        Logger::DebugLog(LOC, "z: " + ToString(z));
        Logger::DebugLog(LOC, "z_arr: " + ToString(z_arr));
        Logger::DebugLog(LOC, "z_vec: " + ToString(z_vec));

        // Validate the result
        if (z != 20 || z_arr != std::array<uint64_t, 2>{5, 8} || z_vec != std::vector<uint64_t>{5, 8, 9, 8, 5})
            throw osuCrypto::UnitTestFail("EvaluateMult failed.");
    }
    Logger::DebugLog(LOC, "Additive2P_EvaluateMult_Online_Test - Passed");
}

void Additive2P_EvaluateSelect_Online_Test(const osuCrypto::CLP &cmd) {
    Logger::DebugLog(LOC, "Additive2P_EvaluateSelect_Online_Test...");
    int party_id = cmd.getOr<int>("party", -1);

    for (const auto &cfg : configs) {
        AdditiveSharing2P ss(cfg);
        uint64_t          bitsize = cfg.arith_bits;
        FileIo            file_io;

        // Start network communication
        TwoPartyNetworkManager net_mgr("Additive2P_EvaluateSelect_Test");

        uint64_t                z_0, z_1, z;
        std::array<uint64_t, 2> z_arr_0, z_arr_1, z_arr;
        std::vector<uint64_t>   z_vec_0, z_vec_1, z_vec;
        std::string             x_path      = kTestAdditivePath + "x_n" + ToString(bitsize);
        std::string             y_path      = kTestAdditivePath + "y_n" + ToString(bitsize);
        std::string             cond_path   = kTestAdditivePath + "cond_n" + ToString(bitsize);
        std::string             triple_path = kTestAdditivePath + "triple_n" + ToString(bitsize);

        // Server task
        auto server_task = [&](osuCrypto::Channel &chl) {
            AdditiveSharing2P       ss(cfg);
            uint64_t                x_0, y_0, cond_0;
            std::array<uint64_t, 2> x_arr_0, y_arr_0, cond_arr_0;
            std::vector<uint64_t>   x_vec_0, y_vec_0, cond_vec_0;

            // Load input
            file_io.ReadBinary(x_path + "_0", x_0);
            file_io.ReadBinary(y_path + "_0", y_0);
            file_io.ReadBinary(cond_path + "_0", cond_0);
            file_io.ReadBinary(x_path + "_arr_0", x_arr_0);
            file_io.ReadBinary(y_path + "_arr_0", y_arr_0);
            file_io.ReadBinary(cond_path + "_arr_0", cond_arr_0);
            file_io.ReadBinary(x_path + "_vec_0", x_vec_0);
            file_io.ReadBinary(y_path + "_vec_0", y_vec_0);
            file_io.ReadBinary(cond_path + "_vec_0", cond_vec_0);
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + x_path + "_0");
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + y_path + "_0");
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + cond_path + "_0");

            // Setup additive sharing
            ss.SetTriples(ringoa::sharing::Load2PTriplesShare(triple_path, /*party_id=*/0));

            // Evaluate Select
            ss.EvaluateSelect(0, chl, x_0, y_0, cond_0, z_0);
            ss.EvaluateSelect(0, chl, x_arr_0, y_arr_0, cond_arr_0, z_arr_0);
            ss.EvaluateSelect(0, chl, x_vec_0, y_vec_0, cond_vec_0, z_vec_0);

            // Reconstruct
            ss.Reconst(0, chl, z_0, z_1, z);
            ss.Reconst(0, chl, z_arr_0, z_arr_1, z_arr);
            ss.Reconst(0, chl, z_vec_0, z_vec_1, z_vec);

            Logger::DebugLog(LOC, "[P0] z_0: " + ToString(z_0));
            Logger::DebugLog(LOC, "[P0] z_arr_0: " + ToString(z_arr_0));
            Logger::DebugLog(LOC, "[P0] z_vec_0: " + ToString(z_vec_0));
        };

        // Client task
        auto client_task = [&](osuCrypto::Channel &chl) {
            AdditiveSharing2P       ss(cfg);
            uint64_t                x_1, y_1, cond_1;
            std::array<uint64_t, 2> x_arr_1, y_arr_1, cond_arr_1;
            std::vector<uint64_t>   x_vec_1, y_vec_1, cond_vec_1;

            // Load input
            file_io.ReadBinary(x_path + "_1", x_1);
            file_io.ReadBinary(y_path + "_1", y_1);
            file_io.ReadBinary(cond_path + "_1", cond_1);
            file_io.ReadBinary(x_path + "_arr_1", x_arr_1);
            file_io.ReadBinary(y_path + "_arr_1", y_arr_1);
            file_io.ReadBinary(cond_path + "_arr_1", cond_arr_1);
            file_io.ReadBinary(x_path + "_vec_1", x_vec_1);
            file_io.ReadBinary(y_path + "_vec_1", y_vec_1);
            file_io.ReadBinary(cond_path + "_vec_1", cond_vec_1);
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + x_path + "_1");
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + y_path + "_1");
            Logger::DebugLog(LOC, "Loaded shared inputs from file: " + cond_path + "_1");

            // Setup additive sharing
            ss.SetTriples(ringoa::sharing::Load2PTriplesShare(triple_path, /*party_id=*/1));

            // Evaluate Select
            ss.EvaluateSelect(1, chl, x_1, y_1, cond_1, z_1);
            ss.EvaluateSelect(1, chl, x_arr_1, y_arr_1, cond_arr_1, z_arr_1);
            ss.EvaluateSelect(1, chl, x_vec_1, y_vec_1, cond_vec_1, z_vec_1);

            // Reconstruct
            ss.Reconst(1, chl, z_0, z_1, z);
            ss.Reconst(1, chl, z_arr_0, z_arr_1, z_arr);
            ss.Reconst(1, chl, z_vec_0, z_vec_1, z_vec);

            Logger::DebugLog(LOC, "[P1] z_1: " + ToString(z_1));
            Logger::DebugLog(LOC, "[P1] z_arr_1: " + ToString(z_arr_1));
            Logger::DebugLog(LOC, "[P1] z_vec_1: " + ToString(z_vec_1));
        };

        // Configure network based on party ID and wait for completion
        net_mgr.AutoConfigure(party_id, server_task, client_task);
        net_mgr.WaitForCompletion();

        Logger::DebugLog(LOC, "z: " + ToString(z));
        Logger::DebugLog(LOC, "z_arr: " + ToString(z_arr));
        Logger::DebugLog(LOC, "z_vec: " + ToString(z_vec));

        // Validate the result
        // Select(x, y, c)  = x if c = 0 otherwise y
        // x = [1, 2, 3, 4, 5]
        // y = [5, 4, 3, 2, 1]
        // c = [1, 0, 1, 0, 1]
        // z = [5, 2, 3, 4, 1]
        if (z != 4 || z_arr != std::array<uint64_t, 2>{5, 2} || z_vec != std::vector<uint64_t>{5, 2, 3, 4, 1})
            throw osuCrypto::UnitTestFail("EvaluateSelect failed.");
    }

    Logger::DebugLog(LOC, "Additive2P_EvaluateSelect_Online_Test - Passed");
}

}    // namespace test_ringoa
