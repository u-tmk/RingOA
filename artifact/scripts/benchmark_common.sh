#!/usr/bin/env bash

# Shared helpers for RingOA artifact benchmark scripts.
# The caller must define SCRIPT_DIR, PROJECT_ROOT, RINGOA_BIN, DB_BITS,
# LOG_DIR, STDOUT_DIR, SET_NETWORK, RESET_NETWORK, and VALID_NETWORKS.

SUDO_KEEPALIVE_PID=""
NETWORK_CONFIGURED=0

check_executable() {
  local path="$1"
  local description="$2"
  local build_hint="${3:-}"
  local override_hint="${4:-}"

  if [[ -x "${path}" ]]; then
    return 0
  fi

  cat >&2 <<EOF_ERROR
Error: ${description} was not found or is not executable.

Expected path:
  ${path}
EOF_ERROR

  if [[ -n "${build_hint}" ]]; then
    printf '\n%s\n' "${build_hint}" >&2
  fi

  if [[ -n "${override_hint}" ]]; then
    printf '\n%s\n' "${override_hint}" >&2
  fi

  return 1
}

wait_for_processes() {
  local rc=0
  local pid

  for pid in "$@"; do
    wait "${pid}" || rc=1
  done

  return "${rc}"
}

start_sudo_keepalive() {
  echo "Acquiring sudo credentials..."
  sudo -v

  (
    while kill -0 "$$" 2>/dev/null; do
      sudo -n -v 2>/dev/null || exit 1
      sleep 60
    done
  ) &

  SUDO_KEEPALIVE_PID=$!
}

stop_sudo_keepalive() {
  if [[ -z "${SUDO_KEEPALIVE_PID}" ]]; then
    return 0
  fi

  kill "${SUDO_KEEPALIVE_PID}" 2>/dev/null || true
  wait "${SUDO_KEEPALIVE_PID}" 2>/dev/null || true
  SUDO_KEEPALIVE_PID=""
}

cleanup_benchmark() {
  local exit_status=$?

  trap - EXIT INT TERM
  stop_sudo_keepalive

  if [[ "${NETWORK_CONFIGURED}" -eq 1 ]]; then
    echo
    echo "Resetting network configuration..."
    if ! sudo -n "${RESET_NETWORK}"; then
      echo "Warning: Failed to reset the network configuration." >&2
    fi
  fi

  exit "${exit_status}"
}

initialize_benchmark_environment() {
  check_executable \
    "${RINGOA_BIN}" \
    "RingOA benchmark binary" \
    "Build RingOA before running this benchmark." \
    "You can specify a different binary with RINGOA_BIN=/path/to/bench_RingOA."

  check_executable "${SET_NETWORK}" "Network setup script"
  check_executable "${RESET_NETWORK}" "Network reset script"

  mkdir -p "${LOG_DIR}" "${STDOUT_DIR}" "${PARSED_DIR}"

  start_sudo_keepalive
  trap cleanup_benchmark EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
}

validate_network() {
  local network="$1"
  local valid_network

  for valid_network in ${VALID_NETWORKS}; do
    if [[ "${network}" == "${valid_network}" ]]; then
      return 0
    fi
  done

  echo "Invalid network: ${network}" >&2
  echo "Valid networks: ${VALID_NETWORKS}" >&2
  return 1
}

set_benchmark_network() {
  local network="$1"

  validate_network "${network}"

  if [[ "${network}" == "IDEAL" ]]; then
    sudo -n "${RESET_NETWORK}"
  else
    sudo -n "${SET_NETWORK}" "${network}"
  fi

  NETWORK_CONFIGURED=1
}

run_ringoa_3pc() {
  local benchmark_id="$1"
  local name="$2"
  local prefix="$3"
  local network="$4"
  shift 4

  echo "=== ${network}: ${name} ==="

  local pids=()
  local party

  for party in 0 1 2; do
    "${RINGOA_BIN}" \
      -b "${benchmark_id}" \
      -d "${DB_BITS}" \
      -party "${party}" \
      -network "${network}" \
      -log_dir "${LOG_DIR}" \
      -no_log_timestamp \
      "$@" \
      > "${STDOUT_DIR}/${network}_${prefix}_p${party}.out" 2>&1 &

    pids+=("$!")
  done

  if ! wait_for_processes "${pids[@]}"; then
    echo "Error: ${name} failed on ${network}." >&2
    echo "See ${STDOUT_DIR} for details." >&2
    return 1
  fi
}

print_network_header() {
  local network="$1"

  echo
  echo "============================================="
  echo "Network: ${network}"
  echo "============================================="
}
