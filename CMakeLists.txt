cmake_minimum_required(VERSION 3.24)
project(PacketCord LANGUAGES C)

# 1. Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# 2. Auto‐update submodules to branch tips
find_package(Git REQUIRED)
execute_process(
  COMMAND ${GIT_EXECUTABLE}
          submodule update --init --recursive --remote --merge
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE _git_submod_res
  OUTPUT_QUIET ERROR_QUIET
)
if(NOT _git_submod_res EQUAL 0)
  message(FATAL_ERROR "Git submodule update failed! Check .gitmodules.")
endif()

# 3. Build cord-flow in GNU mode so Linux APIs are available
# Temporarily switch on GNU extensions
set(CMAKE_C_EXTENSIONS ON CACHE BOOL "" FORCE)
add_subdirectory(modules/cord-flow)
# Restore strict C23 for downstream
set(CMAKE_C_EXTENSIONS OFF CACHE BOOL "" FORCE)

# 4. cord-crypto is still header-only (INTERFACE)
if(EXISTS ${CMAKE_SOURCE_DIR}/modules/cord-crypto/CMakeLists.txt)
  add_subdirectory(modules/cord-crypto)
else()
  add_library(cord_crypto INTERFACE)
  target_include_directories(cord_crypto INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/modules/cord-crypto/include>
    $<INSTALL_INTERFACE:include>
  )
endif()

# 5. Build cord-craft in GNU mode (like cord-flow)
# Temporarily switch on GNU extensions
set(CMAKE_C_EXTENSIONS ON CACHE BOOL "" FORCE)
add_subdirectory(modules/cord-craft)
# Restore strict C23 for downstream
set(CMAKE_C_EXTENSIONS OFF CACHE BOOL "" FORCE)

# 6. Example apps
add_subdirectory(apps/l2_passthrough      EXCLUDE_FROM_ALL)
add_subdirectory(apps/l2_switch           EXCLUDE_FROM_ALL)
add_subdirectory(apps/l3_router           EXCLUDE_FROM_ALL)
add_subdirectory(apps/l3_tunnel)
add_subdirectory(apps/l3_pseudo_tunnel)