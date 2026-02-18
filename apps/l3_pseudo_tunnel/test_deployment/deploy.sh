#!/bin/bash

TEP_APP_NAME='l3_pseudo_tunnel_tep_app'
CLIENT_APP_NAME='l3_pseudo_tunnel_client_app'

TEP_APP_LOCATION='../../../build/apps/l3_pseudo_tunnel/'$TEP_APP_NAME
CLIENT_APP_LOCATION='../../../build/apps/l3_pseudo_tunnel/'$CLIENT_APP_NAME

# Check if TEP_APP_LOCATION is empty
if [[ -z "$TEP_APP_LOCATION" ]]; then
    echo "Error: TEP_APP_LOCATION is not defined. Terminating script."
    exit 1
fi

# Check if CLIENT_APP_LOCATION is empty
if [[ -z "$CLIENT_APP_LOCATION" ]]; then
    echo "Error: CLIENT_APP_LOCATION is not defined. Terminating script."
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
docker run --privileged -dit --name tep --memory=512m alpine:latest
docker run --privileged -dit --name core --memory=512m alpine:latest
docker run --privileged -dit --name router_nat --memory=512m alpine:latest
docker run --privileged -dit --name node_b --memory=512m alpine:latest

#
# Set hostnames inside containers
#
docker exec node_a hostname node_a
docker exec tep hostname tep
docker exec core hostname core
docker exec router_nat hostname router_nat
docker exec node_b hostname node_b

#
# Expose network namespaces
#
node_a_pid="$(docker inspect -f '{{.State.Pid}}' "node_a")"
tep_pid="$(docker inspect -f '{{.State.Pid}}' "tep")"
core_pid="$(docker inspect -f '{{.State.Pid}}' "core")"
router_nat_pid="$(docker inspect -f '{{.State.Pid}}' "router_nat")"
node_b_pid="$(docker inspect -f '{{.State.Pid}}' "node_b")"

sudo mkdir -p /var/run/netns

sudo ln -s /proc/${node_a_pid}/ns/net /var/run/netns/node_a
sudo ln -s /proc/${tep_pid}/ns/net /var/run/netns/tep
sudo ln -s /proc/${core_pid}/ns/net /var/run/netns/core
sudo ln -s /proc/${router_nat_pid}/ns/net /var/run/netns/router_nat
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
sudo ip link set veth1 netns tep
sudo ip link set veth2 netns tep
sudo ip link set veth3 netns core
sudo ip link set veth4 netns core
sudo ip link set veth5 netns router_nat
sudo ip link set veth6 netns router_nat
sudo ip link set veth7 netns node_b

#
# Configure veth interfaces
#
sudo ip netns exec node_a ip link set veth0 up
sudo ip netns exec node_a ip addr add 11.11.11.100/24 dev veth0
sudo ip netns exec node_a ethtool --offload veth0 rx off tx off 2>/dev/null || true

sudo ip netns exec tep ip link set veth1 up
sudo ip netns exec tep ip addr add 11.11.11.1/24 dev veth1
sudo ip netns exec tep ethtool --offload veth1 rx off tx off 2>/dev/null || true
sudo ip netns exec tep ip link set veth2 up
sudo ip netns exec tep ip addr add 198.51.100.1/30 dev veth2
sudo ip netns exec tep ethtool --offload veth2 rx off tx off 2>/dev/null || true

sudo ip netns exec core ip link set veth3 up
sudo ip netns exec core ip addr add 198.51.100.2/30 dev veth3
sudo ip netns exec core ethtool --offload veth3 rx off tx off 2>/dev/null || true
sudo ip netns exec core ip link set veth4 up
sudo ip netns exec core ip addr add 198.51.200.2/30 dev veth4
sudo ip netns exec core ethtool --offload veth4 rx off tx off 2>/dev/null || true

sudo ip netns exec router_nat ip link set veth5 up
sudo ip netns exec router_nat ip addr add 198.51.200.1/30 dev veth5
sudo ip netns exec router_nat ethtool --offload veth5 rx off tx off 2>/dev/null || true
sudo ip netns exec router_nat ip link set veth6 up
sudo ip netns exec router_nat ip addr add 192.168.111.1/24 dev veth6
sudo ip netns exec router_nat ethtool --offload veth6 rx off tx off 2>/dev/null || true

sudo ip netns exec node_b ip link set veth7 up
sudo ip netns exec node_b ip addr add 192.168.111.100/24 dev veth7
sudo ip netns exec node_b ethtool --offload veth7 rx off tx off 2>/dev/null || true

#
# Copy Network Apps into containers
#
# Copy Network App for tep
docker cp $TEP_APP_LOCATION tep:/root/$TEP_APP_NAME
docker exec tep chmod +x /root/$TEP_APP_NAME

# Copy Network App for node_b
docker cp $CLIENT_APP_LOCATION node_b:/root/$CLIENT_APP_NAME
docker exec node_b chmod +x /root/$CLIENT_APP_NAME


#
# Execute shell commands inside containers
#
# Shell commands for node_a
cat > /tmp/node_a_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add netcat-openbsd &&
apk add gcompat &&
ip route del default &&
ip route add default via 11.11.11.1 dev veth0 &&
ip link set mtu 1420 dev veth0
SHELL_COMMANDS_EOF
chmod +x /tmp/node_a_commands.sh
docker cp /tmp/node_a_commands.sh node_a:/tmp/commands.sh
docker exec node_a /bin/sh /tmp/commands.sh
rm -f /tmp/node_a_commands.sh

# Shell commands for tep
cat > /tmp/tep_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add tcpdump &&
apk add gcompat &&
ip route del default &&
ip route add default via 198.51.100.2 dev veth2 &&
ip route add blackhole 192.168.111.0/24 &&
ip link set mtu 1420 dev veth1
SHELL_COMMANDS_EOF
chmod +x /tmp/tep_commands.sh
docker cp /tmp/tep_commands.sh tep:/tmp/commands.sh
docker exec tep /bin/sh /tmp/commands.sh
rm -f /tmp/tep_commands.sh

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

# Shell commands for router_nat
cat > /tmp/router_nat_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add iptables &&
apk add tcpdump &&
apk add gcompat &&
ip route del default &&
ip route add default via 198.51.200.2 dev veth5 &&
ip route add blackhole 11.11.11.0/24 &&
ip link set mtu 1420 dev veth6 &&
iptables -A FORWARD -i veth6 -j ACCEPT &&
iptables -t nat -A POSTROUTING -o veth5 -j MASQUERADE
SHELL_COMMANDS_EOF
chmod +x /tmp/router_nat_commands.sh
docker cp /tmp/router_nat_commands.sh router_nat:/tmp/commands.sh
docker exec router_nat /bin/sh /tmp/commands.sh
rm -f /tmp/router_nat_commands.sh

# Shell commands for node_b
cat > /tmp/node_b_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add netcat-openbsd &&
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
docker ps --filter 'name=node_a|tep|core|router_nat|node_b'