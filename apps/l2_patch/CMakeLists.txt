cmake_minimum_required(VERSION 3.16)
project(l2_patch C)

set(CMAKE_C_STANDARD 23)

# Link against the cord-flow library
find_library(CORD_FLOW_LIB cord_flow PATHS ${CMAKE_SOURCE_DIR}/modules/cord-flow/build)

add_executable(l2_patch l2_patch_main.c)

target_include_directories(l2_patch PRIVATE 
    ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
)

target_link_libraries(l2_patch ${CORD_FLOW_LIB})