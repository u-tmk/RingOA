#!/usr/bin/env bash
set -euo pipefail

LOG_DIR="${1:?Usage: $0 <log_dir> [out_tsv]}"
OUT_TSV="${2:-/dev/stdout}"

{
  echo -e "Protocol\tN\tLAN_Prep_ms\tLAN_Online_ms\tWAN_Prep_ms\tWAN_Online_ms\tComm_Prep_KiB\tComm_Online_KiB"

  awk '
  BEGIN {
    order["ORQ     "] = 1
    order["ORQ-FSC "] = 2
    order["SotRange"] = 3

    timer["ORQ     ", "prep"] = "OblivRange::VAF::Preprocess(P1)"
    timer["ORQ     ", "online"] = "OblivRange::VAF::Eval(P1)"

    timer["ORQ-FSC ", "prep"] = "OblivRangeFSC::VAF::Preprocess(P1)"
    timer["ORQ-FSC ", "online"] = "OblivRangeFSC::VAF::Eval(P1)"

    timer["SotRange", "prep"] = "SotRange::Preprocess(P1)"
    timer["SotRange", "online"] = "SotRange::VAF::Eval(P1)"
  }

  function network(path) {
    if (path ~ /LAN/) return "LAN"
    if (path ~ /WAN/) return "WAN"
    return ""
  }

  function protocol(path, name) {
    name = path
    sub(/^.*\//, "", name)

    if (name ~ /orange_fsc_vaf_/) return "ORQ-FSC "
    if (name ~ /orange_vaf_/) return "ORQ     "
    if (name ~ /sotrange_vaf_/) return "SotRange"

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

    if ($0 ~ /\(P[0-9]\),bytes,d=[0-9]+,sent=[0-9]+/) {
      party = $0
      sub(/^.*\(/, "", party)
      sub(/\).*$/, "", party)

      expected_party = (ph == "prep") ? "P0" : "P1"
      if (party != expected_party) next

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
