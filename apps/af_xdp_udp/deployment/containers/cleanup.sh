#!/bin/bash

echo '=========================================='
echo 'Topology Emulation - Cleanup'
echo '=========================================='

#
# Stop containers
#
docker stop container_1 2>/dev/null || true
docker stop container_2 2>/dev/null || true

#
# Remove containers
#
docker rm container_1 2>/dev/null || true
docker rm container_2 2>/dev/null || true

#
# Remove exposed namespaces
#
sudo rm -f /var/run/netns/container_1
sudo rm -f /var/run/netns/container_2

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