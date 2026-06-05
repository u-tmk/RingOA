#!/usr/bin/env bash
# ============================================================
# Script: extract_vaf_only.sh
# Purpose:
#   Extract only the VAF value (as integer 0–100) from a MAF file,
#   ignoring rows with NA or zero total counts.
#
# Usage:
#   ./extract_vaf_only.sh input.maf.sorted output_vaf.txt
# ============================================================

if [ $# -ne 2 ]; then
    echo "Usage: $0 <input.maf> <output.txt>"
    exit 1
fi

INPUT="$1"
OUTPUT="$2"

awk -F'\t' '($38 != "NA" && $39 != "NA" && $38+$39 > 0) {
    vaf = int(100 * $38 / ($38 + $39) + 0.5);  # 四捨五入
    print vaf;
}' "$INPUT" > "$OUTPUT"

echo "✅ VAF values (0–100 integers) saved to: $OUTPUT"

