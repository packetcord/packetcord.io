#!/bin/bash

# Script to set up network namespaces for iperf3 testing between two interfaces
# This creates two isolated network namespaces and connects them via physical interfaces

set -e  # Exit on error

# Configuration
NS1="ns1"
NS2="ns2"
IF1="enp11s0f0np0"
IF2="enp11s0f1np1"
IP1="192.168.168.1/30"
IP2="192.168.168.2/30"

echo "=== Setting up network namespaces for iperf3 testing ==="

# Clean up any existing namespaces (optional)
echo "Cleaning up existing namespaces..."
sudo ip netns del $NS1 2>/dev/null || true
sudo ip netns del $NS2 2>/dev/null || true

# Create network namespaces
echo "Creating network namespaces: $NS1, $NS2"
sudo ip netns add $NS1
sudo ip netns add $NS2

# Move interfaces to namespaces
echo "Moving $IF1 to $NS1..."
sudo ip link set $IF1 netns $NS1

echo "Moving $IF2 to $NS2..."
sudo ip link set $IF2 netns $NS2

# Configure namespace 1
echo "Configuring $NS1..."
sudo ip netns exec $NS1 ip link set $IF1 up
sudo ip netns exec $NS1 ip addr add $IP1 dev $IF1
sudo ip netns exec $NS1 ip link set lo up

# Configure namespace 2
echo "Configuring $NS2..."
sudo ip netns exec $NS2 ip link set $IF2 up
sudo ip netns exec $NS2 ip addr add $IP2 dev $IF2
sudo ip netns exec $NS2 ip link set lo up

# Show configuration
echo ""
echo "=== Configuration Summary ==="
echo "Namespace $NS1:"
sudo ip netns exec $NS1 ip addr show $IF1

echo ""
echo "Namespace $NS2:"
sudo ip netns exec $NS2 ip addr show $IF2

# Test connectivity
echo ""
echo "=== Testing connectivity ==="
echo "Ping from $NS1 to $NS2..."
sudo ip netns exec $NS1 ping -c 3 192.168.168.2

echo ""
echo "Ping from $NS2 to $NS1..."
sudo ip netns exec $NS2 ping -c 3 192.168.168.1

echo ""
echo "=== Setup complete! ==="
echo ""

echo ""
echo "To clean up later:"
echo "  sudo ./connectx_test_cleanup.sh"
echo "Or delete the namespaces manually: "
echo "  sudo ip netns del $NS1"
echo "  sudo ip netns del $NS2"
