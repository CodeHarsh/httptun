#!/bin/bash

set -e

cidr_ip_address=${1:-192.168.0.2/30}

ip link set tun0 up
ip addr add $cidr_ip_address dev $TUN_IFACE

# route every prefix other than the first one (which is address for the tunnel nic) through the tunnel nic
first_ignored=0
for prefix in "$@"; do
    if [ $first_ignored -eq 0 ]; then
        first_ignored=1
    else
        ip route add $prefix dev $TUN_IFACE
    fi
done
