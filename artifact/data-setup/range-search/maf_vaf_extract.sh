#!/usr/bin/env bash
# ============================================================
# Script: maf_vaf_extract.sh
# Purpose:
#   Extract gene, chromosome, position, t_alt_count, t_ref_count, and VAF
#   (no header) from an ICGC-style MAF file, excluding rows with NA values.
#
# Usage:
#   ./maf_vaf_extract.sh input.maf.sorted output.tsv
# ============================================================

if [ $# -ne 2 ]; then
    echo "Usage: $0 <input.maf> <output.tsv>"
    exit 1
fi

INPUT="$1"
OUTPUT="$2"

awk -F'\t' '($38 != "NA" && $39 != "NA" && $38+$39 > 0) {
    vaf = $38 / ($38 + $39);
    printf "%s\t%s\t%s\t%s\t%s\t%.6f\n", $1, $2, $3, $38, $39, vaf;
}' "$INPUT" > "$OUTPUT"

echo "✅ Extracted data saved to: $OUTPUT"

