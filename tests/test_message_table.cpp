#include "itch/message_table.hpp"

#include <gtest/gtest.h>

namespace {

TEST(MessageTable, InScopeTypesMatchSpecification) {
    EXPECT_EQ(itch::spec_message_length('S'), 12);
    EXPECT_EQ(itch::spec_message_length('A'), 36);
    EXPECT_EQ(itch::spec_message_length('F'), 40);
    EXPECT_EQ(itch::spec_message_length('E'), 31);
    EXPECT_EQ(itch::spec_message_length('C'), 36);
    EXPECT_EQ(itch::spec_message_length('X'), 23);
    EXPECT_EQ(itch::spec_message_length('D'), 19);
    EXPECT_EQ(itch::spec_message_length('U'), 35);
    EXPECT_EQ(itch::spec_message_length('P'), 44);
}

TEST(MessageTable, OutOfScopeTypesAreStillKnown) {
    EXPECT_EQ(itch::spec_message_length('R'), 39);
    EXPECT_EQ(itch::spec_message_length('H'), 25);
    EXPECT_EQ(itch::spec_message_length('Y'), 20);
    EXPECT_EQ(itch::spec_message_length('L'), 26);
    EXPECT_EQ(itch::spec_message_length('V'), 35);
    EXPECT_EQ(itch::spec_message_length('W'), 12);
    EXPECT_EQ(itch::spec_message_length('K'), 28);
    EXPECT_EQ(itch::spec_message_length('J'), 35);
    EXPECT_EQ(itch::spec_message_length('h'), 21);
    EXPECT_EQ(itch::spec_message_length('Q'), 40);
    EXPECT_EQ(itch::spec_message_length('B'), 19);
    EXPECT_EQ(itch::spec_message_length('I'), 50);
    EXPECT_EQ(itch::spec_message_length('N'), 20);
}

TEST(MessageTable, UndefinedTypesReturnZero) {
    EXPECT_EQ(itch::spec_message_length('\0'), 0);
    EXPECT_EQ(itch::spec_message_length('a'), 0);   // lowercase, and not 'h'
    EXPECT_EQ(itch::spec_message_length('Z'), 0);
    EXPECT_EQ(itch::spec_message_length(' '), 0);
    EXPECT_EQ(itch::spec_message_length(static_cast<char>(0xFF)), 0);
}

TEST(MessageTable, HeaderFitsInsideTheShortestMessage) {
    // The shortest messages the spec defines are System Event and MWCB Status,
    // which are the 11-byte header plus a single byte.
    EXPECT_EQ(itch::spec_message_length('S'), itch::kHeaderLength + 1);
    EXPECT_EQ(itch::spec_message_length('W'), itch::kHeaderLength + 1);
}

}  // namespace