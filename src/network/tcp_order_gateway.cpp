#include "tcp_order_gateway.hpp"
#include "core/metrics_collector.hpp"
#include "fix/fix_parser.hpp"
#include "utils/rdtsc.hpp"
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <span>
#include <sys/socket.h>
#include <sys/time.h>
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

namespace
{
bool sendAll(int socketFd, const char *data, size_t length)
{
    size_t sentBytes = 0;
    while (sentBytes < length)
    {
        const ssize_t sentNow = send(socketFd, data + sentBytes, length - sentBytes, 0);
        if (sentNow < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        if (sentNow == 0)
        {
            return false;
        }
        sentBytes += static_cast<size_t>(sentNow);
    }
    return true;
}
} // namespace

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
        shutdown(serverSocket_, SHUT_RDWR);
        close(serverSocket_);
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

    serverSocket_ = -1;
}

void TCPOrderGateway::acceptLoop()
{
    while (running_)
    {
        pollfd pfd{serverSocket_, POLLIN, 0};
        int pollResult = poll(&pfd, 1, 250);
        if (pollResult <= 0)
        {
            continue;
        }

        // Accept incoming client connection
        int clientSocket = accept(serverSocket_, nullptr, nullptr);
        if (clientSocket < 0) // Error occured if less than 0
        {
            if (running_ && errno != EINTR && errno != EAGAIN)
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
    // Ensure blocking socket I/O wakes periodically so stop() cannot hang on idle/stalled clients.
    timeval socketTimeout{};
    socketTimeout.tv_sec = 0;
    socketTimeout.tv_usec = 200000; // 200ms
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &socketTimeout, sizeof(socketTimeout));
    setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, &socketTimeout, sizeof(socketTimeout));

    // Start with 4KB, grow up to 1MB for fragmented or bursty payloads.
    std::vector<char> buffer(4096);
    constexpr size_t kMaxBufferBytes = 1U << 20;
    size_t offset = 0; // Track partially received message data

    while (running_)
    {
        if (offset == buffer.size())
        {
            if (buffer.size() >= kMaxBufferBytes)
            {
                // Drop unbounded malformed stream from this client.
                break;
            }

            const size_t nextSize = std::min(buffer.size() * 2, kMaxBufferBytes);
            buffer.resize(nextSize);
        }

        // Read data from socket into buffer.
        ssize_t bytesRead = recv(clientSocket, buffer.data() + offset, buffer.size() - offset, 0);
        if (bytesRead < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }
            break; // Fatal read error
        }
        if (bytesRead == 0)
        {
            break; // Connection closed by peer
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
                FIXParser::parse(data, consumed); // Advance consumed for complete/invalid U1 frame
                if (consumed == 0)
                {
                    break; // Incomplete frame
                }

                // metrics_ set by main.cpp belonging to the matching engine
                if (metrics_)
                {
                    // Wait for processing to complete (Sync Benchmarking)
                    // Tag 596 in our custom U1 message = ExpectedCount
                    size_t expectedCount = 0;
                    const std::string_view frame(data.data(), consumed);
                    auto expectedCountStr = FIXParser::getTagValue(frame, 596);

                    // Extract expected count from the FIX message
                    if (!expectedCountStr.empty())
                    {
                        auto result = std::from_chars(expectedCountStr.data(),
                                                      expectedCountStr.data() + expectedCountStr.size(), expectedCount);
                        if (result.ec != std::errc{} || result.ptr != expectedCountStr.data() + expectedCountStr.size())
                        {
                            expectedCount = 0;
                        }
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
                    if (len <= 0 || !sendAll(clientSocket, statsData, static_cast<size_t>(len)))
                    {
                        break;
                    }
                }
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
