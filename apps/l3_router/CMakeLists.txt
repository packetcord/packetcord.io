# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/l3_router/CMakeLists.txt
add_executable(l3_router_app l3_router_main.c)

# Link against cord-flow
target_link_libraries(l3_router_app
    PRIVATE
        cord_flow
        -static
)

# Include paths for headers
target_include_directories(l3_router_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
)
