#!/bin/bash

TEP_A_APP_LOCATION='/home/vmetodiev/Projects/packetcord.io/build/apps/l3_tunnel/l3_tunnel_tep_a_app'
TEB_B_APP_LOCATION='/home/vmetodiev/Projects/packetcord.io/build/apps/l3_tunnel/l3_tunnel_tep_b_app'

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
docker run --privileged -dit --name container_1 --memory=512m alpine:latest
docker run --privileged -dit --name container_2 --memory=512m alpine:latest
docker run --privileged -dit --name container_3 --memory=512m alpine:latest
docker run --privileged -dit --name container_4 --memory=512m alpine:latest
docker run --privileged -dit --name container_5 --memory=512m alpine:latest

#
# Set hostnames inside containers
#
docker exec container_1 hostname container_1
docker exec container_2 hostname container_2
docker exec container_3 hostname container_3
docker exec container_4 hostname container_4
docker exec container_5 hostname container_5

#
# Expose network namespaces
#
container_1_pid="$(docker inspect -f '{{.State.Pid}}' "container_1")"
container_2_pid="$(docker inspect -f '{{.State.Pid}}' "container_2")"
container_3_pid="$(docker inspect -f '{{.State.Pid}}' "container_3")"
container_4_pid="$(docker inspect -f '{{.State.Pid}}' "container_4")"
container_5_pid="$(docker inspect -f '{{.State.Pid}}' "container_5")"

sudo mkdir -p /var/run/netns

sudo ln -s /proc/${container_1_pid}/ns/net /var/run/netns/container_1
sudo ln -s /proc/${container_2_pid}/ns/net /var/run/netns/container_2
sudo ln -s /proc/${container_3_pid}/ns/net /var/run/netns/container_3
sudo ln -s /proc/${container_4_pid}/ns/net /var/run/netns/container_4
sudo ln -s /proc/${container_5_pid}/ns/net /var/run/netns/container_5

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
sudo ip link set veth0 netns container_1
sudo ip link set veth1 netns container_2
sudo ip link set veth2 netns container_2
sudo ip link set veth3 netns container_3
sudo ip link set veth4 netns container_3
sudo ip link set veth5 netns container_4
sudo ip link set veth6 netns container_4
sudo ip link set veth7 netns container_5

#
# Configure veth interfaces
#
sudo ip netns exec container_1 ip link set veth0 up
sudo ip netns exec container_1 ip addr add 11.11.11.100/24 dev veth0
sudo ip netns exec container_1 ethtool --offload veth0 rx off tx off 2>/dev/null || true

sudo ip netns exec container_2 ip link set veth1 up
sudo ip netns exec container_2 ip addr add 11.11.11.1/24 dev veth1
sudo ip netns exec container_2 ethtool --offload veth1 rx off tx off 2>/dev/null || true
sudo ip netns exec container_2 ip link set veth2 up
sudo ip netns exec container_2 ip addr add 198.51.100.1/30 dev veth2
sudo ip netns exec container_2 ethtool --offload veth2 rx off tx off 2>/dev/null || true

sudo ip netns exec container_3 ip link set veth3 up
sudo ip netns exec container_3 ip addr add 198.51.100.2/30 dev veth3
sudo ip netns exec container_3 ethtool --offload veth3 rx off tx off 2>/dev/null || true
sudo ip netns exec container_3 ip link set veth4 up
sudo ip netns exec container_3 ip addr add 198.51.200.2/30 dev veth4
sudo ip netns exec container_3 ethtool --offload veth4 rx off tx off 2>/dev/null || true

sudo ip netns exec container_4 ip link set veth5 up
sudo ip netns exec container_4 ip addr add 198.51.200.1/30 dev veth5
sudo ip netns exec container_4 ethtool --offload veth5 rx off tx off 2>/dev/null || true
sudo ip netns exec container_4 ip link set veth6 up
sudo ip netns exec container_4 ip addr add 192.168.111.1/24 dev veth6
sudo ip netns exec container_4 ethtool --offload veth6 rx off tx off 2>/dev/null || true

sudo ip netns exec container_5 ip link set veth7 up
sudo ip netns exec container_5 ip addr add 192.168.111.100/24 dev veth7
sudo ip netns exec container_5 ethtool --offload veth7 rx off tx off 2>/dev/null || true

#
# Copy Network Apps into containers
#
# Copy Network App for container_2
docker cp $TEP_A_APP_LOCATION container_2:/root/l3_tunnel_tep_a_app
docker exec container_2 chmod +x /root/l3_tunnel_tep_a_app

# Copy Network App for container_4
docker cp $TEB_B_APP_LOCATION container_4:/root/l3_tunnel_tep_b_app
docker exec container_4 chmod +x /root/l3_tunnel_tep_b_app


#
# Execute shell commands inside containers
#
# Shell commands for container_1
cat > /tmp/container_1_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add iperf3 &&
apk add gcompat &&
ip route del default &&
ip route add default via 11.11.11.1 dev veth0 &&
ip link set mtu 1420 dev veth0
SHELL_COMMANDS_EOF
chmod +x /tmp/container_1_commands.sh
docker cp /tmp/container_1_commands.sh container_1:/tmp/commands.sh
docker exec container_1 /bin/sh /tmp/commands.sh
rm -f /tmp/container_1_commands.sh

# Shell commands for container_2
cat > /tmp/container_2_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add tcpdump &&
apk add gcompat &&
ip route del default &&
ip route add default via 198.51.100.2 dev veth2 &&
ip route add blackhole 192.168.111.0/24 &&
ip link set mtu 1420 dev veth1
SHELL_COMMANDS_EOF
chmod +x /tmp/container_2_commands.sh
docker cp /tmp/container_2_commands.sh container_2:/tmp/commands.sh
docker exec container_2 /bin/sh /tmp/commands.sh
rm -f /tmp/container_2_commands.sh

# Shell commands for container_3
cat > /tmp/container_3_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add tcpdump &&
apk add gcompat &&
ip route del default
SHELL_COMMANDS_EOF
chmod +x /tmp/container_3_commands.sh
docker cp /tmp/container_3_commands.sh container_3:/tmp/commands.sh
docker exec container_3 /bin/sh /tmp/commands.sh
rm -f /tmp/container_3_commands.sh

# Shell commands for container_4
cat > /tmp/container_4_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add tcpdump &&
apk add gcompat &&
ip route del default &&
ip route add default via 198.51.200.2 dev veth5 &&
ip route add blackhole 11.11.11.0/24 &&
ip link set mtu 1420 dev veth6
SHELL_COMMANDS_EOF
chmod +x /tmp/container_4_commands.sh
docker cp /tmp/container_4_commands.sh container_4:/tmp/commands.sh
docker exec container_4 /bin/sh /tmp/commands.sh
rm -f /tmp/container_4_commands.sh

# Shell commands for container_5
cat > /tmp/container_5_commands.sh << 'SHELL_COMMANDS_EOF'
#!/bin/sh
apk update &&
apk add iperf3 &&
apk add gcompat &&
ip route del default && 
ip route add default via 192.168.111.1 dev veth7 &&
ip link set mtu 1420 dev veth7
SHELL_COMMANDS_EOF
chmod +x /tmp/container_5_commands.sh
docker cp /tmp/container_5_commands.sh container_5:/tmp/commands.sh
docker exec container_5 /bin/sh /tmp/commands.sh
rm -f /tmp/container_5_commands.sh


echo '=========================================='
echo 'Deployment complete!'
echo '=========================================='

# List deployed containers
docker ps --filter 'name=container_1|container_2|container_3|container_4|container_5'