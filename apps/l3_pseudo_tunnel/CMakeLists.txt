# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/l3_pseudo_tunnel/CMakeLists.txt

# Server executable - needs cord_flow and cord_crypto
add_executable(l3_pseudo_tunnel_tep_app l3_pseudo_tunnel_tep_main.c)

target_link_libraries(l3_pseudo_tunnel_tep_app
    PRIVATE
        cord_flow
        cord_crypto
)

target_include_directories(l3_pseudo_tunnel_tep_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_SOURCE_DIR}/modules/cord-crypto/include
)

# Client executable - needs cord_craft and cord_crypto
add_executable(l3_pseudo_tunnel_client_app l3_pseudo_tunnel_client_main.c)

target_link_libraries(l3_pseudo_tunnel_client_app
    PRIVATE
        cord_craft
        cord_crypto
)

target_include_directories(l3_pseudo_tunnel_client_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-craft/include
        ${CMAKE_SOURCE_DIR}/modules/cord-crypto/include
)
