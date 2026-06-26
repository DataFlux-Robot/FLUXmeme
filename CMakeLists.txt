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
    src/port/port_file.c     # portable file backend (stdio/Win32 + flock/LockFileEx)
)
set(FLUX_TRANSCODE
    src/transcode/okf/okf_out.c
    src/transcode/okf/okf_in.c
)

add_library(fluxmeme STATIC ${FLUX_CORE} ${FLUX_TRANSCODE})
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
flux_test(test_record_codec)
flux_test(test_store_mvcc)

# Demos
add_executable(demo_okf demo/demo_okf.c)
target_link_libraries(demo_okf PRIVATE fluxmeme)
