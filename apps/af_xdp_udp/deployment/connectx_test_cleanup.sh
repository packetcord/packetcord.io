#!/bin/bash
# cleanup_namespaces.sh

echo "Cleaning up network namespaces..."
sudo ip netns del ns1 2>/dev/null || true
sudo ip netns del ns2 2>/dev/null || true
echo "Done! Interfaces returned to default namespace."
