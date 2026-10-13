# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# Locate clang, fallback to /usr/bin/clang if not found in PATH
find_program(CLANG_EXECUTABLE clang PATHS /usr/bin /usr/local/bin /usr/lib/llvm/bin)

if(NOT CLANG_EXECUTABLE)
    set(CLANG_EXECUTABLE "clang")
endif()

# Define source and build output paths
set(BPF_C_SRC ${CMAKE_CURRENT_SOURCE_DIR}/udp_rx_filter.bpf.c)
set(BPF_OBJ ${CMAKE_CURRENT_BINARY_DIR}/udp_rx_filter.bpf.o)

# Command to compile the BPF program using Clang
add_custom_command(
    OUTPUT ${BPF_OBJ}
    COMMAND ${CLANG_EXECUTABLE} -g -O2 -target bpf -c ${BPF_C_SRC} -o ${BPF_OBJ}
    DEPENDS ${BPF_C_SRC}
    COMMENT "Building BPF object ${BPF_OBJ}"
    VERBATIM
)

# Custom target to build BPF file during 'make'
add_custom_target(udp_rx_filter_bpf ALL DEPENDS ${BPF_OBJ})

# Side A executable - needs cord_flow & cord_craft
add_executable(af_xdp_udp_a_app af_xdp_udp_a_main.c)
add_executable(af_xdp_udp_a_mlx5_app af_xdp_udp_a_mlx5_main.c)

target_link_libraries(af_xdp_udp_a_app
    PRIVATE
        cord_flow
        cord_craft
)

target_link_libraries(af_xdp_udp_a_mlx5_app
    PRIVATE
        cord_flow
        cord_craft
)

target_include_directories(af_xdp_udp_a_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_SOURCE_DIR}/modules/cord-craft/include
)

target_include_directories(af_xdp_udp_a_mlx5_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_SOURCE_DIR}/modules/cord-craft/include
)

# Side B executable - needs only cord_flow
add_executable(af_xdp_udp_b_app af_xdp_udp_b_main.c)
add_executable(af_xdp_udp_b_mlx5_app af_xdp_udp_b_mlx5_main.c)

target_link_libraries(af_xdp_udp_b_app
    PRIVATE
        cord_flow
)

target_link_libraries(af_xdp_udp_b_mlx5_app
    PRIVATE
        cord_flow
)

target_include_directories(af_xdp_udp_b_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
)

target_include_directories(af_xdp_udp_b_mlx5_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
)
