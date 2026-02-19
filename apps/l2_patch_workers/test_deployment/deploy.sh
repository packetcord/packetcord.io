#!/bin/bash

PATCH_APP_NAME='l2_patch_workers_app'
PATCH_APP_LOCATION='../../../build/apps/l2_patch_workers/'$PATCH_APP_NAME

echo '=========================================='
echo 'Topology Emulation - Container Deployment'
echo '=========================================='

#
# Build container images
#
docker pull alpine:latest
docker pull packetcord/alpine-iperf3:latest

#
# Launch containers
#
docker run --privileged -dit --name node_a --memory=512m --cpuset-cpus=0,1 packetcord/alpine-iperf3:latest
docker run --privileged -dit --name patch --memory=512m --cpuset-cpus=0,2,3 alpine:latest
docker run --privileged -dit --name node_b --memory=512m --cpuset-cpus=0,1 packetcord/alpine-iperf3:latest

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
# Shell commands for patch
cat > /tmp/patch_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add tcpdump &&
apk add gcompat
SHELL_COMMANDS_EOF
chmod +x /tmp/patch_commands.sh
docker cp /tmp/patch_commands.sh patch:/tmp/commands.sh
docker exec patch /bin/sh /tmp/commands.sh
rm -f /tmp/patch_commands.sh


echo '=========================================='
echo 'Deployment complete!'
echo '=========================================='

# List deployed containers
docker ps --filter 'name=node_a|patch|node_b'