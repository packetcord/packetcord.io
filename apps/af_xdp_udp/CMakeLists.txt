# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/af_xdp_udp/CMakeLists.txt

# Side A executable - needs cord_flow
add_executable(af_xdp_udp_b_app af_xdp_udp_b_main.c)

target_link_libraries(af_xdp_udp_b_app
    PRIVATE
        cord_flow
        cord_craft
)

target_include_directories(af_xdp_udp_b_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_SOURCE_DIR}/modules/cord-craft/include
)

# Side B executable - needs cord_flow
add_executable(af_xdp_udp_a_app af_xdp_udp_a_main.c)

target_link_libraries(af_xdp_udp_a_app
    PRIVATE
        cord_flow
        cord_craft
)

target_include_directories(af_xdp_udp_a_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_SOURCE_DIR}/modules/cord-craft/include
)
