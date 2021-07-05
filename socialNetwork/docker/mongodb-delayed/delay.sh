#!/bin/bash

INTERFACE=eth0
IP=$(ping -c 1 $1 | head -n1 | cut -d ')' -f 1 | cut -d '(' -f 2)
DELAY_MS=$2

tc qdisc add dev $INTERFACE root handle 1: prio
tc filter add dev $INTERFACE parent 1:0 protocol ip prio 1 u32 match ip dst $IP flowid 2:1
tc qdisc add dev $INTERFACE parent 1:1 handle 2: netem delay $DELAY_MS