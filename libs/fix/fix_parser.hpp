#pragma once

#include "../../src/core/order.hpp"
#include <charconv>
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

namespace hft
{

class FIXParser
{
  public:
    static constexpr char SOH = '\x01';

    // Parses a single FIX message from the buffer.
    // Returns std::nullopt if incomplete or invalid.
    // Updates bytesConsumed to indicate how much data was processed.
    static inline std::optional<Order> parse(std::span<const char> buffer, size_t &bytesConsumed)
    {
        std::string_view bufferView(buffer.data(), buffer.size());

        // Find end of message (checksum tag 10=...)
        size_t checksumPosition = bufferView.find("\x01\x31\x30\x3d"); // \x0110=
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

        // Basic validation: MsgType(35)
        auto msgType = getTagValue(message, 35);
        if (msgType == "U1") // Custom Stats Request
        {
            return std::nullopt; // Gateway will handle this separately
        }
        
        if (msgType != "D") // NewOrderSingle
        {
            return std::nullopt; // Ignore non-order messages
        }

        Order order{};

        // ClOrdID (11) -> OrderId
        auto clientOrderId = getTagValue(message, 11);
        if (!clientOrderId.empty())
        {
            std::from_chars(clientOrderId.data(), clientOrderId.data() + clientOrderId.size(), order.id);
        }

        // Side (54): 1=Buy, 2=Sell
        auto side = getTagValue(message, 54);
        order.side = (side == "1") ? Side::Buy : Side::Sell;

        // Price (44)
        auto price = getTagValue(message, 44);
        if (!price.empty())
        {
            std::from_chars(price.data(), price.data() + price.size(), order.price);
        }

        // OrderQty (38)
        auto quantityString = getTagValue(message, 38);
        if (!quantityString.empty())
        {
            std::from_chars(quantityString.data(), quantityString.data() + quantityString.size(), order.quantity);
        }

        // OrdType (40): 1=Market, 2=Limit
        auto type = getTagValue(message, 40);
        order.type = (type == "1") ? OrderType::Market : OrderType::Limit;

        // TransactionTime (60) -> sendTimestamp
        auto transTime = getTagValue(message, 60);
        if (!transTime.empty())
        {
            std::from_chars(transTime.data(), transTime.data() + transTime.size(), order.sendTimestamp);
        }

        return order;
    }

    static inline std::string_view getMessageType(std::span<const char> buffer)
    {
        std::string_view message(buffer.data(), buffer.size());
        return getTagValue(message, 35);
    }

    static inline std::string_view getTagValue(std::string_view message, int tag)
    {
        char tagString[16];
        int tagLength = std::snprintf(tagString, sizeof(tagString), "\x01%d=", tag);
        std::string_view tagSearchPattern(tagString, tagLength);

        size_t tagPosition = message.find(tagSearchPattern);
        if (tagPosition == std::string_view::npos)
        {
            // Case for tag at the very beginning of the message (no SOH prefix)
            char startTagString[16];
            int startTagLength = std::snprintf(startTagString, sizeof(startTagString), "%d=", tag);
            std::string_view startTagPattern(startTagString, startTagLength);

            if (message.starts_with(startTagPattern))
            {
                size_t valueStart = startTagLength;
                size_t valueEnd = message.find(SOH, valueStart);
                if (valueEnd != std::string_view::npos)
                {
                    return message.substr(valueStart, valueEnd - valueStart);
                }
            }
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

  private:
};

} // namespace hft
