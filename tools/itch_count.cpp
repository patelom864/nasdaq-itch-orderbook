#include "itch/mapped_file.hpp"
#include "itch/parser.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

// A census of one ITCH session.
//
// Counts every message by type and checks the two session-level facts that can be
// verified without maintaining a book:
//
//   * timestamps never go backwards, and
//   * the system event codes arrive in the order the specification defines for a
//     full trading day: O S Q M E C.
//
// A violation of either means the walk is misaligned or the file is partial. It
// does not mean the exchange is wrong.
class SessionCensus {
public:
    void on(const itch::SystemEvent& message) {
        note(message.header);
        system_events_.push_back(message.event_code);
    }
    void on(const itch::AddOrder& message) { note(message.header); }
    void on(const itch::AddOrderWithMpid& message) { note(message.order.header); }
    void on(const itch::OrderExecuted& message) { note(message.header); }
    void on(const itch::OrderExecutedWithPrice& message) { note(message.executed.header); }
    void on(const itch::OrderCancel& message) { note(message.header); }
    void on(const itch::OrderDelete& message) { note(message.header); }
    void on(const itch::OrderReplace& message) { note(message.header); }
    void on(const itch::TradeNonCross& message) { note(message.header); }

    void on_other(char, std::span<const std::byte> record) {
        note(itch::decode_header(itch::ByteReader{record}));
    }

    std::uint64_t count(char type) const noexcept {
        return counts_[static_cast<unsigned char>(type)];
    }
    std::uint64_t total() const noexcept { return total_; }
    std::uint64_t backwards_timestamps() const noexcept { return backwards_timestamps_; }
    const std::string& system_events() const noexcept { return system_events_; }
    std::uint64_t first_timestamp_ns() const noexcept { return first_timestamp_ns_; }
    std::uint64_t last_timestamp_ns() const noexcept { return last_timestamp_ns_; }

private:
    void note(const itch::MessageHeader& header) noexcept {
        ++counts_[static_cast<unsigned char>(header.type)];
        if (total_ == 0) {
            first_timestamp_ns_ = header.timestamp_ns;
        } else if (header.timestamp_ns < last_timestamp_ns_) {
            ++backwards_timestamps_;
        }
        last_timestamp_ns_ = header.timestamp_ns;
        ++total_;
    }

    std::array<std::uint64_t, 256> counts_{};
    std::uint64_t total_ = 0;
    std::uint64_t backwards_timestamps_ = 0;
    std::uint64_t first_timestamp_ns_ = 0;
    std::uint64_t last_timestamp_ns_ = 0;
    std::string system_events_;
};

std::string format_time_of_day(std::uint64_t nanoseconds_since_midnight) {
    const auto total_seconds = nanoseconds_since_midnight / 1'000'000'000ULL;
    const auto nanoseconds = nanoseconds_since_midnight % 1'000'000'000ULL;
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%02llu:%02llu:%02llu.%09llu",
                  static_cast<unsigned long long>(total_seconds / 3600),
                  static_cast<unsigned long long>((total_seconds / 60) % 60),
                  static_cast<unsigned long long>(total_seconds % 60),
                  static_cast<unsigned long long>(nanoseconds));
    return buffer;
}

const char* describe_type(char type) {
    switch (type) {
    case 'S': return "System Event";
    case 'R': return "Stock Directory";
    case 'H': return "Stock Trading Action";
    case 'Y': return "Reg SHO Restriction";
    case 'L': return "Market Participant Position";
    case 'V': return "MWCB Decline Level";
    case 'W': return "MWCB Status";
    case 'K': return "IPO Quoting Period Update";
    case 'J': return "LULD Auction Collar";
    case 'h': return "Operational Halt";
    case 'A': return "Add Order";
    case 'F': return "Add Order with MPID";
    case 'E': return "Order Executed";
    case 'C': return "Order Executed with Price";
    case 'X': return "Order Cancel";
    case 'D': return "Order Delete";
    case 'U': return "Order Replace";
    case 'P': return "Trade (non-cross)";
    case 'Q': return "Cross Trade";
    case 'B': return "Broken Trade";
    case 'I': return "Net Order Imbalance Indicator";
    case 'N': return "Retail Price Improvement";
    default:  return "";
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: itch_count <session.NASDAQ_ITCH50>\n";
        return 2;
    }

    try {
        const itch::MappedFile file(argv[1]);
        SessionCensus census;

        const auto started = std::chrono::steady_clock::now();
        const auto outcome = itch::parse_all(file.bytes(), census);
        const auto elapsed = std::chrono::steady_clock::now() - started;

        const auto file_size = file.bytes().size();

        std::vector<std::pair<char, std::uint64_t>> present;
        for (int i = 0; i < 256; ++i) {
            const char type = static_cast<char>(i);
            if (census.count(type) > 0) {
                present.emplace_back(type, census.count(type));
            }
        }
        std::sort(present.begin(), present.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

        std::cout << "file            " << argv[1] << '\n'
                  << "bytes           " << file_size << '\n'
                  << "bytes consumed  " << outcome.bytes_consumed << '\n'
                  << "messages        " << outcome.messages << "\n\n";

        std::cout << "type  count            share   description\n";
        for (const auto& [type, count] : present) {
            const double share = 100.0 * static_cast<double>(count)
                                       / static_cast<double>(census.total());
            std::cout << "  " << type << "   " << std::setw(14) << count << "  "
                      << std::fixed << std::setprecision(2) << std::setw(6) << share << "%  "
                      << describe_type(type) << '\n';
        }

        std::cout << "\nfirst timestamp " << format_time_of_day(census.first_timestamp_ns())
                  << "\nlast timestamp  " << format_time_of_day(census.last_timestamp_ns())
                  << "\nsystem events   " << census.system_events() << '\n';

        // Invariants. Each is printed with its verdict so a failing run says which.
        const bool framed_completely = outcome.complete() && outcome.bytes_consumed == file_size;
        const bool counts_agree = census.total() == outcome.messages;
        const bool timestamps_monotonic = census.backwards_timestamps() == 0;
        const bool session_complete = census.system_events() == "OSQMEC";

        std::cout << "\ninvariants\n"
                  << "  framed to last byte      " << (framed_completely ? "ok" : "FAILED") << '\n'
                  << "  counts agree with framer " << (counts_agree ? "ok" : "FAILED") << '\n'
                  << "  timestamps non-decreasing "
                  << (timestamps_monotonic
                          ? "ok"
                          : "FAILED (" + std::to_string(census.backwards_timestamps()) + ")")
                  << '\n'
                  << "  full session O S Q M E C " << (session_complete ? "ok" : "not present")
                  << '\n';

        if (!outcome.complete()) {
            std::cout << "\nframing stopped: " << itch::describe(*outcome.error)
                      << " at offset " << outcome.error_offset;
            if (outcome.error_type != '\0') {
                std::cout << " (type '" << outcome.error_type << "')";
            }
            std::cout << '\n';
        }

        // Throughput, not latency. No warm-up, no percentiles, no pinned core:
        // this number belongs nowhere near BENCHMARKS.md or a resume.
        const auto seconds = std::chrono::duration<double>(elapsed).count();
        std::cout << "\nwalked in " << std::setprecision(2) << seconds << " s";
        if (seconds > 0.0) {
            std::cout << " ("
                      << static_cast<std::uint64_t>(static_cast<double>(outcome.messages) / seconds)
                      << " msg/s, rough throughput only — not a benchmark)";
        }
        std::cout << '\n';

        const bool healthy = framed_completely && counts_agree && timestamps_monotonic;
        return healthy ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "itch_count: " << error.what() << '\n';
        return 2;
    }
}