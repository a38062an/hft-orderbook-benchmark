#pragma once

#include "core/Order.hpp"
#include "utils/rdtsc.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace hft
{

class MockClient
{
  public:
    MockClient(const std::string &host, int port) : host_(host), port_(port), sock_(-1)
    {
    }
    ~MockClient()
    {
        disconnect();
    }

    bool connect()
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0)
            return false;

        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port_);

        if (inet_pton(AF_INET, host_.c_str(), &serv_addr.sin_addr) <= 0)
            return false;
        if (::connect(sock_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            return false;

        // Prevent indefinite blocking when waiting for server responses.
        timeval socketTimeout{};
        socketTimeout.tv_sec = 5;
        socketTimeout.tv_usec = 0;
        setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &socketTimeout, sizeof(socketTimeout));
        setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO, &socketTimeout, sizeof(socketTimeout));

        return true;
    }

    void disconnect()
    {
        if (sock_ >= 0)
        {
            close(sock_);
            sock_ = -1;
        }
    }

    bool sendOrder(const Order &order)
    {
        std::string fix = toFIX(order);
        return sendAll(fix.data(), fix.size());
    }

    struct ServerStats
    {
        double mean;
        unsigned long long p99;
        unsigned long long max;
        unsigned long long processed;
        double netMean;
        double queMean;
        double engMean;
    };

    std::optional<ServerStats> requestServerStats(size_t expectedCount = 0)
    {
        char reqBuf[128];
        int reqLen = std::snprintf(reqBuf, sizeof(reqBuf),
                                   "8=FIX.4.2\x01"
                                   "35=U1\x01"
                                   "596=%zu\x01"
                                   "10=000\x01",
                                   expectedCount);
        if (reqLen <= 0 || !sendAll(reqBuf, static_cast<size_t>(reqLen)))
        {
            return std::nullopt;
        }

        char buffer[1024];
        ssize_t received = recv(sock_, buffer, sizeof(buffer), 0);
        if (received <= 0)
        {
            return std::nullopt;
        }

        std::string_view response(buffer, received);
        if (response.find("35=U2") == std::string_view::npos)
        {
            return std::nullopt;
        }

        ServerStats stats{};
        auto getVal = [&](std::string_view tag)
        {
            size_t pos = response.find(tag);
            if (pos == std::string_view::npos)
            {
                return std::string_view();
            }
            size_t start = pos + tag.length();
            size_t end = response.find('\x01', start);
            if (end == std::string_view::npos)
            {
                return std::string_view();
            }
            return response.substr(start, end - start);
        };

        auto meanStr = getVal("Mean=");
        auto p99Str = getVal("P99=");
        auto maxStr = getVal("Max=");
        auto countStr = getVal("Count=");
        auto netMeanStr = getVal("NetMean=");
        auto queMeanStr = getVal("QueMean=");
        auto engMeanStr = getVal("EngMean=");

        if (meanStr.empty())
        {
            return std::nullopt;
        }

        try
        {
            stats.mean = std::stod(std::string(meanStr));
            stats.p99 = std::stoull(std::string(p99Str));
            stats.max = std::stoull(std::string(maxStr));
            stats.processed = std::stoull(std::string(countStr));

            if (!netMeanStr.empty())
            {
                stats.netMean = std::stod(std::string(netMeanStr));
            }
            if (!queMeanStr.empty())
            {
                stats.queMean = std::stod(std::string(queMeanStr));
            }
            if (!engMeanStr.empty())
            {
                stats.engMean = std::stod(std::string(engMeanStr));
            }
        }
        catch (...)
        {
            return std::nullopt;
        }

        return stats;
    }

  private:
    bool sendAll(const char *data, size_t length)
    {
        size_t sentBytes = 0;
        while (sentBytes < length)
        {
            ssize_t sentNow = send(sock_, data + sentBytes, length - sentBytes, 0);
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

    std::string toFIX(const Order &order)
    {
        // Simple FIX 4.2 NewOrderSingle (D)
        // 8=FIX.4.2|9=000|35=D|11=ID|54=SIDE|44=PRICE|38=QTY|40=TYPE|10=000|
        char buffer[256];
        int len = std::snprintf(buffer, sizeof(buffer),
                                "8=FIX.4.2\x01"
                                "35=D\x01"
                                "11=%llu\x01"
                                "54=%d\x01"
                                "44=%llu\x01"
                                "38=%llu\x01"
                                "40=%d\x01"
                                "60=%llu\x01",
                                (unsigned long long)order.id, (order.side == Side::Buy ? 1 : 2),
                                (unsigned long long)order.price, (unsigned long long)order.quantity,
                                (order.type == OrderType::Market ? 1 : 2),
                                (unsigned long long)getCurrentTimeNs()); // order.sendTimestamp is set here

        // Add checksum (simplified for benchmark)
        int totalLen = std::snprintf(buffer + len, sizeof(buffer) - len, "10=000\x01");
        return std::string(buffer, len + totalLen);
    }

    std::string host_;
    int port_;
    int sock_;
};

} // namespace hft
