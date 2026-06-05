#include "plain_wm.h"

#include <algorithm>
#include <sdsl/csa_wt.hpp>

#include "RingOA/utils/logger.h"
#include "RingOA/utils/to_string.h"
#include "RingOA/utils/utils.h"

namespace {

std::string GetBuildOrderString(ringoa::wm::BuildOrder order) {
    switch (order) {
        case ringoa::wm::BuildOrder::MSBFirst:
            return "MSB -> LSB";
        case ringoa::wm::BuildOrder::LSBFirst:
            return "LSB -> MSB";
        default:
            return "Unknown";
    }
}

}    // namespace

namespace ringoa {
namespace wm {

CharMapper::CharMapper(CharType type)
    : type_(type) {
    Initialize(type);
}

void CharMapper::Initialize(CharType type) {
    char2id_.clear();
    id2char_.clear();

    switch (type) {
        case CharType::DNA:
            sigma_   = 3;
            char2id_ = {{'$', 0}, {'A', 1}, {'C', 2}, {'G', 3}, {'T', 4}, {'N', 5}};
            break;
        case CharType::PROTEIN:
            sigma_   = 5;
            char2id_ = {
                {'$', 0},
                {'A', 1},
                {'C', 2},
                {'D', 3},
                {'E', 4},
                {'F', 5},
                {'G', 6},
                {'H', 7},
                {'I', 8},
                {'K', 9},
                {'L', 10},
                {'M', 11},
                {'N', 12},
                {'P', 13},
                {'Q', 14},
                {'R', 15},
                {'S', 16},
                {'T', 17},
                {'V', 18},
                {'W', 19},
                {'Y', 20}};
            break;
        default:
            throw std::invalid_argument(LOC + " Unsupported CharType in CharMapper.");
    }

    id2char_.resize(char2id_.size());
    for (const auto &[ch, id] : char2id_) {
        id2char_[id] = ch;
    }
}

size_t CharMapper::GetSigma() const {
    return sigma_;
}

CharType CharMapper::GetType() const {
    return type_;
}

bool CharMapper::IsValidChar(char c) const {
    return char2id_.find(c) != char2id_.end();
}

std::vector<uint64_t> CharMapper::ToIds(const std::string &s) const {
    std::vector<uint64_t> result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(ToId(c));
    }
    return result;
}

uint64_t CharMapper::ToId(char c) const {
    auto it = char2id_.find(c);
    if (it == char2id_.end()) {
        throw std::invalid_argument(LOC + " Character '" + std::string(1, c) + "' not found in alphabet");
    }
    return it->second;
}

std::string CharMapper::ToString(const std::vector<uint64_t> &v) const {
    std::string result;
    result.reserve(v.size());
    for (uint64_t id : v) {
        result += id2char_.at(id);
    }
    return result;
}

const std::unordered_map<char, uint64_t> &CharMapper::GetMap() const {
    return char2id_;
}

std::string CharMapper::MapToString() const {
    std::string result;
    for (const auto &[ch, id] : char2id_) {
        result += ch;
        result += ":";
        result += std::to_string(id);
        result += " ";
    }
    return result;
}

WaveletMatrix::WaveletMatrix(const std::string &data, const CharType type, const BuildOrder order)
    : length_(0), sigma_(0), order_(order), mapper_(type) {
    data_  = mapper_.ToIds(data);
    sigma_ = mapper_.GetSigma();
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma_));
    Logger::DebugLog(LOC, "Mapping: " + mapper_.MapToString());
    Logger::DebugLog(LOC, "Order: " + GetBuildOrderString(order_));
    Logger::DebugLog(LOC, "Data: " + ToString(data_));
    Logger::DebugLog(LOC, "Length: " + ToString(data.size()));
#endif
    Build(data_);
}

WaveletMatrix::WaveletMatrix(const std::vector<uint64_t> &data, const size_t sigma, const BuildOrder order)
    : length_(0), sigma_(sigma), order_(order) {
    data_ = data;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Sigma: " + ToString(sigma_));
    Logger::DebugLog(LOC, "Order: " + GetBuildOrderString(order_));
    Logger::DebugLog(LOC, "Data: " + ToString(data_));
    Logger::DebugLog(LOC, "Length: " + ToString(data.size()));
#endif
    Build(data_);
}

size_t WaveletMatrix::GetLength() const {
    return length_;
}

size_t WaveletMatrix::GetSigma() const {
    return sigma_;
}

const CharMapper &WaveletMatrix::GetMapper() const {
    return mapper_;
}

std::string WaveletMatrix::GetMapString() const {
    return mapper_.MapToString();
}

const std::vector<uint64_t> &WaveletMatrix::GetData() const {
    return data_;
}

const std::vector<uint64_t> &WaveletMatrix::GetRank0Tables() const {
    return rank0_tables_;
}

BuildOrder WaveletMatrix::GetBuildOrder() const {
    return order_;
}

void WaveletMatrix::PrintRank0Tables() const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    const size_t stride = length_ + 1;
    for (size_t bit = 0; bit < sigma_; ++bit) {
        size_t                    off = bit * stride;
        std::span<const uint64_t> tbl(&rank0_tables_[off], stride);
        Logger::DebugLog(
            LOC,
            "Rank0 Table[" + ToString(bit) + "]: " +
                ToString(tbl));
    }
#endif
}

uint64_t WaveletMatrix::Access(size_t i) const {
    if (i >= length_)
        throw std::out_of_range("Access index out of range");
    const size_t stride = length_ + 1;
    uint64_t     result = 0;

    // Traverse from MSB to LSB
    for (size_t lvl = sigma_; lvl > 0; --lvl) {
        const size_t bit = lvl - 1;
        const size_t off = bit * stride;

        const size_t z_before = rank0_tables_[off + i];
        const size_t z_after  = rank0_tables_[off + i + 1];    // safe: i < length_
        const bool   is_zero  = (z_after - z_before) == 1;

        if (is_zero) {
            // current bit is 0
            i = z_before;    // number of zeros before i
            // result bit stays 0
        } else {
            // current bit is 1
            const size_t total_zeros = rank0_tables_[off + length_];
            const size_t ones_before = i - z_before;                 // #ones in [0,i)
            i                        = total_zeros + ones_before;    // map into 1-bucket
            result |= (1ULL << bit);
        }
    }
    return result;
}

uint64_t WaveletMatrix::Quantile(size_t l, size_t r, size_t k) const {
    if (l >= r)
        throw std::invalid_argument("Quantile: empty range");
    if (k >= r - l)
        throw std::out_of_range("Quantile: k out of range");

    const size_t stride = length_ + 1;
    uint64_t     result = 0;
    size_t       left = l, right = r;

    // Traverse from MSB to LSB
    for (size_t lvl = sigma_; lvl > 0; --lvl) {
        const size_t bit = lvl - 1;
        const size_t off = bit * stride;

        size_t z_left     = rank0_tables_[off + left];
        size_t z_right    = rank0_tables_[off + right];
        size_t zero_count = z_right - z_left;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Bit " + ToString(bit) + ", k = " + ToString(k));
        Logger::DebugLog(LOC, "Left: " + ToString(left) + ", Right: " + ToString(right));
        Logger::DebugLog(LOC, "Z_Left: " + ToString(z_left) + ", Z_Right: " + ToString(z_right));
        Logger::DebugLog(LOC, "Zero_Count: " + ToString(zero_count));
#endif

        if (k < zero_count) {
            // k-th lies in the 0-bucket
            left  = z_left;
            right = z_right;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, "Update to 0-bucket: Left=" + ToString(left) + ", Right=" + ToString(right));
            Logger::DebugLog(LOC, "Result so far: " + ToString(result));
#endif
        } else {
            // k-th lies in the 1-bucket
            k -= zero_count;
            size_t total_zeros = rank0_tables_[off + length_];
            size_t o_left      = left - z_left;
            size_t o_right     = right - z_right;
            left               = total_zeros + o_left;
            right              = total_zeros + o_right;
            result |= (1ULL << bit);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, "Update to 1-bucket: Left=" + ToString(left) + ", Right=" + ToString(right));
            Logger::DebugLog(LOC, "Result so far: " + ToString(result));
#endif
        }
    }
    return result;
}

uint64_t WaveletMatrix::RangeMin(size_t l, size_t r) const {
    if (l >= r)
        throw std::invalid_argument("RangeMin: empty range");
    return Quantile(l, r, 0);
}

uint64_t WaveletMatrix::RangeMax(size_t l, size_t r) const {
    if (l >= r)
        throw std::invalid_argument("RangeMax: empty range");
    return Quantile(l, r, (r - l) - 1);
}

uint64_t WaveletMatrix::RangeFreq(size_t l, size_t r,
                                  uint64_t x, uint64_t y) const {
    if (l >= r || x >= y)
        return 0;

    struct Node {
        size_t   left, right;
        size_t   lvl;
        uint64_t prefix;
    };
    std::stack<Node> st;
    st.push({l, r, sigma_, 0});
    uint64_t     count  = 0;
    const size_t stride = length_ + 1;

    Logger::DebugLog(LOC, "RangeFreq begin");
    Logger::DebugLog(LOC, "Query: l=" + ToString(l) + ", r=" + ToString(r) +
                              ", x=" + ToString(x) + ", y=" + ToString(y));

    while (!st.empty()) {
        auto [left, right, lvl, prefix] = st.top();
        st.pop();
        if (left >= right)
            continue;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "---- Pop node ----");
        Logger::DebugLog(LOC, "left=" + ToString(left) + ", right=" + ToString(right) +
                                  ", lvl=" + ToString(lvl) +
                                  ", prefix=" + ToString(prefix));
#endif

        if (lvl == 0) {
            if (x <= prefix && prefix < y)
                count += (right - left);
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, "Leaf hits range: prefix in [x,y). "
                                  "Add count += " +
                                      ToString(right - left) +
                                      " -> total=" + ToString(count));
#endif
            continue;
        }

        size_t bit = lvl - 1;
        size_t off = bit * stride;

        size_t z_left  = rank0_tables_[off + left];
        size_t z_right = rank0_tables_[off + right];
        size_t nl0 = z_left, nr0 = z_right;

        size_t total_zeros = rank0_tables_[off + length_];
        size_t o_left      = left - z_left;
        size_t o_right     = right - z_right;
        size_t nl1         = total_zeros + o_left;
        size_t nr1         = total_zeros + o_right;

        uint64_t low0  = prefix;
        uint64_t high0 = prefix + (1ULL << bit);
        uint64_t low1  = prefix | (1ULL << bit);
        uint64_t high1 = prefix + (1ULL << (bit + 1));

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Level bit=" + ToString(bit));
        Logger::DebugLog(LOC, "rank0: z_left=" + ToString(z_left) +
                                  ", z_right=" + ToString(z_right) +
                                  ", total_zeros=" + ToString(total_zeros));
        Logger::DebugLog(LOC, "Map to children:");
        Logger::DebugLog(LOC, "  zeros:  [nl0,nr0)=[" + ToString(nl0) + "," + ToString(nr0) + ")");
        Logger::DebugLog(LOC, "  ones:   [nl1,nr1)=[" + ToString(nl1) + "," + ToString(nr1) + ")");
        Logger::DebugLog(LOC, "Value ranges of children:");
        Logger::DebugLog(LOC, "  zeros:  [" + ToString(low0) + "," + ToString(high0) + ")");
        Logger::DebugLog(LOC, "  ones:   [" + ToString(low1) + "," + ToString(high1) + ")");
#endif

        bool inter0 = !(high0 <= x || y <= low0);
        bool inter1 = !(high1 <= x || y <= low1);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Intersect with [x,y): zeros=" + ToString(inter0) +
                                  ", ones=" + ToString(inter1));
#endif

        if (inter0) {
            st.push({nl0, nr0, bit, low0});
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, " -> push zeros child");
#endif
        }
        if (inter1) {
            st.push({nl1, nr1, bit, low1});
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            Logger::DebugLog(LOC, " -> push ones child");
#endif
        }
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "RangeFreq end: total count = " + ToString(count));
#endif
    return count;
}

void WaveletMatrix::RangeList(size_t l, size_t r,
                              uint64_t x, uint64_t y,
                              std::vector<std::pair<uint64_t, size_t>> &out) const {
    out.clear();
    if (l >= r || x >= y)
        return;

    struct Node {
        size_t   left, right;
        size_t   lvl;
        uint64_t prefix;
    };
    std::stack<Node> st;
    st.push({l, r, sigma_, 0});
    const size_t stride = length_ + 1;

    while (!st.empty()) {
        auto [left, right, lvl, prefix] = st.top();
        st.pop();
        if (left >= right)
            continue;

        if (lvl == 0) {
            if (x <= prefix && prefix < y) {
                out.emplace_back(prefix, right - left);
            }
            continue;
        }

        size_t bit = lvl - 1;
        size_t off = bit * stride;

        size_t z_left  = rank0_tables_[off + left];
        size_t z_right = rank0_tables_[off + right];
        size_t nl0 = z_left, nr0 = z_right;

        size_t total_zeros = rank0_tables_[off + length_];
        size_t o_left      = left - z_left;
        size_t o_right     = right - z_right;
        size_t nl1         = total_zeros + o_left;
        size_t nr1         = total_zeros + o_right;

        uint64_t low0  = prefix;
        uint64_t high0 = prefix + (1ULL << bit);
        uint64_t low1  = prefix | (1ULL << bit);
        uint64_t high1 = prefix + (1ULL << (bit + 1));

        if (!(high0 <= x || y <= low0)) {
            st.push({nl0, nr0, bit, low0});
        }
        if (!(high1 <= x || y <= low1)) {
            st.push({nl1, nr1, bit, low1});
        }
    }
}

std::vector<std::pair<uint64_t, size_t>>
WaveletMatrix::TopK(size_t l, size_t r, size_t k) const {
    std::vector<std::pair<uint64_t, size_t>> freq;
    RangeList(l, r, 0, (1ULL << sigma_), freq);

    if (freq.size() <= k) {
        std::sort(freq.begin(), freq.end(),
                  [](auto &a, auto &b) { return a.second > b.second; });
        return freq;
    }

    std::partial_sort(freq.begin(), freq.begin() + k, freq.end(),
                      [](auto &a, auto &b) { return a.second > b.second; });
    freq.resize(k);
    return freq;
}

uint64_t WaveletMatrix::RankCF(uint64_t c, size_t position) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "RankCF(" + ToString(c) + ", " + ToString(position) + ")");
#endif
    if (order_ != BuildOrder::LSBFirst) {
        throw std::invalid_argument(LOC + " RankCF is only implemented for LSB -> MSB order");
    }
    if (length_ == 0)
        return 0;

    const size_t stride = length_ + 1;

    // Traverse from LSB to MSB
    for (size_t bit = 0; bit < sigma_; ++bit) {
        const size_t off = bit * stride;
        const bool   b   = (c >> bit) & 1ULL;

        // zeros prefix in [0, position)
        const size_t zpos = rank0_tables_[off + position];

        if (!b) {
            // 0-bit: jump to 0-bucket
            position = zpos;
        } else {
            // 1-bit: jump to 1-bucket = totalZeros + ones_prefix
            const size_t total_zeros = rank0_tables_[off + length_];
            const size_t ones_prefix = position - zpos;
            position                 = total_zeros + ones_prefix;
        }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC,
                         "(" + ToString(bit) + ") bit=" + ToString(b) +
                             " zpos=" + ToString(zpos) +
                             " -> pos=" + ToString(position));
#endif
    }
    return position;    // == C[c] + rank(c, position) under LSB->MSB build
}

void WaveletMatrix::Build(const std::vector<uint64_t> &data) {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "WaveletMatrix Build...");
#endif
    length_ = data.size();
    if (length_ == 0) {
        rank0_tables_.clear();
        return;
    }

    const size_t stride = length_ + 1;
    rank0_tables_.assign(sigma_ * stride, 0);

    std::vector<uint64_t> current = data;
    if (order_ == BuildOrder::MSBFirst) {
        BuildMsbFirst(std::move(current));
    } else {
        BuildLsbFirst(std::move(current));
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    PrintRank0Tables();
    Logger::DebugLog(LOC, "WaveletMatrix Build - Done");
#endif
}

void WaveletMatrix::BuildMsbFirst(std::vector<uint64_t> current) {
    const size_t          stride = length_ + 1;
    std::vector<uint64_t> zero_bucket(length_), one_bucket(length_);

    for (size_t lvl = sigma_; lvl > 0; --lvl) {
        const size_t bit   = lvl - 1;
        const size_t off   = bit * stride;
        size_t       zeros = 0, ones = 0;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        std::string bit_str;
        bit_str.reserve(length_);
#endif

        for (size_t i = 0; i < length_; ++i) {
            const bool is_one = (current[i] >> bit) & 1U;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            bit_str.push_back(is_one ? '1' : '0');
#endif
            if (is_one) {
                one_bucket[ones++] = current[i];
            } else {
                zero_bucket[zeros++] = current[i];
                ++rank0_tables_[off + i + 1];
            }
            rank0_tables_[off + i + 1] += rank0_tables_[off + i];
        }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Bit Vector [" + ToString(bit) + "]: " + bit_str +
                                  " (0: " + ToString(zeros) +
                                  ", 1: " + ToString(ones) + ")");
#endif

        std::copy(zero_bucket.begin(), zero_bucket.begin() + zeros, current.begin());
        std::copy(one_bucket.begin(), one_bucket.begin() + ones, current.begin() + zeros);
    }
}

void WaveletMatrix::BuildLsbFirst(std::vector<uint64_t> current) {
    const size_t          stride = length_ + 1;
    std::vector<uint64_t> zero_bucket(length_), one_bucket(length_);

    for (size_t bit = 0; bit < sigma_; ++bit) {
        const size_t off   = bit * stride;
        size_t       zeros = 0, ones = 0;

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        std::string bit_str;
        bit_str.reserve(length_);
#endif

        for (size_t i = 0; i < length_; ++i) {
            const bool is_one = (current[i] >> bit) & 1U;
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
            bit_str.push_back(is_one ? '1' : '0');
#endif
            if (is_one) {
                one_bucket[ones++] = current[i];
            } else {
                zero_bucket[zeros++] = current[i];
                ++rank0_tables_[off + i + 1];
            }
            rank0_tables_[off + i + 1] += rank0_tables_[off + i];
        }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "Bit Vector [" + ToString(bit) + "]: " + bit_str +
                                  " (0: " + ToString(zeros) +
                                  ", 1: " + ToString(ones) + ")");
#endif

        std::copy(zero_bucket.begin(), zero_bucket.begin() + zeros, current.begin());
        std::copy(one_bucket.begin(), one_bucket.begin() + ones, current.begin() + zeros);
    }
}

FMIndex::FMIndex(const std::string &text, const CharType type) {
    // 1) Set text
    text_ = text;
    std::reverse(text_.begin(), text_.end());

    // 2) Build BWT from text
    BuildBwt();

    // 3) Convert bwt_str_ to integers
    wm_ = WaveletMatrix(bwt_str_, type, BuildOrder::LSBFirst);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
    Logger::DebugLog(LOC, "Alphabet size   : " + ToString(wm_.GetSigma()));
    Logger::DebugLog(LOC, "Mapping         : " + wm_.GetMapString());
    Logger::DebugLog(LOC, "Text            : " + text_);
    Logger::DebugLog(LOC, "BWT             : " + bwt_str_);
    Logger::DebugLog(LOC, "BWT as integers : " + ToString(wm_.GetData()));
    wm_.PrintRank0Tables();
    Logger::DebugLog(LOC, Logger::MakeSeparatorLine());
#endif
}

const WaveletMatrix &FMIndex::GetWaveletMatrix() const {
    return wm_;
}

const std::vector<uint64_t> &FMIndex::GetRank0Tables() const {
    return wm_.GetRank0Tables();
}

std::vector<uint64_t> FMIndex::ConvertToBitMatrix(const std::string &query) const {
    // 1) string -> ID
    std::vector<uint64_t> nums = wm_.GetMapper().ToIds(query);

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "Query: " + query);
    Logger::DebugLog(LOC, "Query as numbers: " + ToString(nums));
#endif

    // 2) Convert to bits
    uint64_t              sigma = wm_.GetSigma();
    std::vector<uint64_t> bits(nums.size() * sigma);
    for (size_t i = 0; i < nums.size(); ++i) {
        uint64_t val = nums[i];
        for (size_t b = 0; b < sigma; ++b) {
            bits[i * sigma + b] = (val >> b) & 1U;
        }
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    for (size_t i = 0; i < bits.size(); ++i) {
        Logger::DebugLog(LOC, "bit_row[" + ToString(i) + "]: " + ToString(bits[i]));
    }
#endif
    return bits;
}

void FMIndex::BuildBwt() {
    // Construct the suffix array using the SDSL library
    sdsl::csa_wt<> csa;
    sdsl::construct_im(csa, text_, 1);
    // Convert the BWT to a string
    for (size_t i = 0; i < text_.size() + 1; ++i) {
        if (csa.bwt[i]) {
            bwt_str_ += csa.bwt[i];
        } else {
            bwt_str_ += '$';
        }
    }
}

void FMIndex::BackwardSearch(char c, uint64_t &left, uint64_t &right) const {
    if (!wm_.GetMapper().IsValidChar(c)) {
        throw std::invalid_argument(LOC + " Invalid character '" + std::string(1, c) + "' in BackwardSearch");
    }
    uint64_t cid = wm_.GetMapper().ToId(c);
    left         = wm_.RankCF(cid, left);
    right        = wm_.RankCF(cid, right);
}

uint64_t FMIndex::ComputeLPMfromWM(const std::string &query) const {
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "lpm_len(" + query + ")");
#endif

    // Backward search for last character
    uint64_t              left  = 0;
    uint64_t              right = static_cast<uint64_t>(bwt_str_.size());
    std::vector<uint64_t> intervals;
    // Traverse query from end to front
    for (size_t i = 0; i < query.size(); ++i) {
        char c = query[i];
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "(char " + std::string(1, c) + ") (l, r) == (" + ToString(left) + ", " + ToString(right) + ")");
#endif
        BackwardSearch(c, left, right);
        intervals.push_back(right - left);
    }
#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "(l, r) == (" + ToString(left) + ", " + ToString(right) + ")");
    Logger::DebugLog(LOC, "Intervals: " + ToString(intervals));
#endif
    uint64_t lpm_len = 0;
    for (size_t i = 0; i < intervals.size(); ++i) {
        if (intervals[i] == 0) {
            break;
        }
        lpm_len++;
    }
    return lpm_len;
}

uint64_t FMIndex::ComputeLPMfromBWT(const std::string &query) const {
    const std::string &bwt = bwt_str_;
    const size_t       n   = bwt.size();

    // Step 1: count character frequency
    std::unordered_map<char, int> char_count;
    for (char c : bwt) {
        char_count[c]++;
    }

    // Step 2: build F[c] = offset (no need for range)
    std::map<char, int> F;
    int                 offset = 0;
    for (const auto &[c, count] : std::map<char, int>(char_count.begin(), char_count.end())) {
        F[c] = offset;
        offset += count;
    }
    // #if LOG_LEVEL >= LOG_LEVEL_DEBUG
    //     Logger::DebugLog(LOC, "F[c] = offset:");
    //     for (const auto &[c, offset] : F) {
    //         Logger::DebugLog(LOC, "  '" + std::string(1, c) + "' : " + ToString(offset));
    //     }
    // #endif

    // Step 3: backward search with rank() + offset
    int                   f       = 0;
    int                   g       = static_cast<int>(n);
    uint64_t              lpm_len = 0;
    std::vector<uint64_t> intervals;

    for (size_t qi = 0; qi < query.size(); ++qi) {
        char        c     = query[qi];
        std::string bwt_f = bwt.substr(0, f);
        std::string bwt_g = bwt.substr(0, g);

        int rank_f = static_cast<int>(std::count(bwt_f.begin(), bwt_f.end(), c));
        int rank_g = static_cast<int>(std::count(bwt_g.begin(), bwt_g.end(), c));
        int offset = F[c];

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
        Logger::DebugLog(LOC, "(char: " + std::string(1, c) + ") (l, r) == (" +
                                  ToString(f) + ", " + ToString(g) + ")");
        Logger::DebugLog(LOC, "(char: " + std::string(1, c) + ") l = offset(" + ToString(offset) +
                                  ") + rank(" + std::string(1, c) + ", " + ToString(f) + ")(" +
                                  ToString(rank_f) + ")");
        Logger::DebugLog(LOC, "(char: " + std::string(1, c) + ") r = offset(" + ToString(offset) +
                                  ") + rank(" + std::string(1, c) + ", " + ToString(g) + ")(" +
                                  ToString(rank_g) + ")");
#endif

        f = offset + rank_f;
        g = offset + rank_g;

        if (f < g) {
            lpm_len++;
        }
        intervals.push_back(g - f);
    }

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
    Logger::DebugLog(LOC, "(l, r) == (" + ToString(f) + ", " + ToString(g) + ")");
    Logger::DebugLog(LOC, "Intervals: " + ToString(intervals));
    Logger::DebugLog(LOC, "LPM length (without WM): " + ToString(lpm_len));
#endif

    return lpm_len;
}

}    // namespace wm
}    // namespace ringoa
