#include "wm_test.h"

#include <cryptoTools/Common/TestCollection.h>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/timer.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"
#include "RingOA/wm/plain_wm.h"

namespace test_ringoa {

using ringoa::Logger;
using ringoa::ToString;
using ringoa::wm::BuildOrder;
using ringoa::wm::CharType;
using ringoa::wm::FMIndex;
using ringoa::wm::WaveletMatrix;

void WaveletMatrix_Access_Test() {
    Logger::DebugLog(LOC, "WaveletMatrix_Access_Test...");

    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // idx   |0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15 |
    //  S    |0   3   7   1   4   6   3   7   2   5   6   0   3   5   2   4  |
    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    std::vector<uint64_t> data = {0, 3, 7, 1, 4, 6, 3, 7, 2, 5, 6, 0, 3, 5, 2, 4};
    WaveletMatrix         wm   = WaveletMatrix(data, 3, BuildOrder::MSBFirst);

    for (size_t i = 0; i < data.size(); ++i) {
        uint64_t val = wm.Access(i);
        Logger::DebugLog(LOC, "Access(" + ToString(i) + ") = " + ToString(val));

        if (val != data[i]) {
            throw osuCrypto::UnitTestFail("Access mismatch at i=" + ToString(i) +
                                          " (expected " + ToString(data[i]) +
                                          ", got " + ToString(val) + ")");
        }
    }
}

void WaveletMatrix_Quantile_Test() {
    Logger::DebugLog(LOC, "WaveletMatrix_Quantile_Test...");

    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // idx   |0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15 |
    //  S    |0   3   7   1   4   6   3   7   2   5   6   0   3   5   2   4  |
    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    std::vector<uint64_t> data = {0, 3, 7, 1, 4, 6, 3, 7, 2, 5, 6, 0, 3, 5, 2, 4};
    WaveletMatrix         wm   = WaveletMatrix(data, 3, BuildOrder::MSBFirst);

    {
        size_t l = 2, r = 8;
        // 区間: [7, 1, 4, 6, 3, 7] → ソートすると [1,3,4,6,7,7]
        uint64_t q0 = wm.Quantile(l, r, 0);    // 最小
        uint64_t q3 = wm.Quantile(l, r, 3);    // 4番目 = 6
        uint64_t q5 = wm.Quantile(l, r, 5);    // 最大
        Logger::DebugLog(LOC, "Quantile(" + ToString(l) + ", " + ToString(r) + ", 0) = " + ToString(q0));
        Logger::DebugLog(LOC, "Quantile(" + ToString(l) + ", " + ToString(r) + ", 3) = " + ToString(q3));
        Logger::DebugLog(LOC, "Quantile(" + ToString(l) + ", " + ToString(r) + ", 5) = " + ToString(q5));

        if (q0 != 1)
            throw osuCrypto::UnitTestFail("Expected Quantile(2,8,0) == 1");
        if (q3 != 6)
            throw osuCrypto::UnitTestFail("Expected Quantile(2,8,3) == 6");
        if (q5 != 7)
            throw osuCrypto::UnitTestFail("Expected Quantile(2,8,5) == 7");
    }

    {
        for (size_t l = 0; l < data.size(); ++l) {
            for (size_t r = l + 1; r <= data.size(); ++r) {
                std::vector<uint64_t> sub(data.begin() + l, data.begin() + r);
                std::sort(sub.begin(), sub.end());
                for (size_t k = 0; k < sub.size(); ++k) {
                    uint64_t q = wm.Quantile(l, r, k);
                    if (q != sub[k]) {
                        throw osuCrypto::UnitTestFail(
                            "Quantile(" + ToString(l) + "," + ToString(r) + "," + ToString(k) +
                            ") expected " + ToString(sub[k]) + " got " + ToString(q));
                    }
                }
            }
        }
    }

    {
        size_t l = 2, r = 8;
        // 区間: [7, 1, 4, 6, 3, 7]
        uint64_t rangeMin = wm.RangeMin(l, r);
        uint64_t rangeMax = wm.RangeMax(l, r);

        Logger::DebugLog(LOC, "RangeMin(" + ToString(l) + ", " + ToString(r) + ") = " + ToString(rangeMin));
        Logger::DebugLog(LOC, "RangeMax(" + ToString(l) + ", " + ToString(r) + ") = " + ToString(rangeMax));

        if (rangeMin != 1) {
            throw osuCrypto::UnitTestFail("Expected RangeMin(2, 8) == 1");
        }
        if (rangeMax != 7) {
            throw osuCrypto::UnitTestFail("Expected RangeMax(2, 8) == 7");
        }
    }
}

void WaveletMatrix_RangeFreqTest() {
    Logger::DebugLog(LOC, "WaveletMatrix_RangeFreqTest...");

    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // idx   |0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15 |
    //  S    |0   3   7   1   4   6   3   7   2   5   6   0   3   5   2   4  |
    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    std::vector<uint64_t> data = {0, 3, 7, 1, 4, 6, 3, 7, 2, 5, 6, 0, 3, 5, 2, 4};
    WaveletMatrix         wm   = WaveletMatrix(data, 3, BuildOrder::MSBFirst);

    {
        size_t l = 2, r = 8;
        // 区間: [7, 1, 4, 6, 3, 7]
        // 値が [2,6) → {4,3} の2個
        uint64_t cnt = wm.RangeFreq(l, r, 2, 6);
        Logger::DebugLog(LOC, "RangeFreq(" + ToString(l) + ", " + ToString(r) + ", 2, 6) = " + ToString(cnt));

        if (cnt != 2)
            throw osuCrypto::UnitTestFail("Expected RangeFreq(2,8,2,6) == 2");
    }

    {
        size_t                                   l = 2, r = 8;
        std::vector<std::pair<uint64_t, size_t>> out;
        wm.RangeList(l, r, 0, (1ULL << 3), out);
        Logger::DebugLog(LOC, "RangeList(" + ToString(l) + ", " + ToString(r) + ", 0, 8):");
        for (auto &p : out) {
            Logger::DebugLog(LOC, "  Value " + ToString(p.first) + " : " + ToString(p.second) + " times");
        }

        // 出現頻度マップを作る
        std::map<uint64_t, size_t> freq;
        for (auto v : {7, 1, 4, 6, 3, 7})
            ++freq[v];

        for (auto &p : out) {
            if (freq[p.first] != p.second) {
                throw osuCrypto::UnitTestFail("RangeList mismatch for value " + ToString(p.first));
            }
        }
    }
}

void WaveletMatrix_TopK_Test() {
    Logger::DebugLog(LOC, "WaveletMatrix_TopK_Test...");

    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // idx   |0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15 |
    //  S    |0   3   7   1   4   6   3   7   2   5   6   0   3   5   2   4  |
    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    std::vector<uint64_t> data = {0, 3, 7, 1, 4, 6, 3, 7, 2, 5, 6, 0, 3, 5, 2, 4};
    WaveletMatrix         wm   = WaveletMatrix(data, 3, BuildOrder::MSBFirst);

    {
        size_t l = 2, r = 8;
        auto   top2 = wm.TopK(l, r, 2);
        // 区間 [7,1,4,6,3,7] → 出現頻度 {7:2, 1:1, 4:1, 6:1, 3:1}
        // 上位2個は 7(2), 1(1) or 4/6/3(1)
        Logger::DebugLog(LOC, "TopK(" + ToString(l) + ", " + ToString(r) + ", 2):");
        for (auto &p : top2) {
            Logger::DebugLog(LOC, "  Value " + ToString(p.first) + " : " + ToString(p.second) + " times");
        }

        if (top2.empty() || top2[0].first != 7 || top2[0].second != 2) {
            throw osuCrypto::UnitTestFail("Expected TopK first element to be (7,2)");
        }
    }
}

void WaveletMatrix_RankCF_Test() {
    Logger::DebugLog(LOC, "WaveletMatrix_RankCF_Test...");

    std::string text = "ACGTACGT";
    Logger::DebugLog(LOC, "Text: " + text);

    WaveletMatrix wm(text, CharType::DNA, BuildOrder::LSBFirst);

    uint64_t cid     = wm.GetMapper().ToId('G');
    size_t   pos     = 6;    // up to position 6 (exclusive)
    uint64_t rank_cf = wm.RankCF(cid, pos);

    Logger::DebugLog(LOC, "RankCF('G', " + ToString(pos) + ") = " + ToString(rank_cf));

    if (rank_cf != 5) {
        throw osuCrypto::UnitTestFail("Expected RankCF('G', 6) == 5");
    }
}

void FMIndex_Test() {
    Logger::DebugLog(LOC, "FMIndex_Test...");

    std::string dna   = "GATTACA";
    std::string query = "GATTG";

    // Build FM-index
    FMIndex fm(dna, CharType::DNA);

    // Convert to bit matrix
    std::vector<uint64_t> bit_matrix = fm.ConvertToBitMatrix(query);

    uint64_t lpm_len     = fm.ComputeLPMfromWM(query);
    uint64_t lpm_len_bwt = fm.ComputeLPMfromBWT(query);

    Logger::DebugLog(LOC, "LPM(WM)  = " + ToString(lpm_len));
    Logger::DebugLog(LOC, "LPM(BWT) = " + ToString(lpm_len_bwt));

    if (lpm_len != lpm_len_bwt)
        throw osuCrypto::UnitTestFail("LPM mismatch: WM = " + ToString(lpm_len) + ", BWT = " + ToString(lpm_len_bwt));

    std::string text = "ARNDCQILVVFP";
    query            = "DCQPP";

    // Build FM-index
    fm = FMIndex(text, CharType::PROTEIN);

    // Convert to bit matrix
    bit_matrix = fm.ConvertToBitMatrix(query);

    lpm_len     = fm.ComputeLPMfromWM(query);
    lpm_len_bwt = fm.ComputeLPMfromBWT(query);

    Logger::DebugLog(LOC, "LPM(WM)  = " + ToString(lpm_len));
    Logger::DebugLog(LOC, "LPM(BWT) = " + ToString(lpm_len_bwt));

    if (lpm_len != lpm_len_bwt)
        throw osuCrypto::UnitTestFail("LPM mismatch: FM = " + ToString(lpm_len) + ", BWT = " + ToString(lpm_len_bwt));

    Logger::DebugLog(LOC, "FMIndex_Test - Passed");
}

}    // namespace test_ringoa
