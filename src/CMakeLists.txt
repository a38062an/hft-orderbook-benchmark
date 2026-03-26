# src/CMakeLists.txt

# Create a library for the core components
add_library(hft_core
    core/order.hpp
    core/trade.hpp
    core/i_order_book.hpp
    core/types.hpp
    core/matching_engine.cpp
    core/matching_engine.hpp
    core/metrics_collector.hpp
    orderbooks/map_order_book.cpp
    orderbooks/map_order_book.hpp
    orderbooks/vector_order_book.cpp
    orderbooks/vector_order_book.hpp
    orderbooks/array_order_book.cpp
    orderbooks/array_order_book.hpp
    orderbooks/hybrid_order_book.cpp
    orderbooks/hybrid_order_book.hpp
    orderbooks/pool_order_book.cpp
    orderbooks/pool_order_book.hpp
    network/tcp_order_gateway.cpp
    network/tcp_order_gateway.hpp
    utils/rdtsc.hpp
    utils/lock_free_queue.hpp
)

target_include_directories(hft_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs
)

target_link_libraries(hft_core PUBLIC pthread)

# Main executable
add_executable(hft_exchange_server main.cpp)
target_link_libraries(hft_exchange_server PRIVATE hft_core)

# C++ Client
add_executable(tcp_order_sender ../clients/tcp_order_sender.cpp)
