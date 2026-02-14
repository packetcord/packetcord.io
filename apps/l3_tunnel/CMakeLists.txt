# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/l3_tunnel/CMakeLists.txt
add_executable(l3_tunnel_tep_a_app l3_tunnel_tep_a_main.c)
add_executable(l3_tunnel_tep_b_app l3_tunnel_tep_b_main.c)

# Link against cord-flow (and cord-crypto if needed)
target_link_libraries(l3_tunnel_tep_a_app
    PRIVATE
        cord_flow
        cord_crypto   # remove if not using crypto
)

target_link_libraries(l3_tunnel_tep_b_app
    PRIVATE
        cord_flow
        cord_crypto   # remove if not using crypto
)

# Include paths for headers
target_include_directories(l3_tunnel_tep_a_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_SOURCE_DIR}/modules/cord-crypto/include
)

target_include_directories(l3_tunnel_tep_b_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_SOURCE_DIR}/modules/cord-crypto/include
)