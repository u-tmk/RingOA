#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(
  cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1
  pwd
)"
PROJECT_ROOT="$(
  cd -- "${SCRIPT_DIR}/../.." >/dev/null 2>&1
  pwd
)"

RINGOA_BIN="${RINGOA_BIN:-${PROJECT_ROOT}/out/build/linux/bin/bench_RingOA}"
SET_NETWORK="${SET_NETWORK:-${SCRIPT_DIR}/set_network.sh}"
RESET_NETWORK="${RESET_NETWORK:-${SCRIPT_DIR}/reset_network.sh}"
PARSE_SCRIPT="${PARSE_SCRIPT:-${SCRIPT_DIR}/parse_fts_log.sh}"

DB_BITS="${DB_BITS:-20:26:2}"
NETWORKS="${NETWORKS:-LAN WAN}"
VALID_NETWORKS="LAN MAN WAN"

RUN_ID="${RUN_ID:-$(date +%Y%m%d_%H%M%S)_fts}"
RUN_DIR="${RUN_DIR:-${PROJECT_ROOT}/data/results/${RUN_ID}}"

LOG_DIR="${RUN_DIR}/logs"
STDOUT_DIR="${RUN_DIR}/stdout"
PARSED_DIR="${RUN_DIR}/parsed"
META="${RUN_DIR}/run_meta.txt"
PARSED_TABLE="${PARSED_DIR}/fts_table.tsv"

source "${SCRIPT_DIR}/benchmark_common.sh"

check_executable "${PARSE_SCRIPT}" "FTS log parser"
initialize_benchmark_environment

run_benchmarks_for_network() {
  local network="$1"

  run_ringoa_3pc 11 "OblivFMI Preprocess" "ofmi_preproc" "${network}"
  run_ringoa_3pc 12 "OblivFMI Online" "ofmi_online" "${network}"
  run_ringoa_3pc 13 "OblivFMI FSC Preprocess" "ofmi_fsc_preproc" "${network}"
  run_ringoa_3pc 14 "OblivFMI FSC Online" "ofmi_fsc_online" "${network}"
  run_ringoa_3pc 15 "SotFMI Preprocess" "sotfmi_preproc" "${network}"
  run_ringoa_3pc 16 "SotFMI Online" "sotfmi_online" "${network}"
}

run_network() {
  local network="$1"

  print_network_header "${network}"
  set_benchmark_network "${network}"

  cat >> "${META}" <<EOF_META
network=${network}
network_started_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META

  run_benchmarks_for_network "${network}"

  cat >> "${META}" <<EOF_META
network_finished_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META
}

cat > "${META}" <<EOF_META
run_id=${RUN_ID}
ringoa_bin=${RINGOA_BIN}
db_bits=${DB_BITS}
networks=${NETWORKS}
started_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META

echo "Run directory: ${RUN_DIR}"
echo "Log directory: ${LOG_DIR}"
echo "Networks: ${NETWORKS}"

for network in ${NETWORKS}; do
  run_network "${network}"
done

cat >> "${META}" <<EOF_META
finished_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META

echo
echo "Parsing logs..."
"${PARSE_SCRIPT}" "${LOG_DIR}" | tee "${PARSED_TABLE}"

echo
echo "Benchmark completed."
echo "Results are stored in: ${RUN_DIR}"
echo "Parsed table: ${PARSED_TABLE}"
