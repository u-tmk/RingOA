#!/usr/bin/env bash

set -e

IFACE="lo"

if [ "$EUID" -ne 0 ]; then
  echo "Please run with sudo"
  exit 1
fi

MODE="$1"

if [ -z "$MODE" ]; then
  echo "Usage: $0 {LAN|MAN|WAN}"
  exit 1
fi

# Clean existing qdisc
tc qdisc del dev $IFACE root >/dev/null 2>&1 || true

case "$MODE" in
  LAN)
    echo "Setting LAN profile: RTT ~0.2ms, 10Gbps"
    tc qdisc add dev $IFACE root handle 1: tbf rate 10gbit burst 1mb limit 40mb
    tc qdisc add dev $IFACE parent 1:1 handle 10: netem delay 0.1ms
    ;;

  MAN)
    echo "Setting MAN profile: RTT ~5ms, 400Mbps"
    tc qdisc add dev $IFACE root handle 1: tbf rate 400mbit burst 1mb limit 40mb
    tc qdisc add dev $IFACE parent 1:1 handle 10: netem delay 2.5ms
    ;;

  WAN)
    echo "Setting WAN profile: RTT ~100ms, 10Mbps"
    tc qdisc add dev $IFACE root handle 1: tbf rate 10mbit burst 1mb limit 40mb
    tc qdisc add dev $IFACE parent 1:1 handle 10: netem delay 50ms
    ;;

  *)
    echo "Invalid option: $MODE"
    echo "Usage: $0 {LAN|MAN|WAN}"
    exit 1
    ;;
esac
