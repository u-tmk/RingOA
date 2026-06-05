#!/usr/bin/env bash
# ============================================================
# Script: gene_line_ranges.sh
# Purpose:
#   From a maf_vaf.tsv (columns: Gene Chr Pos Alt Ref VAF),
#   find the first and last line numbers (1-based) for specified genes
#   and save them as TSV: Gene  StartLine  EndLine
#
# Usage:
#   ./gene_line_ranges.sh maf_vaf.tsv output.tsv "BRCA1,BRCA2"
# ============================================================

set -euo pipefail

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  echo "Usage: $0 <maf_vaf.tsv> <output.tsv> [GENES(comma-separated, default: BRCA1,BRCA2)]"
  exit 1
fi

INPUT="$1"
OUTPUT="$2"
GENES="${3:-BRCA1,BRCA2}"

awk -v genes="$GENES" '
BEGIN{
  FS = "[\t ]+"; OFS = "\t";
  n = split(genes, order, ",");           # preserve user-specified order
  for(i=1;i<=n;i++){ target[order[i]]=1; }
}
{
  g = $1;
  if (g in target){
    if (!(g in start)) start[g] = NR;     # first time we see g
    end[g] = NR;                          # keep updating to the last seen line
  }
}
END{
  for(i=1;i<=n;i++){
    g = order[i];
    if (g in start){
      print g, start[g], end[g];
    }
  }
}
' "$INPUT" > "$OUTPUT"

echo "Saved: $OUTPUT"

