#include "seq_io.h"

#include <cctype>
#include <fstream>
#include <stdexcept>

namespace ringoa {

std::string ReadFastaSequence(const std::string &fasta_path) {
    std::ifstream fin(fasta_path, std::ios::binary);
    if (!fin) {
        throw std::runtime_error("Failed to open file: " + fasta_path);
    }

    std::string seq, line;
    while (std::getline(fin, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();    // handle CRLF
        if (line.empty() || line[0] == '>')
            continue;
        for (unsigned char ch : line)
            seq.push_back(static_cast<char>(std::toupper(ch)));
    }
    return seq;
}

std::string CutPrefix(const std::string &full_seq, std::size_t length) {
    if (length > full_seq.size()) {
        throw std::out_of_range("Requested length exceeds sequence size.");
    }
    return full_seq.substr(0, length);
}

ChromosomeLoader::ChromosomeLoader(std::vector<std::string> fasta_paths)
    : fasta_paths_(std::move(fasta_paths)) {
}

std::string ChromosomeLoader::EnsurePrefix(std::size_t length) {
    while (current_.size() < length && next_idx_ < fasta_paths_.size()) {
        current_ += ReadFastaSequence(fasta_paths_[next_idx_++]);
    }
    if (current_.size() < length) {
        throw std::runtime_error(
            "Insufficient total sequence length. Needed " + std::to_string(length) +
            ", available " + std::to_string(current_.size()) + ".");
    }
    return current_.substr(0, length);
}

void ChromosomeLoader::Reset() {
    current_.clear();
    next_idx_ = 0;
}

}    // namespace ringoa
