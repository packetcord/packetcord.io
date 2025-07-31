cmake_minimum_required(VERSION 3.24)
project(PacketCord LANGUAGES C)

# — Use strict ISO C23 mode —
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)    # -std=c23 not -std=gnu23

# — Automatically init & update all submodules to their branch tips —
find_package(Git REQUIRED)
execute_process(
  COMMAND ${GIT_EXECUTABLE}
          submodule update --init --recursive --remote --merge
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE _submod_result
  OUTPUT_QUIET ERROR_QUIET
)
if(NOT _submod_result EQUAL 0)
  message(FATAL_ERROR "Git submodule update failed! Please check .gitmodules.")
endif()

# — Cord-Flow always ships its own CMakeLists.txt —
add_subdirectory(modules/cord-flow)

# — Cord-Crypto (header-only or INTERFACE) —
if(EXISTS ${CMAKE_SOURCE_DIR}/modules/cord-crypto/CMakeLists.txt)
  add_subdirectory(modules/cord-crypto)
else()
  add_library(cord_crypto INTERFACE)
  target_include_directories(cord_crypto INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/modules/cord-crypto/include>
    $<INSTALL_INTERFACE:include>
  )
endif()

# — Cord-Craft (header-only or INTERFACE) —
if(EXISTS ${CMAKE_SOURCE_DIR}/modules/cord-craft/CMakeLists.txt)
  add_subdirectory(modules/cord-craft)
else()
  add_library(cord_craft INTERFACE)
  target_include_directories(cord_craft INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/modules/cord-craft/include>
    $<INSTALL_INTERFACE:include>
  )
endif()

# — Example applications —
add_subdirectory(apps/l2_passthrough EXCLUDE_FROM_ALL)
add_subdirectory(apps/l2_switch       EXCLUDE_FROM_ALL)
add_subdirectory(apps/l3_router       EXCLUDE_FROM_ALL)
add_subdirectory(apps/l3_tunnel)       # l3_tunnel is the VPN‐style app

