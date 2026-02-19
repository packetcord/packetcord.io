#!/bin/bash

echo '=========================================='
echo 'Topology Emulation - Cleanup'
echo '=========================================='

#
# Stop containers
#
docker stop node_a 2>/dev/null || true
docker stop patch 2>/dev/null || true
docker stop node_b 2>/dev/null || true

#
# Remove containers
#
docker rm node_a 2>/dev/null || true
docker rm patch 2>/dev/null || true
docker rm node_b 2>/dev/null || true

#
# Remove exposed namespaces
#
sudo rm -f /var/run/netns/node_a
sudo rm -f /var/run/netns/patch
sudo rm -f /var/run/netns/node_b

#
# Reset hugepages (both 2MB and 1GB)
#
# Reset 2MB hugepages
echo 0 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
# Reset 1GB hugepages
echo 0 | sudo tee /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages

echo '=========================================='
echo 'Cleanup complete!'
echo '=========================================='