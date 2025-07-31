cmake_minimum_required(VERSION 3.24)
project(PacketCord LANGUAGES C)

# Request ISO C23
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)    # use “-std=c23” not “-std=gnu23”

# === Auto-update submodules ===
find_package(Git REQUIRED)
execute_process(
  COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE _submod_result
  OUTPUT_QUIET ERROR_QUIET
)
if(NOT _submod_result EQUAL 0)
  message(FATAL_ERROR "Git submodule update failed!")
endif()

# === Add modules & apps ===
add_subdirectory(modules/cord-flow)
add_subdirectory(modules/cord-crypto)
add_subdirectory(modules/cord-craft)

add_subdirectory(apps/vpn)
add_subdirectory(apps/l2_passthrough EXCLUDE_FROM_ALL)
add_subdirectory(apps/l2_switch       EXCLUDE_FROM_ALL)
add_subdirectory(apps/l3_router       EXCLUDE_FROM_ALL)

