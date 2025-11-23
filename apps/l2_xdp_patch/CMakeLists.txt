set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

add_executable(l2_xdp_patch_app l2_xdp_patch_main.c)

target_link_libraries(l2_xdp_patch_app
    PRIVATE
        cord_flow
)

target_include_directories(l2_xdp_patch_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
)

target_compile_definitions(l2_xdp_patch_app
    PRIVATE
        ENABLE_XDP_DATAPLANE
)
