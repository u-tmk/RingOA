#!/usr/bin/env bash
set -euo pipefail

INPUT="${1:?Usage: $0 <duoram_all.log | log_dir> [out_tsv]}"
OUT_TSV="${2:-/dev/stdout}"

if [[ -d "${INPUT}" ]]; then
  LOG_FILES=("${INPUT}"/*.log)
else
  LOG_FILES=("${INPUT}")
fi

{
  echo -e "Protocol\tN\tLAN_Prep_ms\tLAN_Online_ms\tWAN_Prep_ms\tWAN_Online_ms\tComm_Prep_KiB\tComm_Online_KiB"

  awk '
  function reset_section() {
    phase = ""
    network = ""
    bits = ""
    count = ""
    trial = ""
    party = ""
  }

  function phase_key(p) {
    if (p == "Preprocess") return "prep"
    if (p == "Online") return "online"
    return ""
  }

  function add_time(net, d, ph, value_ms) {
    time_sum[d, net, ph] += value_ms
    time_cnt[d, net, ph] += 1
    keys[d] = 1
  }

  BEGIN {
    reset_section()
  }

  /^Phase:[[:space:]]+/ {
    phase = $2
    next
  }

  /^Network:[[:space:]]+/ {
    network = $2
    next
  }

  /^Bitsize:[[:space:]]+/ {
    bits = $2
    next
  }

  /^Count:[[:space:]]+/ {
    count = $2
    next
  }

  /^Trial:[[:space:]]+/ {
    trial = $2
    next
  }

  /^\[P0\]/ {
    party = "P0"
    next
  }

  /^\[P1\]/ {
    party = "P1"
    next
  }

  /^\[P2\]/ {
    party = "P2"
    next
  }

  party == "P0" && phase == "Preprocess" && network != "" && bits != "" && count != "" &&
  /microseconds wall clock time/ {
    ph = phase_key(phase)

    # Preprocessing generated count values, so convert total wall time to per-value time.
    value_ms = ($1 / count) / 1000.0
    add_time(network, bits, ph, value_ms)
    next
  }

  party == "P0" && phase == "Online" && network != "" && bits != "" &&
  /avg_read_us:/ {
    ph = phase_key(phase)

    value_us = $2
    value_ms = value_us / 1000.0
    add_time(network, bits, ph, value_ms)
    next
  }

  party == "P2" && network == "IDEAL" && bits != "" &&
  /message bytes sent/ {
    ph = phase_key(phase)
    if (ph != "") {
      comm[bits, ph] = $1 / 1024.0
      keys[bits] = 1
    }
    next
  }

  END {
    for (d in keys) {
      lan_prep = ""
      lan_online = ""
      wan_prep = ""
      wan_online = ""
      comm_prep = ""
      comm_online = ""

      if (time_cnt[d, "LAN", "prep"] > 0) {
        lan_prep = time_sum[d, "LAN", "prep"] / time_cnt[d, "LAN", "prep"]
      }
      if (time_cnt[d, "LAN", "online"] > 0) {
        lan_online = time_sum[d, "LAN", "online"] / time_cnt[d, "LAN", "online"]
      }
      if (time_cnt[d, "WAN", "prep"] > 0) {
        wan_prep = time_sum[d, "WAN", "prep"] / time_cnt[d, "WAN", "prep"]
      }
      if (time_cnt[d, "WAN", "online"] > 0) {
        wan_online = time_sum[d, "WAN", "online"] / time_cnt[d, "WAN", "online"]
      }
      if ((d, "prep") in comm) {
        comm_prep = comm[d, "prep"]
      }
      if ((d, "online") in comm) {
        comm_online = comm[d, "online"]
      }

      printf "%d\tDuORAM  \t2^%s\t", d, d

      if (lan_prep == "") printf "\t"; else printf "%.2f\t", lan_prep
      if (lan_online == "") printf "\t"; else printf "%.2f\t", lan_online
      if (wan_prep == "") printf "\t"; else printf "%.2f\t", wan_prep
      if (wan_online == "") printf "\t"; else printf "%.2f\t", wan_online
      if (comm_prep == "") printf "\t"; else printf "%.2f\t", comm_prep
      if (comm_online == "") printf "\n"; else printf "%.2f\n", comm_online
    }
  }
  ' "${LOG_FILES[@]}" | sort -t $'\t' -k1,1n | cut -f2-

} > "${OUT_TSV}"
