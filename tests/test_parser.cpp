#include "itch/parser.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "fixtures.hpp"

namespace {

using testing_support::header;

struct RecordingHandler {
    std::vector<char> decoded;
    std::vector<char> undecoded;

    void on(const itch::SystemEvent&) { decoded.push_back('S'); }
    void on(const itch::AddOrder&) { decoded.push_back('A'); }
    void on(const itch::AddOrderWithMpid&) { decoded.push_back('F'); }
    void on(const itch::OrderExecuted&) { decoded.push_back('E'); }
    void on(const itch::OrderExecutedWithPrice&) { decoded.push_back('C'); }
    void on(const itch::OrderCancel&) { decoded.push_back('X'); }
    void on(const itch::OrderDelete&) { decoded.push_back('D'); }
    void on(const itch::OrderReplace&) { decoded.push_back('U'); }
    void on(const itch::TradeNonCross&) { decoded.push_back('P'); }
    void on_other(char type, std::span<const std::byte>) { undecoded.push_back(type); }
};

void append(std::vector<std::byte>& into, const std::vector<std::byte>& tail) {
    into.insert(into.end(), tail.begin(), tail.end());
}

TEST(Parser, DispatchesEveryInScopeTypeAndDefersTheRest) {
    std::vector<std::byte> file;

    { auto b = header('S', 0, 0, 100); b.ch('O');                                  append(file, b.framed()); }
    { auto b = header('A', 1, 0, 200); b.u64(1).ch('B').u32(100).alpha("AAPL", 8).u32(1650000);
                                                                                   append(file, b.framed()); }
    { auto b = header('F', 1, 0, 300); b.u64(2).ch('S').u32(200).alpha("AAPL", 8).u32(1660000).alpha("NSDQ", 4);
                                                                                   append(file, b.framed()); }
    { auto b = header('E', 1, 0, 400); b.u64(1).u32(50).u64(9);                    append(file, b.framed()); }
    { auto b = header('C', 1, 0, 500); b.u64(1).u32(25).u64(10).ch('Y').u32(1649900);
                                                                                   append(file, b.framed()); }
    { auto b = header('X', 1, 0, 600); b.u64(1).u32(10);                           append(file, b.framed()); }
    { auto b = header('D', 1, 0, 700); b.u64(1);                                   append(file, b.framed()); }
    { auto b = header('U', 1, 0, 800); b.u64(2).u64(3).u32(150).u32(1655000);      append(file, b.framed()); }
    { auto b = header('P', 1, 0, 900); b.u64(4).ch('B').u32(75).alpha("AAPL", 8).u32(1652000).u64(11);
                                                                                   append(file, b.framed()); }
    // Out of scope but defined: Stock Directory, 39 bytes.
    { auto b = header('R', 1, 0, 1000);
      b.alpha("AAPL", 8).ch('Q').ch('N').u32(100).ch('N').ch('C').alpha("  ", 2)
       .ch('P').ch('N').ch('N').ch('1').ch('N').u32(0).ch('N');
      append(file, b.framed()); }

    RecordingHandler handler;
    const auto outcome = itch::parse_all(file, handler);

    ASSERT_TRUE(outcome.complete()) << itch::describe(*outcome.error);
    EXPECT_EQ(outcome.bytes_consumed, file.size());
    EXPECT_EQ(outcome.messages, 10u);
    EXPECT_EQ(handler.decoded, (std::vector<char>{'S', 'A', 'F', 'E', 'C', 'X', 'D', 'U', 'P'}));
    EXPECT_EQ(handler.undecoded, (std::vector<char>{'R'}));
}

}  // namespace