# AF_XDP UDP Example (OCUDU Proposal)

## Build instrcutions
Clone the repository, checkout to the `af_xdp_udp` branch and build the example:
```bash
mkdir build && cd build
cmake .. -DENABLE_XDP_DATAPLANE=ON -DENABLE_EBPF=ON --fresh
make
```

## Testbench
Two Testbenches have been provided:
- Containerised (over direct veth pair connection)
- Bare-metal (over physical optical patch between the two ports of Mellanox ConnectX-4Lx)

Rationale is to demonstrate that the example will work both with true zero copy (XDP_ZEROCOPY supported by the NVIDIA hardware and driver) and also sucessfully fallback to the other modes (like XDP_COPY or XDP_SKB) in case
deployed in a software environment.

### Containerised
```console
          Node A                                   Node B
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
         Namespace A                              Namespace B
  +-----------------------+                +-----------------------+
  |                       |                |                       |
  |                       |                |                       |
  |                       |  optical patch |                       |
  |          enp11s0f0np0 |<==============>| enp11s0f1np1          |
  |         192.168.168.2 |     loop       | 192.168.168.2         |
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

## Deploy
... TBD ...
### Dockerfile
... TBD ..
