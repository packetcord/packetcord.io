# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      ON   CACHE BOOL   "" FORCE)

# apps/l2_tpacketv3_patch_workers/CMakeLists.txt
add_executable(l2_tpacketv3_patch_workers_app l2_tpacketv3_patch_workers_main.c)

# Link against cord-flow
target_link_libraries(l2_tpacketv3_patch_workers_app
    PRIVATE
        cord_flow
)

# Include paths for headers
target_include_directories(l2_tpacketv3_patch_workers_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
)
