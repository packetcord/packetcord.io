# L2 Virtual Patch (with TPACKETv3, single-thread)

A demonstation of a virtual patch between two nodes.

## Topology diagram
![description](images/patch.png)

## Steps

### Build the project (all examples)
```bash
cd packetcord.io
mkdir build
cd build
cmake .. --fresh
make
```

### Start the test deployment

```bash
cd ..
cd apps/l2_tpacketv3_patch/test_deployment/
sudo ./deploy.sh
```

### Go to the shell of Patch
```bash
docker exec -it patch /bin/sh
```

Inside the container, run the following commands and leave the shell open:
```bash
cd /root
./l2_tpacketv3_patch_app
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
64 bytes from 172.16.111.1: seq=0 ttl=64 time=3.795 ms
64 bytes from 172.16.111.1: seq=1 ttl=64 time=2.764 ms
64 bytes from 172.16.111.1: seq=2 ttl=64 time=3.742 ms
64 bytes from 172.16.111.1: seq=3 ttl=64 time=2.772 ms
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
[  5] local 172.16.111.2 port 39392 connected to 172.16.111.1 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   447 MBytes  3.74 Gbits/sec    0   3.89 MBytes       
[  5]   1.00-2.00   sec   459 MBytes  3.85 Gbits/sec    0   3.89 MBytes       
[  5]   2.00-3.00   sec   488 MBytes  4.09 Gbits/sec    0   3.89 MBytes       
[  5]   3.00-4.00   sec   490 MBytes  4.11 Gbits/sec    0   3.89 MBytes       
[  5]   4.00-5.00   sec   489 MBytes  4.11 Gbits/sec    0   3.89 MBytes       
[  5]   5.00-6.00   sec   491 MBytes  4.12 Gbits/sec    0   3.89 MBytes       
[  5]   6.00-7.00   sec   490 MBytes  4.11 Gbits/sec    0   3.89 MBytes       
[  5]   7.00-8.00   sec   490 MBytes  4.11 Gbits/sec    0   3.89 MBytes       
[  5]   8.00-9.00   sec   490 MBytes  4.11 Gbits/sec    0   3.89 MBytes       
[  5]   9.00-10.00  sec   488 MBytes  4.09 Gbits/sec    0   3.89 MBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  4.71 GBytes  4.04 Gbits/sec    0            sender
[  5]   0.00-10.01  sec  4.71 GBytes  4.04 Gbits/sec                 receiver
```

## Destroy the test deployment
Close all container shells. On the host, inside the l3_tunnel/test_deployment directory, execute:
```bash
sudo ./cleanup.sh
```