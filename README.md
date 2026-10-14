# AF_XDP UDP Example (OCUDU Project N3 GTP-U Offload Proposal)

> Note: This experimental work was conducted specifically to support the N3 GTP-U Offload Proposal. This branch can be removed from the repository upon request or other related reason.

## Description
An attempt to implement UDP communucation over the AF_XDP socket family. The working princle is divided into transmit and receive paths, briefly described below.

```console
                    UMEM region
  +--------------+--------------+   +--------------+
  |              |              |   |              |
  |   PAYLOAD    |   PAYLOAD    |   |   PAYLOAD    |
  |              |              |   |              |
  |--------------|--------------|   |--------------|
  | UDP Header   | UDP Header   |...| UDP Header   |
  | IPv4 Header  | IPv4 Header  |   | IPv4 Header  |
  | Eth Header   | Eth Header   |   | Eth Header   |
  +--------------+--------------+   +--------------+
```

### Transmit path
1. The UMEM region of N chucks (representing Ethernet frames) is first initialised with the necessary header information - Layer 2 (Ethernet), Layer 3 (IPv4) and UDP.
   This is somehow similar to initialising a UDP socket.
2. The application then sets the payload. Additionally, this involves calculating and setting the necessary lengths and checksums.
3. The frames are tramismitted (single or batch) over AF_XDP.

### Receive path
1. The frames are filtered and received as batches from the AF_XDP.
2. For each received frame batch within a single EPOLL event, the payload of the UDP is extracted and used accordingly.

Several aspects were considered as part of this experimental work:
- Batch TX and RX processing: Since AF_XDP cannot work over IOVEC, batched transmission and reception are demonstrated as a relevant alternative.
- XDP and eBPF filtering: Both the default XDP program and a separate eBPF program capable of filtering packets up to Layer 4 is supported and demonstrated.
- UMEM allocation and mapping - AF_XDP allocates its UMEM region using mmap(), making it possible to use hugepages.
- EPOLL-based reception - works in both level-trigger and edge-trigger modes.
- Offload and zero-copy - works on virual interfaces like `veth` in non-zero-copy mode, as well as on hardware NICs with complete XDP_ZEROCOPY support.

Both the sender and the receiver have been tested against 'netcat' and observed in packet sniffers like `tcpdump` and `tshark` until achieving one-to-one representation (and behaviour) of the UDP.

## Code
The `apps/af_xdp_udp` directory contains the code implementation and the related deployment scripts.

## Build instrcutions
Clone the repository, checkout to the `af_xdp_udp` branch and build the example:
```bash
mkdir build && cd build
cmake .. -DENABLE_XDP_DATAPLANE=ON -DENABLE_EBPF=ON --fresh
make
```

## Testbench
Two testbenches are provided:
- Containerised (over direct veth pair connection)
- Bare-metal (over physical optical patch between the two ports of Mellanox ConnectX-4 Lx)

Rationale is to demonstrate that the example will work both with true zero copy (XDP_ZEROCOPY supported by the NVIDIA hardware and driver) and also sucessfully fallback to the other modes (like XDP_COPY or XDP_SKB) in case
deployed in a software environment.

### Containerised
```console
         Container A                              Container B
  +-----------------------+                +-----------------------+
  |                       |                |                       |
  |                       |                |                       |
  |                       |   veth pair    |                       |
  |                 veth1 |<==============>| veth2                 |
  |        172.16.16.1/30 |                | 172.16.16.2/30        |
  |                       |                |                       |
  |                       |                |                       |
  |                       |                |                       |
  +-----------------------+                +-----------------------+
```

### Bare-metal
```console
         Namespace 1                              Namespace 2
  +-----------------------+                +-----------------------+
  |                       |                |                       |
  |                       |                |                       |
  |                       |  optical patch |                       |
  |          enp11s0f0np0 |<==============>| enp11s0f1np1          |
  |         192.168.168.2 |     (loop)     | 192.168.168.2         |
  |                       |                |                       |
  |                       |                |                       |
  |                       |                |                       |
  +-----------------------+                +-----------------------+
```

#### Card, firmware and driver details
```bash
$ modinfo mlx5_core
filename:       /lib/modules/6.8.0-60-generic/updates/dkms/mlx5_core.ko.zst
alias:          auxiliary:mlx5_core.eth-rep
alias:          auxiliary:mlx5_core.eth
basedon:        Korg 6.8-rc4
version:        24.10-5.1.6
license:        Dual BSD/GPL
description:    Mellanox 5th generation network adapters (ConnectX series) core driver
```

```bash
$ sudo mst start && sudo mlxconfig -d /dev/mst/mt4117_pciconf0 q
Starting MST (Mellanox Software Tools) driver set
Loading MST PCI module - Success
Loading MST PCI configuration module - Success
Create devices
Unloading MST PCI module (unused) - Success

Device #1:
----------

Device type:        ConnectX4LX         
Name:               MCX4121A-XCA_Ax     
Description:        ConnectX-4 Lx EN network interface card; 10GbE dual-port SFP28; PCIe3.0 x8; ROHS R6
Device:             /dev/mst/mt4117_pciconf0
```

## Launch
Deployment shell scripts are provided for Both the containerised and bare-metal cases.

### Containerised
First, run the Docker topology:
```bash
cd apps/af_xdp_udp/deployment/containers
./deploy.sh
```

Then, copy the binaries to the corresponding containers:
```bash
# Go to the build location
cd ../../../../build/apps/af_xdp_udp

# Binary for Container A (sender only)
docker cp af_xdp_udp_a_app container_1:/root/ 

# Binary for Container B (receiver only)
docker cp af_xdp_udp_b_app container_2:/root/
docker cp udp_rx_filter.bpf.o container_2:/root/
```

Open shell to each container in a separate terminal window:
```bash
# To Container A
docker exec -it container_1 /bin/sh
```

```bash
# To Container B
docker exec -it container_2 /bin/sh
```

Finally, run the examples:
```bash
# Inside the terminal window of Container B, launch the receiver binary
# ./af_xdp_udp_b_app 
[CordApp] Launching the PacketCord AF_XDP UDP App!
[cord_xdp_socket_init] attempting zero-copy mode with dedicated UMEM...
[cord_xdp_socket_init] fallback to copy mode
libbpf: elf: skipping unrecognized data section(8) .xdp_run_config
libbpf: elf: skipping unrecognized data section(9) xdp_metadata
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
libxdp: No bpffs found at /sys/fs/bpf
libxdp: Can't use dispatcher without a working bpffs
libxdp: Falling back to loading single prog without dispatcher
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
```

```bash
# Inside the terminal window of Container A, send the batch of UDP packets
./# ./af_xdp_udp_a_app 
[CordApp] Launching the PacketCord AF_XDP UDP App!
[cord_xdp_socket_init] attempting zero-copy mode with dedicated UMEM...
[cord_xdp_socket_init] fallback to copy mode
libbpf: elf: skipping unrecognized data section(8) .xdp_run_config
libbpf: elf: skipping unrecognized data section(9) xdp_metadata
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
libxdp: No bpffs found at /sys/fs/bpf
libxdp: Can't use dispatcher without a working bpffs
libxdp: Falling back to loading single prog without dispatcher
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
[CordApp] Sending batch of 16 UDP Packets...
[CordApp] Batch of 16 packets enqueued and kernel kicked!
[CordApp] Destroying all objects!
libxdp: No bpffs found at /sys/fs/bpf
```

The log on the receiver end looks as follows:
```console
Payload #0.
Payload #1..
Payload #2...
Payload #3....
Payload #4.....
Payload #5......
Payload #6.......
Payload #7........
Payload #8.........
Payload #9..........
Payload #10..........
Payload #11...........
Payload #12............
Payload #13.............
Payload #14..............
Payload #15...............
```

To cleanup (destroy) the Docker deployment, do the following:
```bash
# On the host, from the apps/af_xdp_udp/deployment/containers
./cleanup.sh
```

### Bare-metal
First, run the scripts that create the namespaces on the host that assign and configure the network interfaces (you may need to change the names, as they are currently hard-coded):
```bash
cd apps/af_xdp_udp/deployment/connectx
sudo ./connectx_test.sh
```

Then, copy the binaries to the corresponding containers:
```bash
cp ../../../build/apps/af_xdp_udp/af_xdp_udp_a_mlx5_app /tmp/
cp ../../../build/apps/af_xdp_udp/af_xdp_udp_b_mlx5_app /tmp/
cp ../../../build/apps/af_xdp_udp/udp_rx_filter.bpf.o /tmp/
```

Then, run both sides as follows:
```bash
# Go to /tmp
cd /tmp
```

```bash
# Inside a separate terminal window (launch the receiver side first)
sudo ip netns exec ns2 /tmp/af_xdp_udp_b_mlx5_app
[sudo] password for vmetodiev: 
[CordApp] Launching the PacketCord AF_XDP UDP App!
[cord_xdp_socket_init] attempting zero-copy mode with dedicated UMEM...
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
libxdp: No bpffs found at /sys/fs/bpf
libxdp: Can't use dispatcher without a working bpffs
libxdp: Falling back to loading single prog without dispatcher
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
```

> Note: Compared to the `veth` case, observe that here we do not have a fallback after the `attempting zero-copy mode with dedicated UMEM...` message. This is indeed XDP_ZEROCOPY mode.

```bash
# Inside a separate terminal window (the sender)
sudo ip netns exec ns1 /tmp/af_xdp_udp_a_mlx5_app
[CordApp] Launching the PacketCord AF_XDP UDP App!
[cord_xdp_socket_init] attempting zero-copy mode with dedicated UMEM...
libbpf: elf: skipping unrecognized data section(8) .xdp_run_config
libbpf: elf: skipping unrecognized data section(9) xdp_metadata
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
libxdp: No bpffs found at /sys/fs/bpf
libxdp: Can't use dispatcher without a working bpffs
libxdp: Falling back to loading single prog without dispatcher
libbpf: elf: skipping unrecognized data section(7) xdp_metadata
[CordApp] Sending batch of 16 UDP Packets...
[CordApp] Batch of 16 packets enqueued and kernel kicked!
[CordApp] Destroying all objects!
libxdp: No bpffs found at /sys/fs/bpf
```

The log on the receiver end looks as follows:
```console
Payload #0.
Payload #1..
Payload #2...
Payload #3....
Payload #4.....
Payload #5......
Payload #6.......
Payload #7........
Payload #8.........
Payload #9..........
Payload #10..........
Payload #11...........
Payload #12............
Payload #13.............
Payload #14..............
Payload #15...............
```

To cleanup (destroy) the namespace deployment, do the following:
```bash
# On the host, from the apps/af_xdp_udp/deployment/connectx
sudo ./connectx_test_cleanup.sh
```

## Limitations
- Bypassing the kernel IP stack may result in unwanted ICMP messages (because there is no `bind()` operation that registers the port). One possible solution is to disable the ICMP.
- Only IPv4 supported at OSI Layer 3.
- Checksums for the IPv4 and UDP headers are calculated entirely in software - although this is not a tedious operation, those could be offload to the NIC (possibly as AF_XDP metadata).

## Future
- Probably evolve towards a (zero-copy) UDP/IP stack
- Probably start working on an integration of the concept inside the OCUDU Project
- Test the performance and resource utilisation - in terms of hardware, syscall count, throughput (with RFC2544), delay, jitter, etc.

## References
- https://docs.ebpf.io/ebpf-library/
- Google Gemini - helping with the XSK API, UMEM and AF_XDP socket creation
- https://github.com/xdp-project/bpf-examples/tree/main/AF_XDP-example
