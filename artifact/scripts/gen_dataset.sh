#!/usr/bin/env bash
set -euo pipefail

BIN="${BIN:-./out/build/linux/bin/bench_RingOA}"

# Used for Oblivious Access and FM-Index.
DB_BITS="${DB_BITS:-20:26:2}"

# Range Search internally uses fixed 2^25,
# but the option is still required.
RANGE_DB_BITS="${RANGE_DB_BITS:-25}"

OUT_DIR="${OUT_DIR:-./data/logs/misc}"

usage() {
  echo "Usage: $0 <oa|fts|range> [real]"
  echo
  echo "Examples:"
  echo "  $0 oa"
  echo "  $0 fts"
  echo "  $0 fts real"
  echo "  $0 range"
  echo "  $0 range real"
  echo
  echo "Environment variables:"
  echo "  DB_BITS=20:26:2     # DB sizes for OA and FM-Index"
  echo "  RANGE_DB_BITS=25    # DB size option for Range Search"
  exit 1
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
fi

TARGET="$1"
DATA_MODE="${2:-synthetic}"

case "${TARGET}" in
  oa|fts|range) ;;
  *) usage ;;
esac

case "${DATA_MODE}" in
  synthetic|real) ;;
  *) usage ;;
esac

if [[ "${TARGET}" == "oa" && "${DATA_MODE}" == "real" ]]; then
  echo "Error: Oblivious Access datasets do not support real data mode."
  echo
  usage
fi

mkdir -p "${OUT_DIR}"

run_dataset() {
  local bench_id="$1"
  local name="$2"
  local db_bits="$3"
  local out_file="$4"
  shift 4

  local cmd=(
    "${BIN}"
    -b "${bench_id}"
    -d "${db_bits}"
    "$@"
  )

  echo "[Run] ${name}"
  echo "Command: ${cmd[*]}"

  "${cmd[@]}" > "${OUT_DIR}/${out_file}" 2>&1

  echo "[Done] ${OUT_DIR}/${out_file}"
}

extra_args=()
data_suffix="synthetic"

if [[ "${DATA_MODE}" == "real" ]]; then
  extra_args=(-real)
  data_suffix="real"
fi

case "${TARGET}" in
  oa)
    run_dataset 0 "Oblivious Access" \
      "${DB_BITS}" "oa_dataset.log"
    ;;

  fts)
    run_dataset 1 "FM-Index (${data_suffix})" \
      "${DB_BITS}" "fts_${data_suffix}_dataset.log" \
      "${extra_args[@]}"
    ;;

  range)
    run_dataset 2 "Range Search (${data_suffix})" \
      "${RANGE_DB_BITS}" "range_${data_suffix}_dataset.log" \
      "${extra_args[@]}"
    ;;
esac

echo "Dataset generation completed successfully."
echo "Logs are stored in: ${OUT_DIR}"
