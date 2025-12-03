#include "FIXParser.hpp"
#include <charconv>
#include <cstring>

namespace hft
{

    constexpr char SOH = '\x01';

    std::optional<Order> FIXParser::parse(std::span<const char> buffer, size_t &bytesConsumed)
    {
        std::string_view bufferView(buffer.data(), buffer.size());

        // Find end of message (checksum tag 10=...)
        // A full FIX message ends with 10=XXX<SOH>
        // We look for "10=" and then the next SOH

        size_t checksumPosition = bufferView.find("\x01"
                                                  "10=");
        if (checksumPosition == std::string_view::npos)
        {
            bytesConsumed = 0;
            return std::nullopt; // Incomplete
        }

        // Find the SOH after 10=XXX
        size_t endOfMessagePosition = bufferView.find(SOH, checksumPosition + 1);
        if (endOfMessagePosition == std::string_view::npos)
        {
            bytesConsumed = 0;
            return std::nullopt; // Incomplete
        }

        bytesConsumed = endOfMessagePosition + 1;
        std::string_view message = bufferView.substr(0, bytesConsumed);

        // Basic validation: MsgType(35) = D (NewOrderSingle)
        auto msgType = getTagValue(message, 35);
        if (msgType != "D")
        {
            return std::nullopt; // Ignore non-order messages
        }

        Order order{};

        // ClOrdID (11) -> OrderId
        auto clientOrderId = getTagValue(message, 11);
        std::from_chars(clientOrderId.data(), clientOrderId.data() + clientOrderId.size(), order.id);

        // Side (54): 1=Buy, 2=Sell
        auto side = getTagValue(message, 54);
        order.side = (side == "1") ? Side::Buy : Side::Sell;

        // Price (44)
        auto price = getTagValue(message, 44);
        // Simplified: assume integer prices for benchmark
        std::from_chars(price.data(), price.data() + price.size(), order.price);

        // OrderQty (38)
        auto quantityString = getTagValue(message, 38);
        std::from_chars(quantityString.data(), quantityString.data() + quantityString.size(), order.quantity);

        // OrdType (40): 1=Market, 2=Limit
        auto type = getTagValue(message, 40);
        order.type = (type == "1") ? OrderType::Market : OrderType::Limit;

        return order;
    }

    std::string_view FIXParser::getTagValue(std::string_view message, int tag)
    {
        // Search for "\x01TAG="
        // Note: The first tag (8=) doesn't start with SOH, but we usually skip header parsing for speed in this benchmark
        // Or we can handle it.

        char tagString[16];
        int tagLength = std::sprintf(tagString, "\x01%d=", tag);
        std::string_view tagSearchPattern(tagString, tagLength);

        size_t tagPosition = message.find(tagSearchPattern);
        if (tagPosition == std::string_view::npos)
        {
            // Special case for first tag if needed, but usually we look for tags inside body
            return {};
        }

        size_t valueStart = tagPosition + tagLength;
        size_t valueEnd = message.find(SOH, valueStart);
        if (valueEnd == std::string_view::npos)
        {
            return {};
        }

        return message.substr(valueStart, valueEnd - valueStart);
    }

} // namespace hft
