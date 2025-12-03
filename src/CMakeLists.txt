# src/CMakeLists.txt

# Create a library for the core components
add_library(hft_core
    core/Order.hpp
    core/Trade.hpp
    core/IOrderBook.hpp
    core/Types.hpp
    core/MatchingEngine.cpp
    core/MatchingEngine.hpp
    core/MetricsCollector.hpp
    orderbooks/MapOrderBook.cpp
    orderbooks/MapOrderBook.hpp
    network/TCPOrderGateway.cpp
    network/TCPOrderGateway.hpp
    network/FIXParser.cpp
    network/FIXParser.hpp
    utils/rdtsc.hpp
    utils/LockFreeQueue.hpp
)

target_include_directories(hft_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(hft_core PUBLIC pthread)

# Main executable
add_executable(hft_exchange_server main.cpp)
target_link_libraries(hft_exchange_server PRIVATE hft_core)

# C++ Client
add_executable(tcp_order_sender ../clients/tcp_order_sender.cpp)
