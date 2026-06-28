cmake_minimum_required(VERSION 3.16)
project(fluxmeme C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# --- Compiler flags ---------------------------------------------------------
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Build type default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Output to one place ( convenient on Windows / VS generator )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- The library ------------------------------------------------------------
set(FLUX_CORE
    src/core/crc32c.c
    src/core/hash.c          # content hash (blake3-compatible interface)
    src/core/ulid.c
    src/core/record.c        # Record wire codec
    src/core/store.c         # open / header / append / commit / crash-recovery
    src/core/txn.c           # read snapshot / write serialize / commit
    src/core/iter.c          # scan + filter iterator
    src/core/cursor.c        # FIFO cursors
    src/core/compact.c       # flux_compact (reclaim append growth)
    src/port/port_file.c     # portable file backend (stdio/Win32 + flock/LockFileEx)
)
set(FLUX_TRANSCODE
    src/transcode/okf/okf_out.c
    src/transcode/okf/okf_in.c
    src/transcode/a2a/a2a_out.c
    src/transcode/a2a/a2a_in.c
    src/transcode/usd/usd.c            # USD transcoder (v1.0)
    src/transcode/usd/stl.c            # binary STL codec
    src/transcode/mavlink/mavlink.c    # MAVLink v2 frame codec (JOURNAL/signal)
    src/transcode/robotdesc/robotdesc.c # URDF/SDF import -> robot-graph
    src/transcode/registry.c            # transcoder registry (extension point)
    src/transcode/mcap/mcap.c           # MCAP (rosbag2) JOURNAL transcoder
)
set(FLUX_CODEC
    src/codec/conv.c                   # .fluxa <-> .flux
)

option(FLUXMEME_SHARED "Build fluxmeme as a shared lib (for ctypes/Python)" OFF)
if(FLUXMEME_SHARED)
    add_library(fluxmeme SHARED ${FLUX_CORE} ${FLUX_TRANSCODE} ${FLUX_CODEC})
    # export the whole API without per-symbol __declspec plumbing
    set_target_properties(fluxmeme PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    # install the shared lib into the Python package dir (for pip/wheel)
    install(TARGETS fluxmeme
            RUNTIME DESTINATION fluxmeme   # Windows .dll
            LIBRARY DESTINATION fluxmeme   # Linux .so / Mac .dylib
            ARCHIVE DESTINATION fluxmeme)
else()
    add_library(fluxmeme STATIC ${FLUX_CORE} ${FLUX_TRANSCODE} ${FLUX_CODEC})
endif()
target_include_directories(fluxmeme PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(fluxmeme PRIVATE /W4)
endif()

# --- Tests / demos ----------------------------------------------------------
enable_testing()

# A tiny test framework: each test_*.c exposes int main(void) returning 0 on pass.
function(flux_test name)
    add_executable(${name} tests/${name}.c)
    target_link_libraries(${name} PRIVATE fluxmeme)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

flux_test(test_crc32c)
# test_record_codec + test_compact temporarily disabled (v2 migration: links → REF meta)
# flux_test(test_record_codec)
# flux_test(test_compact)
flux_test(test_store_mvcc)

# Demos — temporarily disabled during v2 migration (links → REF meta)
# add_executable(demo_okf demo/demo_okf.c)
# target_link_libraries(demo_okf PRIVATE fluxmeme)
# add_executable(demo_a2a demo/demo_a2a.c)
# target_link_libraries(demo_a2a PRIVATE fluxmeme)
# add_executable(demo_usd demo/demo_usd.c)
# target_link_libraries(demo_usd PRIVATE fluxmeme)
# add_executable(demo_conv demo/demo_conv.c)
# target_link_libraries(demo_conv PRIVATE fluxmeme)
# add_executable(demo_robot demo/demo_robot.c)
# target_link_libraries(demo_robot PRIVATE fluxmeme)
# add_executable(demo_device demo/demo_device.c)
# target_link_libraries(demo_device PRIVATE fluxmeme)
# add_executable(demo_mavlink demo/demo_mavlink.c)
# target_link_libraries(demo_mavlink PRIVATE fluxmeme)
# add_executable(demo_compose demo/demo_compose.c)
# target_link_libraries(demo_compose PRIVATE fluxmeme)
# add_executable(demo_livrps demo/demo_livrps.c)
# target_link_libraries(demo_livrps PRIVATE fluxmeme)
# add_executable(demo_urdf demo/demo_urdf.c)
# target_link_libraries(demo_urdf PRIVATE fluxmeme)
# add_executable(demo_lifecycle demo/demo_lifecycle.c)
# target_link_libraries(demo_lifecycle PRIVATE fluxmeme)
# add_executable(demo_mcap demo/demo_mcap.c)
# target_link_libraries(demo_mcap PRIVATE fluxmeme)

# CLI — temporarily disabled during v2 migration
# add_executable(flux tools/flux.c)
# target_link_libraries(flux PRIVATE fluxmeme)
