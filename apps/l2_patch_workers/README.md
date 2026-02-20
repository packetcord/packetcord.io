# L2 Virtual Patch (multi-thread)

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
cd apps/l2_patch_workers/test_deployment/
sudo ./deploy.sh
```

### Go to the shell of Patch
```bash
docker exec -it patch /bin/sh
```

Inside the container, run the following commands and leave the shell open:
```bash
cd /root
./l2_patch_workers_app
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
64 bytes from 172.16.111.1: seq=0 ttl=64 time=0.054 ms
64 bytes from 172.16.111.1: seq=1 ttl=64 time=0.040 ms
64 bytes from 172.16.111.1: seq=2 ttl=64 time=0.036 ms
64 bytes from 172.16.111.1: seq=3 ttl=64 time=0.039 ms
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
[  5] local 172.16.111.2 port 36536 connected to 172.16.111.1 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   344 MBytes  2.89 Gbits/sec  1756    153 KBytes       
[  5]   1.00-2.00   sec   392 MBytes  3.29 Gbits/sec  1872    110 KBytes       
[  5]   2.00-3.00   sec   391 MBytes  3.28 Gbits/sec  1649    106 KBytes       
[  5]   3.00-4.00   sec   392 MBytes  3.28 Gbits/sec  1658    110 KBytes       
[  5]   4.00-5.00   sec   392 MBytes  3.29 Gbits/sec  1689    103 KBytes       
[  5]   5.00-6.00   sec   389 MBytes  3.27 Gbits/sec  1886    105 KBytes       
[  5]   6.00-7.00   sec   388 MBytes  3.26 Gbits/sec  1793    107 KBytes       
[  5]   7.00-8.00   sec   391 MBytes  3.28 Gbits/sec  1670    102 KBytes       
[  5]   8.00-9.00   sec   392 MBytes  3.29 Gbits/sec  1792    105 KBytes       
[  5]   9.00-10.00  sec   391 MBytes  3.28 Gbits/sec  1691    103 KBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  3.77 GBytes  3.24 Gbits/sec  17456            sender
[  5]   0.00-10.00  sec  3.77 GBytes  3.24 Gbits/sec                   receiver
```

## Destroy the test deployment
Close all container shells. On the host, inside the l3_tunnel/test_deployment directory, execute:
```bash
sudo ./cleanup.sh
```