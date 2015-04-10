#!/bin/sh

# Script to bring up SLIP interface. Assumes interface will receive 'sl0' identifier
# i.e. any existing interface has been brought down using slipdown.sh

# Argument #1: Source (this) IP address ex: 10.0.2.1
# Argument #2: Destination (distant) IP address ex: 10.0.2.2

IPADDR1=${1}
IPADDR2=${2}

sudo mkdir -p /var/slip
sudo modprobe slip 
sudo socat -d -d pty,link=/var/slip/slip1,raw,echo=0 pty,link=/var/slip/slip2,raw,echo=0 &
sleep 2
sudo slattach -p slip -s 115200 /var/slip/slip1 &
sudo ifconfig sl0 ${IPADDR1} pointopoint ${IPADDR2} up
