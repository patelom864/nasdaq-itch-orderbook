#pragma once

#include "itch/byte_reader.hpp"
#include "itch/message_table.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string_view>

namespace itch {

// Prices are unsigned 4-byte integers in units of $0.0001 on the wire, and stay
// in that form throughout. Converting to dollars is a presentation concern and
// never happens on a decode or book-update path: floating point here would cost
// both accuracy and cycles for nothing.
using PriceTicks = std::uint32_t;

using Stock = std::array<char, 8>;  // space-padded ASCII
using Mpid = std::array<char, 4>;   // space-padded ASCII

enum class Side : char { Buy = 'B', Sell = 'S' };

constexpr bool is_valid_side(char value) noexcept { return value == 'B' || value == 'S'; }

// The significant characters of a space-padded alpha field. The returned view
// aliases `field` and must not outlive it.
template <std::size_t N>
std::string_view trim(const std::array<char, N>& field) noexcept {
    const std::string_view view(field.data(), N);
    const auto last = view.find_last_not_of(' ');
    return last == std::string_view::npos ? std::string_view{} : view.substr(0, last + 1);
}

struct MessageHeader {
    char type;
    std::uint16_t stock_locate;
    std::uint16_t tracking_number;
    std::uint64_t timestamp_ns;  // nanoseconds since midnight, US/Eastern
};

// Layout, offset: 0 type | 1 stock locate | 3 tracking | 5 timestamp (6)
inline MessageHeader decode_header(ByteReader reader) noexcept {
    return MessageHeader{
        .type = reader.read_char(0),
        .stock_locate = reader.read_u16(1),
        .tracking_number = reader.read_u16(3),
        .timestamp_ns = reader.read_u48(5),
    };
}

// 'S' — 12 bytes. Event codes: O start of messages, S start of system hours,
// Q start of market hours, M end of market hours, E end of system hours,
// C end of messages.
struct SystemEvent {
    MessageHeader header;
    char event_code;
};

inline SystemEvent decode_system_event(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('S'));
    return SystemEvent{decode_header(reader), reader.read_char(11)};
}

// 'A' — 36 bytes.
// 11 order ref (8) | 19 side (1) | 20 shares (4) | 24 stock (8) | 32 price (4)
struct AddOrder {
    MessageHeader header;
    std::uint64_t order_reference;
    Side side;
    std::uint32_t shares;
    Stock stock;
    PriceTicks price;
};

inline AddOrder decode_add_order(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('A'));
    assert(is_valid_side(reader.read_char(19)));
    return AddOrder{
        .header = decode_header(reader),
        .order_reference = reader.read_u64(11),
        .side = static_cast<Side>(reader.read_char(19)),
        .shares = reader.read_u32(20),
        .stock = reader.read_alpha<8>(24),
        .price = reader.read_u32(32),
    };
}

// 'F' — 40 bytes: an Add Order followed by a 4-byte MPID at offset 36.
// Composition, not inheritance: the attribution is extra data on the same event,
// and every consumer that cares about the add wants the same AddOrder value.
struct AddOrderWithMpid {
    AddOrder order;
    Mpid attribution;
};

inline AddOrderWithMpid decode_add_order_with_mpid(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('F'));
    assert(is_valid_side(reader.read_char(19)));
    // The leading 36 bytes are laid out exactly like an 'A', so decode them as one.
    const AddOrder order{
        .header = decode_header(reader),
        .order_reference = reader.read_u64(11),
        .side = static_cast<Side>(reader.read_char(19)),
        .shares = reader.read_u32(20),
        .stock = reader.read_alpha<8>(24),
        .price = reader.read_u32(32),
    };
    return AddOrderWithMpid{order, reader.read_alpha<4>(36)};
}

// 'E' — 31 bytes. Note: executed shares, not remaining shares.
// 11 order ref (8) | 19 executed shares (4) | 23 match number (8)
struct OrderExecuted {
    MessageHeader header;
    std::uint64_t order_reference;
    std::uint32_t executed_shares;
    std::uint64_t match_number;
};

inline OrderExecuted decode_order_executed(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('E'));
    return OrderExecuted{
        .header = decode_header(reader),
        .order_reference = reader.read_u64(11),
        .executed_shares = reader.read_u32(19),
        .match_number = reader.read_u64(23),
    };
}

// 'C' — 36 bytes: an Order Executed plus a printable flag and the price the
// trade actually printed at, which differs from the resting order's price.
struct OrderExecutedWithPrice {
    OrderExecuted executed;
    char printable;  // 'Y' or 'N'
    PriceTicks execution_price;
};

inline OrderExecutedWithPrice decode_order_executed_with_price(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('C'));
    const OrderExecuted executed{
        .header = decode_header(reader),
        .order_reference = reader.read_u64(11),
        .executed_shares = reader.read_u32(19),
        .match_number = reader.read_u64(23),
    };
    return OrderExecutedWithPrice{executed, reader.read_char(31), reader.read_u32(32)};
}

// 'X' — 23 bytes. A partial reduction; the order stays on the book.
struct OrderCancel {
    MessageHeader header;
    std::uint64_t order_reference;
    std::uint32_t cancelled_shares;
};

inline OrderCancel decode_order_cancel(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('X'));
    return OrderCancel{
        .header = decode_header(reader),
        .order_reference = reader.read_u64(11),
        .cancelled_shares = reader.read_u32(19),
    };
}

// 'D' — 19 bytes. Removes the order entirely.
struct OrderDelete {
    MessageHeader header;
    std::uint64_t order_reference;
};

inline OrderDelete decode_order_delete(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('D'));
    return OrderDelete{
        .header = decode_header(reader),
        .order_reference = reader.read_u64(11),
    };
}

// 'U' — 35 bytes. Retires the original reference and creates a new one, which
// joins the back of the queue at its price. Carries no side or symbol: those
// come from the original order, which is why the book needs an order lookup.
// 11 original ref (8) | 19 new ref (8) | 27 shares (4) | 31 price (4)
struct OrderReplace {
    MessageHeader header;
    std::uint64_t original_order_reference;
    std::uint64_t new_order_reference;
    std::uint32_t shares;
    PriceTicks price;
};

inline OrderReplace decode_order_replace(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('U'));
    return OrderReplace{
        .header = decode_header(reader),
        .original_order_reference = reader.read_u64(11),
        .new_order_reference = reader.read_u64(19),
        .shares = reader.read_u32(27),
        .price = reader.read_u32(31),
    };
}

// 'P' — 44 bytes. Non-displayable execution: no book effect. Kept because it is
// the exchange's own account of a trade, which is the ground truth the book's
// executions get reconciled against in Phase 2.
// 11 order ref (8) | 19 side (1) | 20 shares (4) | 24 stock (8) | 32 price (4)
//   | 36 match number (8)
struct TradeNonCross {
    MessageHeader header;
    std::uint64_t order_reference;
    Side side;
    std::uint32_t shares;
    Stock stock;
    PriceTicks price;
    std::uint64_t match_number;
};

inline TradeNonCross decode_trade_non_cross(ByteReader reader) noexcept {
    assert(reader.size() == spec_message_length('P'));
    assert(is_valid_side(reader.read_char(19)));
    return TradeNonCross{
        .header = decode_header(reader),
        .order_reference = reader.read_u64(11),
        .side = static_cast<Side>(reader.read_char(19)),
        .shares = reader.read_u32(20),
        .stock = reader.read_alpha<8>(24),
        .price = reader.read_u32(32),
        .match_number = reader.read_u64(36),
    };
}

}  // namespace itch