#!/usr/bin/env python3
"""
replace_ambiguous_bases.py
--------------------------
Replace ambiguous IUPAC base symbols (R, Y, K, M, S, W, B, D, H, V) in a FASTA file with 'N'.

Usage:
    python3 replace_ambiguous_bases.py input.fa output.fa
"""

import sys

AMBIGUOUS = str.maketrans({
    "R": "N",
    "Y": "N",
    "K": "N",
    "M": "N",
    "S": "N",
    "W": "N",
    "B": "N",
    "D": "N",
    "H": "N",
    "V": "N",
})


def replace_ambiguous(input_path: str, output_path: str):
    with open(input_path, "r") as fin, open(output_path, "w") as fout:
        for line in fin:
            if line.startswith(">"):
                fout.write(line)
            else:
                fout.write(line.translate(AMBIGUOUS))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 replace_ambiguous_bases.py <input.fa> <output.fa>", file=sys.stderr)
        sys.exit(1)

    replace_ambiguous(sys.argv[1], sys.argv[2])
    print(f"✅ Replaced ambiguous bases with 'N': {sys.argv[2]}")
