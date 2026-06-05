#!/usr/bin/env python3
"""
count_base.py
-----------------------------
Count nucleotide symbols in a FASTA file (case-sensitive).

Usage:
    python3 count_base.py chr1.fa
"""

import sys
from collections import Counter


def count_base(fasta_path: str):
    counts = Counter()
    total = 0

    with open(fasta_path, "r") as f:
        for line in f:
            if line.startswith(">"):
                continue  # skip header
            seq = line.strip()  # ← upper() をしない
            counts.update(seq)
            total += len(seq)

    print(f"# Case-sensitive base composition for {fasta_path}")
    print(f"{'Char':<5} {'Count':>15} {'Percent':>10}")
    print("-" * 35)

    for base, c in sorted(counts.items()):
        print(f"{base:<5} {c:>15,} {c/total*100:>9.4f}%")
    print("-" * 35)
    print(f"{'Total':<5} {total:>15,} {100.0:>9.4f}%")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 count_base.py <fasta_file>",
              file=sys.stderr)
        sys.exit(1)
    count_base(sys.argv[1])
