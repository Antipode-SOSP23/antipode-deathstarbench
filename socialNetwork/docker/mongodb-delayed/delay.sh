#!/bin/bash

DELAY_MS=$2
JITTER_MS=$3
CORRELATION=$([ "$4" = "0%" ] && echo "" || echo "$4")
DISTRIBUTION=$([ "$5" = "uniform" ] && echo "" || echo "distribution $5")

ifconfig -a | grep -e '^eth' | cut -d ':' -f 1 | while read -r INTERFACE ; do
  # clean existing rules
  echo "Removing old rules ..."
  tc qdisc del dev $INTERFACE root
  echo "Done!"

  echo "Adding new rules ..."
  # apply to all eth* interfaces
  tc qdisc add dev $INTERFACE root handle 1: prio

  # apply rules for each docker IP - weird docker stack bs where containers have multiple ips
  for IP in $(dig +short $1); do
    echo "  - Add rule for dst $IP"
    tc filter add dev $INTERFACE parent 1:0 protocol ip prio 1 u32 match ip dst $IP flowid 2:1
  done;

  echo "  - Adding 'netem delay $DELAY_MS $JITTER_MS $CORRELATION $DISTRIBUTION'"
  tc qdisc add dev $INTERFACE parent 1:1 handle 2: netem delay $DELAY_MS $JITTER_MS $CORRELATION $DISTRIBUTION
  echo "Done!"
done;