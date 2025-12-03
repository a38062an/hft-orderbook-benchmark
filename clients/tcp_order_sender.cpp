#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

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
    int bodyLength = std::snprintf(messageBody, sizeof(messageBody), "35=D\x01""11=%llu\x01""54=%d\x01""38=%d\x01""44=%d\x01""40=2\x01", 
        id, side, quantity, price);
    
    char messageHeader[256];
    // Construct the header, including the body length
    int headerLength = std::snprintf(messageHeader, sizeof(messageHeader), "8=FIX.4.2\x01""9=%d\x01", bodyLength);
    
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

int main(int argc, char* argv[])
{
    int serverPort = 12345;
    std::string serverHost = "127.0.0.1";
    int orderCount = 1000000; // Default to 1 million orders

    // Parse command line argument for number of orders
    if (argc > 1)
    {
        orderCount = std::atoi(argv[1]);
    }

    std::cout << "Preparing " << orderCount << " orders in memory..." << std::endl;

    // Pre-generate orders to measure PURE network throughput
    // This avoids measuring order generation time during the send phase
    std::vector<std::string> orders;
    orders.reserve(orderCount);
    
    std::mt19937 randomGenerator(42);
    std::uniform_int_distribution<int> priceDistribution(90, 110);
    std::uniform_int_distribution<int> quantityDistribution(1, 100);
    std::uniform_int_distribution<int> sideDistribution(1, 2);

    for (int i = 0; i < orderCount; ++i)
    {
        orders.push_back(create_fix_message(i, priceDistribution(randomGenerator), quantityDistribution(randomGenerator), sideDistribution(randomGenerator)));
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
    // to network byte order (Big-Endian). This ensures the server reads the correct port number regardless of its CPU architecture.
    serverAddress.sin_port = htons(serverPort);
    
    // inet_pton (Presentation TO Network): Converts the IP address from a human-readable string (e.g., "127.0.0.1")
    // to its binary network representation.
    inet_pton(AF_INET, serverHost.c_str(), &serverAddress.sin_addr);

    // Connect to server
    // Initiates the TCP 3-way handshake (SYN, SYN-ACK, ACK) to establish a connection with the server.
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("connect");
        return 1;
    }

    std::cout << "Sending..." << std::endl;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Send all orders
    for (const auto& fixMessage : orders)
    {
        // send: Transmits the data over the connected socket.
        // Returns the number of bytes actually sent. In a robust app, we should check if all bytes were sent.
        send(clientSocket, fixMessage.data(), fixMessage.size(), 0);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = endTime - startTime;
    
    double throughput = orderCount / duration.count();
    
    std::cout << "Sent " << orderCount << " orders in " << duration.count() << "s" << std::endl;
    std::cout << "Throughput: " << throughput << " orders/s" << std::endl;

    close(clientSocket);
    return 0;
}
