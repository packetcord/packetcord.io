# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# Find libbpf using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBPF REQUIRED libbpf) # Note: NO "IMPORTED_TARGET" here

# apps/l2_patch_ebpf/CMakeLists.txt
add_executable(l2_patch_ebpf_app l2_patch_ebpf_main.c)

# Link against cord-flow and LibBPF variables
target_link_libraries(l2_patch_ebpf_app
    PRIVATE
        cord_flow
        ${LIBBPF_LIBRARIES}     # Resolves to 'bpf' (which passes -lbpf safely!)
)

# Include paths for headers
target_include_directories(l2_patch_ebpf_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${LIBBPF_INCLUDE_DIRS}   # Automatically finds where bpf/libbpf.h lives
)

# Add LibBPF compile options (just in case pkg-config provides any)
target_compile_options(l2_patch_ebpf_app
    PRIVATE
        ${LIBBPF_CFLAGS_OTHER}
)

# Add LibBPF link directories so GCC knows where the library file lives
target_link_directories(l2_patch_ebpf_app
    PRIVATE
        ${LIBBPF_LIBRARY_DIRS}
)
