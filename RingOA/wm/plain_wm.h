#ifndef WM_PLAIN_WM_H_
#define WM_PLAIN_WM_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ringoa {
namespace wm {

/**
 * @brief Character type enumeration
 * This enum is used to specify the type of characters in the text.
 * It can be either DNA or protein.
 */
enum class CharType
{
    DNA,
    PROTEIN,
};

/**
 * @brief Build order enumeration
 * This enum is used to specify the build order of the wavelet matrix.
 * It can be either MSBFirst (Most Significant Bit First) or LSBFirst (Least Significant Bit First).
 */
enum class BuildOrder
{
    MSBFirst,
    LSBFirst,
};

/**
 * @brief CharMapper class for character mapping
 */
class CharMapper {
public:
    CharMapper(CharType type = CharType::DNA);
    void Initialize(CharType type);

    const std::unordered_map<char, uint64_t> &GetMap() const;
    size_t                                    GetSigma() const;
    CharType                                  GetType() const;
    bool                                      IsValidChar(char c) const;

    std::vector<uint64_t> ToIds(const std::string &s) const;
    uint64_t              ToId(char c) const;
    std::string           ToString(const std::vector<uint64_t> &v) const;
    std::string           MapToString() const;

private:
    std::unordered_map<char, uint64_t> char2id_;
    std::vector<char>                  id2char_;
    size_t                             sigma_;
    CharType                           type_;
};

/**
 * @brief WaveletMatrix class for plain computation.
 */
class WaveletMatrix {
public:
    WaveletMatrix() = default;

    explicit WaveletMatrix(const std::string &data, const CharType type = CharType::DNA, const BuildOrder order = BuildOrder::MSBFirst);
    explicit WaveletMatrix(const std::vector<uint64_t> &data, const size_t sigma, const BuildOrder order = BuildOrder::MSBFirst);

    // --- Basic info ---
    size_t                       GetLength() const;
    size_t                       GetSigma() const;
    BuildOrder                   GetBuildOrder() const;
    const CharMapper            &GetMapper() const;
    std::string                  GetMapString() const;
    const std::vector<uint64_t> &GetData() const;
    const std::vector<uint64_t> &GetRank0Tables() const;

    // --- Debugging ---
    void PrintRank0Tables() const;

    // --- Core queries ---
    uint64_t Access(size_t i) const;                                         ///< Return T[i]
    uint64_t Quantile(size_t l, size_t r, size_t k) const;                   ///< k-th smallest in [l,r)
    uint64_t RangeMin(size_t l, size_t r) const;                             ///< min in [l,r)
    uint64_t RangeMax(size_t l, size_t r) const;                             ///< max in [l,r)
    uint64_t RangeFreq(size_t l, size_t r, uint64_t x, uint64_t y) const;    ///< count of x ≤ v < y in [l,r)
    void     RangeList(size_t l, size_t r, uint64_t x, uint64_t y,
                       std::vector<std::pair<uint64_t, size_t>> &out) const;

    std::vector<std::pair<uint64_t, size_t>> TopK(size_t l, size_t r, size_t k) const;    ///< top-k frequent in [l,r)

    // --- FM-index style (valid only if built LSB→MSB) ---
    uint64_t RankCF(uint64_t c, size_t position) const;

private:
    size_t                length_;
    size_t                sigma_;
    BuildOrder            order_;
    CharMapper            mapper_;
    std::vector<uint64_t> data_;
    std::vector<uint64_t> rank0_tables_;

    // --- Build routines ---
    void Build(const std::vector<uint64_t> &data);
    void BuildMsbFirst(std::vector<uint64_t> current);
    void BuildLsbFirst(std::vector<uint64_t> current);
};

class FMIndex {
public:
    FMIndex(const std::string &text, const CharType type = CharType::DNA);
    FMIndex(const FMIndex &)                = default;
    FMIndex(FMIndex &&) noexcept            = default;
    FMIndex &operator=(const FMIndex &)     = default;
    FMIndex &operator=(FMIndex &&) noexcept = default;

    const WaveletMatrix         &GetWaveletMatrix() const;
    const std::vector<uint64_t> &GetRank0Tables() const;

    std::vector<uint64_t> ConvertToBitMatrix(const std::string &query) const;

    // Search query in text, returns the Longest Prefix Match Length
    uint64_t ComputeLPMfromWM(const std::string &query) const;
    uint64_t ComputeLPMfromBWT(const std::string &query) const;

private:
    std::string   text_;    /**< original text + sentinel */
    std::string   bwt_str_; /**< BWT of text */
    WaveletMatrix wm_;      /**< Wavelet matrix built over bwt_str (as integer array) */

    // Build BWT from suffix array
    void BuildBwt();

    // Backward search [top, bottom) range
    void BackwardSearch(char c, uint64_t &left, uint64_t &right) const;
};

}    // namespace wm
}    // namespace ringoa

#endif    // WM_PLAIN_WM_H_
