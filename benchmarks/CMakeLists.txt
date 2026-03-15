# benchmarks/CMakeLists.txt

# OrderBook benchmark executable
add_executable(orderbook_benchmark
    main.cpp
    modules/order_generator.cpp
    modules/order_book_benchmark.cpp
)

target_link_libraries(orderbook_benchmark
    PRIVATE
        hft_core
)

target_include_directories(orderbook_benchmark
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs
)

# Copy to build directory for easy execution
set_target_properties(orderbook_benchmark
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks
)
