#!/usr/bin/env bash
# extract_and_clean_grch38_chr.sh
#
# Purpose:
#   Download the GRCh38 reference genome, index it, extract selected chromosomes,
#   convert sequences to uppercase, replace ambiguous bases with N, and count bases.
#
# Requirements:
#   - wget
#   - gunzip
#   - samtools
#   - python3
#   - helper scripts in the same directory:
#       count_base.py
#       to_upper_fasta.py
#       replace_ambiguous_base.py
#
# Default behavior:
#   Prepare chr1 through chr6.
#
# Working directory:
#   All downloaded files, intermediate files, cleaned FASTA files, and count reports
#   are written to:
#     ./data/bench/grch38

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
WORK_DIR="${PROJECT_ROOT}/data/bench/grch38"

mkdir -p "$WORK_DIR"

COUNT_BASE="${SCRIPT_DIR}/count_base.py"
TO_UPPER="${SCRIPT_DIR}/to_upper_fasta.py"
REPLACE_AMBIG="${SCRIPT_DIR}/replace_ambiguous_base.py"

BASE_URL="https://ftp.ncbi.nlm.nih.gov/genomes/all/GCF/000/001/405/GCF_000001405.26_GRCh38"
FA_GZ="GCF_000001405.26_GRCh38_genomic.fna.gz"
FA="GCF_000001405.26_GRCh38_genomic.fna"

# Use data/bench/grch38 as the working directory.
cd "$WORK_DIR"

# Default targets: chr1 through chr6.
# Override with arguments, for example:
#   bash extract_and_clean_grch38_chr.sh 1 2 X Y MT
if [[ "$#" -gt 0 ]]; then
  TARGETS=("$@")
else
  TARGETS=(1 2 3 4 5 6)
fi

# RefSeq accession map for GRCh38.
declare -A ACC=(
  [1]=NC_000001.11
  [2]=NC_000002.12
  [3]=NC_000003.12
  [4]=NC_000004.12
  [5]=NC_000005.10
  [6]=NC_000006.12
  [7]=NC_000007.14
  [8]=NC_000008.11
  [9]=NC_000009.12
  [10]=NC_000010.11
  [11]=NC_000011.10
  [12]=NC_000012.12
  [13]=NC_000013.11
  [14]=NC_000014.9
  [15]=NC_000015.10
  [16]=NC_000016.10
  [17]=NC_000017.11
  [18]=NC_000018.10
  [19]=NC_000019.10
  [20]=NC_000020.11
  [21]=NC_000021.9
  [22]=NC_000022.11
  [X]=NC_000023.11
  [Y]=NC_000024.10
  [MT]=NC_012920.1
)

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "ERROR: '$1' not found." >&2
    exit 1
  }
}

need_file() {
  [[ -f "$1" ]] || {
    echo "ERROR: required file '$1' not found." >&2
    exit 1
  }
}

need_cmd wget
need_cmd gunzip
need_cmd samtools
need_cmd python3

need_file "$COUNT_BASE"
need_file "$TO_UPPER"
need_file "$REPLACE_AMBIG"

fetch_if_needed() {
  if [[ ! -f "$FA" && ! -f "$FA_GZ" ]]; then
    echo "[+] Downloading ${FA_GZ}"
    wget "${BASE_URL}/${FA_GZ}"
  fi

  if [[ ! -f "$FA" ]]; then
    echo "[+] Decompressing ${FA_GZ}"
    gunzip -f "$FA_GZ"
  fi

  if [[ ! -f "${FA}.fai" ]]; then
    echo "[+] Building FASTA index"
    samtools faidx "$FA"
  fi
}

extract_and_process() {
  local c="$1"
  local acc="${ACC[$c]:-}"

  if [[ -z "$acc" ]]; then
    echo "ERROR: unknown chromosome target '${c}'." >&2
    exit 1
  fi

  local tmp_dir
  tmp_dir="$(mktemp -d)"
  trap 'rm -rf "$tmp_dir"' RETURN

  local chr_fa="${tmp_dir}/chr${c}.fa"
  local upper_fa="${tmp_dir}/chr${c}_upper.fa"
  local clean_fa="chr${c}_clean.fa"

  echo "[+] Extracting chr${c} (${acc})"
  samtools faidx "$FA" "$acc" > "$chr_fa"

  echo "    - Converting to uppercase"
  python3 "$TO_UPPER" "$chr_fa" "$upper_fa"

  echo "    - Replacing ambiguous bases with N"
  python3 "$REPLACE_AMBIG" "$upper_fa" "$clean_fa"

  echo "    - Counting cleaned bases"
  python3 "$COUNT_BASE" "$clean_fa" > "chr${c}_clean.count.txt"
}

fetch_if_needed

for c in "${TARGETS[@]}"; do
  extract_and_process "$c"
done

echo "[✓] Completed."
echo "    Working directory: ${WORK_DIR}"
echo "    Reference FASTA: ${WORK_DIR}/${FA}"
echo "    FASTA index: ${WORK_DIR}/${FA}.fai"
echo "    Extracted FASTA files: ${WORK_DIR}/chr*.fa"
echo "    Uppercase FASTA files: ${WORK_DIR}/chr*_upper.fa"
echo "    Cleaned FASTA files: ${WORK_DIR}/chr*_clean.fa"
echo "    Count reports: ${WORK_DIR}/chr*.count.txt, ${WORK_DIR}/chr*_upper.count.txt, ${WORK_DIR}/chr*_clean.count.txt"
