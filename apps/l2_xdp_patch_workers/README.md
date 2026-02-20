# L2 Virtual Patch (with XDP, multi-thread)

A demonstation of a virtual patch between two nodes.

## Topology diagram
![description](images/patch.png)

## Steps

### Build the project (all examples)
```bash
cd packetcord.io
mkdir build
cd build
cmake -DENABLE_XDP_DATAPLANE=ON .. --fresh
make
```

### Start the test deployment

```bash
cd ..
cd apps/l2_xdp_patch_workers/test_deployment/
sudo ./deploy.sh
```

### Go to the shell of Patch
```bash
docker exec -it patch /bin/sh
```

Inside the container, run the following commands and leave the shell open:
```bash
cd /root
./l2_xdp_patch_workers_app
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
64 bytes from 172.16.111.1: seq=0 ttl=64 time=0.049 ms
64 bytes from 172.16.111.1: seq=1 ttl=64 time=0.061 ms
64 bytes from 172.16.111.1: seq=2 ttl=64 time=0.055 ms
64 bytes from 172.16.111.1: seq=3 ttl=64 time=0.068 ms
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
[  5] local 172.16.111.2 port 34842 connected to 172.16.111.1 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   602 MBytes  5.05 Gbits/sec  112    547 KBytes       
[  5]   1.00-2.00   sec   721 MBytes  6.05 Gbits/sec   84    724 KBytes       
[  5]   2.00-3.00   sec   743 MBytes  6.23 Gbits/sec  112    675 KBytes       
[  5]   3.00-4.00   sec   716 MBytes  6.01 Gbits/sec  112    546 KBytes       
[  5]   4.00-5.00   sec   717 MBytes  6.01 Gbits/sec   84    706 KBytes       
[  5]   5.00-6.00   sec   738 MBytes  6.19 Gbits/sec  112    611 KBytes       
[  5]   6.00-7.00   sec   736 MBytes  6.17 Gbits/sec  112    547 KBytes       
[  5]   7.00-8.00   sec   699 MBytes  5.87 Gbits/sec   84    660 KBytes       
[  5]   8.00-9.00   sec   716 MBytes  6.01 Gbits/sec  112    594 KBytes       
[  5]   9.00-10.00  sec   738 MBytes  6.19 Gbits/sec   84    725 KBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  6.96 GBytes  5.98 Gbits/sec  1008            sender
[  5]   0.00-10.00  sec  6.95 GBytes  5.97 Gbits/sec                  receiver
```

## Destroy the test deployment
Close all container shells. On the host, inside the l2_xdp_patch_workers/test_deployment directory, execute:
```bash
sudo ./cleanup.sh
```