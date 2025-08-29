# 🌐 PacketCord.io

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C Standard](https://img.shields.io/badge/C-C23-blue.svg)](https://en.wikipedia.org/wiki/C23_%28C_standard_revision%29)
[![Build System](https://img.shields.io/badge/Build-CMake-brightgreen.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Embedded-lightgrey.svg)](https://github.com/packetcord/packetcord.io)

**High-Performance Network Programming Framework**

PacketCord.io is a comprehensive C-based framework designed for high-performance network programming, packet processing, and cryptographic operations. Built with modularity and performance in mind, it provides the building blocks for creating sophisticated network applications from embedded systems to high-end servers.

---

## 📦 Framework Components

### 🌊 CORD-FLOW
![In Development](https://img.shields.io/badge/Status-In_Development-orange)
[![Repository](https://img.shields.io/badge/Repo-cord--flow-blue)](https://github.com/packetcord/cord-flow)

High-performance packet flow programming with receive-match-action-transmit logic.

**Key Features:**
- Zero-copy packet processing
- Multiple FlowPoint abstractions (Raw sockets, DPDK, XDP)
- Linux API event handling
- Protocol header parsing (L2-L7)
- Memory management with huge page support

### 🔐 CORD-CRYPTO
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

### 🔨 CORD-CRAFT
![Planned](https://img.shields.io/badge/Status-Planned-red)
[![Repository](https://img.shields.io/badge/Repo-cord--craft-blue)](https://github.com/packetcord/cord-craft)

Network packet crafting and generation tools for testing and development.

**Planned Features:**
- Protocol-aware packet generation
- Traffic pattern simulation
- Network testing utilities
- Integration with CORD-FLOW

---

## 📱 Example Applications

### ✅ Implemented Applications

| Application | Status | Description |
|-------------|--------|-------------|
| `l3_tunnel` | ![Implemented](https://img.shields.io/badge/Status-Implemented-brightgreen) | IPv4 tunnel implementation using L2/L3/L4 FlowPoints |

### 📋 Planned Applications

| Application | Status | Description |
|-------------|--------|-------------|
| `l2_patch` | ![Planned](https://img.shields.io/badge/Status-Planned-red) | Virtual ethernet link between VM/container interfaces |
| `l2_passthrough` | ![Planned](https://img.shields.io/badge/Status-Planned-red) | Layer 2 frame passthrough processing |
| `l2_switch` | ![Planned](https://img.shields.io/badge/Status-Planned-red) | Software-based Ethernet switch |
| `l3_router` | ![Planned](https://img.shields.io/badge/Status-Planned-red) | Software-based IP router |

---

## 🛠️ Build Instructions

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

### Building Example Applications
```bash
# Build l3_tunnel example
cd apps/l3_tunnel
mkdir build && cd build
cmake .. && make -j$(nproc)
```

---

## 📁 Repository Structure

```
packetcord.io/
├── CMakeLists.txt             # Main build configuration
├── README.md                  # This file
├── apps/                      # Example applications
│   ├── l3_tunnel/             # IPv4 tunnel (implemented)
│   ├── l2_patch/              # Virtual ethernet patch (planned)
│   ├── l2_passthrough/        # L2 passthrough (planned)
│   ├── l2_switch/             # Software switch (planned)
│   └── l3_router/             # Software router (planned)
└── modules/                   # Framework components
    ├── cord-flow/             # Packet flow programming
    ├── cord-crypto/           # Cryptographic operations
    │   └── cord-aes-cipher/   # AES cipher implementation
    └── cord-craft/            # Packet crafting (planned)
```

---

## 🚀 Getting Started

### Running the L3 Tunnel Example

The `l3_tunnel` application demonstrates PacketCord.io capabilities by creating an IPv4 tunnel:

```bash
cd apps/l3_tunnel/build
sudo ./l3_tunnel
```

This example showcases:
- L2 raw socket packet capture
- IPv4 header parsing and matching
- L4 UDP tunneling
- L3 stack injection
- Event-driven packet processing

---

## 🎯 Use Cases

PacketCord.io is designed for building:

- **Network Function Virtualization (NFV)** - Virtual network functions and service chaining
- **Software-Defined Networking (SDN)** - Custom packet processing and forwarding
- **Security Applications** - Deep packet inspection and traffic analysis
- **Protocol Development** - Custom protocol implementation and testing
- **Embedded Networking** - Resource-constrained network applications
- **High-Performance Computing** - Zero-copy packet processing for HPC networks

---

## 🤝 Contributing

We welcome contributions to PacketCord.io! Please:

1. 🍴 Fork the repository
2. 🌟 Create a feature branch
3. 🧪 Add tests for new functionality
4. 📝 Update documentation
5. 🔄 Submit a pull request

For major changes, please open an issue first to discuss your proposed changes.

---

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## 🏷️ Keywords

`networking` `packet-processing` `high-performance` `zero-copy` `embedded` `c` `nfv` `sdn` `cryptography` `framework`

---

<div align="center">

**Built with ❤️ by the PacketCord Team**

[![GitHub Stars](https://img.shields.io/github/stars/packetcord/packetcord.io?style=social)](https://github.com/packetcord/packetcord.io)
[![GitHub Forks](https://img.shields.io/github/forks/packetcord/packetcord.io?style=social)](https://github.com/packetcord/packetcord.io)

</div>