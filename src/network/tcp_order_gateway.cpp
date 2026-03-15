#include "tcp_order_gateway.hpp"
#include "core/metrics_collector.hpp"
#include "fix/fix_parser.hpp"
#include "utils/rdtsc.hpp"
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <span>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

/**
 *
 * Each client sends there order through TCP and get there own buffer and therad from the exchange
 * The buffer is large and track incremently by seeing what has been processed
 *
 */

namespace hft
{

TCPOrderGateway::TCPOrderGateway(int port, LockFreeQueue<Order, 1024> &queue)
    : serverSocket_{-1}, port_{port}, orderQueue_{queue}, running_{false}
{
}

TCPOrderGateway::~TCPOrderGateway()
{
    stop();
}

void TCPOrderGateway::start()
{
    // Create TCP socket: AF_INET = IPv4, SOCK_STREAM = TCP, 0 = default protocol
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }

    // Allow immediate socket reuse after restart (avoids "Address already in use" errors)
    int reuseAddressOption = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &reuseAddressOption, sizeof(reuseAddressOption));

    // Configure server address structure
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;         // IPv4
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Bind to all network interfaces (0.0.0.0)
    serverAddress.sin_port = htons(port_);      // Convert port to network byte order (big-endian)

    // Bind socket to the address and port
    if (bind(serverSocket_, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        throw std::runtime_error("Failed to bind socket");
    }

    // Start listening for connections (backlog of 5 pending connections)
    if (listen(serverSocket_, 5) < 0)
    {
        throw std::runtime_error("Failed to listen");
    }

    // Start accepting connections in a separate thread
    running_ = true;
    acceptConnectionThread_ = std::jthread(&TCPOrderGateway::acceptLoop, this);
}

void TCPOrderGateway::stop()
{
    running_ = false;
    if (serverSocket_ >= 0)
    {
        close(serverSocket_);
        serverSocket_ = -1;
    }
    // Although using J:thread auto joins we call .join() for dterministic shutdown ordering
    if (acceptConnectionThread_.joinable())
    {
        acceptConnectionThread_.join();
    }
    for (auto &clientThread : clientThreads_)
    {
        if (clientThread.joinable())
        {
            clientThread.join();
        }
    }
}

void TCPOrderGateway::acceptLoop()
{
    while (running_)
    {
        // Block waiting for incoming client connections
        int clientSocket = accept(serverSocket_, nullptr, nullptr);
        if (clientSocket < 0) // Error occured if less than 0
        {
            if (running_)
            {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }

        // Spawn a dedicated thread to handle this client's orders
        // In a real HFT system, we'd use epoll/io_uring for zero-copy I/O.
        // For this benchmark, one thread per client is fine (usually just 1 client).
        clientThreads_.emplace_back(&TCPOrderGateway::clientHandler, this, clientSocket);
    }
}

void TCPOrderGateway::clientHandler(int clientSocket)
{
    // 4KB buffer for incoming FIX messages
    std::vector<char> buffer(4096);
    size_t offset = 0; // Track partially received message data

    while (running_)
    {
        // Read data from socket into buffer (non-blocking after initial data)
        ssize_t bytesRead = recv(clientSocket, buffer.data() + offset, buffer.size() - offset, 0);
        if (bytesRead <= 0)
        {
            break; // Connection closed or error
        }

        size_t totalBytes = offset + bytesRead;
        size_t processed = 0;

        // Parse all complete FIX messages in the buffer
        while (processed < totalBytes)
        {
            size_t consumed = 0;
            std::span<const char> data(buffer.data() + processed, totalBytes - processed);

            // Check message type
            auto msgType = FIXParser::getMessageType(data);

            // CLIENT REQUESTS STATS
            if (msgType == "U1")
            {
                // metrics_ set by main.cpp belonging to the matching engine
                if (metrics_)
                {
                    // Wait for processing to complete (Sync Benchmarking)
                    // Tag 596 in our custom U1 message = ExpectedCount
                    size_t expectedCount = 0;
                    std::string dStr(data.data(), data.size());
                    size_t pos = dStr.find("596=");

                    // Extract expected count from the FIX message
                    if (pos != std::string::npos)
                    {
                        expectedCount = std::stoull(dStr.substr(pos + 4, dStr.find('\x01', pos) - (pos + 4)));
                    }

                    // Wait until matching engine matches expected number of orders
                    while (metrics_->getOrderCount() < expectedCount && running_)
                    {
                        std::this_thread::yield();
                    }

                    auto stats = metrics_->getStats();
                    auto netStats = metrics_->getNetworkStats();
                    auto engStats = metrics_->getEngineStats();
                    auto queStats = metrics_->getQueueStats();

                    char statsData[512];
                    int len = std::snprintf(statsData, sizeof(statsData),
                                            "8=FIX.4.2\x01"
                                            "35=U2\x01"
                                            "Mean=%0.2f\x01"
                                            "P99=%llu\x01"
                                            "Max=%llu\x01"
                                            "NetMean=%0.2f\x01"
                                            "QueMean=%0.2f\x01"
                                            "EngMean=%0.2f\x01"
                                            "Count=%llu\x01"
                                            "10=000\x01",
                                            stats.mean, (unsigned long long)stats.p99, (unsigned long long)stats.max,
                                            netStats.mean, queStats.mean, engStats.mean,
                                            (unsigned long long)metrics_->getOrderCount());
                    send(clientSocket, statsData, len, 0);
                }
                FIXParser::parse(data, consumed); // Still call parse to advance consumed
            }
            // CLIENT SENDING REGULAR ORDER
            else
            {
                auto order = FIXParser::parse(data, consumed);
                if (consumed == 0)
                {
                    break; // Incomplete message - need more data from socket
                }

                // If parsing succeeded, push order to lock-free queue for processing
                if (order)
                {
                    // order->receiveTimestamp is set here, receiveTimestamp - sendTimestamp = network latency
                    order->receiveTimestamp = getCurrentTimeNs();
                    while (!orderQueue_.push(*order))
                    {
                        // Queue full - yield CPU and retry (busy-wait for low latency)
                        std::this_thread::yield();
                    }
                }
            }
            processed += consumed;
        }

        // Move any unparsed/incomplete message data to the front of the buffer
        // This handles FIX messages that span multiple recv() calls
        if (processed < totalBytes)
        {
            std::memmove(buffer.data(), buffer.data() + processed, totalBytes - processed);
            offset = totalBytes - processed; // Next recv() appends after this
        }
        else
        {
            offset = 0; // Buffer fully consumed, start fresh
        }
    }
    close(clientSocket); // Clean up when client disconnects
}

} // namespace hft
