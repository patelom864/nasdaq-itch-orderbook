#include "itch/framer.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "fixtures.hpp"

namespace {

using testing_support::header;

struct Capture {
    std::vector<char> types;
    std::vector<std::size_t> sizes;

    void operator()(char type, std::span<const std::byte> record) {
        types.push_back(type);
        sizes.push_back(record.size());
    }
};

std::vector<std::byte> add_order(std::uint64_t order_reference) {
    auto builder = header('A', 1, 0, 1000);
    builder.u64(order_reference).ch('B').u32(100).alpha("AAPL", 8).u32(1650000);
    return builder.framed();
}

std::vector<std::byte> order_delete(std::uint64_t order_reference) {
    auto builder = header('D', 1, 0, 2000);
    builder.u64(order_reference);
    return builder.framed();
}

void append(std::vector<std::byte>& into, const std::vector<std::byte>& tail) {
    into.insert(into.end(), tail.begin(), tail.end());
}

TEST(Framer, EmptyBufferIsCompleteWithNoMessages) {
    Capture capture;
    const auto outcome = itch::frame_all({}, capture);

    EXPECT_TRUE(outcome.complete());
    EXPECT_EQ(outcome.messages, 0u);
    EXPECT_EQ(outcome.bytes_consumed, 0u);
}

TEST(Framer, FramesASingleRecord) {
    const auto file = add_order(1);
    Capture capture;
    const auto outcome = itch::frame_all(file, capture);

    EXPECT_TRUE(outcome.complete());
    EXPECT_EQ(outcome.messages, 1u);
    EXPECT_EQ(outcome.bytes_consumed, file.size());
    ASSERT_EQ(capture.types.size(), 1u);
    EXPECT_EQ(capture.types[0], 'A');
    EXPECT_EQ(capture.sizes[0], itch::spec_message_length('A'));
}

TEST(Framer, FramesRecordsOfDifferentLengthsInOrder) {
    std::vector<std::byte> file;
    append(file, add_order(1));
    append(file, order_delete(1));
    append(file, add_order(2));

    Capture capture;
    const auto outcome = itch::frame_all(file, capture);

    EXPECT_TRUE(outcome.complete());
    EXPECT_EQ(outcome.messages, 3u);
    EXPECT_EQ(outcome.bytes_consumed, file.size());
    EXPECT_EQ(capture.types, (std::vector<char>{'A', 'D', 'A'}));
    EXPECT_EQ(capture.sizes, (std::vector<std::size_t>{36, 19, 36}));
}

TEST(Framer, RejectsATruncatedLengthPrefix) {
    std::vector<std::byte> file = add_order(1);
    file.push_back(std::byte{0});  // one stray byte: not enough for a prefix

    Capture capture;
    const auto outcome = itch::frame_all(file, capture);

    ASSERT_TRUE(outcome.error.has_value());
    EXPECT_EQ(*outcome.error, itch::FrameError::TruncatedPrefix);
    EXPECT_EQ(outcome.messages, 1u);
    EXPECT_EQ(outcome.error_offset, file.size() - 1);
}

TEST(Framer, RejectsAZeroLengthRecord) {
    std::vector<std::byte> file{std::byte{0}, std::byte{0}};

    Capture capture;
    const auto outcome = itch::frame_all(file, capture);

    ASSERT_TRUE(outcome.error.has_value());
    EXPECT_EQ(*outcome.error, itch::FrameError::ZeroLength);
    EXPECT_EQ(outcome.error_offset, 0u);
}

TEST(Framer, RejectsARecordRunningPastTheEnd) {
    auto file = add_order(1);
    file.resize(file.size() - 4);  // prefix still claims 36 bytes

    Capture capture;
    const auto outcome = itch::frame_all(file, capture);

    ASSERT_TRUE(outcome.error.has_value());
    EXPECT_EQ(*outcome.error, itch::FrameError::TruncatedRecord);
    EXPECT_EQ(outcome.messages, 0u);
}

TEST(Framer, RejectsAnUndefinedMessageType) {
    auto file = add_order(1);
    file[2] = static_cast<std::byte>('Z');  // first byte of the record

    Capture capture;
    const auto outcome = itch::frame_all(file, capture);

    ASSERT_TRUE(outcome.error.has_value());
    EXPECT_EQ(*outcome.error, itch::FrameError::UnknownType);
    EXPECT_EQ(outcome.error_type, 'Z');
}

TEST(Framer, RejectsALengthThatDisagreesWithTheSpecification) {
    // A 36-byte Add Order framed as if it were 35 bytes: exactly the shape of a
    // one-byte misalignment, and the check that stops it propagating.
    auto builder = header('A', 1, 0, 1000);
    builder.u64(1).ch('B').u32(100).alpha("AAPL", 8).u32(1650000);
    auto file = builder.framed();
    file[1] = static_cast<std::byte>(35);
    file.pop_back();

    Capture capture;
    const auto outcome = itch::frame_all(file, capture);

    ASSERT_TRUE(outcome.error.has_value());
    EXPECT_EQ(*outcome.error, itch::FrameError::LengthMismatch);
    EXPECT_EQ(outcome.error_type, 'A');
    EXPECT_EQ(outcome.messages, 0u);
}

TEST(Framer, DescribesEveryError) {
    for (auto error : {itch::FrameError::TruncatedPrefix, itch::FrameError::ZeroLength,
                       itch::FrameError::TruncatedRecord, itch::FrameError::UnknownType,
                       itch::FrameError::LengthMismatch}) {
        EXPECT_FALSE(std::string(itch::describe(error)).empty());
    }
}

}  // namespace