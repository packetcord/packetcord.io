# PacketCord.io

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C Standard](https://img.shields.io/badge/C-C23-blue.svg)](https://en.wikipedia.org/wiki/C23_%28C_standard_revision%29)
[![Build System](https://img.shields.io/badge/Build-CMake-brightgreen.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Embedded-lightgrey.svg)](https://github.com/packetcord/packetcord.io)

Build efficient packet processing logic, custom protocols, packet crafting and network applications.


## Use Cases

PacketCord.io could be used for the development of:

- Network processing and forwarding logic
- Routing and management protocols
- Tools for NAT traversal, peer-to-peer networking, pentesting and research
- Tunnels and VPNs
- Industrial and time-sensitive networking
- Firewalls, IPS, IDS, SIEM and other security systems
- Protocol bridges and proxies
- IP over any SerDes
- IP over custom wire, radio and optical links


## Framework Components

The PacketCord.io comprises the following submodules: 
- **CORD-FLOW**: Packet processing library
- **CORD-CRYPTO**: Crypto library
- **CORD-CRAFT**: Packet crafting and injection library

Each of the components exists as a separate repository. You don't have to worry about the build process - CMake takes care of everything.


## Examples

Just go to the *apps* directory to get started with the avaialable examples - a README.md tutorial is provided for each app.


## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.


<div align="center">

[![GitHub Stars](https://img.shields.io/github/stars/packetcord/packetcord.io?style=social)](https://github.com/packetcord/packetcord.io)
[![GitHub Forks](https://img.shields.io/github/forks/packetcord/packetcord.io?style=social)](https://github.com/packetcord/packetcord.io)

</div>
