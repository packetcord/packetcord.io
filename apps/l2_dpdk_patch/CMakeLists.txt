# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/l2_dpdk_patch/CMakeLists.txt
add_executable(l2_dpdk_patch_app l2_dpdk_patch_main.c)

# Find DPDK using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# Link against cord-flow and DPDK
target_link_libraries(l2_dpdk_patch_app
    PRIVATE
        cord_flow
        ${DPDK_LIBRARIES}
)

# Include paths for headers
target_include_directories(l2_dpdk_patch_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${DPDK_INCLUDE_DIRS}
)

# Add DPDK compile options
target_compile_options(l2_dpdk_patch_app
    PRIVATE
        ${DPDK_CFLAGS_OTHER}
)

# Add DPDK link options
target_link_directories(l2_dpdk_patch_app
    PRIVATE
        ${DPDK_LIBRARY_DIRS}
)