#!/usr/bin/env bash
set -euo pipefail

LOG_DIR="${1:?Usage: $0 <log_dir> [out_tsv]}"
OUT_TSV="${2:-/dev/stdout}"

{
  echo -e "Protocol\tN\tLAN_Prep_ms\tLAN_Online_ms\tWAN_Prep_ms\tWAN_Online_ms\tComm_Prep_KiB\tComm_Online_KiB"

  awk '
  BEGIN {
    order["RingOA   "] = 1
    order["RingOA-FSC"] = 2
    order["Shared OT"] = 3
    order["Bai et al."] = 4

    timer["RingOA   ", "prep"] = "RingOA::Preprocess(P0)"
    timer["RingOA   ", "online"] = "RingOA::Eval(P0)"

    timer["RingOA-FSC", "prep"] = "RingOAFSC::Preprocess(P0)"
    timer["RingOA-FSC", "online"] = "RingOAFSC::Eval(P0)"

    timer["Shared OT", "prep"] = "SharedOT::Preprocess(P0)"
    timer["Shared OT", "online"] = "SharedOT::Eval(P0)"

    timer["Bai et al.", "prep"] = "OblivSelect::SA::Preprocess(P0)"
    timer["Bai et al.", "online"] = "OblivSelect::SA::Eval(P0)"
  }

  function network(path) {
    if (path ~ /LAN/) return "LAN"
    if (path ~ /WAN/) return "WAN"
    return ""
  }

  function protocol(path, name) {
    name = path
    sub(/^.*\//, "", name)

    if (name ~ /ringoa_fsc_/) return "RingOA-FSC"
    if (name ~ /ringoa_/) return "RingOA   "
    if (name ~ /sharedot_/) return "Shared OT"
    if (name ~ /os_sa_/) return "Bai et al."

    return ""
  }

  function phase(path, name) {
    name = path
    sub(/^.*\//, "", name)

    if (name ~ /preproc/) return "prep"
    if (name ~ /online/) return "online"

    return ""
  }

  function to_ms(avg, unit) {
    if (unit == "us" || unit == "µs") return avg / 1000.0
    if (unit == "s") return avg * 1000.0
    return avg
  }

  {
    net = network(FILENAME)
    proto = protocol(FILENAME)
    ph = phase(FILENAME)

    if (net == "" || proto == "" || ph == "") next

    if ($0 ~ /\(P0\),bytes,d=[0-9]+,sent=[0-9]+/) {
      d = $0
      sub(/^.*d=/, "", d)
      sub(/,.*$/, "", d)

      sent = $0
      sub(/^.*sent=/, "", sent)
      sub(/[^0-9].*$/, "", sent)

      comm[proto, d, ph] = sent / 1024.0
      next
    }

    if ($0 ~ /\[CSV\],[0-9]+,"[^"]+",[^,]+,"d=[0-9]+",[0-9]+,[^,]+,[^,]+/) {
      csv = $0
      sub(/^.*\[CSV\],/, "", csv)
      split(csv, f, ",")

      name = f[2]
      gsub(/^"/, "", name)
      gsub(/"$/, "", name)

      if (name != timer[proto, ph]) next

      unit = f[3]

      d = f[4]
      gsub(/^"d=/, "", d)
      gsub(/"$/, "", d)

      avg = f[7]

      time[proto, d, net, ph] = to_ms(avg, unit)
      keys[proto, d] = 1
    }
  }

  END {
    for (k in keys) {
      split(k, a, SUBSEP)
      proto = a[1]
      d = a[2]

      printf "%d\t%d\t%s\t2^%s\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n", \
        order[proto], d, proto, d, \
        time[proto, d, "LAN", "prep"], \
        time[proto, d, "LAN", "online"], \
        time[proto, d, "WAN", "prep"], \
        time[proto, d, "WAN", "online"], \
        comm[proto, d, "prep"], \
        comm[proto, d, "online"]
    }
  }
  ' "${LOG_DIR}"/*.log | sort -t $'\t' -k1,1n -k2,2n | cut -f3-
} > "${OUT_TSV}"
