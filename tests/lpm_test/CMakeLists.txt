# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/lpm_test/CMakeLists.txt
add_executable(lpm_test_app lpm_test_main.c)

# Link against cord-flow
target_link_libraries(lpm_test_app
    PRIVATE
        cord_flow
)

# Include paths for headers
target_include_directories(lpm_test_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
)
