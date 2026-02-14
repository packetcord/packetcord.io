#!/bin/bash

echo '=========================================='
echo 'Topology Emulation - Cleanup'
echo '=========================================='

#
# Stop containers
#
docker stop container_1 2>/dev/null || true
docker stop container_2 2>/dev/null || true
docker stop container_3 2>/dev/null || true
docker stop container_4 2>/dev/null || true
docker stop container_5 2>/dev/null || true

#
# Remove containers
#
docker rm container_1 2>/dev/null || true
docker rm container_2 2>/dev/null || true
docker rm container_3 2>/dev/null || true
docker rm container_4 2>/dev/null || true
docker rm container_5 2>/dev/null || true

#
# Remove exposed namespaces
#
sudo rm -f /var/run/netns/container_1
sudo rm -f /var/run/netns/container_2
sudo rm -f /var/run/netns/container_3
sudo rm -f /var/run/netns/container_4
sudo rm -f /var/run/netns/container_5

echo '=========================================='
echo 'Cleanup complete!'
echo '=========================================='