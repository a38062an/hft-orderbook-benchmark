#include <gtest/gtest.h>

#include <string>

#include "fix/fix_parser.hpp"

namespace hft
{
namespace
{

std::string makeNewOrderFix()
{
    return "8=FIX.4.2\x01"
           "9=67\x01"
           "35=D\x01"
           "11=123\x01"
           "54=1\x01"
           "38=50\x01"
           "44=130\x01"
           "40=2\x01"
           "60=123456789\x01"
           "10=000\x01";
}

TEST(FixParserTest, ParsesValidNewOrderSingle)
{
    std::string msg = makeNewOrderFix();
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
    EXPECT_EQ(order->id, 123u);
    EXPECT_EQ(order->side, Side::Buy);
    EXPECT_EQ(order->quantity, 50u);
    EXPECT_EQ(order->price, 130u);
    EXPECT_EQ(order->type, OrderType::Limit);
    EXPECT_EQ(order->sendTimestamp, 123456789u);
}

TEST(FixParserTest, IncompleteMessageConsumesNothing)
{
    std::string partial = "8=FIX.4.2\x01"
                          "35=D\x01"
                          "11=1\x01";
    size_t consumed = 999;

    auto order = FIXParser::parse(std::span<const char>(partial.data(), partial.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, 0u);
}

TEST(FixParserTest, ChecksumWithoutTrailingSohIsIncomplete)
{
    std::string msg = "8=FIX.4.2\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "10=000";
    size_t consumed = 123;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, 0u);
}

TEST(FixParserTest, StatsRequestReturnsNullOrder)
{
    std::string msg = "8=FIX.4.2\x01"
                      "35=U1\x01"
                      "596=100\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, NonOrderMessageReturnsNullOrder)
{
    std::string msg = "8=FIX.4.2\x01"
                      "35=A\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, ParseSellMarketOrder)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=456\x01"
                      "54=2\x01"
                      "38=99\x01"
                      "44=140\x01"
                      "40=1\x01"
                      "60=777\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->id, 456u);
    EXPECT_EQ(order->side, Side::Sell);
    EXPECT_EQ(order->type, OrderType::Market);
    EXPECT_EQ(order->sendTimestamp, 777u);
}

TEST(FixParserTest, MissingRequiredFieldsAreRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "35=D\x01"
                      "54=1\x01"
                      "40=2\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    ASSERT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, InvalidSideValueIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=9\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, InvalidNumericFieldIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=ABC\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, MalformedChecksumDigitsAreRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=0A0\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, MalformedChecksumFirstDigitIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=A00\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, MalformedChecksumThirdDigitIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=00A\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, MalformedChecksumTerminatorIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000X";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, MissingQuantityIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, InvalidQuantityIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=10x\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, QuantityWithoutAnyDigitsIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=x\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, ZeroQuantityIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=0\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, InvalidPriceIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130x\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, PriceWithoutAnyDigitsIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=x\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, ZeroLimitPriceIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=0\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, InvalidOrderTypeIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=9\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, LimitOrderWithoutPriceIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, InvalidTimestampIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=1x\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, TimestampWithoutAnyDigitsIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=123\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=x\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, PartialOrderIdIsRejected)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=12x\x01"
                      "54=1\x01"
                      "38=50\x01"
                      "44=130\x01"
                      "40=2\x01"
                      "60=123456789\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    EXPECT_FALSE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
}

TEST(FixParserTest, MarketOrderWithoutTransTimeStillParses)
{
    std::string msg = "8=FIX.4.2\x01"
                      "9=67\x01"
                      "35=D\x01"
                      "11=999\x01"
                      "54=1\x01"
                      "38=10\x01"
                      "40=1\x01"
                      "10=000\x01";
    size_t consumed = 0;

    auto order = FIXParser::parse(std::span<const char>(msg.data(), msg.size()), consumed);

    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(consumed, msg.size());
    EXPECT_EQ(order->id, 999u);
    EXPECT_EQ(order->sendTimestamp, 0u);
    EXPECT_EQ(order->type, OrderType::Market);
}

TEST(FixParserTest, GetTagValueSupportsBeginningOfMessage)
{
    std::string_view message = "35=D\x01"
                               "54=1\x01";
    auto value = FIXParser::getTagValue(message, 35);
    EXPECT_EQ(value, "D");
}

TEST(FixParserTest, GetTagValueReturnsEmptyWhenMissing)
{
    std::string_view message = "8=FIX.4.2\x01"
                               "35=D\x01";
    auto value = FIXParser::getTagValue(message, 999);
    EXPECT_TRUE(value.empty());
}

TEST(FixParserTest, GetTagValueReturnsEmptyWhenTagNotTerminated)
{
    std::string_view message = "8=FIX.4.2\x01"
                               "35=D";
    auto value = FIXParser::getTagValue(message, 35);
    EXPECT_TRUE(value.empty());
}

TEST(FixParserTest, GetTagValueAtStartWithoutSohReturnsEmpty)
{
    std::string_view message = "35=D";
    auto value = FIXParser::getTagValue(message, 35);
    EXPECT_TRUE(value.empty());
}

TEST(FixParserTest, GetMessageTypeReadsTag35)
{
    std::string msg = "8=FIX.4.2\x01"
                      "35=U1\x01"
                      "10=000\x01";
    auto type = FIXParser::getMessageType(std::span<const char>(msg.data(), msg.size()));
    EXPECT_EQ(type, "U1");
}

} // namespace
} // namespace hft
