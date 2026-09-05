#include "itch/messages.hpp"

#include <gtest/gtest.h>

#include "fixtures.hpp"

namespace {

using testing_support::RecordBuilder;
using testing_support::header;

// Every field gets a distinct value so that two transposed offsets cannot both
// happen to hold the same bytes and slip past the assertions.
constexpr std::uint16_t kStockLocate = 0x1234;
constexpr std::uint16_t kTrackingNumber = 0x5678;
constexpr std::uint64_t kTimestampNs = 34200ULL * 1000000000ULL + 123456789ULL;

TEST(Messages, DecodesSystemEvent) {
    auto builder = header('S', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.ch('Q');
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('S'));

    const auto message = itch::decode_system_event(itch::ByteReader{bytes});
    EXPECT_EQ(message.header.type, 'S');
    EXPECT_EQ(message.header.stock_locate, kStockLocate);
    EXPECT_EQ(message.header.tracking_number, kTrackingNumber);
    EXPECT_EQ(message.header.timestamp_ns, kTimestampNs);
    EXPECT_EQ(message.event_code, 'Q');
}

TEST(Messages, DecodesAddOrder) {
    auto builder = header('A', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.u64(0x1122334455667788ULL).ch('B').u32(500).alpha("AAPL", 8).u32(1650000);
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('A'));

    const auto message = itch::decode_add_order(itch::ByteReader{bytes});
    EXPECT_EQ(message.header.timestamp_ns, kTimestampNs);
    EXPECT_EQ(message.order_reference, 0x1122334455667788ULL);
    EXPECT_EQ(message.side, itch::Side::Buy);
    EXPECT_EQ(message.shares, 500u);
    EXPECT_EQ(itch::trim(message.stock), "AAPL");
    EXPECT_EQ(message.price, 1650000u);  // $165.0000
}

TEST(Messages, DecodesAddOrderAtDomainBoundaries) {
    auto builder = header('A', 0, 0, 0);
    builder.u64(~0ULL).ch('S').u32(0).alpha("", 8).u32(0xFFFFFFFFu);
    const auto bytes = builder.bytes();

    const auto message = itch::decode_add_order(itch::ByteReader{bytes});
    EXPECT_EQ(message.order_reference, ~0ULL);
    EXPECT_EQ(message.side, itch::Side::Sell);
    EXPECT_EQ(message.shares, 0u);
    EXPECT_TRUE(itch::trim(message.stock).empty());
    EXPECT_EQ(message.price, 0xFFFFFFFFu);
}

TEST(Messages, DecodesAddOrderWithMpid) {
    auto builder = header('F', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.u64(42).ch('S').u32(100).alpha("MSFT", 8).u32(4200000).alpha("NSDQ", 4);
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('F'));

    const auto message = itch::decode_add_order_with_mpid(itch::ByteReader{bytes});
    EXPECT_EQ(message.order.order_reference, 42u);
    EXPECT_EQ(message.order.side, itch::Side::Sell);
    EXPECT_EQ(message.order.shares, 100u);
    EXPECT_EQ(itch::trim(message.order.stock), "MSFT");
    EXPECT_EQ(message.order.price, 4200000u);
    EXPECT_EQ(itch::trim(message.attribution), "NSDQ");
}

TEST(Messages, DecodesOrderExecuted) {
    auto builder = header('E', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.u64(0x0A0B0C0D0E0F1011ULL).u32(250).u64(0x00FF00FF00FF00FFULL);
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('E'));

    const auto message = itch::decode_order_executed(itch::ByteReader{bytes});
    EXPECT_EQ(message.order_reference, 0x0A0B0C0D0E0F1011ULL);
    EXPECT_EQ(message.executed_shares, 250u);
    EXPECT_EQ(message.match_number, 0x00FF00FF00FF00FFULL);
}

TEST(Messages, DecodesOrderExecutedWithPrice) {
    auto builder = header('C', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.u64(7).u32(300).u64(99).ch('Y').u32(1649900);
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('C'));

    const auto message = itch::decode_order_executed_with_price(itch::ByteReader{bytes});
    EXPECT_EQ(message.executed.order_reference, 7u);
    EXPECT_EQ(message.executed.executed_shares, 300u);
    EXPECT_EQ(message.executed.match_number, 99u);
    EXPECT_EQ(message.printable, 'Y');
    EXPECT_EQ(message.execution_price, 1649900u);
}

TEST(Messages, DecodesOrderCancel) {
    auto builder = header('X', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.u64(0xDEADBEEFCAFEF00DULL).u32(75);
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('X'));

    const auto message = itch::decode_order_cancel(itch::ByteReader{bytes});
    EXPECT_EQ(message.order_reference, 0xDEADBEEFCAFEF00DULL);
    EXPECT_EQ(message.cancelled_shares, 75u);
}

TEST(Messages, DecodesOrderDelete) {
    auto builder = header('D', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.u64(0x0102030405060708ULL);
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('D'));

    const auto message = itch::decode_order_delete(itch::ByteReader{bytes});
    EXPECT_EQ(message.order_reference, 0x0102030405060708ULL);
}

TEST(Messages, DecodesOrderReplace) {
    auto builder = header('U', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.u64(1111).u64(2222).u32(400).u32(1655000);
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('U'));

    const auto message = itch::decode_order_replace(itch::ByteReader{bytes});
    EXPECT_EQ(message.original_order_reference, 1111u);
    EXPECT_EQ(message.new_order_reference, 2222u);
    EXPECT_EQ(message.shares, 400u);
    EXPECT_EQ(message.price, 1655000u);
}

TEST(Messages, DecodesTradeNonCross) {
    auto builder = header('P', kStockLocate, kTrackingNumber, kTimestampNs);
    builder.u64(31337).ch('B').u32(600).alpha("NVDA", 8).u32(1234500).u64(0xABCDEFULL);
    const auto bytes = builder.bytes();
    ASSERT_EQ(bytes.size(), itch::spec_message_length('P'));

    const auto message = itch::decode_trade_non_cross(itch::ByteReader{bytes});
    EXPECT_EQ(message.order_reference, 31337u);
    EXPECT_EQ(message.side, itch::Side::Buy);
    EXPECT_EQ(message.shares, 600u);
    EXPECT_EQ(itch::trim(message.stock), "NVDA");
    EXPECT_EQ(message.price, 1234500u);
    EXPECT_EQ(message.match_number, 0xABCDEFULL);
}

TEST(Messages, TrimHandlesFullAndEmptyFields) {
    const itch::Stock full{{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'}};
    EXPECT_EQ(itch::trim(full), "ABCDEFGH");

    const itch::Stock blank{{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}};
    EXPECT_TRUE(itch::trim(blank).empty());
}

}  // namespace