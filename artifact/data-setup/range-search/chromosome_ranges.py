#!/usr/bin/env python3
# ============================================================
# Script: chromosome_ranges.py
# Purpose:
#   Count line ranges (start and end line numbers) for each chromosome
#   in a sorted maf_vaf.tsv file and save to file.
#
# Usage:
#   ./chromosome_ranges.py maf_vaf.tsv output.tsv
# ============================================================

import sys

if len(sys.argv) != 3:
    print("Usage: {} <maf_vaf.tsv> <output.tsv>".format(sys.argv[0]))
    sys.exit(1)

input_path = sys.argv[1]
output_path = sys.argv[2]

ranges = {}
current_chr = None
start_line = 1  # 1-based line number
line_num = 0

with open(input_path, "r") as f:
    for line in f:
        line_num += 1
        fields = line.strip().split()
        if len(fields) < 3:
            continue
        chrom = fields[1]

        if chrom != current_chr:
            if current_chr is not None:
                ranges[current_chr] = (start_line, line_num - 1)
            current_chr = chrom
            start_line = line_num

if current_chr is not None:
    ranges[current_chr] = (start_line, line_num)

# ファイルに出力
with open(output_path, "w") as out:
    out.write("Chr\tStartLine\tEndLine\n")
    for chrom, (s, e) in ranges.items():
        out.write(f"{chrom}\t{s}\t{e}\n")

print(f"✅ Results saved to {output_path}")
