cmake_minimum_required(VERSION 3.20)
project(hft-orderbook-benchmark CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

option(HFT_ENABLE_INTEGRATION_TESTS "Build integration tests (TCP/socket path)" OFF)
option(HFT_ENABLE_COVERAGE "Enable compiler coverage instrumentation" OFF)

if(HFT_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping -O0 -g)
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    else()
        message(WARNING "Coverage requested but compiler is unsupported: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# Compiler optimizations
if (CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_options(-O3 -march=native -DNDEBUG)
else()
    add_compile_options(-g -O0 -Wall -Wextra)
endif()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(benchmarks)
