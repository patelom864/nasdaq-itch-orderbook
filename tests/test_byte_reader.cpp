#include "itch/byte_reader.hpp"

#include <gtest/gtest.h>

#include "fixtures.hpp"

namespace {

using testing_support::RecordBuilder;

TEST(ByteReader, ReadsUnsignedFieldsBigEndian) {
    RecordBuilder builder;
    builder.u8(0x7F).u16(0x0102).u32(0x01020304).u64(0x0102030405060708ULL);
    const auto bytes = builder.bytes();
    const itch::ByteReader reader{bytes};

    EXPECT_EQ(reader.read_u8(0), 0x7Fu);
    EXPECT_EQ(reader.read_u16(1), 0x0102u);
    EXPECT_EQ(reader.read_u32(3), 0x01020304u);
    EXPECT_EQ(reader.read_u64(7), 0x0102030405060708ULL);
}

TEST(ByteReader, ReadsBoundaryValues) {
    RecordBuilder builder;
    builder.u16(0).u16(0xFFFF).u32(0).u32(0xFFFFFFFFu).u64(0).u64(~0ULL);
    const auto bytes = builder.bytes();
    const itch::ByteReader reader{bytes};

    EXPECT_EQ(reader.read_u16(0), 0u);
    EXPECT_EQ(reader.read_u16(2), 0xFFFFu);
    EXPECT_EQ(reader.read_u32(4), 0u);
    EXPECT_EQ(reader.read_u32(8), 0xFFFFFFFFu);
    EXPECT_EQ(reader.read_u64(12), 0ULL);
    EXPECT_EQ(reader.read_u64(20), ~0ULL);
}

TEST(ByteReader, ReadsSixByteTimestamps) {
    // 09:30:00.000000000 Eastern, expressed as nanoseconds since midnight.
    constexpr std::uint64_t market_open_ns = 34200ULL * 1000000000ULL;

    RecordBuilder builder;
    builder.u48(0).u48(market_open_ns).u48(0xFFFFFFFFFFFFULL);
    const auto bytes = builder.bytes();
    const itch::ByteReader reader{bytes};

    EXPECT_EQ(reader.read_u48(0), 0ULL);
    EXPECT_EQ(reader.read_u48(6), market_open_ns);
    EXPECT_EQ(reader.read_u48(12), 0xFFFFFFFFFFFFULL);
}

TEST(ByteReader, ReadsFixedWidthAlphaFieldsIncludingPadding) {
    RecordBuilder builder;
    builder.alpha("AAPL", 8).alpha("ZVZZT", 8).alpha("", 8).alpha("NSDQ", 4);
    const auto bytes = builder.bytes();
    const itch::ByteReader reader{bytes};

    const auto apple = reader.read_alpha<8>(0);
    EXPECT_EQ(std::string_view(apple.data(), 8), "AAPL    ");

    const auto test_symbol = reader.read_alpha<8>(8);
    EXPECT_EQ(std::string_view(test_symbol.data(), 8), "ZVZZT   ");

    const auto blank = reader.read_alpha<8>(16);
    EXPECT_EQ(std::string_view(blank.data(), 8), "        ");

    const auto mpid = reader.read_alpha<4>(24);
    EXPECT_EQ(std::string_view(mpid.data(), 4), "NSDQ");
}

TEST(ByteReader, ReportsRecordSize) {
    RecordBuilder builder;
    builder.u32(0);
    const auto bytes = builder.bytes();
    EXPECT_EQ(itch::ByteReader{bytes}.size(), 4u);
}

}  // namespace