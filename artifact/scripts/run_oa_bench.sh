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
DUORAM_BIN="${DUORAM_BIN:-${PROJECT_ROOT}/artifact/baseline/prac/out/prac}"
SET_NETWORK="${SET_NETWORK:-${SCRIPT_DIR}/set_network.sh}"
RESET_NETWORK="${RESET_NETWORK:-${SCRIPT_DIR}/reset_network.sh}"
PARSE_OA_SCRIPT="${PARSE_OA_SCRIPT:-${SCRIPT_DIR}/parse_oa_log.sh}"
PARSE_DUORAM_SCRIPT="${PARSE_DUORAM_SCRIPT:-${SCRIPT_DIR}/parse_duoram_log.sh}"

THREADS="${THREADS:-1}"
HOST="${HOST:-localhost}"
DB_BITS="${DB_BITS:-20:26:2}"
NETWORKS="${NETWORKS:-LAN WAN}"
VALID_NETWORKS="IDEAL LAN MAN WAN"
RUN_BYTES="${RUN_BYTES:-1}"

RUN_ID="${RUN_ID:-$(date +%Y%m%d_%H%M%S)_oa}"
RUN_DIR="${RUN_DIR:-${PROJECT_ROOT}/data/results/${RUN_ID}}"

LOG_DIR="${RUN_DIR}/logs"
STDOUT_DIR="${RUN_DIR}/stdout"
PARSED_DIR="${RUN_DIR}/parsed"
META="${RUN_DIR}/run_meta.txt"
DUORAM_LOG="${LOG_DIR}/duoram_all.log"
OA_TABLE="${PARSED_DIR}/oa_table.tsv"
DUORAM_DATA_DIR="${DUORAM_DATA_DIR:-${PROJECT_ROOT}/data/duoram}"

source "${SCRIPT_DIR}/benchmark_common.sh"

check_executable \
  "${DUORAM_BIN}" \
  "DuORAM benchmark binary" \
  "Build the DuORAM baseline by following artifact/baseline/README.md." \
  "You can specify a different binary with DUORAM_BIN=/path/to/prac."
check_executable "${PARSE_OA_SCRIPT}" "OA log parser"
check_executable "${PARSE_DUORAM_SCRIPT}" "DuORAM log parser"
initialize_benchmark_environment

: > "${DUORAM_LOG}"
mkdir -p "${DUORAM_DATA_DIR}"

expand_bits() {
  local spec="$1"

  if [[ "${spec}" =~ ^[0-9]+$ ]]; then
    echo "${spec}"
  elif [[ "${spec}" =~ ^[0-9]+(,[0-9]+)+$ ]]; then
    tr ',' '\n' <<< "${spec}"
  elif [[ "${spec}" =~ ^([0-9]+):([0-9]+)$ ]]; then
    seq "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
  elif [[ "${spec}" =~ ^([0-9]+):([0-9]+):([0-9]+)$ ]]; then
    seq "${BASH_REMATCH[1]}" "${BASH_REMATCH[3]}" "${BASH_REMATCH[2]}"
  else
    echo "Invalid DB_BITS specification: ${spec}" >&2
    echo "Supported formats: 20, 20,22,24, 20:24, 20:24:2" >&2
    exit 1
  fi
}

append_duoram_party_logs() {
  local phase="$1"
  local network="$2"
  local bits="$3"
  local count="$4"
  local trial="$5"
  local out0="$6"
  local out1="$7"
  local out2="$8"

  {
    echo
    echo "============================================================"
    echo "Phase:   ${phase}"
    echo "Network: ${network}"
    echo "Bitsize: ${bits}"
    echo "Count:   ${count}"
    echo "Trial:   ${trial}"
    echo "============================================================"
    echo
    echo "[P0]"
    cat "${out0}"
    echo
    echo "[P1]"
    cat "${out1}"
    echo
    echo "[P2]"
    cat "${out2}"
    echo
  } >> "${DUORAM_LOG}"
}

run_duoram_preprocess_once() {
  local network="$1"
  local bits="$2"
  local count="$3"
  local trial="$4"
  local phase="$5"
  local arg="r${bits}:${count}"
  local out0 out1 out2 rc

  out0="$(mktemp)"
  out1="$(mktemp)"
  out2="$(mktemp)"

  echo "=== ${network}: DuORAM ${phase} b=${bits} count=${count} trial=${trial} ===" \
    >> "${DUORAM_LOG}"

  set +e
  "${DUORAM_BIN}" -t "${THREADS}" -p 0 > "${out0}" 2>&1 &
  local p0=$!
  sleep 0.1
  "${DUORAM_BIN}" -t "${THREADS}" -p 1 "${HOST}" > "${out1}" 2>&1 &
  local p1=$!
  sleep 0.1
  "${DUORAM_BIN}" -t "${THREADS}" -p 2 "${HOST}" "${HOST}" "${arg}" \
    > "${out2}" 2>&1 &
  local p2=$!
  wait_for_processes "${p0}" "${p1}" "${p2}"
  rc=$?
  set -e

  append_duoram_party_logs \
    "${phase}" "${network}" "${bits}" "${count}" "${trial}" \
    "${out0}" "${out1}" "${out2}"
  rm -f "${out0}" "${out1}" "${out2}"

  if [[ "${rc}" -ne 0 ]]; then
    echo "Error: DuORAM ${phase} failed. See ${DUORAM_LOG}." >&2
    exit 1
  fi
}

run_duoram_online_once() {
  local network="$1"
  local bits="$2"
  local count="$3"
  local trial="$4"
  local out0 out1 out2 rc

  out0="$(mktemp)"
  out1="$(mktemp)"
  out2="$(mktemp)"

  echo "=== ${network}: DuORAM Online b=${bits} count=${count} trial=${trial} ===" \
    >> "${DUORAM_LOG}"

  set +e
  "${DUORAM_BIN}" -t "${THREADS}" 0 read "${bits}" "${count}" \
    > "${out0}" 2>&1 &
  local p0=$!
  sleep 0.1
  "${DUORAM_BIN}" -t "${THREADS}" 1 "${HOST}" read "${bits}" "${count}" \
    > "${out1}" 2>&1 &
  local p1=$!
  sleep 0.1
  "${DUORAM_BIN}" -t "${THREADS}" 2 "${HOST}" "${HOST}" read "${bits}" "${count}" \
    > "${out2}" 2>&1 &
  local p2=$!
  wait_for_processes "${p0}" "${p1}" "${p2}"
  rc=$?
  set -e

  append_duoram_party_logs \
    "Online" "${network}" "${bits}" "${count}" "${trial}" \
    "${out0}" "${out1}" "${out2}"
  rm -f "${out0}" "${out1}" "${out2}"

  if [[ "${rc}" -ne 0 ]]; then
    echo "Error: DuORAM Online failed. See ${DUORAM_LOG}." >&2
    exit 1
  fi
}

get_preprocess_reps() {
  local bits="$1"

  if (( bits <= 24 )); then
    echo 10
  else
    echo 3
  fi
}

get_online_count() {
  local bits="$1"

  if (( bits <= 15 )); then
    echo 150
  elif (( bits <= 17 )); then
    echo 100
  elif (( bits <= 20 )); then
    echo 10
  elif (( bits <= 24 )); then
    echo 5
  else
    echo 1
  fi
}

get_online_reps() {
  local bits="$1"

  if (( bits <= 17 )); then
    echo 10
  elif (( bits <= 24 )); then
    echo 5
  else
    echo 3
  fi
}

run_duoram_preprocess_measurements() {
  local network="$1"

  echo "=== ${network}: DuORAM Preprocess ==="

  for bits in $(expand_bits "${DB_BITS}"); do
    local reps
    reps="$(get_preprocess_reps "${bits}")"

    for trial in $(seq 1 "${reps}"); do
      run_duoram_preprocess_once \
        "${network}" "${bits}" 1 "${trial}" "Preprocess"
    done
  done
}

prepare_duoram_online_keys() {
  local network="$1"

  echo "=== ${network}: DuORAM Online Setup ==="

  for bits in $(expand_bits "${DB_BITS}"); do
    local count
    count="$(get_online_count "${bits}")"
    run_duoram_preprocess_once \
      "${network}" "${bits}" "${count}" 1 "OnlineSetup"
  done
}

run_duoram_online_measurements() {
  local network="$1"

  echo "=== ${network}: DuORAM Online ==="

  for bits in $(expand_bits "${DB_BITS}"); do
    local count reps
    count="$(get_online_count "${bits}")"
    reps="$(get_online_reps "${bits}")"

    for trial in $(seq 1 "${reps}"); do
      run_duoram_online_once \
        "${network}" "${bits}" "${count}" "${trial}"
    done
  done
}

run_network() {
  local network="$1"

  echo
  echo "============================================="
  echo "Network: ${network}"
  echo "============================================="

  set_benchmark_network "${network}"

  cat >> "${META}" <<EOF_META
network=${network}
network_started_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META

  run_ringoa_3pc 3 "RingOA Preprocess" "ringoa_preproc" "${network}"
  run_ringoa_3pc 4 "RingOA Online" "ringoa_online" "${network}"
  run_ringoa_3pc 5 "RingOA FSC Preprocess" "ringoa_fsc_preproc" "${network}"
  run_ringoa_3pc 6 "RingOA FSC Online" "ringoa_fsc_online" "${network}"
  run_ringoa_3pc 7 "SharedOT Preprocess" "sharedot_preproc" "${network}"
  run_ringoa_3pc 8 "SharedOT Online" "sharedot_online" "${network}"
  run_ringoa_3pc 9 "Bai et al. Preprocess" "os_sa_preproc" "${network}"
  run_ringoa_3pc 10 "Bai et al. Online" "os_sa_online" "${network}"

  run_duoram_preprocess_measurements "${network}"
  prepare_duoram_online_keys "${network}"
  run_duoram_online_measurements "${network}"

  cat >> "${META}" <<EOF_META
network_finished_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META
}

run_duoram_bytes_once() {
  local network="IDEAL"
  local count=1
  local trial=1

  echo
  echo "=== IDEAL: DuORAM Communication ==="
  set_benchmark_network "IDEAL"

  cat >> "${META}" <<EOF_META
bytes_network=${network}
bytes_started_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META

  for bits in $(expand_bits "${DB_BITS}"); do
    run_duoram_preprocess_once \
      "${network}" "${bits}" "${count}" "${trial}" "Preprocess"
    run_duoram_online_once \
      "${network}" "${bits}" "${count}" "${trial}"
  done

  cat >> "${META}" <<EOF_META
bytes_finished_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META
}

cat > "${META}" <<EOF_META
run_id=${RUN_ID}
ringoa_bin=${RINGOA_BIN}
duoram_bin=${DUORAM_BIN}
threads=${THREADS}
host=${HOST}
db_bits=${DB_BITS}
networks=${NETWORKS}
run_bytes=${RUN_BYTES}
preprocess_plan=bits=10-24 count=1 reps=10; bits=25-30 count=1 reps=3
online_plan=bits=10-15 count=150 reps=10; bits=16-17 count=100 reps=10; bits=18-20 count=10 reps=5; bits=21-24 count=5 reps=5; bits=25-30 count=1 reps=3
bytes_plan=IDEAL count=1 reps=1 bits=${DB_BITS}
started_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META

echo "Run directory: ${RUN_DIR}"
echo "Log directory: ${LOG_DIR}"
echo "Networks: ${NETWORKS}"

for network in ${NETWORKS}; do
  run_network "${network}"
done

if [[ "${RUN_BYTES}" == "1" ]]; then
  run_duoram_bytes_once
fi

cat >> "${META}" <<EOF_META
finished_at=$(date '+%Y-%m-%d %H:%M:%S')
EOF_META

echo
echo "Parsing logs..."
"${PARSE_OA_SCRIPT}" "${LOG_DIR}" > "${OA_TABLE}"

# The RingOA parser writes the shared column header. Append only DuORAM rows.
"${PARSE_DUORAM_SCRIPT}" "${DUORAM_LOG}" | tail -n +2 >> "${OA_TABLE}"
cat "${OA_TABLE}"

echo
echo "Benchmark completed."
echo "Results are stored in: ${RUN_DIR}"
echo "Parsed table: ${OA_TABLE}"
