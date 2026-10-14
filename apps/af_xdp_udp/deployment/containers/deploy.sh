#!/bin/bash

echo '=========================================='
echo 'Topology Emulation - Container Deployment'
echo '=========================================='

#
# Setup hugepages
#
echo 256 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# Verify hugepage allocation
echo 'Available hugepages:' && cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

#
# Build container images
#
docker build -t alpine_af_xdp_udp:latest .

#
# Launch containers
#
docker run --privileged -dit --name container_1 --memory=1024m -v /dev/hugepages:/dev/hugepages alpine_af_xdp_udp:latest
docker run --privileged -dit --name container_2 --memory=1024m -v /dev/hugepages:/dev/hugepages alpine_af_xdp_udp:latest

#
# Set hostnames inside containers
#
docker exec container_1 hostname container_1
docker exec container_2 hostname container_2

#
# Expose network namespaces
#
container_1_pid="$(docker inspect -f '{{.State.Pid}}' "container_1")"
container_2_pid="$(docker inspect -f '{{.State.Pid}}' "container_2")"

sudo mkdir -p /var/run/netns

sudo ln -s /proc/${container_1_pid}/ns/net /var/run/netns/container_1
sudo ln -s /proc/${container_2_pid}/ns/net /var/run/netns/container_2

#
# Create veth pairs
#
sudo ip link add veth1 type veth peer name veth2

#
# Assign veth interfaces to containers
#
sudo ip link set veth1 netns container_1
sudo ip link set veth2 netns container_2

#
# Configure veth interfaces
#
sudo ip netns exec container_1 ip link set veth1 address 02:42:AC:AA:AA:AA
sudo ip netns exec container_1 ip link set veth1 up
sudo ip netns exec container_1 ip addr add 172.16.16.1/24 dev veth1
sudo ip netns exec container_1 ethtool --offload veth1 rx off tx off 2>/dev/null || true

sudo ip netns exec container_2 ip link set veth2 address 02:42:AC:BB:BB:BB
sudo ip netns exec container_2 ip link set veth2 up
sudo ip netns exec container_2 ip addr add 172.16.16.2/24 dev veth2
sudo ip netns exec container_2 ethtool --offload veth2 rx off tx off 2>/dev/null || true

#
# Copy Network Apps into containers
#

#
# Execute shell commands inside containers
#

echo '=========================================='
echo 'Deployment complete!'
echo '=========================================='

# List deployed containers
docker ps --filter 'name=container_1|container_2'