#pragma once

#include "itch/framer.hpp"
#include "itch/messages.hpp"

#include <span>

namespace itch {

// What a client must provide to consume a session. Stating it as a concept means
// a handler with a wrong signature fails at the parse_all call site with a
// readable diagnostic, instead of somewhere inside the dispatch template.
//
// on_other exists so that a client needing a message this parser does not decode
// can reach the raw bytes without the parser changing.
template <class H>
concept MessageHandler = requires(H& handler,
                                  const SystemEvent& system_event,
                                  const AddOrder& add,
                                  const AddOrderWithMpid& add_with_mpid,
                                  const OrderExecuted& executed,
                                  const OrderExecutedWithPrice& executed_with_price,
                                  const OrderCancel& cancel,
                                  const OrderDelete& deletion,
                                  const OrderReplace& replace,
                                  const TradeNonCross& trade,
                                  char other_type,
                                  std::span<const std::byte> other_record) {
    handler.on(system_event);
    handler.on(add);
    handler.on(add_with_mpid);
    handler.on(executed);
    handler.on(executed_with_price);
    handler.on(cancel);
    handler.on(deletion);
    handler.on(replace);
    handler.on(trade);
    handler.on_other(other_type, other_record);
};

// Frames `file`, decodes each in-scope message, and delivers it to `handler`.
// Returns the framer's outcome unchanged. Never throws unless the handler does.
template <MessageHandler H>
FrameOutcome parse_all(std::span<const std::byte> file, H& handler) {
    return frame_all(file, [&handler](char type, std::span<const std::byte> record) {
        const ByteReader reader{record};

        // Cases are written in descending order of frequency in a real session
        // (adds, deletes and executes dominate). Whether that ordering matters
        // at all is a Phase 3 measurement, not an assumption: the compiler is
        // free to build a jump table and ignore it entirely.
        switch (type) {
        case 'A': handler.on(decode_add_order(reader)); break;
        case 'D': handler.on(decode_order_delete(reader)); break;
        case 'E': handler.on(decode_order_executed(reader)); break;
        case 'X': handler.on(decode_order_cancel(reader)); break;
        case 'U': handler.on(decode_order_replace(reader)); break;
        case 'F': handler.on(decode_add_order_with_mpid(reader)); break;
        case 'C': handler.on(decode_order_executed_with_price(reader)); break;
        case 'P': handler.on(decode_trade_non_cross(reader)); break;
        case 'S': handler.on(decode_system_event(reader)); break;
        default:  handler.on_other(type, record); break;
        }
    });
}

}  // namespace itch