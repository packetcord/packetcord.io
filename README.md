# PacketCord.io

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C Standard](https://img.shields.io/badge/C-C23-blue.svg)](https://en.wikipedia.org/wiki/C23_%28C_standard_revision%29)
[![Build System](https://img.shields.io/badge/Build-CMake-brightgreen.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Embedded-lightgrey.svg)](https://github.com/packetcord/packetcord.io)

**High-Performance Network Programming Framework**

PacketCord.io is a comprehensive C-based framework designed for high-performance network programming, packet processing, and cryptographic operations. Built with modularity and performance in mind, it provides the building blocks for creating sophisticated network applications from embedded systems to high-end servers.

---

## Framework Components

### CORD-FLOW
![In Development](https://img.shields.io/badge/Status-In_Development-orange)
[![Repository](https://img.shields.io/badge/Repo-cord--flow-blue)](https://github.com/packetcord/cord-flow)

High-performance packet flow programming with receive-match-action-transmit logic.

**Key Features:**
- Zero-copy packet processing
- Multiple FlowPoint abstractions (Raw sockets, DPDK, XDP)
- Linux API event handling
- Protocol header parsing (L2-L7)
- Memory management with huge page support

### CORD-CRYPTO
![Limited Implementation](https://img.shields.io/badge/Status-Limited_Implementation-yellow)
[![Repository](https://img.shields.io/badge/Repo-cord--crypto-blue)](https://github.com/packetcord/cord-crypto)

Cryptographic operations and security primitives for network applications.

**Current Implementation:**
- **[cord-aes-cipher](https://github.com/packetcord/cord-aes-cipher)** ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) - Hardware-accelerated AES-128/192/256
  - Software implementation with AES-NI and NEON acceleration
  - Configurable for embedded systems
  - Platform-independent design

**Planned Components:**
- AES cipher modes (CBC, CTR, ECB, GCM) ![Planned](https://img.shields.io/badge/Status-Planned-red)
- ChaCha20 cipher ![Planned](https://img.shields.io/badge/Status-Planned-red)

### CORD-CRAFT
![Planned](https://img.shields.io/badge/Status-Planned-red)
[![Repository](https://img.shields.io/badge/Repo-cord--craft-blue)](https://github.com/packetcord/cord-craft)

Network packet crafting and generation tools for testing and development.

**Planned Features:**
- Protocol-aware packet generation
- Network testing utilities
- Integration with CORD-FLOW

---

## Example Applications

### Implemented Applications

| Application | Status | Description |
|-------------|--------|-------------|
| `l3_tunnel` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | IPv4 tunnel implementation using L2/L3/L4 (UDP) FlowPoints |
| `l3_pseudo_tunnel` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | IPv4 pseudo-tunnel implementation with client and server part (UDP) |
| `l2_patch` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | Virtual Ethernet link (AF_PACKET)|
| `l2_patch_workers` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | Multi-threaded Virtual Ethernet link (AF_PACKET)|
| `l2_dpdk_patch` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | Virtual Ethernet link (DPDK)|
| `l2_dpdk_patch_workers` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | Multi-threaded Virtual Ethernet link (DPDK)|
| `l2_tpacketv3_patch` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | Virtual Ethernet link (TPACKETv3)|
| `l2_tpacketv3_patch_workers` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | Multi-threaded Virtual Ethernet link (TPACKETv3)|
| `l2_xdp_patch` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | Virtual Ethernet link (AF_XDP)|
| `l2_xdp_patch_workers` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | Multi-threaded Virtual Ethernet link (AF_XDP)|

### Planned Applications

| Application | Status | Description |
|-------------|--------|-------------|
| `l2_switch` | ![Planned](https://img.shields.io/badge/Status-Planned-red) | Software-based Ethernet switch |
| `l3_router` | ![Planned](https://img.shields.io/badge/Status-Planned-red) | Software-based IPv4 router |

---

## Build Instructions

### Prerequisites
- **CMake** 3.16 or higher
- **GCC** with C23 support
- **Linux** development headers

### Building the Framework
```bash
git clone https://github.com/packetcord/packetcord.io.git
cd packetcord.io
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Building Individual Components
```bash
# Build CORD-FLOW
cd modules/cord-flow
mkdir build && cd build
cmake .. && make -j$(nproc)

# Build CORD-CRYPTO
cd modules/cord-crypto
mkdir build && cd build
cmake .. && make -j$(nproc)
```

---

## Use Cases

PacketCord.io is designed for building:

- **Network Function Virtualization (NFV)**
- **Software-Defined Networking (SDN)**
- **Security Applications**
- **Protocol Development**
- **Embedded Networking**
- **High-Performance Computing**

---

## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## Keywords

`networking` `packet-processing` `high-performance` `zero-copy` `embedded` `c` `nfv` `sdn` `cryptography` `framework`

---

<div align="center">

**Built with ❤️ by the PacketCord Team**

[![GitHub Stars](https://img.shields.io/github/stars/packetcord/packetcord.io?style=social)](https://github.com/packetcord/packetcord.io)
[![GitHub Forks](https://img.shields.io/github/forks/packetcord/packetcord.io?style=social)](https://github.com/packetcord/packetcord.io)

</div>