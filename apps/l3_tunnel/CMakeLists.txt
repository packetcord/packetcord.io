# apps/l3_tunnel/CMakeLists.txt
add_executable(l3_tunnel_app l3_tunnel_main.c)

# Link against cord-flow (and cord-crypto if needed)
target_link_libraries(l3_tunnel_app
    PRIVATE
        cord_flow
        cord_crypto   # remove if not using crypto
)

# Include paths for headers
target_include_directories(l3_tunnel_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_SOURCE_DIR}/modules/cord-crypto/include
)
