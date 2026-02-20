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
64 bytes from 172.16.111.1: seq=0 ttl=64 time=0.071 ms
64 bytes from 172.16.111.1: seq=1 ttl=64 time=0.047 ms
64 bytes from 172.16.111.1: seq=2 ttl=64 time=0.038 ms
64 bytes from 172.16.111.1: seq=3 ttl=64 time=0.043 ms
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
[  5] local 172.16.111.2 port 46630 connected to 172.16.111.1 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   651 MBytes  5.45 Gbits/sec   52   1.41 MBytes
[  5]   1.00-2.00   sec   621 MBytes  5.21 Gbits/sec    2   1.37 MBytes
[  5]   2.00-3.00   sec   624 MBytes  5.24 Gbits/sec    2   1.32 MBytes
[  5]   3.00-4.00   sec   606 MBytes  5.08 Gbits/sec    1   1.27 MBytes
[  5]   4.00-5.00   sec   598 MBytes  5.02 Gbits/sec    2   1.21 MBytes
[  5]   5.00-6.00   sec   573 MBytes  4.81 Gbits/sec    7   1.12 MBytes
[  5]   6.00-7.00   sec   620 MBytes  5.21 Gbits/sec    1   1.07 MBytes
[  5]   7.00-8.00   sec   579 MBytes  4.85 Gbits/sec    0   1.41 MBytes
[  5]   8.00-9.00   sec   590 MBytes  4.95 Gbits/sec    1   1.36 MBytes
[  5]   9.00-10.00  sec   571 MBytes  4.79 Gbits/sec    1   1.28 MBytes
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  5.89 GBytes  5.06 Gbits/sec   69            sender
[  5]   0.00-10.00  sec  5.89 GBytes  5.06 Gbits/sec                 receiver
```

## Destroy the test deployment
Close all container shells. On the host, inside the l2_dpdk_patch/test_deployment directory, execute:
```bash
sudo ./cleanup.sh
```