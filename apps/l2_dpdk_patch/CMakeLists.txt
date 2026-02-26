# Default: strict ISO C23 for everything
set(CMAKE_C_STANDARD        23 CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_REQUIRED ON   CACHE BOOL   "" FORCE)
set(CMAKE_C_EXTENSIONS      OFF  CACHE BOOL   "" FORCE)

# apps/l2_dpdk_patch/CMakeLists.txt
add_executable(l2_dpdk_patch_app l2_dpdk_patch_main.c)

# Find DPDK using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

# Resolve rte_* libs to their full .a paths to force static linking.
# System deps (systemd, isal, numa, bsd, …) are checked for availability:
# present → link dynamically; absent → skip with a warning (optional backends).
list(GET DPDK_LIBRARY_DIRS 0 _dpdk_lib_dir)
set(_dpdk_link_libs "")
foreach(_lib ${DPDK_STATIC_LIBRARIES})
    if(_lib MATCHES "^rte_")
        # rte_* name → resolve to full .a path
        set(_lib_path "${_dpdk_lib_dir}/lib${_lib}.a")
        if(EXISTS "${_lib_path}")
            list(APPEND _dpdk_link_libs "${_lib_path}")
        else()
            list(APPEND _dpdk_link_libs "${_lib}")
        endif()
    elseif(_lib MATCHES "^:")
        # pkg-config colon-filename syntax (:libfoo.a) → resolve to full path
        string(SUBSTRING "${_lib}" 1 -1 _lib_filename)
        set(_lib_path "${_dpdk_lib_dir}/${_lib_filename}")
        if(EXISTS "${_lib_path}")
            list(APPEND _dpdk_link_libs "${_lib_path}")
        else()
            message(STATUS "Optional DPDK dep '${_lib}' not found at ${_lib_path} — skipping")
        endif()
    else()
        # System dep — link dynamically if present, skip if not
        find_library(_sys_lib_found NAMES ${_lib} HINTS ${DPDK_LIBRARY_DIRS})
        if(_sys_lib_found)
            list(APPEND _dpdk_link_libs "${_lib}")
        else()
            message(STATUS "Optional DPDK system dep '${_lib}' not found — skipping")
        endif()
        unset(_sys_lib_found CACHE)
    endif()
endforeach()

# Link against cord-flow and DPDK
target_link_libraries(l2_dpdk_patch_app
    PRIVATE
        cord_flow
        ${_dpdk_link_libs}
)

# Include paths for headers
target_include_directories(l2_dpdk_patch_app
    PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/cord-flow/include
        ${DPDK_INCLUDE_DIRS}
)

# Add DPDK compile options
target_compile_options(l2_dpdk_patch_app
    PRIVATE
        ${DPDK_CFLAGS_OTHER}
)

# Add DPDK link options
target_link_directories(l2_dpdk_patch_app
    PRIVATE
        ${DPDK_LIBRARY_DIRS}
)
