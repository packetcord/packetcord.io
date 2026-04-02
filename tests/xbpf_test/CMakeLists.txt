# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/xbpf_test/CMakeLists.txt
add_executable(xbpf_test_app xbpf_test_main.c)

# Link against cord-flow
target_link_libraries(xbpf_test_app
    PRIVATE
        cord_flow
)

# Include paths for headers
target_include_directories(xbpf_test_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
)
