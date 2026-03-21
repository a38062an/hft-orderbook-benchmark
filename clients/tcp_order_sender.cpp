#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// Minimal FIX Message Construction
// 8=FIX.4.2|9=LEN|35=D|11=ID|54=Side|38=Qty|44=Price|40=2|10=CS|

/**
 * Creates a FIX 4.2 New Order Single message.
 *
 * @param id Unique order ID
 * @param price Order price
 * @param quantity Order quantity
 * @param side Order side (1=Buy, 2=Sell)
 * @return Formatted FIX message string
 */
std::string create_fix_message(uint64_t id, int price, int quantity, int side)
{
    char messageBody[256];
    // Construct the body of the FIX message
    int bodyLength = std::snprintf(messageBody, sizeof(messageBody),
                                   "35=D\x01"
                                   "11=%llu\x01"
                                   "54=%d\x01"
                                   "38=%d\x01"
                                   "44=%d\x01"
                                   "40=2\x01",
                                   id, side, quantity, price);

    char messageHeader[256];
    // Construct the header, including the body length
    int headerLength = std::snprintf(messageHeader, sizeof(messageHeader),
                                     "8=FIX.4.2\x01"
                                     "9=%d\x01",
                                     bodyLength);

    std::string fixMessage;
    fixMessage.reserve(headerLength + bodyLength + 8); // + 8 for the checksum
    fixMessage.append(messageHeader, headerLength);
    fixMessage.append(messageBody, bodyLength);

    // Calculate Checksum
    unsigned int checksum = 0;
    for (char c : fixMessage)
    {
        checksum += (unsigned char)c;
    }

    char messageTrailer[16];
    // Append the checksum trailer
    std::snprintf(messageTrailer, sizeof(messageTrailer), "10=%03d\x01", checksum % 256);
    fixMessage.append(messageTrailer);

    return fixMessage;
}

int main(int argc, char *argv[])
{
    // Default configuration
    int serverPort = 12345;
    std::string serverHost = "127.0.0.1";
    int orderCount = 1000000;
    int minPrice = 90;
    int maxPrice = 110;
    int tickSize = 1;
    int minQty = 1;
    int maxQty = 100;
    uint32_t seed = 42;

    // Parse command line arguments
    // Usage: ./tcp_order_sender [options]
    // Options:
    //   --count <n>        Number of orders (default: 1000000)
    //   --host <ip>        Server host (default: 127.0.0.1)
    //   --port <n>         Server port (default: 12345)
    //   --min-price <n>    Minimum price (default: 90)
    //   --max-price <n>    Maximum price (default: 110)
    //   --tick <n>         Tick size (default: 1)
    //   --min-qty <n>      Minimum quantity (default: 1)
    //   --max-qty <n>      Maximum quantity (default: 100)
    //   --seed <n>         Random seed (default: 42)
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--count" && i + 1 < argc)
        {
            orderCount = std::atoi(argv[++i]);
        }
        else if (arg == "--host" && i + 1 < argc)
        {
            serverHost = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            serverPort = std::atoi(argv[++i]);
        }
        else if (arg == "--min-price" && i + 1 < argc)
        {
            minPrice = std::atoi(argv[++i]);
        }
        else if (arg == "--max-price" && i + 1 < argc)
        {
            maxPrice = std::atoi(argv[++i]);
        }
        else if (arg == "--tick" && i + 1 < argc)
        {
            tickSize = std::atoi(argv[++i]);
        }
        else if (arg == "--min-qty" && i + 1 < argc)
        {
            minQty = std::atoi(argv[++i]);
        }
        else if (arg == "--max-qty" && i + 1 < argc)
        {
            maxQty = std::atoi(argv[++i]);
        }
        else if (arg == "--seed" && i + 1 < argc)
        {
            seed = std::atoi(argv[++i]);
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --count <n>        Number of orders (default: 1000000)\n"
                      << "  --host <ip>        Server host (default: 127.0.0.1)\n"
                      << "  --port <n>         Server port (default: 12345)\n"
                      << "  --min-price <n>    Minimum price (default: 90)\n"
                      << "  --max-price <n>    Maximum price (default: 110)\n"
                      << "  --tick <n>         Tick size (default: 1)\n"
                      << "  --min-qty <n>      Minimum quantity (default: 1)\n"
                      << "  --max-qty <n>      Maximum quantity (default: 100)\n"
                      << "  --seed <n>         Random seed for reproducibility (default: 42)\n"
                      << "  --help, -h         Show this help message\n";
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::cerr << "Use --help for usage information\n";
            return 1;
        }
    }

    // Validate configuration
    if (minPrice >= maxPrice)
    {
        std::cerr << "Error: min-price must be less than max-price\n";
        return 1;
    }
    if (tickSize <= 0)
    {
        std::cerr << "Error: tick must be greater than 0\n";
        return 1;
    }
    if ((maxPrice - minPrice) % tickSize != 0)
    {
        std::cerr << "Warning: price range (" << maxPrice - minPrice << ") is not evenly divisible by tick size ("
                  << tickSize << ")\n";
    }

    std::cout << "Configuration:\n"
              << "  Orders: " << orderCount << "\n"
              << "  Server: " << serverHost << ":" << serverPort << "\n"
              << "  Price range: [" << minPrice << ", " << maxPrice << "] with tick " << tickSize << "\n"
              << "  Quantity range: [" << minQty << ", " << maxQty << "]\n"
              << "  Random seed: " << seed << "\n";

    std::cout << "Preparing " << orderCount << " orders in memory..." << std::endl;

    // Pre-generate orders to measure PURE network throughput
    // This avoids measuring order generation time during the send phase
    std::vector<std::string> orders;
    orders.reserve(orderCount);

    std::mt19937 randomGenerator(seed);

    // Generate tick-aligned prices only
    std::vector<int> validPrices;
    for (int price = minPrice; price <= maxPrice; price += tickSize)
    {
        validPrices.push_back(price);
    }

    std::uniform_int_distribution<size_t> priceIndexDistribution(0, validPrices.size() - 1);
    std::uniform_int_distribution<int> quantityDistribution(minQty, maxQty);
    std::uniform_int_distribution<int> sideDistribution(1, 2);

    for (int i = 0; i < orderCount; ++i)
    {
        int price = validPrices[priceIndexDistribution(randomGenerator)];
        orders.push_back(
            create_fix_message(i, price, quantityDistribution(randomGenerator), sideDistribution(randomGenerator)));
    }

    std::cout << "Connecting to " << serverHost << ":" << serverPort << "..." << std::endl;

    // Create socket
    // AF_INET: Address Family for IPv4 (Internet Protocol v4)
    // SOCK_STREAM: Socket Type for TCP (Transmission Control Protocol) - reliable, connection-oriented
    // 0: Protocol (0 lets OS choose the default protocol for the family and type, which is TCP)
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;

    // htons (Host TO Network Short): Converts the port number from host byte order (usually Little-Endian on x86/ARM)
    // to network byte order (Big-Endian). This ensures the server reads the correct port number regardless of its CPU
    // architecture.
    serverAddress.sin_port = htons(serverPort);

    // inet_pton (Presentation TO Network): Converts the IP address from a human-readable string (e.g., "127.0.0.1")
    // to its binary network representation.
    if (inet_pton(AF_INET, serverHost.c_str(), &serverAddress.sin_addr) != 1)
    {
        std::cerr << "Invalid IPv4 host: " << serverHost << "\n";
        close(clientSocket);
        return 1;
    }

    // Connect to server
    // Initiates the TCP 3-way handshake (SYN, SYN-ACK, ACK) to establish a connection with the server.
    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("connect");
        close(clientSocket);
        return 1;
    }

    std::cout << "Sending..." << std::endl;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Send all orders
    for (const auto &fixMessage : orders)
    {
        size_t bytesSent = 0;
        while (bytesSent < fixMessage.size())
        {
            // send can write fewer bytes than requested; loop until the full FIX frame is sent.
            ssize_t sentNow = send(clientSocket, fixMessage.data() + bytesSent, fixMessage.size() - bytesSent, 0);
            if (sentNow < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                perror("send");
                close(clientSocket);
                return 1;
            }
            if (sentNow == 0)
            {
                std::cerr << "send returned 0 bytes" << std::endl;
                close(clientSocket);
                return 1;
            }
            bytesSent += static_cast<size_t>(sentNow);
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = endTime - startTime;
    double throughput = orderCount / duration.count();
    std::cout << "Sent " << orderCount << " orders in " << duration.count() << "s" << std::endl;
    std::cout << "Throughput: " << throughput << " orders/s" << std::endl;

    close(clientSocket);
    return 0;
}
