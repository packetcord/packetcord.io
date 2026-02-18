#!/bin/bash

TEP_A_APP_NAME='l3_tunnel_tep_a_app'
TEP_B_APP_NAME='l3_tunnel_tep_b_app'

TEP_A_APP_LOCATION='../../../build/apps/l3_tunnel/'$TEP_A_APP_NAME
TEB_B_APP_LOCATION='../../../build/apps/l3_tunnel/'$TEP_B_APP_NAME

# Check if TEP_A_APP_LOCATION is empty
if [[ -z "$TEP_A_APP_LOCATION" ]]; then
    echo "Error: TEP_A_APP_LOCATION is not defined. Terminating script."
    exit 1
fi

# Check if TEB_B_APP_LOCATION is empty
if [[ -z "$TEB_B_APP_LOCATION" ]]; then
    echo "Error: TEB_B_APP_LOCATION is not defined. Terminating script."
    exit 1
fi

echo '=========================================='
echo 'Topology Emulation - Container Deployment'
echo '=========================================='

#
# Build container images
#
docker pull alpine:latest

#
# Launch containers
#
docker run --privileged -dit --name node_a --memory=512m alpine:latest
docker run --privileged -dit --name tep_a --memory=512m alpine:latest
docker run --privileged -dit --name core --memory=512m alpine:latest
docker run --privileged -dit --name tep_b --memory=512m alpine:latest
docker run --privileged -dit --name node_b --memory=512m alpine:latest

#
# Set hostnames inside containers
#
docker exec node_a hostname node_a
docker exec tep_a hostname tep_a
docker exec core hostname core
docker exec tep_b hostname tep_b
docker exec node_b hostname node_b

#
# Expose network namespaces
#
node_a_pid="$(docker inspect -f '{{.State.Pid}}' "node_a")"
tep_a_pid="$(docker inspect -f '{{.State.Pid}}' "tep_a")"
core_pid="$(docker inspect -f '{{.State.Pid}}' "core")"
tep_b_pid="$(docker inspect -f '{{.State.Pid}}' "tep_b")"
node_b_pid="$(docker inspect -f '{{.State.Pid}}' "node_b")"

sudo mkdir -p /var/run/netns

sudo ln -s /proc/${node_a_pid}/ns/net /var/run/netns/node_a
sudo ln -s /proc/${tep_a_pid}/ns/net /var/run/netns/tep_a
sudo ln -s /proc/${core_pid}/ns/net /var/run/netns/core
sudo ln -s /proc/${tep_b_pid}/ns/net /var/run/netns/tep_b
sudo ln -s /proc/${node_b_pid}/ns/net /var/run/netns/node_b

#
# Create veth pairs
#
sudo ip link add veth0 type veth peer name veth1
sudo ip link add veth2 type veth peer name veth3
sudo ip link add veth4 type veth peer name veth5
sudo ip link add veth6 type veth peer name veth7

#
# Assign veth interfaces to containers
#
sudo ip link set veth0 netns node_a
sudo ip link set veth1 netns tep_a
sudo ip link set veth2 netns tep_a
sudo ip link set veth3 netns core
sudo ip link set veth4 netns core
sudo ip link set veth5 netns tep_b
sudo ip link set veth6 netns tep_b
sudo ip link set veth7 netns node_b

#
# Configure veth interfaces
#
sudo ip netns exec node_a ip link set veth0 up
sudo ip netns exec node_a ip addr add 11.11.11.100/24 dev veth0
sudo ip netns exec node_a ethtool --offload veth0 rx off tx off 2>/dev/null || true

sudo ip netns exec tep_a ip link set veth1 up
sudo ip netns exec tep_a ip addr add 11.11.11.1/24 dev veth1
sudo ip netns exec tep_a ethtool --offload veth1 rx off tx off 2>/dev/null || true
sudo ip netns exec tep_a ip link set veth2 up
sudo ip netns exec tep_a ip addr add 198.51.100.1/30 dev veth2
sudo ip netns exec tep_a ethtool --offload veth2 rx off tx off 2>/dev/null || true

sudo ip netns exec core ip link set veth3 up
sudo ip netns exec core ip addr add 198.51.100.2/30 dev veth3
sudo ip netns exec core ethtool --offload veth3 rx off tx off 2>/dev/null || true
sudo ip netns exec core ip link set veth4 up
sudo ip netns exec core ip addr add 198.51.200.2/30 dev veth4
sudo ip netns exec core ethtool --offload veth4 rx off tx off 2>/dev/null || true

sudo ip netns exec tep_b ip link set veth5 up
sudo ip netns exec tep_b ip addr add 198.51.200.1/30 dev veth5
sudo ip netns exec tep_b ethtool --offload veth5 rx off tx off 2>/dev/null || true
sudo ip netns exec tep_b ip link set veth6 up
sudo ip netns exec tep_b ip addr add 192.168.111.1/24 dev veth6
sudo ip netns exec tep_b ethtool --offload veth6 rx off tx off 2>/dev/null || true

sudo ip netns exec node_b ip link set veth7 up
sudo ip netns exec node_b ip addr add 192.168.111.100/24 dev veth7
sudo ip netns exec node_b ethtool --offload veth7 rx off tx off 2>/dev/null || true

#
# Copy Network Apps into containers
#
# Copy Network App for tep_a
docker cp $TEP_A_APP_LOCATION tep_a:/root/$TEP_A_APP_NAME
docker exec tep_a chmod +x /root/$TEP_A_APP_NAME

# Copy Network App for tep_b
docker cp $TEB_B_APP_LOCATION tep_b:/root/$TEP_B_APP_NAME
docker exec tep_b chmod +x /root/$TEP_B_APP_NAME


#
# Execute shell commands inside containers
#
# Shell commands for node_a
cat > /tmp/node_a_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add iperf3 &&
apk add gcompat &&
ip route del default &&
ip route add default via 11.11.11.1 dev veth0 &&
ip link set mtu 1420 dev veth0
SHELL_COMMANDS_EOF
chmod +x /tmp/node_a_commands.sh
docker cp /tmp/node_a_commands.sh node_a:/tmp/commands.sh
docker exec node_a /bin/sh /tmp/commands.sh
rm -f /tmp/node_a_commands.sh

# Shell commands for tep_a
cat > /tmp/tep_a_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add tcpdump &&
apk add gcompat &&
ip route del default &&
ip route add default via 198.51.100.2 dev veth2 &&
ip route add blackhole 192.168.111.0/24 &&
ip link set mtu 1420 dev veth1
SHELL_COMMANDS_EOF
chmod +x /tmp/tep_a_commands.sh
docker cp /tmp/tep_a_commands.sh tep_a:/tmp/commands.sh
docker exec tep_a /bin/sh /tmp/commands.sh
rm -f /tmp/tep_a_commands.sh

# Shell commands for core
cat > /tmp/core_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add tcpdump &&
apk add gcompat &&
ip route del default
SHELL_COMMANDS_EOF
chmod +x /tmp/core_commands.sh
docker cp /tmp/core_commands.sh core:/tmp/commands.sh
docker exec core /bin/sh /tmp/commands.sh
rm -f /tmp/core_commands.sh

# Shell commands for tep_b
cat > /tmp/tep_b_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add tcpdump &&
apk add gcompat &&
ip route del default &&
ip route add default via 198.51.200.2 dev veth5 &&
ip route add blackhole 11.11.11.0/24 &&
ip link set mtu 1420 dev veth6
SHELL_COMMANDS_EOF
chmod +x /tmp/tep_b_commands.sh
docker cp /tmp/tep_b_commands.sh tep_b:/tmp/commands.sh
docker exec tep_b /bin/sh /tmp/commands.sh
rm -f /tmp/tep_b_commands.sh

# Shell commands for node_b
cat > /tmp/node_b_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add iperf3 &&
apk add gcompat &&
ip route del default && 
ip route add default via 192.168.111.1 dev veth7 &&
ip link set mtu 1420 dev veth7
SHELL_COMMANDS_EOF
chmod +x /tmp/node_b_commands.sh
docker cp /tmp/node_b_commands.sh node_b:/tmp/commands.sh
docker exec node_b /bin/sh /tmp/commands.sh
rm -f /tmp/node_b_commands.sh


echo '=========================================='
echo 'Deployment complete!'
echo '=========================================='

# List deployed containers
docker ps --filter 'name=node_a|tep_a|core|tep_b|node_b'