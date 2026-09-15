#include "itch/order_book.hpp"

#include <gtest/gtest.h>

namespace {

using itch::AddOrder;
using itch::AddOrderWithMpid;
using itch::MessageHeader;
using itch::OrderBook;
using itch::OrderCancel;
using itch::OrderDelete;
using itch::OrderExecuted;
using itch::OrderExecutedWithPrice;
using itch::OrderReplace;
using itch::Side;
using itch::TradeNonCross;

constexpr std::uint16_t kLocate = 7;

MessageHeader header(char type, std::uint16_t locate, std::uint64_t timestamp_ns) {
    return MessageHeader{
        .type = type, .stock_locate = locate, .tracking_number = 0, .timestamp_ns = timestamp_ns};
}

AddOrder add(std::uint64_t reference, Side side, std::uint32_t shares, itch::PriceTicks price,
             std::uint16_t locate = kLocate) {
    return AddOrder{
        .header = header('A', locate, reference),
        .order_reference = reference,
        .side = side,
        .shares = shares,
        .stock = {},
        .price = price,
    };
}

OrderBook make_book() {
    OrderBook book;
    book.set_locate(kLocate);
    return book;
}

TEST(OrderBook, AddOrderSetsBestBidAndAsk) {
    OrderBook book = make_book();
    book.on(add(1, Side::Buy, 100, 15000));
    book.on(add(2, Side::Sell, 200, 15100));

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->price, 15000u);
    EXPECT_EQ(book.best_bid()->shares, 100u);
    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(book.best_ask()->price, 15100u);
    EXPECT_EQ(book.best_ask()->shares, 200u);
    EXPECT_TRUE(book.check_rep());
}

TEST(OrderBook, MultipleOrdersAtSamePriceAggregateShares) {
    OrderBook book = make_book();
    book.on(add(1, Side::Buy, 100, 15000));
    book.on(add(2, Side::Buy, 50, 15000));

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->shares, 150u);
    EXPECT_EQ(book.bid_level_count(), 1u);
    EXPECT_EQ(book.order_count(), 2u);
    EXPECT_TRUE(book.check_rep());
}

TEST(OrderBook, IgnoresMessagesForADifferentLocate) {
    OrderBook book = make_book();
    book.on(add(1, Side::Buy, 100, 15000, /*locate=*/kLocate + 1));

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_EQ(book.order_count(), 0u);
    EXPECT_EQ(book.stats().adds, 0u);
}

TEST(OrderBook, AddOrderWithMpidDelegatesToThePlainAdd) {
    OrderBook book = make_book();
    book.on(AddOrderWithMpid{add(1, Side::Buy, 100, 15000), {}});

    EXPECT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.stats().adds, 1u);
}

TEST(OrderBook, CancelReducesSharesWithoutRemovingTheOrder) {
    OrderBook book = make_book();
    book.on(add(1, Side::Buy, 100, 15000));
    book.on(OrderCancel{header('X', kLocate, 2), 1, 40});

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->shares, 60u);
    EXPECT_EQ(book.order_count(), 1u);
    EXPECT_EQ(book.stats().cancels, 1u);
    EXPECT_TRUE(book.check_rep());
}

TEST(OrderBook, DeleteRemovesTheOrderAndEmptiesTheLevel) {
    OrderBook book = make_book();
    book.on(add(1, Side::Buy, 100, 15000));
    book.on(OrderDelete{header('D', kLocate, 2), 1});

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_EQ(book.order_count(), 0u);
    EXPECT_EQ(book.bid_level_count(), 0u);
    EXPECT_EQ(book.stats().deletes, 1u);
    EXPECT_TRUE(book.check_rep());
}

TEST(OrderBook, ExecutedReducesSharesLikeACancel) {
    OrderBook book = make_book();
    book.on(add(1, Side::Sell, 100, 15100));
    book.on(OrderExecuted{header('E', kLocate, 2), 1, 30, 999});

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_EQ(book.best_ask()->shares, 70u);
    EXPECT_EQ(book.stats().executions, 1u);
    EXPECT_TRUE(book.check_rep());
}

TEST(OrderBook, ExecutedWithPriceDelegatesToTheEmbeddedExecuted) {
    OrderBook book = make_book();
    book.on(add(1, Side::Sell, 100, 15100));
    book.on(OrderExecutedWithPrice{OrderExecuted{header('C', kLocate, 2), 1, 100, 999}, 'Y', 15050});

    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.stats().executions, 1u);
    EXPECT_TRUE(book.check_rep());
}

TEST(OrderBook, ReplaceMovesTheOrderToItsNewPriceAtTheBackOfTheQueue) {
    OrderBook book = make_book();
    book.on(add(1, Side::Buy, 100, 15000));
    book.on(add(2, Side::Buy, 50, 15000));
    book.on(OrderReplace{header('U', kLocate, 3), 1, 3, 80, 15050});

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_EQ(book.best_bid()->price, 15050u);
    EXPECT_EQ(book.best_bid()->shares, 80u);
    EXPECT_EQ(book.order_count(), 2u);  // order 2 still at 15000, order 3 now at 15050
    EXPECT_EQ(book.stats().replaces, 1u);
    EXPECT_TRUE(book.check_rep());
}

TEST(OrderBook, TradeNonCrossTalliesWithoutTouchingTheBook) {
    OrderBook book = make_book();
    book.on(add(1, Side::Buy, 100, 15000));
    book.on(TradeNonCross{header('P', kLocate, 2), 9, Side::Buy, 100, {}, 15000, 555});

    EXPECT_EQ(book.stats().trades, 1u);
    EXPECT_EQ(book.best_bid()->shares, 100u);  // Unchanged: 'P' has no book effect.
}

}  // namespace