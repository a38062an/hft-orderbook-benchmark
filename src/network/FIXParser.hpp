#pragma once

#include "core/Order.hpp"
#include <string_view>
#include <optional>
#include <span>

namespace hft 
{

class FIXParser 
{
public:
    // Parses a single FIX message from the buffer.
    // Returns std::nullopt if incomplete or invalid.
    // Updates bytesConsumed to indicate how much data was processed.
    static std::optional<Order> parse(std::span<const char> buffer, size_t& bytesConsumed);

private:
    static std::string_view getTagValue(std::string_view message, int tag);
};

} // namespace hft
