#include "itch/mapped_file.hpp"
#include "itch/order_book.hpp"
#include "itch/parser.hpp"

#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

// Wraps an OrderBook to add the one thing it can't do on its own: resolve
// which locate to accept. A Stock Directory message announces a symbol
// and the locate assigned to it for the session. This watches for the
// one matching `symbol_` and calls book_.set_locate() as soon as it shows
// up, then just forwards everything else to the book for the rest of the
// file.
class BookReplayHandler {
public:
    BookReplayHandler(itch::OrderBook& book, std::string symbol)
        : book_(book), symbol_(std::move(symbol)) {}

    void on(const itch::StockDirectory& message) {
        if (!resolved_ && itch::trim(message.stock) == symbol_) {
            book_.set_locate(message.header.stock_locate);
            resolved_ = true;
        }
    }

    void on(const itch::SystemEvent& message) { book_.on(message); }
    void on(const itch::AddOrder& message) { book_.on(message); }
    void on(const itch::AddOrderWithMpid& message) { book_.on(message); }
    void on(const itch::OrderExecuted& message) { book_.on(message); }
    void on(const itch::OrderExecutedWithPrice& message) { book_.on(message); }
    void on(const itch::OrderCancel& message) { book_.on(message); }
    void on(const itch::OrderDelete& message) { book_.on(message); }
    void on(const itch::OrderReplace& message) { book_.on(message); }
    void on(const itch::TradeNonCross& message) { book_.on(message); }
    void on_other(char, std::span<const std::byte>) {}

    bool resolved() const noexcept { return resolved_; }

private:
    itch::OrderBook& book_;
    std::string symbol_;
    bool resolved_ = false;
};

void print_field(std::string_view label, auto&& value) {
    std::cout << std::left << std::setw(16) << std::string(label) << value << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: book_replay <session.NASDAQ_ITCH50> <symbol>\n";
        return 2;
    }

    try {
        const itch::MappedFile file(argv[1]);
        const std::string symbol(argv[2]);

        itch::OrderBook book;
        BookReplayHandler handler(book, symbol);
        const auto outcome = itch::parse_all(file.bytes(), handler);

        if (!handler.resolved()) {
            std::cerr << "book_replay: " << symbol
                      << " was never announced in a Stock Directory message\n";
            return 1;
        }

        print_field("symbol", symbol);
        print_field("locate", book.locate());
        print_field("adds", book.stats().adds);
        print_field("executions", book.stats().executions);
        print_field("cancels", book.stats().cancels);
        print_field("deletes", book.stats().deletes);
        print_field("replaces", book.stats().replaces);
        print_field("trades", book.stats().trades);
        print_field("resting orders", book.order_count());
        print_field("bid levels", book.bid_level_count());
        print_field("ask levels", book.ask_level_count());

        std::cout << std::left << std::setw(16) << "best bid";
        if (const auto bid = book.best_bid()) {
            std::cout << bid->price << " (" << bid->shares << " shares)\n";
        } else {
            std::cout << "none\n";
        }
        std::cout << std::left << std::setw(16) << "best ask";
        if (const auto ask = book.best_ask()) {
            std::cout << ask->price << " (" << ask->shares << " shares)\n";
        } else {
            std::cout << "none\n";
        }

        if (!outcome.complete()) {
            std::cout << "\nframing stopped: " << itch::describe(*outcome.error) << " at offset "
                      << outcome.error_offset << '\n';
        }

        return outcome.complete() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "book_replay: " << error.what() << '\n';
        return 2;
    }
}