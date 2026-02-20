# L2 Virtual Patch (with DPDK, single-thread)

A demonstation of a virtual patch between two nodes.

## Topology diagram
![description](images/patch.png)

## Steps

### Build the project (all examples)
```bash
cd packetcord.io
mkdir build
cd build
cmake -DENABLE_DPDK_DATAPLANE=ON .. --fresh
make
```

### Start the test deployment

```bash
cd ..
cd apps/l2_dpdk_patch/test_deployment/
sudo ./deploy.sh
```

### Go to the shell of Patch
```bash
docker exec -it patch /bin/sh
```

Inside the container, run the following commands and leave the shell open:
```bash
cd /root
./l2_dpdk_patch_app
```

### Result
Open the shells of Node A and Node B. Try to ping each other (172.16.111.1 and 172.16.111.2).

```bash
docker exec -it node_a /bin/sh
```

```bash
docker exec -it node_b /bin/sh
```

```console
PING 172.16.111.1 (172.16.111.1): 56 data bytes
64 bytes from 172.16.111.1: seq=0 ttl=64 time=0.053 ms
64 bytes from 172.16.111.1: seq=1 ttl=64 time=0.029 ms
64 bytes from 172.16.111.1: seq=2 ttl=64 time=0.027 ms
```

Let's also run iperf3 between Node A (server) and Node B (client):

#### On Node A
```bash
iperf3 -s
```

#### On Node B
```bash
iperf3 -c 172.16.111.1
```

```console
Connecting to host 172.16.111.1, port 5201
[  5] local 172.16.111.2 port 43948 connected to 172.16.111.1 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec  1.06 GBytes  9.09 Gbits/sec    8   1.34 MBytes       
[  5]   1.00-2.00   sec  1.07 GBytes  9.22 Gbits/sec    4   1.12 MBytes       
[  5]   2.00-3.00   sec  1.07 GBytes  9.20 Gbits/sec    6   1.34 MBytes       
[  5]   3.00-4.00   sec  1.08 GBytes  9.31 Gbits/sec    2   1.11 MBytes       
[  5]   4.00-5.00   sec  1.08 GBytes  9.27 Gbits/sec    1   1.33 MBytes       
[  5]   5.00-6.00   sec  1.08 GBytes  9.25 Gbits/sec    8   1.08 MBytes       
[  5]   6.00-7.00   sec  1.09 GBytes  9.34 Gbits/sec   10   1.31 MBytes       
[  5]   7.00-8.00   sec  1.08 GBytes  9.31 Gbits/sec    6   1.06 MBytes       
[  5]   8.00-9.00   sec  1.10 GBytes  9.43 Gbits/sec    2   1.30 MBytes       
[  5]   9.00-10.00  sec  1.11 GBytes  9.58 Gbits/sec    7   1.07 MBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  10.8 GBytes  9.32 Gbits/sec   54            sender
[  5]   0.00-10.00  sec  10.8 GBytes  9.31 Gbits/sec                 receiver
```

## Destroy the test deployment
Close all container shells. On the host, inside the l3_tunnel/test_deployment directory, execute:
```bash
sudo ./cleanup.sh
```