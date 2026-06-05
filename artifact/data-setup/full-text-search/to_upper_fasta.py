#!/usr/bin/env python3
"""
to_upper_fasta.py
-----------------
Convert all nucleotide letters in a FASTA file to uppercase.

Usage:
    python3 to_upper_fasta.py input.fa output.fa
"""

import sys


def fasta_to_upper(input_path: str, output_path: str):
    with open(input_path, "r") as fin, open(output_path, "w") as fout:
        for line in fin:
            if line.startswith(">"):
                fout.write(line)  # keep header as is
            else:
                fout.write(line.upper())  # convert sequence to uppercase


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 to_upper_fasta.py <input.fa> <output.fa>",
              file=sys.stderr)
        sys.exit(1)

    fasta_to_upper(sys.argv[1], sys.argv[2])
    print(f"✅ Converted to uppercase: {sys.argv[2]}")
