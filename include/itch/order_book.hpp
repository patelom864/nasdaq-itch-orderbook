#pragma once

#include "itch/messages.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <span>
#include <unordered_map>

namespace itch {

// One resting order in a price level's FIFO queue. The queue order is
// what gives time priority, not any field here.
struct RestingOrder {
    std::uint64_t order_reference;
    std::uint32_t shares;
};

// A single price level: its resting orders, oldest first, plus the total
// shares so top-of-book and check_rep don't have to walk the queue just
// to answer how much is resting here.
struct PriceLevel {
    std::list<RestingOrder> orders;
    std::uint64_t total_shares = 0;
};

// A snapshot of one side's best price and how much is resting there.
struct BookLevel {
    PriceTicks price;
    std::uint64_t shares;
};

// A single-symbol, price-time-priority limit order book, built from ITCH
// Add, Execute, Cancel, Delete, and Replace messages.
//
// Containers are naive on purpose: std::map for price levels, std::list
// for the FIFO queue at a level, std::unordered_map for order lookup by
// reference. Phase 4 swaps these for a tick-indexed array and an
// open-addressing table. Right now the goal is a book that's obviously
// correct, not fast.
//
// Scoped to one locate. Every on() is a no-op until set_locate() has run,
// and stays a no-op after that for any message whose header carries a
// different locate. That means feeding it a whole session's messages
// filters down to one symbol automatically, no special-casing needed
// beyond that one comparison in each mutator.
//
// Satisfies the MessageHandler concept (parser.hpp) directly, so it can
// be driven by parse_all on its own in tests. book_replay.cpp only wraps
// it to add the bootstrap step, watching for the Stock Directory message
// that says which locate to resolve.
class OrderBook {
public:
    // Tallies of what's happened to the book, independent of its current
    // state, so a caller can report a session summary without
    // re-deriving it from the price maps.
    struct Stats {
        std::uint64_t adds = 0;
        std::uint64_t executions = 0;
        std::uint64_t cancels = 0;
        std::uint64_t deletes = 0;
        std::uint64_t replaces = 0;
        std::uint64_t trades = 0;  // Non-cross trades, no book effect, counted anyway.
    };

    // Assigns the locate this book accepts messages for. Can only be
    // called once, since a book is scoped to one symbol for its whole
    // life. 0 is the sentinel for not resolved yet.
    void set_locate(std::uint16_t locate) noexcept {
        assert(locate_ == 0 && "set_locate must only be called once");
        assert(locate != 0 && "0 is the unresolved sentinel, not a valid locate");
        locate_ = locate;
    }
    std::uint16_t locate() const noexcept { return locate_; }

    void on(const AddOrder& message);
    void on(const AddOrderWithMpid& message) { on(message.order); }
    void on(const OrderExecuted& message);
    void on(const OrderExecutedWithPrice& message) { on(message.executed); }
    void on(const OrderCancel& message);
    void on(const OrderDelete& message);
    void on(const OrderReplace& message);
    void on(const TradeNonCross& message) noexcept {
        if (message.header.stock_locate == locate_) ++stats_.trades;
    }
    void on(const SystemEvent&) noexcept {}
    void on(const StockDirectory&) noexcept {}
    void on_other(char, std::span<const std::byte>) noexcept {}

    std::optional<BookLevel> best_bid() const noexcept;
    std::optional<BookLevel> best_ask() const noexcept;

    std::size_t order_count() const noexcept { return orders_.size(); }
    std::size_t bid_level_count() const noexcept { return bids_.size(); }
    std::size_t ask_level_count() const noexcept { return asks_.size(); }
    const Stats& stats() const noexcept { return stats_; }

    // Full representation-invariant check. Confirms every order in
    // orders_ points at a real position in the level its own record
    // names, every level's total_shares matches the sum of its queue,
    // and no level is left in the map with an empty queue. That's
    // O(book size), so it's for tests and periodic audits, not the hot
    // path. The mutators below carry their own O(1) assert-guarded
    // invariants for that, see reduce_or_remove.
    bool check_rep() const;

private:
    struct OrderLocation {
        Side side;
        PriceTicks price;
        std::list<RestingOrder>::iterator position;
    };

    // Inserts a new resting order at `price` in `levels`, creating the
    // level if this is its first order there, and records where to find
    // it in orders_. Levels is one of the two price-map types below.
    // They only differ in comparator, so one definition covers both sides.
    template <class Levels>
    void insert_order(Levels& levels, Side side, std::uint64_t reference, PriceTicks price,
                       std::uint32_t shares) {
        PriceLevel& level = levels[price];
        level.orders.push_back(RestingOrder{reference, shares});
        level.total_shares += shares;
        orders_.emplace(reference, OrderLocation{side, price, std::prev(level.orders.end())});
    }

    // Removes `shares_removed` shares from the resting order at `location`
    // (all of them if `remove_all` is set), and erases the order, plus
    // its level if that empties it, from both `levels` and orders_.
    // Shared by reduce_or_remove for whichever side the order actually
    // sits on.
    template <class Levels>
    void reduce_or_remove_in(Levels& levels, const OrderLocation& location,
                              std::uint64_t reference, std::uint32_t shares_removed,
                              bool remove_all) {
        const auto level_it = levels.find(location.price);
        assert(level_it != levels.end());
        PriceLevel& level = level_it->second;
        RestingOrder& resting = *location.position;

        const std::uint32_t removed = remove_all ? resting.shares : shares_removed;
        assert(removed <= resting.shares);
        level.total_shares -= removed;
        resting.shares -= removed;

        if (remove_all || resting.shares == 0) {
            level.orders.erase(location.position);
            orders_.erase(reference);
            if (level.orders.empty()) levels.erase(level_it);
        }
    }

    // Shared by Executed, Cancel, and Delete. (Executed-with-Price and
    // Add-with-Mpid delegate to the plain message instead of coming
    // through here.) A reference this book has never seen, whether it's
    // for a different locate or already removed, is a silent no-op. The
    // locate check in each on() means that's the normal case for most of
    // a session's messages, not a bug.
    void reduce_or_remove(std::uint64_t reference, std::uint32_t shares_removed, bool remove_all);

    std::uint16_t locate_ = 0;
    std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>> bids_;
    std::map<PriceTicks, PriceLevel> asks_;
    std::unordered_map<std::uint64_t, OrderLocation> orders_;
    Stats stats_;
};

}  // namespace itch