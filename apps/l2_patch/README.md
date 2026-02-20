# L2 Virtual Patch (single-thread)

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
cd apps/l2_patch/test_deployment/
sudo ./deploy.sh
```

### Go to the shell of Patch
```bash
docker exec -it patch /bin/sh
```

Inside the container, run the following commands and leave the shell open:
```bash
cd /root
./l2_patch_app
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
64 bytes from 172.16.111.1: seq=0 ttl=64 time=0.061 ms
64 bytes from 172.16.111.1: seq=1 ttl=64 time=0.100 ms
64 bytes from 172.16.111.1: seq=2 ttl=64 time=0.099 ms
64 bytes from 172.16.111.1: seq=3 ttl=64 time=0.062 ms
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
[  5] local 172.16.111.2 port 42196 connected to 172.16.111.1 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec   213 MBytes  1.79 Gbits/sec  1254    103 KBytes       
[  5]   1.00-2.00   sec   242 MBytes  2.03 Gbits/sec  1451    100 KBytes       
[  5]   2.00-3.00   sec   244 MBytes  2.05 Gbits/sec  1461   99.0 KBytes       
[  5]   3.00-4.00   sec   243 MBytes  2.04 Gbits/sec  1446   99.0 KBytes       
[  5]   4.00-5.00   sec   243 MBytes  2.04 Gbits/sec  1490    103 KBytes       
[  5]   5.00-6.00   sec   242 MBytes  2.03 Gbits/sec  1428    100 KBytes       
[  5]   6.00-7.00   sec   241 MBytes  2.02 Gbits/sec  1528    100 KBytes       
[  5]   7.00-8.00   sec   243 MBytes  2.04 Gbits/sec  1487    100 KBytes       
[  5]   8.00-9.00   sec   243 MBytes  2.04 Gbits/sec  1499    100 KBytes       
[  5]   9.00-10.00  sec   243 MBytes  2.04 Gbits/sec  1471    103 KBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec  2.34 GBytes  2.01 Gbits/sec  14515            sender
[  5]   0.00-10.00  sec  2.34 GBytes  2.01 Gbits/sec                   receiver
```

## Destroy the test deployment
Close all container shells. On the host, inside the l3_tunnel/test_deployment directory, execute:
```bash
sudo ./cleanup.sh
```