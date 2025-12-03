cmake_minimum_required(VERSION 3.20)
project(hft-orderbook-benchmark CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler optimizations
if (CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_options(-O3 -march=native -DNDEBUG)
else()
    add_compile_options(-g -O0 -Wall -Wextra)
endif()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)
