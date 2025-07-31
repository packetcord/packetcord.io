cmake_minimum_required(VERSION 3.20)
project(PacketCord LANGUAGES C CXX)

# 3.1 Add the libraries
add_subdirectory(modules/cord-flow)
add_subdirectory(modules/cord-crypto)
add_subdirectory(modules/cord-craft)

# 3.2 Add the apps (vpn builds by default; others are placeholders)
add_subdirectory(apps/vpn)
add_subdirectory(apps/l2_passthrough EXCLUDE_FROM_ALL)
add_subdirectory(apps/l2_switch       EXCLUDE_FROM_ALL)
add_subdirectory(apps/l3_router       EXCLUDE_FROM_ALL)
