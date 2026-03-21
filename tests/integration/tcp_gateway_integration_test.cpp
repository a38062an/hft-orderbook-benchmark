#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "core/matching_engine.hpp"
#include "core/order_book_factory.hpp"
#include "network/tcp_order_gateway.hpp"
#include "utils/lock_free_queue.hpp"

namespace hft
{
namespace
{

std::string makeFixNewOrder(OrderId id, Price price, Quantity qty, Side side)
{
    const char sideTag = (side == Side::Buy) ? '1' : '2';
    // Parser only requires MsgType plus standard tags and checksum field terminator.
    return "8=FIX.4.2\x01"
           "9=64\x01"
           "35=D\x01"
           "11=" +
           std::to_string(id) + "\x01" + "54=" + std::string(1, sideTag) + "\x01" + "38=" + std::to_string(qty) +
           "\x01" + "44=" + std::to_string(price) + "\x01" +
           "40=2\x01"
           "60=123456789\x01"
           "10=000\x01";
}

int connectClient(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        close(sock);
        return -1;
    }

    return sock;
}

bool waitUntil(std::function<bool()> predicate, std::chrono::milliseconds timeout)
{
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout)
    {
        if (predicate())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

TEST(TcpGatewayIntegrationTest, SingleClientOrderReachesMatchingEngine)
{
    const int port = static_cast<int>(22000 + (getpid() % 1000));

    LockFreeQueue<Order, 1024> queue;
    auto orderBook = OrderBookFactory::create("map");
    MatchingEngine engine(queue, *orderBook);
    TCPOrderGateway gateway(port, queue);

    std::atomic<bool> running{true};
    std::thread engineThread([&]() { engine.run(running); });

    gateway.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    const std::string message = makeFixNewOrder(1001, 130, 7, Side::Buy);
    int client = connectClient(port);
    ASSERT_GE(client, 0);
    ASSERT_EQ(send(client, message.data(), message.size(), 0), static_cast<ssize_t>(message.size()));
    close(client);

    ASSERT_TRUE(waitUntil([&]() { return engine.getMetrics().getOrderCount() >= 1; }, std::chrono::milliseconds(500)));

    running.store(false);
    gateway.stop();
    engineThread.join();

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_EQ(engine.getOrderBook().getOrderCount(), 1u);
    EXPECT_EQ(engine.getOrderBook().getBestBid(), 130u);
}

TEST(TcpGatewayIntegrationTest, PartialTcpFramesStillParseSingleOrder)
{
    const int port = static_cast<int>(23000 + (getpid() % 1000));

    LockFreeQueue<Order, 1024> queue;
    auto orderBook = OrderBookFactory::create("map");
    MatchingEngine engine(queue, *orderBook);
    TCPOrderGateway gateway(port, queue);

    std::atomic<bool> running{true};
    std::thread engineThread([&]() { engine.run(running); });

    gateway.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    const std::string message = makeFixNewOrder(2001, 131, 8, Side::Buy);
    const size_t split = message.size() / 2;

    int client = connectClient(port);
    ASSERT_GE(client, 0);

    ASSERT_EQ(send(client, message.data(), split, 0), static_cast<ssize_t>(split));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(send(client, message.data() + split, message.size() - split, 0),
              static_cast<ssize_t>(message.size() - split));
    close(client);

    ASSERT_TRUE(waitUntil([&]() { return engine.getMetrics().getOrderCount() >= 1; }, std::chrono::milliseconds(500)));

    running.store(false);
    gateway.stop();
    engineThread.join();

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_EQ(engine.getOrderBook().getOrderCount(), 1u);
    EXPECT_EQ(engine.getOrderBook().getBestBid(), 131u);
}

TEST(TcpGatewayIntegrationTest, SplitChecksumAcrossPacketsStillParsesOrder)
{
    const int port = static_cast<int>(24000 + (getpid() % 1000));

    LockFreeQueue<Order, 1024> queue;
    auto orderBook = OrderBookFactory::create("map");
    MatchingEngine engine(queue, *orderBook);
    TCPOrderGateway gateway(port, queue);

    std::atomic<bool> running{true};
    std::thread engineThread([&]() { engine.run(running); });

    gateway.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    const std::string message = makeFixNewOrder(3001, 132, 9, Side::Buy);

    const size_t checksumStart = message.find("10=");
    ASSERT_NE(checksumStart, std::string::npos);
    ASSERT_GT(checksumStart, 0u);

    // Send all fields up to (but not including) checksum digits to force a checksum split.
    const size_t split = checksumStart + 3;

    int client = connectClient(port);
    ASSERT_GE(client, 0);

    ASSERT_EQ(send(client, message.data(), split, 0), static_cast<ssize_t>(split));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_EQ(send(client, message.data() + split, message.size() - split, 0),
              static_cast<ssize_t>(message.size() - split));
    close(client);

    ASSERT_TRUE(waitUntil([&]() { return engine.getMetrics().getOrderCount() >= 1; }, std::chrono::milliseconds(500)));

    running.store(false);
    gateway.stop();
    engineThread.join();

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_EQ(engine.getOrderBook().getOrderCount(), 1u);
    EXPECT_EQ(engine.getOrderBook().getBestBid(), 132u);
}

TEST(TcpGatewayIntegrationTest, OversizedMalformedStreamDoesNotBreakGateway)
{
    const int port = static_cast<int>(25000 + (getpid() % 1000));

    LockFreeQueue<Order, 1024> queue;
    auto orderBook = OrderBookFactory::create("map");
    MatchingEngine engine(queue, *orderBook);
    TCPOrderGateway gateway(port, queue);

    std::atomic<bool> running{true};
    std::thread engineThread([&]() { engine.run(running); });

    gateway.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // Build a payload larger than gateway max framing buffer (1MB) with no valid FIX terminator.
    const std::vector<char> malformed(1100000, 'X');
    int badClient = connectClient(port);
    ASSERT_GE(badClient, 0);
    ASSERT_GT(send(badClient, malformed.data(), malformed.size(), 0), 0);
    close(badClient);

    // Gateway should keep serving and accept a valid order from a new client.
    const std::string valid = makeFixNewOrder(4001, 133, 10, Side::Buy);
    int goodClient = connectClient(port);
    ASSERT_GE(goodClient, 0);
    ASSERT_EQ(send(goodClient, valid.data(), valid.size(), 0), static_cast<ssize_t>(valid.size()));
    close(goodClient);

    ASSERT_TRUE(waitUntil([&]() { return engine.getMetrics().getOrderCount() >= 1; }, std::chrono::milliseconds(1000)));

    running.store(false);
    gateway.stop();
    engineThread.join();

    EXPECT_EQ(engine.getMetrics().getOrderCount(), 1u);
    EXPECT_EQ(engine.getOrderBook().getOrderCount(), 1u);
    EXPECT_EQ(engine.getOrderBook().getBestBid(), 133u);
}

} // namespace
} // namespace hft
