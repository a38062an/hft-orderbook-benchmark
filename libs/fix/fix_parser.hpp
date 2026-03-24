#pragma once

#include "core/Order.hpp"
#include <cctype>
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

        // Require full checksum field body (10=XXX<SOH>) before consuming a message.
        // If the 3 checksum digits are not yet present, treat as incomplete.
        if (bufferView.size() < checksumPosition + 8)
        {
            bytesConsumed = 0;
            return std::nullopt;
        }

        if (!std::isdigit(static_cast<unsigned char>(bufferView[checksumPosition + 4])) ||
            !std::isdigit(static_cast<unsigned char>(bufferView[checksumPosition + 5])) ||
            !std::isdigit(static_cast<unsigned char>(bufferView[checksumPosition + 6])))
        {
            bytesConsumed = checksumPosition + 8;
            return std::nullopt; // Malformed checksum value in a complete frame
        }

        // Find the SOH after 10=XXX
        size_t endOfMessagePosition = checksumPosition + 7;
        if (bufferView[endOfMessagePosition] != SOH)
        {
            bytesConsumed = checksumPosition + 8;
            return std::nullopt; // Malformed checksum terminator
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
        if (clientOrderId.empty())
        {
            return std::nullopt;
        }
        auto idResult = std::from_chars(clientOrderId.data(), clientOrderId.data() + clientOrderId.size(), order.id);
        if (idResult.ec != std::errc{} || idResult.ptr != clientOrderId.data() + clientOrderId.size())
        {
            return std::nullopt;
        }

        // Side (54): 1=Buy, 2=Sell
        auto side = getTagValue(message, 54);
        if (side == "1")
        {
            order.side = Side::Buy;
        }
        else if (side == "2")
        {
            order.side = Side::Sell;
        }
        else
        {
            return std::nullopt;
        }

        // Price (44)
        auto price = getTagValue(message, 44);
        if (!price.empty())
        {
            auto priceResult = std::from_chars(price.data(), price.data() + price.size(), order.price);
            if (priceResult.ec != std::errc{} || priceResult.ptr != price.data() + price.size())
            {
                return std::nullopt;
            }
        }

        // OrderQty (38)
        auto quantityString = getTagValue(message, 38);
        if (quantityString.empty())
        {
            return std::nullopt;
        }
        auto quantityResult =
            std::from_chars(quantityString.data(), quantityString.data() + quantityString.size(), order.quantity);
        if (quantityResult.ec != std::errc{} || quantityResult.ptr != quantityString.data() + quantityString.size())
        {
            return std::nullopt;
        }
        if (order.quantity == 0)
        {
            return std::nullopt;
        }

        // OrdType (40): 1=Market, 2=Limit
        auto type = getTagValue(message, 40);
        if (type == "1")
        {
            order.type = OrderType::Market;
        }
        else if (type == "2")
        {
            order.type = OrderType::Limit;
        }
        else
        {
            return std::nullopt;
        }

        if (order.type == OrderType::Limit && price.empty())
        {
            return std::nullopt;
        }
        if (order.type == OrderType::Limit && order.price == 0)
        {
            return std::nullopt;
        }

        // TransactionTime (60) -> sendTimestamp
        auto transTime = getTagValue(message, 60);
        if (!transTime.empty())
        {
            auto tsResult = std::from_chars(transTime.data(), transTime.data() + transTime.size(), order.sendTimestamp);
            if (tsResult.ec != std::errc{} || tsResult.ptr != transTime.data() + transTime.size())
            {
                return std::nullopt;
            }
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
