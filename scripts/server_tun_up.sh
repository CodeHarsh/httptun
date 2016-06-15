#!/bin/bash

set -e

cidr_ip_address=${1:-192.168.0.1/30}
out_nic="$2"

if [ "x$out_nic" == "x" ]; then
    out_nic=$(ip route show default 0.0.0.0/0 | cut -d' ' -f5)
    echo "Defaulting to out nic: $out_nic"
fi

ip link set tun0 up
ip addr add $cidr_ip_address dev $TUN_IFACE


# setup NATing (here onwards)
sudo sysctl -w net.ipv4.ip_forward=1

#ensure iptables exists
#    because we'll be running stuff with "set +e"
#    NOTE: if anyone knows a better way of doing it without "set +e", please educate me :-(
iptables -L >/dev/null

set +e
iptables -t nat -C POSTROUTING -o $out_nic -j MASQUERADE
has_r1=$?
iptables -C FORWARD -i $TUN_IFACE -o $out_nic -j ACCEPT
has_r2=$?
iptables -C FORWARD -i $out_nic -o $TUN_IFACE -m state --state RELATED,ESTABLISHED -j ACCEPT
has_r3=$?
set -e

if [ $has_r1 -ne 0 ]; then
    iptables -t nat -A POSTROUTING -o $out_nic -j MASQUERADE
fi
if [ $has_r2 -ne 0 ]; then
    iptables -A FORWARD -i $TUN_IFACE -o $out_nic -j ACCEPT
fi
if [ $has_r3 -ne 0 ]; then
    iptables -A FORWARD -i $out_nic -o $TUN_IFACE -m state --state RELATED,ESTABLISHED -j ACCEPT
fi
