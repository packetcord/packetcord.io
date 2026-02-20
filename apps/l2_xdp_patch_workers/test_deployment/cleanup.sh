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

echo '=========================================='
echo 'Cleanup complete!'
echo '=========================================='