#!/bin/bash

PATCH_APP_NAME='l2_dpdk_patch_app'
PATCH_APP_LOCATION='../../../build/apps/l2_dpdk_patch/'$PATCH_APP_NAME

echo '=========================================='
echo 'Topology Emulation - Container Deployment'
echo '=========================================='

#
# Setup hugepages
#
# Allocating 2048 x 2MB hugepages = 4.0GB total
echo 2048 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

# Verify hugepage allocation
echo 'Available hugepages:' && cat /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

#
# Build container images
#
docker pull packetcord/alpine-dpdk:latest
docker pull packetcord/alpine-iperf3:latest

#
# Launch containers
#
docker run --privileged -dit --name node_a --memory=512m packetcord/alpine-iperf3:latest
docker run --privileged -dit --name patch --memory=1024m --cpuset-cpus=0,2,3 -v /dev/hugepages:/dev/hugepages packetcord/alpine-dpdk:latest
docker run --privileged -dit --name node_b --memory=512m packetcord/alpine-iperf3:latest

#
# Set hostnames inside containers
#
docker exec node_a hostname node_a
docker exec patch hostname patch
docker exec node_b hostname node_b

#
# Expose network namespaces
#
node_a_pid="$(docker inspect -f '{{.State.Pid}}' "node_a")"
patch_pid="$(docker inspect -f '{{.State.Pid}}' "patch")"
node_b_pid="$(docker inspect -f '{{.State.Pid}}' "node_b")"

sudo mkdir -p /var/run/netns

sudo ln -s /proc/${node_a_pid}/ns/net /var/run/netns/node_a
sudo ln -s /proc/${patch_pid}/ns/net /var/run/netns/patch
sudo ln -s /proc/${node_b_pid}/ns/net /var/run/netns/node_b

#
# Create veth pairs
#
sudo ip link add veth0 type veth peer name veth1
sudo ip link add veth2 type veth peer name veth3

#
# Assign veth interfaces to containers
#
sudo ip link set veth0 netns node_a
sudo ip link set veth1 netns patch
sudo ip link set veth2 netns patch
sudo ip link set veth3 netns node_b

#
# Configure veth interfaces
#
sudo ip netns exec node_a ip link set veth0 up
sudo ip netns exec node_a ip addr add 172.16.111.1/24 dev veth0
sudo ip netns exec node_a ethtool --offload veth0 rx off tx off 2>/dev/null || true

sudo ip netns exec patch ip link set veth1 up
sudo ip netns exec patch ethtool --offload veth1 rx off tx off 2>/dev/null || true
sudo ip netns exec patch ip link set veth2 up
sudo ip netns exec patch ethtool --offload veth2 rx off tx off 2>/dev/null || true

sudo ip netns exec node_b ip link set veth3 up
sudo ip netns exec node_b ip addr add 172.16.111.2/24 dev veth3
sudo ip netns exec node_b ethtool --offload veth3 rx off tx off 2>/dev/null || true

#
# Copy Network Apps into containers
#
# Copy Network App for patch
docker cp $PATCH_APP_LOCATION patch:/root/$PATCH_APP_NAME
docker exec patch chmod +x /root/$PATCH_APP_NAME


#
# Execute shell commands inside containers
#

echo '=========================================='
echo 'Deployment complete!'
echo '=========================================='

# List deployed containers
docker ps --filter 'name=node_a|patch|node_b'