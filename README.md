# AF_XDP UDP Example (OCUDU proposal)

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
// Dockerfile

// MLX5 hardware
