#!/bin/bash

IP=$1
DELAY_MS=$2
JITTER_MS=$3

# clean existing rules
# apply to all eth* interfaces
ifconfig -a | grep -e '^eth' | cut -d ':' -f 1 | while read -r INTERFACE ; do
  tc qdisc del dev $INTERFACE root
  echo "APPLY $INTERFACE"
done

# apply rules
ifconfig -a | grep -e '^eth' | cut -d ':' -f 1 | while read -r INTERFACE ; do
  tc qdisc add dev $INTERFACE root handle 1: prio
  tc filter add dev $INTERFACE parent 1:0 protocol ip prio 1 u32 match ip dst $IP flowid 2:1
  tc qdisc add dev $INTERFACE parent 1:1 handle 2: netem delay $DELAY_MS $JITTER_MS
done;