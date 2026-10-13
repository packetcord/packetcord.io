# AF_XDP UDP Example (OCUDU Proposal)

## Build instrcutions

mkdir build && cd build
cmake .. -DENABLE_XDP_DATAPLANE=ON -DENABLE_EBPF=ON --fresh
make

// Copy to the container (container name is `container_2` in the below command)
docker cp udp_rx_filter.bpf.o container_2:/root/

// Check if BPF FS is mounted (inside the container)
cat /proc/mounts | grep 'bpf'

// If not, mount the BPF FS (inside the container)
mount -t bpf bpf /sys/fs/bpf

## Deployment
### Dockerfile

### MLX5 hardware
```bash
$ modinfo mlx5_core
```
```console
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
```
```console
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
