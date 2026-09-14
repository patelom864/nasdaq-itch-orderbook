#include "itch/order_book.hpp"

namespace itch {

void OrderBook::on(const AddOrder& message) {
    if (message.header.stock_locate != locate_) return;
    if (message.side == Side::Buy) {
        insert_order(bids_, Side::Buy, message.order_reference, message.price, message.shares);
    } else {
        insert_order(asks_, Side::Sell, message.order_reference, message.price, message.shares);
    }
    ++stats_.adds;
}

void OrderBook::on(const OrderExecuted& message) {
    if (message.header.stock_locate != locate_) return;
    reduce_or_remove(message.order_reference, message.executed_shares, /*remove_all=*/false);
    ++stats_.executions;
}

void OrderBook::on(const OrderCancel& message) {
    if (message.header.stock_locate != locate_) return;
    reduce_or_remove(message.order_reference, message.cancelled_shares, /*remove_all=*/false);
    ++stats_.cancels;
}

void OrderBook::on(const OrderDelete& message) {
    if (message.header.stock_locate != locate_) return;
    reduce_or_remove(message.order_reference, 0, /*remove_all=*/true);
    ++stats_.deletes;
}

void OrderBook::on(const OrderReplace& message) {
    if (message.header.stock_locate != locate_) return;
    const auto it = orders_.find(message.original_order_reference);
    if (it == orders_.end()) return;
    const Side side = it->second.side;

    reduce_or_remove(message.original_order_reference, 0, /*remove_all=*/true);
    if (side == Side::Buy) {
        insert_order(bids_, Side::Buy, message.new_order_reference, message.price, message.shares);
    } else {
        insert_order(asks_, Side::Sell, message.new_order_reference, message.price, message.shares);
    }
    ++stats_.replaces;
}

void OrderBook::reduce_or_remove(std::uint64_t reference, std::uint32_t shares_removed,
                                  bool remove_all) {
    const auto it = orders_.find(reference);
    if (it == orders_.end()) return;
    const OrderLocation location = it->second;  // Copy it out first, erasing below invalidates `it`.

    if (location.side == Side::Buy) {
        reduce_or_remove_in(bids_, location, reference, shares_removed, remove_all);
    } else {
        reduce_or_remove_in(asks_, location, reference, shares_removed, remove_all);
    }
}

std::optional<BookLevel> OrderBook::best_bid() const noexcept {
    if (bids_.empty()) return std::nullopt;
    const auto& [price, level] = *bids_.begin();
    return BookLevel{price, level.total_shares};
}

std::optional<BookLevel> OrderBook::best_ask() const noexcept {
    if (asks_.empty()) return std::nullopt;
    const auto& [price, level] = *asks_.begin();
    return BookLevel{price, level.total_shares};
}

bool OrderBook::check_rep() const {
    std::size_t total_orders = 0;

    auto check_side = [&](const auto& levels, Side expected_side) {
        for (const auto& [price, level] : levels) {
            if (level.orders.empty()) return false;  // A price level should never sit empty.
            std::uint64_t summed_shares = 0;
            for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
                summed_shares += it->shares;
                const auto found = orders_.find(it->order_reference);
                if (found == orders_.end()) return false;
                const OrderLocation& location = found->second;
                if (location.side != expected_side || location.price != price
                    || location.position != it) {
                    return false;
                }
                ++total_orders;
            }
            if (summed_shares != level.total_shares) return false;
        }
        return true;
    };

    if (!check_side(bids_, Side::Buy)) return false;
    if (!check_side(asks_, Side::Sell)) return false;
    return total_orders == orders_.size();
}

}  // namespace itch