#!/usr/bin/env bash

set -e

IFACE="lo"

if [ "$EUID" -ne 0 ]; then
  echo "Please run with sudo"
  exit 1
fi

echo "Resetting network settings..."

tc qdisc del dev $IFACE root >/dev/null 2>&1 || true

echo "Done. Network emulation removed."
