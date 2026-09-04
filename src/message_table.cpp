#include "itch/message_table.hpp"

#include <array>

namespace itch {
namespace {

// A 256-entry lookup rather than a switch: the framer consults this once per
// message, and a dense table indexed by the type byte is both branch-free and
// trivially auditable against the specification's own table.
constexpr std::array<std::uint16_t, 256> kSpecLengths = [] {
    std::array<std::uint16_t, 256> lengths{};
    auto set = [&lengths](char type, std::uint16_t length) {
        lengths[static_cast<unsigned char>(type)] = length;
    };

    // In scope for the order book.
    set('S', 12);  // System Event
    set('A', 36);  // Add Order, no MPID attribution
    set('F', 40);  // Add Order with MPID attribution
    set('E', 31);  // Order Executed
    set('C', 36);  // Order Executed with Price
    set('X', 23);  // Order Cancel
    set('D', 19);  // Order Delete
    set('U', 35);  // Order Replace
    set('P', 44);  // Trade, non-cross

    // Defined by the specification, not decoded here. They must still be in the
    // table: the framer validates every record's length, including these.
    set('R', 39);  // Stock Directory
    set('H', 25);  // Stock Trading Action
    set('Y', 20);  // Reg SHO Restriction
    set('L', 26);  // Market Participant Position
    set('V', 35);  // MWCB Decline Level
    set('W', 12);  // MWCB Status
    set('K', 28);  // IPO Quoting Period Update
    set('J', 35);  // LULD Auction Collar
    set('h', 21);  // Operational Halt
    set('Q', 40);  // Cross Trade
    set('B', 19);  // Broken Trade
    set('I', 50);  // Net Order Imbalance Indicator
    set('N', 20);  // Retail Price Improvement Indicator

    return lengths;
}();

}  // namespace

std::uint16_t spec_message_length(char message_type) noexcept {
    return kSpecLengths[static_cast<unsigned char>(message_type)];
}

}  // namespace itch