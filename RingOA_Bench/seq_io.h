#ifndef BENCH_SEQ_IO_H_
#define BENCH_SEQ_IO_H_

#include <string>
#include <vector>

namespace ringoa {

// Read a single FASTA file (skip '>' header lines, uppercase sequence)
std::string ReadFastaSequence(const std::string &fasta_path);

// Return prefix [0, length). Throws if length > full_seq.size().
std::string CutPrefix(const std::string &full_seq, std::size_t length);

// Stateful chromosome loader that preserves `current_` and `next_idx_` across calls.
class ChromosomeLoader {
public:
    explicit ChromosomeLoader(std::vector<std::string> fasta_paths);

    // Ensure the internal buffer has at least `length` bases; append as needed.
    // Returns a copy of prefix [0, length).
    std::string EnsurePrefix(std::size_t length);

    // Accessors
    std::size_t loaded_count() const {
        return next_idx_;
    }    // how many files consumed
    const std::string &buffer() const {
        return current_;
    }    // current concatenated buffer
    bool exhausted() const {
        return next_idx_ >= fasta_paths_.size();
    }
    const std::vector<std::string> &paths() const {
        return fasta_paths_;
    }

    // Reset state (optional)
    void Reset();

private:
    std::vector<std::string> fasta_paths_;
    std::size_t              next_idx_ = 0;
    std::string              current_;
};

}    // namespace ringoa

#endif    // BENCH_SEQ_IO_H_
