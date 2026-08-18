# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/ids/CMakeLists.txt
add_executable(ids_app
    ids_main.c
    src/signature_ocl_matrix.c
)

# Find OpenCL
find_package(OpenCL REQUIRED)

# Compile definitions
target_compile_definitions(ids_app
    PRIVATE
        CL_TARGET_OPENCL_VERSION=120
        _GNU_SOURCE
)

# Link against cord-flow and OpenCL
target_link_libraries(ids_app
    PRIVATE
        cord_flow
        OpenCL::OpenCL
)

# Include paths for headers
target_include_directories(ids_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

# Copy OpenCL kernel to build directory
add_custom_command(TARGET ids_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vecMatchKernel.cl
        $<TARGET_FILE_DIR:ids_app>/vecMatchKernel.cl
)
